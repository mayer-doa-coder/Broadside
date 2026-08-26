// Broadside — render loop, input, orchestration.
// Phase 1: the loop skeleton and the simulation clock (PRD Requirement 3).
// Phase 2: the shader program and the first triangle.
// Phase 3: MVP matrices and the orbit camera.
// Phase 4: seven generators cover the eight PRD mesh roles; all are indexed.
// Phase 5: the L8 illumination model and the L9 Flat / Gouraud / Phong switch.
// Phase 6: the full L8 slide-60 material table and the second light.
// Phase 7: the static ship hierarchy - two 4-level chains and a two-axis gimbal.
// Phase 8: GPU ocean waves, and a hull that rocks because of the water under it.
//
// The one architectural rule this phase exists to establish:
// every animated quantity in every later phase must be a closed form of `now`
// (Requirement 12 — no pre-computed animation, no keyframe tables, no frame-indexed arrays).
// updateScene / renderScene are the only two places motion is allowed to come from.

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Camera.h"
#include "Lighting.h"
#include "Material.h"
#include "Mesh.h"
#include "Shader.h"
#include "Wave.h"

#include <cstddef>
#include <cstdio>
#include <string>

// ---------------------------------------------------------------------------
// Window / loop constants
// ---------------------------------------------------------------------------
static const int   WINDOW_WIDTH  = 1280;
static const int   WINDOW_HEIGHT = 720;
static const float MAX_DELTA     = 0.10f;   // spike guard: a dragged or stalled window must not
                                            // hand a 3-second dt to the update step

// ---------------------------------------------------------------------------
// Simulation clock
//
// `now` is the single source of truth for animation. It is wall time minus the
// total time spent paused, so it stays an exact closed form of glfwGetTime()
// (Requirement 12) while P can still freeze the scene for inspection.
// While paused dt is 0, so every future update becomes a no-op for free.
// ---------------------------------------------------------------------------
struct FrameClock {
    double lastWall    = 0.0;   // glfwGetTime() at the previous frame
    double pausedTotal = 0.0;   // accumulated wall time spent paused
    float  lastNow     = 0.0f;  // simulation time at the previous frame
    bool   paused      = false;
};

static FrameClock g_clock;

// ---------------------------------------------------------------------------
// GPU resources
//
// Seven GPU meshes cover the eight roles in PRD section 6; the later HUD reuses
// the quad. Every object is one of these under a different model matrix. The
// library is built at startup and rebuilt only when a tessellation value changes.
// ---------------------------------------------------------------------------
static Shader g_shader;

static Mesh g_cube;        // M1  hull, deck, cannon mount, enemy hull
static Mesh g_cylinder;    // M2  cannon barrel, masts x2, yards x2
static Mesh g_sphere;      // M3  cannonball, muzzle flash, smoke, splash
static Mesh g_quad;        // M4  sails x2, flag
static Mesh g_grid;        // M5  ocean surface
static Mesh g_ring;        // M6  splash ring
static Mesh g_cone;        // M7  bowsprit tip

// ---------------------------------------------------------------------------
// Tessellation state (guide 15.1)
//
// The two numbers that make the L9 comparison possible at all. They live here,
// not inside a generator, because the SAME count has to drive several meshes at
// once: the sphere's slices follow the barrel so that Demo A and Demo B degrade
// together when the grader presses '-'.
//
// rebuildMeshes() is called on keypress and NEVER per frame — regenerating a
// 128x128 grid every frame would allocate in the render loop, which AGENT.md
// forbids outright.
// ---------------------------------------------------------------------------
static int g_barrelSegments = 32;
static int g_oceanRes       = 64;

static void rebuildMeshes()
{
    g_cube     = makeCube();
    g_cylinder = makeCylinder(g_barrelSegments);
    g_sphere   = makeSphere(g_barrelSegments / 2, g_barrelSegments);
    g_quad     = makeQuad();
    g_grid     = makeGrid(g_oceanRes);
    g_ring     = makeRing(g_barrelSegments);
    g_cone     = makeCone(g_barrelSegments);

    const int tris = g_cube.triangleCount() + g_cylinder.triangleCount()
                   + g_sphere.triangleCount() + g_quad.triangleCount()
                   + g_grid.triangleCount() + g_ring.triangleCount()
                   + g_cone.triangleCount();
    const int verts = g_cube.vertexCount() + g_cylinder.vertexCount()
                    + g_sphere.vertexCount() + g_quad.vertexCount()
                    + g_grid.vertexCount() + g_ring.vertexCount()
                    + g_cone.vertexCount();

    std::printf("[mesh] rebuilt: barrelSegments=%d oceanRes=%d\n", g_barrelSegments, g_oceanRes);
    std::printf("       cube %5d tri | cylinder %5d tri | sphere %5d tri | quad %5d tri\n",
                g_cube.triangleCount(), g_cylinder.triangleCount(),
                g_sphere.triangleCount(), g_quad.triangleCount());
    std::printf("       grid %5d tri | ring     %5d tri | cone   %5d tri\n",
                g_grid.triangleCount(), g_ring.triangleCount(), g_cone.triangleCount());
    std::printf("       total %d triangles, %d vertices across 7 VAOs\n", tris, verts);
    std::fflush(stdout);
}

// Wireframe is the Phase 4 checkpoint instrument: it is how "the polygon count
// visibly changed" is verified rather than asserted (guide Phase 4 DONE WHEN).
static bool g_wireframe = false;

// ---------------------------------------------------------------------------
// Shading mode — the entire L9 lecture behind three keys (PRD 12.1)
//
// ONE shader program with a branch, never three programs. Three would be a scope
// violation and would also forfeit the "no state-change cost on mode switch"
// argument: switching mode here is a single glUniform1i, not a glUseProgram.
// ---------------------------------------------------------------------------
static int g_shadingMode = 2;                       // 0 Flat, 1 Gouraud, 2 Phong
static const char* SHADING_NAMES[3] = { "Flat", "Gouraud", "Phong" };

// Wired to keys in Phase 15; the shader already reads both, so the demo suite is
// a keypress away rather than a shader change.
static int g_useBlinn = 0;                          // 0 = (R.V)^n, 1 = (N.H)^n
static int g_termMask = 7;                          // ambient | diffuse | specular

// ---------------------------------------------------------------------------
// H — the hierarchy proof (PRD 5.3 and 8, CLAUDE.md).
//
// It zeroes roll, pitch and heave for the PARENT NODE ONLY. Everything below it
// keeps its own local transform, so the hull stops following the water while the
// sea goes on heaving straight through it, and the cannon, masts and sails stay
// welded to a ship that is now visibly floating in the wrong place. The absurdity
// IS the evidence: it can only look like that if the children really were being
// carried by the parent.
//
// The key binding is nominally Phase 15 work, but the branch it drives is written
// into the Phase 8 shipMatrix() snippet and is dead, untestable code without it.
// Phase 15 only has to leave it alone.
// ---------------------------------------------------------------------------
static bool g_hierarchy = true;

// ---------------------------------------------------------------------------
// Lighting (PRD 11.1) — TWO lights, no more, no fewer.
//
// Both are live as of Phase 6, and they are deliberately of DIFFERENT kinds,
// because the pair is what the L8 light model is being demonstrated with:
//
//   Light 0  directional  L = -normalize(direction)  no attenuation   L8 s19
//   Light 1  point        L = normalize(pos - frag)  1/(a0+a1d+a2d^2) L8 s21
//
// The shader sums them in one loop (L8 s56) — it does not special-case either.
// ---------------------------------------------------------------------------
struct Light {
    int       type;          // 0 = directional, 1 = point
    glm::vec3 direction;     // directional only
    glm::vec3 position;      // point only
    glm::vec3 diffuse;       // I_d
    glm::vec3 specular;      // I_s
    glm::vec3 attenuation;   // (a0, a1, a2)  — L8 slide 21
    float     enabled;
};

// Light 0 — the sun. Directional: parallel rays, no attenuation (L8 s19).
// The direction is deliberately low and grazing so the ocean throws a long
// specular streak in Phase 8, which is what makes Demo A work.
static const Light SUN = {
    0,
    glm::vec3(-0.4f, -0.35f, -0.5f),
    glm::vec3(0.0f),
    glm::vec3(1.00f, 0.96f, 0.86f),        // warm afternoon
    glm::vec3(1.00f, 1.00f, 0.98f),
    glm::vec3(1.0f, 0.0f, 0.0f),           // unused for a directional light
    1.0f
};

// Light 1 — the muzzle flash. A POINT light, and the only reason attenuation
// exists in this project at all.
//
// As of Phase 7 this light rides the hierarchy: its position is overwritten every
// frame with the muzzle's world point, extracted from the barrel frame. Phase 6
// had to park it at an arbitrary spot because there was no cannon to attach it to.
//
// Phase 12 adds the only piece still missing — switching `enabled` off again
// 0.15 s after each shot. It stays lit here because attenuation is not checkable
// in a still frame if the light is dark, and because a lit muzzle is the clearest
// possible marker of where the hierarchy says the barrel's mouth actually is.
//
// Naming: MUZZLE_LIGHT is the light; MUZZLE_FLASH (Material.h) is the emissive
// material on the sphere drawn at the same point. Two different L8 concepts,
// deliberately kept as two objects.
static const Light MUZZLE_LIGHT = {
    1,
    glm::vec3(0.0f),                       // unused for a point light
    glm::vec3(0.0f),                       // overwritten per frame from the barrel frame
    glm::vec3(1.00f, 0.75f, 0.35f),        // orange flare
    glm::vec3(1.00f, 0.85f, 0.60f),
    glm::vec3(1.0f, 0.09f, 0.032f),        // a0, a1, a2  — L8 slide 21
    1.0f
};

// I_a_global — cool sky-scattered fill (L8 slide 57)
static const glm::vec3 GLOBAL_AMBIENT(0.15f, 0.15f, 0.18f);

// Materials live in src/Material.h as of Phase 6: BRASS, POLISHED_SILVER,
// BLACK_PLASTIC (verbatim, L8 slide 60), OCEAN, HULL_WOOD, SAILCLOTH (tuned),
// and MUZZLE_FLASH (emissive only, L8 slide 55).

// ---------------------------------------------------------------------------
// Camera and viewport state
// ---------------------------------------------------------------------------
static OrbitCamera g_camera;

// The projection needs the real framebuffer size every frame, or resizing the
// window stretches the image instead of widening the field of view.
static int g_fbWidth  = WINDOW_WIDTH;
static int g_fbHeight = WINDOW_HEIGHT;

static bool   g_dragging    = false;
static double g_lastCursorX = 0.0;
static double g_lastCursorY = 0.0;

static const float MOUSE_SENSITIVITY = 0.006f;   // radians per pixel
static const float KEY_ZOOM_RATE     = 1.6f;     // e-folds per second (W/S)
static const float SCROLL_ZOOM_STEP  = 0.12f;    // e-folds per wheel notch

// ---------------------------------------------------------------------------
// GLFW callbacks
// ---------------------------------------------------------------------------
static void glfwErrorCallback(int code, const char* description)
{
    std::fprintf(stderr, "[GLFW error %d] %s\n", code, description);
}

static void framebufferSizeCallback(GLFWwindow* /*window*/, int width, int height)
{
    glViewport(0, 0, width, height);
    g_fbWidth  = width;
    g_fbHeight = height;
}

// Scroll has no polling API in GLFW — it only arrives as an event.
static void scrollCallback(GLFWwindow* /*window*/, double /*xoffset*/, double yoffset)
{
    g_camera.zoom((float)-yoffset * SCROLL_ZOOM_STEP);
}

// ---------------------------------------------------------------------------
// Input
//
// Held keys (camera, aim) poll with glfwGetKey. Toggles (P, and later H/B/TAB)
// must be edge-triggered or one press flips the state every frame it is held.
// ---------------------------------------------------------------------------
static bool keyPressedOnce(GLFWwindow* window, int key)
{
    static bool wasDown[GLFW_KEY_LAST + 1] = { false };

    if (key < 0 || key > GLFW_KEY_LAST)   // GLFW_KEY_UNKNOWN is -1
        return false;

    const bool isDown  = (glfwGetKey(window, key) == GLFW_PRESS);
    const bool pressed = isDown && !wasDown[key];
    wasDown[key] = isDown;
    return pressed;
}

// realDt is WALL time, not simulation time. The camera must keep responding while
// the scene is paused — "freeze the frame, then orbit around it to inspect the
// highlight" is the whole point of the P key, and simulation dt is 0 there.
static void processInput(GLFWwindow* window, float realDt)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GLFW_TRUE);

    // P — freeze time to study a single frame (PRD section 10)
    if (keyPressedOnce(window, GLFW_KEY_P)) {
        g_clock.paused = !g_clock.paused;
        std::printf("[input] %s\n", g_clock.paused ? "paused" : "resumed");
        std::fflush(stdout);
    }

    // 1 / 2 / 3 — Flat / Gouraud / Phong (PRD section 10).
    // One uniform changes. No program switch, no state churn — that is the whole
    // argument for a single program with a branch (PRD 12.1).
    for (int mode = 0; mode < 3; ++mode) {
        if (keyPressedOnce(window, GLFW_KEY_1 + mode)) {
            g_shadingMode = mode;
            std::printf("[input] shading mode %d (%s)\n", mode, SHADING_NAMES[mode]);
            std::fflush(stdout);
        }
    }

    // W / S — zoom (PRD section 10)
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) g_camera.zoom(-KEY_ZOOM_RATE * realDt);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) g_camera.zoom(+KEY_ZOOM_RATE * realDt);

    // + / - — tessellation (guide 15.1). Edge-triggered: holding the key must not
    // rebuild every frame. Both the main row and the numeric keypad are accepted
    // because '+' is a shifted key on most layouts and glfwGetKey reports the
    // unshifted GLFW_KEY_EQUAL.
    // Each key is polled into its own variable first: keyPressedOnce has the side
    // effect of latching, so a short-circuiting || would leave the keypad key's
    // latch stale and make it fire on the frame after it was released.
    const bool rowUp     = keyPressedOnce(window, GLFW_KEY_EQUAL);
    const bool padUp     = keyPressedOnce(window, GLFW_KEY_KP_ADD);
    const bool rowDown   = keyPressedOnce(window, GLFW_KEY_MINUS);
    const bool padDown   = keyPressedOnce(window, GLFW_KEY_KP_SUBTRACT);
    const bool tessUp    = rowUp   || padUp;
    const bool tessDown  = rowDown || padDown;

    if (tessUp || tessDown) {
        const int oldBarrel = g_barrelSegments;
        const int oldOcean  = g_oceanRes;

        if (tessUp) {
            g_barrelSegments = (g_barrelSegments * 2 <  64) ? g_barrelSegments * 2 : 64;
            g_oceanRes       = (g_oceanRes       * 2 < 128) ? g_oceanRes       * 2 : 128;
        }
        if (tessDown) {
            g_barrelSegments = (g_barrelSegments / 2 >  6) ? g_barrelSegments / 2 : 6;
            g_oceanRes       = (g_oceanRes       / 2 >  8) ? g_oceanRes       / 2 : 8;
        }

        // Already at a clamp: skip the rebuild rather than re-uploading identical
        // buffers, so a grader leaning on '-' does not churn the GPU.
        if (g_barrelSegments != oldBarrel || g_oceanRes != oldOcean)
            rebuildMeshes();
    }

    // H — disable / enable hull rocking in the hierarchy (PRD section 10).
    // The single clearest hierarchical-transform demonstration in the project:
    // one bool, applied to the ROOT only, and the whole ship stops following the
    // sea while the sea carries on without it.
    if (keyPressedOnce(window, GLFW_KEY_H)) {
        g_hierarchy = !g_hierarchy;
        std::printf("[input] hierarchy %s - the hull %s the waves\n",
                    g_hierarchy ? "on" : "off",
                    g_hierarchy ? "follows" : "ignores");
        std::fflush(stdout);
    }

    // F — wireframe. The Phase 4 checkpoint tool (CLAUDE.md debug snippets).
    if (keyPressedOnce(window, GLFW_KEY_F)) {
        g_wireframe = !g_wireframe;
        std::printf("[input] wireframe %s\n", g_wireframe ? "on" : "off");
        std::fflush(stdout);
    }

    // Left-drag orbits. The cursor position is latched on the press rather than
    // read continuously, so picking the window up mid-screen does not snap the view.
    double cursorX = 0.0, cursorY = 0.0;
    glfwGetCursorPos(window, &cursorX, &cursorY);
    const bool leftDown = (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS);

    if (leftDown && !g_dragging) {
        g_dragging    = true;
        g_lastCursorX = cursorX;
        g_lastCursorY = cursorY;
    } else if (!leftDown) {
        g_dragging = false;
    }

    if (g_dragging) {
        const float dx = (float)(cursorX - g_lastCursorX);
        const float dy = (float)(cursorY - g_lastCursorY);
        g_lastCursorX = cursorX;
        g_lastCursorY = cursorY;

        // Signs give "grab the object and turn it": drag right and the near face
        // swings right, which means the camera itself orbits left.
        g_camera.orbit(-dx * MOUSE_SENSITIVITY, dy * MOUSE_SENSITIVITY);
    }

    // Aim and light-toggle keys land here in later phases.
}

// ===========================================================================
// PLAYER SHIP — the transform hierarchy (PRD section 7, Requirement 6e)
//
// Two independent 4-level chains hang off one root:
//
//   hull -> deck -> mount -> yoke(R_y az) -> barrel(R_x el) -> muzzle
//   hull -> deck -> mast  -> yard         -> sail
//
// The cannon chain is the load-bearing one. Because the barrel's world matrix is
//   M_ship * M_deck * M_mount * R_y(az) * R_x(el)
// the muzzle inherits the ship's motion for free, and Phase 12's projectile comes
// out along the barrel's true world direction with no special-case code anywhere.
// That inheritance IS the reason the hierarchy exists; it is not decoration.
//
// THE RULE THIS SECTION EXISTS TO ENFORCE (AGENT.md, the single most common bug):
// a frame carries position and orientation ONLY. Never scale. The hull is scaled
// 4x in Z; if that scale reached the deck frame, every mast, yard, sail and the
// entire cannon below it would come out as a 4x-stretched noodle. Scale appears
// exclusively inside a drawMesh() argument, and there is not one glm::scale in
// buildShipFrames() below - that absence is the guarantee.
// ===========================================================================

// The ship sits with its waterline at y = 0 and its bow toward +Z.
static const glm::vec3 SHIP_POS(0.0f, 0.12f, 0.0f);

// Level 1 — hull. A stretched cube, and the reason the normal matrix (M^-1)^T
// matters: 1.2 x 0.6 x 4.0 is about as non-uniform as a scale gets.
static const glm::vec3 HULL_DIMS(1.20f, 0.75f, 4.00f);

// Level 2 — deck. DECK_Y lands the plank exactly on the hull top (0.12 + 0.30).
static const float     DECK_Y = 0.375f;
static const glm::vec3 DECK_DIMS(1.10f, 0.10f, 3.80f);
static const glm::vec3 BOWSPRIT_POS(0.0f, 0.02f, 2.00f);
static const glm::vec3 BOWSPRIT_DIMS(0.17f, 0.95f, 0.17f);

// Level 3 — cannon mount, out on the starboard rail where a broadside gun lives.
static const glm::vec3 MOUNT_POS(0.48f, 0.14f, 0.55f);
static const glm::vec3 MOUNT_DIMS(0.36f, 0.28f, 0.36f);

// Level 4 — yoke (traverse) and barrel (elevate): the two-axis gimbal.
static const float     YOKE_Y        = 0.24f;   // trunnion height above the mount
static const glm::vec3 TRUNNION_DIMS(0.13f, 0.40f, 0.13f);
static const float     BARREL_LENGTH = 1.45f;
static const float     BARREL_RADIUS = 0.095f;
static const float     BARREL_OFFSET = 0.45f;   // how far the barrel is run out from the pivot

// The muzzle is the barrel's +Z tip. Deriving it from the geometry constants
// rather than typing a number means it cannot drift out of sync with the barrel
// the day someone lengthens it - and Phase 12 fires from exactly this point.
static const float     MUZZLE_Z = BARREL_OFFSET + 0.5f * BARREL_LENGTH;

// Rigging. Fore is nearer the bow (+Z); the main mast is taller, as it should be.
static const float MAST_RADIUS   = 0.055f;
static const float YARD_RADIUS   = 0.035f;
static const float MAST_FORE_Z   =  0.95f, MAST_FORE_H = 2.30f;
static const float MAST_MAIN_Z   = -0.55f, MAST_MAIN_H = 2.85f;
static const float YARD_HEIGHT_F = 0.72f;       // fraction of mast height the yard is crossed at
static const float YARD_FORE_SPAN = 1.55f, SAIL_FORE_W = 1.40f, SAIL_FORE_H = 1.15f;
static const float YARD_MAIN_SPAN = 1.90f, SAIL_MAIN_W = 1.70f, SAIL_MAIN_H = 1.45f;
static const glm::vec3 FLAG_DIMS(0.55f, 0.30f, 1.0f);

// ---------------------------------------------------------------------------
// Aim state — static in Phase 7. Phase 11 makes these track the enemy.
//
// Both are deliberately NON-ZERO. A composite T * R_y * R_x * S that is only ever
// exercised on one axis proves nothing about the other, and a gimbal parked at
// identity is indistinguishable from a gimbal that was never built.
// ---------------------------------------------------------------------------
// 138 degrees is abaft the starboard beam - a real broadside, trained aft toward
// where PRD 10 puts the patrolling enemy. It is also very nearly perpendicular to
// the default camera yaw, which is what lets the barrel be SEEN at its full length
// instead of foreshortened into the mount.
static float g_azimuth   = glm::radians(138.0f);  // traverse, about the yoke's +Y
static float g_elevation = glm::radians(16.0f);   // elevate,  about the trunnion axis

// ---------------------------------------------------------------------------
// Every node of the ship's transform chain, computed once per frame.
//
// These are FRAMES: unscaled, so any of them can safely be a parent. The chain is
// built in updateScene rather than inside the draw pass because Light 1 sits at
// the muzzle, and a uniform has to be uploaded before the draws that read it -
// including the ocean's, which is drawn first.
// ---------------------------------------------------------------------------
struct ShipFrames {
    glm::mat4 ship;
    glm::mat4 deck;
    glm::mat4 mount, yoke, barrel;
    glm::mat4 mastFore, yardFore;
    glm::mat4 mastMain, yardMain, flagMain;
    glm::vec3 muzzlePos;    // world position of the barrel tip     (w = 1)
    glm::vec3 muzzleFwd;    // world firing direction, unit length   (w = 0)
};

static ShipFrames g_ship;

// PRD 9.2 — the root matrix: T(pos) . R_z(roll(t)) . R_x(pitch(t)).
//
// This one function is the whole of Phase 8 on the CPU side. Nothing else in the
// ship changed: the hull, deck, mount, gimbal, masts, yards, sails and flag all
// rock because their frames hang off this matrix, and not one of them knows the
// sea exists. That is the payoff the Phase 7 hierarchy was built for.
//
// `pos.y` is FREEBOARD - how high the hull centre rides above mean sea level -
// so the heave adds to it rather than replacing it. The guide's snippet writes
// `y = waveHeight(...)` because its hull sits at y = 0; ours floats at 0.12.
//
// Roll and pitch come from the SLOPE of the same wave the GPU is drawing, sampled
// at the hull's own (x, z). The ship therefore tilts because of the water beneath
// it, not from an unrelated sine that happens to look similar (PRD 9.2).
static glm::mat4 shipMatrix(const glm::vec3& pos, float t, bool hierarchyEnabled)
{
    float y     = pos.y + waveHeight(pos.x, pos.z, t);
    float roll  = std::atan(waveSlopeX(pos.x, t)) * WAVE_FOLLOW;
    float pitch = std::atan(waveSlopeZ(pos.z, t)) * WAVE_FOLLOW;

    // The H key. Parent node only - the children are not consulted.
    if (!hierarchyEnabled) { y = pos.y; roll = 0.0f; pitch = 0.0f; }

    glm::mat4 M = glm::translate(glm::mat4(1.0f), glm::vec3(pos.x, y, pos.z));

    // R_z: heel. About +Z a positive rotation leans the masts to port, which is
    // what a surface rising to starboard (waveSlopeX > 0) should do.
    M = glm::rotate(M, roll, glm::vec3(0.0f, 0.0f, 1.0f));

    // R_x: trim, about -X for the same reason the gun elevates about -X. A
    // positive rotation about +X leans the masts FORWARD and drops the bow, so a
    // sea rising toward the bow would push the bow under instead of lifting it.
    M = glm::rotate(M, pitch, glm::vec3(-1.0f, 0.0f, 0.0f));
    return M;
}

static ShipFrames buildShipFrames(const glm::mat4& root)
{
    ShipFrames f;
    f.ship = root;

    // Level 2 — the deck frame. Note what is NOT here: HULL_DIMS.
    f.deck = f.ship * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, DECK_Y, 0.0f));

    // Level 3 — the mount, a child of the deck's frame.
    f.mount = f.deck * glm::translate(glm::mat4(1.0f), MOUNT_POS);

    // Level 4 — the gimbal, and the order matters.
    // Azimuth FIRST (traverse), then elevation, so the barrel elevates within the
    // already-traversed frame. Swap them and the gun tilts before it turns, which
    // is a different mechanism that points somewhere else entirely.
    f.yoke = f.mount * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, YOKE_Y, 0.0f))
                     * glm::rotate(glm::mat4(1.0f), g_azimuth, glm::vec3(0.0f, 1.0f, 0.0f));

    // Elevation turns about the trunnion, which lies along the yoke's local X.
    // The axis is -X, not +X: about +X a right-handed rotation swings the barrel's
    // +Z forward vector toward -Y, so a positive "elevation" would depress the gun.
    f.barrel = f.yoke * glm::rotate(glm::mat4(1.0f), g_elevation, glm::vec3(-1.0f, 0.0f, 0.0f));

    // The muzzle point — the empty transform at the end of the chain (PRD s7).
    // w = 1 for the POSITION, w = 0 for the DIRECTION. Getting these backwards is
    // the bug that sends the cannonball toward the world origin instead of out of
    // the barrel; w = 0 is what makes the direction immune to translation.
    f.muzzlePos = glm::vec3(f.barrel * glm::vec4(0.0f, 0.0f, MUZZLE_Z, 1.0f));
    f.muzzleFwd = glm::normalize(glm::vec3(f.barrel * glm::vec4(0.0f, 0.0f, 1.0f, 0.0f)));

    // The second chain: two masts, each carrying a yard. The sail hangs off the
    // yard frame in drawShip, so the full depth is deck -> mast -> yard -> sail.
    f.mastFore = f.deck     * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, MAST_FORE_Z));
    f.yardFore = f.mastFore * glm::translate(glm::mat4(1.0f),
                                             glm::vec3(0.0f, MAST_FORE_H * YARD_HEIGHT_F, 0.0f));
    f.mastMain = f.deck     * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, MAST_MAIN_Z));
    f.yardMain = f.mastMain * glm::translate(glm::mat4(1.0f),
                                             glm::vec3(0.0f, MAST_MAIN_H * YARD_HEIGHT_F, 0.0f));

    // The PRD places the flag below the main yard in the scene graph. Keep that
    // exact parent relationship, then move from the yard up to the masthead.
    // Phase 9 can add its independent flag rotation after this frame.
    f.flagMain = f.yardMain * glm::translate(glm::mat4(1.0f),
                                             glm::vec3(0.0f, MAST_MAIN_H * (1.0f - YARD_HEIGHT_F), 0.0f));
    return f;
}

// ---------------------------------------------------------------------------
// Scene update — ALL motion is computed here, from `now`, every frame.
//
// The chain is rebuilt from `now` every frame, never cached and never stored.
// waveHeight and its two slopes are closed forms of t with no state of any kind,
// so the ship's pose at any instant is DERIVED, not looked up - which is what
// Requirement 12 asks for. Rewind the clock and you get the identical pose back.
// ---------------------------------------------------------------------------
static void updateScene(float now, float /*dt*/)
{
    g_ship = buildShipFrames(shipMatrix(SHIP_POS, now, g_hierarchy));

    // Phase 9  : sail flutter and flag, layered below the root
    // Phase 10 : enemy patrol position
    // Phase 11 : cannon azimuth and elevation
    // Phase 12 : projectile position from p0 + v0*tau + 0.5*g*tau^2
    // Phase 14 : particle ages
}

// ---------------------------------------------------------------------------
// Resource creation — runs once, at startup. Nothing here may happen per frame.
// ---------------------------------------------------------------------------
static bool initResources()
{
    // The illumination model is spliced into BOTH stages from one C++ string, so
    // Gouraud (vertex) and Phong (fragment) evaluate literally the same code
    // (guide 5.2, implementation note).
    // Two shared blocks, one splice: the illumination model and the wave surface.
    // Both stages get both. The fragment stage never calls oceanSurface, and an
    // unused uniform is optimised out, so that costs nothing and keeps the splice
    // a single concatenation instead of a per-stage special case.
    const std::string commonGLSL = std::string(LIGHTING_GLSL) + WAVE_GLSL;

    if (!g_shader.loadFromFiles("shaders/phong.vert", "shaders/phong.frag", commonGLSL))
        return false;

    // The whole mesh library is generated here, once. Every later phase draws
    // these same seven VAOs under different model matrices — that reuse is the
    // optimization claimed in Requirement 11, not an afterthought.
    rebuildMeshes();

    if (!g_cube.valid() || !g_cylinder.valid() || !g_sphere.valid() ||
        !g_quad.valid() || !g_grid.valid() || !g_ring.valid() || !g_cone.valid()) {
        std::fprintf(stderr, "[mesh] one or more meshes failed to upload\n");
        return false;
    }
    return true;
}

// Every GL object must be released while the context is still current. These are
// globals, so their destructors would otherwise run after glfwTerminate has
// already torn the context down.
static void destroyResources()
{
    g_cube.destroy();
    g_cylinder.destroy();
    g_sphere.destroy();
    g_quad.destroy();
    g_grid.destroy();
    g_ring.destroy();
    g_cone.destroy();
    g_shader.destroy();
}

// ---------------------------------------------------------------------------
// The ocean. Drawn at y = 0, which IS mean sea level: waveHeight() returns
// displacement about that plane, so the grid needs no vertical offset at all.
//
// 48 units square at the default 64x64 grid gives cells of 0.75 against a
// wavelength of 11.4 - about fifteen samples per wave, smooth enough to read as
// water while still leaving plenty of room for '-' to coarsen it for Demo A.
// ---------------------------------------------------------------------------
static const float OCEAN_EXTENT = 48.0f;

static int g_drawCalls = 0;   // Phase 17 budget check, wired up from day one
static int g_triangles = 0;

// ---------------------------------------------------------------------------
// One light -> seven uniforms. The struct is flattened by name because GLSL
// exposes uLights[i].field as an individually addressable uniform.
//
// Called twice per frame, outside the object loop: the lights do not change
// between objects, so setting them per object would be pure waste (Requirement 11).
// ---------------------------------------------------------------------------
static void setLight(const Shader& shader, int index, const Light& light)
{
    char name[64];
    const int i = index;

    std::snprintf(name, sizeof(name), "uLights[%d].type", i);
    shader.setInt(name, light.type);
    std::snprintf(name, sizeof(name), "uLights[%d].direction", i);
    shader.setVec3(name, light.direction);
    std::snprintf(name, sizeof(name), "uLights[%d].position", i);
    shader.setVec3(name, light.position);
    std::snprintf(name, sizeof(name), "uLights[%d].diffuse", i);
    shader.setVec3(name, light.diffuse);
    std::snprintf(name, sizeof(name), "uLights[%d].specular", i);
    shader.setVec3(name, light.specular);
    std::snprintf(name, sizeof(name), "uLights[%d].attenuation", i);
    shader.setVec3(name, light.attenuation);
    std::snprintf(name, sizeof(name), "uLights[%d].enabled", i);
    shader.setFloat(name, light.enabled);
}

// ---------------------------------------------------------------------------
// One material -> five uniforms (L8 slide 60 / slide 55).
//
// This is the one uniform group that CANNOT be hoisted out of the object loop:
// the material is the per-object half of the illumination equation, so it has to
// change between draws. Everything else — view, projection, uViewPos, both
// lights, the mode flags — is identical for every object in the frame and is set
// once above (Requirement 11, uniform hoisting).
// ---------------------------------------------------------------------------
static void setMaterial(const Shader& shader, const Material& m)
{
    shader.setVec3("uKa", m.ka);
    shader.setVec3("uKd", m.kd);
    shader.setVec3("uKs", m.ks);
    shader.setVec3("uEmission", m.emission);
    shader.setFloat("uShininess", m.shininess);
}

// ---------------------------------------------------------------------------
// drawMesh takes the object's FINAL model matrix — scale included — and its
// material, exactly the shape the guide's Phase 7 hierarchy calls it in
// (drawMesh(cubeMesh, deck * scale(...), BLACK_PLASTIC)).
//
// That shape is what keeps a parent's scale out of its children: the frames in
// ShipFrames stay unscaled, and glm::scale appears only in the argument to this
// call, never above it.
// ---------------------------------------------------------------------------
static void drawMesh(const Mesh& mesh, const glm::mat4& model, const Material& material)
{
    // (M^-1)^T on the CPU, once per object per frame (guide 5.3). The hull is a
    // 1.2 x 0.6 x 4.0 cube — under that non-uniform scale a naively transformed
    // normal shears off the true surface and the whole hull lights wrongly.
    const glm::mat3 normalMatrix = glm::mat3(glm::transpose(glm::inverse(model)));

    g_shader.setMat4("uModel", model);
    g_shader.setMat3("uNormalMatrix", normalMatrix);
    setMaterial(g_shader, material);
    mesh.draw();
    ++g_drawCalls;
    g_triangles += mesh.triangleCount();
}

// Sails and the flag are single quads: a camera behind one sees only its back
// face, and with culling on that is nothing at all. Culling stays ON for the
// other twelve draws (Requirement 11); these three turn it off for one draw each,
// which is six GL state calls a frame against a scene of ~19k triangles.
//
// Known consequence, stated rather than hidden: the shader lights the back of a
// sail with the front normal, so a backlit sail reads as if it were front-lit.
// Correcting that needs gl_FrontFacing in the fragment shader, which is not a
// Phase 7 change.
static void drawMeshTwoSided(const Mesh& mesh, const glm::mat4& model, const Material& material)
{
    glDisable(GL_CULL_FACE);
    drawMesh(mesh, model, material);
    if (!g_wireframe) glEnable(GL_CULL_FACE);   // wireframe wants culling off for the whole frame
}

// ---------------------------------------------------------------------------
// Mesh-axis fixups. makeCylinder and makeCone build along +Y (Mesh.h); these put
// a unit mesh on a different axis.
//
// These belong in a drawMesh ARGUMENT and never in a frame. The barrel frame's
// +Z is the firing direction Phase 12 reads out of the hierarchy — bake a 90
// degree turn into it and the cannonball leaves through the side of the barrel.
// ---------------------------------------------------------------------------
static glm::mat4 alongZ()   // +Y  ->  +Z
{
    return glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
}
static glm::mat4 alongX()   // +Y  ->  -X  (sign is irrelevant for a symmetric rod)
{
    return glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
}

static glm::mat4 translated(const glm::vec3& v)
{
    return glm::translate(glm::mat4(1.0f), v);
}
static glm::mat4 scaled(const glm::vec3& v)
{
    return glm::scale(glm::mat4(1.0f), v);
}

// ---------------------------------------------------------------------------
// One mast, its yard, and its sail — the rigging chain, drawn twice.
//
// Called with the fore and main frames in turn: same code, same meshes, different
// matrices. That reuse is the mesh-reuse optimization argument made concrete —
// five cylinder draws in this scene all come out of one VAO.
// ---------------------------------------------------------------------------
static void drawRig(const glm::mat4& mastFrame, const glm::mat4& yardFrame,
                    float mastHeight, float yardSpan, float sailW, float sailH)
{
    // Mast: the cylinder is centred on its own origin, so it is lifted by half its
    // height to stand ON the deck rather than half through it.
    drawMesh(g_cylinder,
             mastFrame * translated(glm::vec3(0.0f, mastHeight * 0.5f, 0.0f))
                       * scaled(glm::vec3(MAST_RADIUS * 2.0f, mastHeight, MAST_RADIUS * 2.0f)),
             HULL_WOOD);

    // Yard: a child of the mast frame, laid across the ship on the local X axis.
    drawMesh(g_cylinder,
             yardFrame * alongX()
                       * scaled(glm::vec3(YARD_RADIUS * 2.0f, yardSpan, YARD_RADIUS * 2.0f)),
             HULL_WOOD);

    // Sail: a child of the YARD, hanging below it. Phase 9 puts the flutter
    // rotation R_z(A sin(wt + phi)) between the yard frame and this quad, so the
    // sail swings about the yard exactly as PRD 9.7 specifies.
    drawMeshTwoSided(g_quad,
                     yardFrame * translated(glm::vec3(0.0f, -sailH * 0.5f, 0.0f))
                               * scaled(glm::vec3(sailW, sailH, 1.0f)),
                     SAILCLOTH);
}

// ---------------------------------------------------------------------------
// The ship. Thirteen draws, five materials, four unique meshes.
//
// Read the left-hand side of every drawMesh call: a frame, then translate and
// scale. No frame is ever built FROM a scaled matrix — that is the whole
// discipline of this function.
// ---------------------------------------------------------------------------
static void drawShip(const ShipFrames& f)
{
    // --- Level 1: hull -------------------------------------------------------
    drawMesh(g_cube, f.ship * scaled(HULL_DIMS), HULL_WOOD);

    // --- Level 2: deck, and the bowsprit at the bow --------------------------
    // f.deck is the hull's FRAME, not the hull's matrix. HULL_DIMS stops here.
    drawMesh(g_cube, f.deck * scaled(DECK_DIMS), HULL_WOOD);
    drawMesh(g_cone,
             f.deck * translated(BOWSPRIT_POS) * alongZ() * scaled(BOWSPRIT_DIMS),
             HULL_WOOD);

    // --- Level 3: cannon mount ----------------------------------------------
    drawMesh(g_cube, f.mount * scaled(MOUNT_DIMS), BLACK_PLASTIC);

    // --- Level 4: the gimbal -------------------------------------------------
    // The trunnion is Polished Silver (n_s 89.6) and the barrel is Brass
    // (n_s 27.9), touching each other. That adjacency is a deliberate side-by-side
    // n_s comparison a grader can make in one glance without pressing a key
    // (PRD s13) — a tiny hard pinpoint against a broad warm glint.
    drawMesh(g_cylinder,
             f.yoke * alongX() * scaled(TRUNNION_DIMS),
             POLISHED_SILVER);
    drawMesh(g_cylinder,
             f.barrel * translated(glm::vec3(0.0f, 0.0f, BARREL_OFFSET)) * alongZ()
                      * scaled(glm::vec3(BARREL_RADIUS * 2.0f, BARREL_LENGTH, BARREL_RADIUS * 2.0f)),
             BRASS);

    // --- the second chain: masts, yards, sails -------------------------------
    drawRig(f.mastFore, f.yardFore, MAST_FORE_H, YARD_FORE_SPAN, SAIL_FORE_W, SAIL_FORE_H);
    drawRig(f.mastMain, f.yardMain, MAST_MAIN_H, YARD_MAIN_SPAN, SAIL_MAIN_W, SAIL_MAIN_H);

    // Flag at the main masthead, parented to the main yard exactly as in PRD s7.
    // The half-width shift makes the flag's left edge meet the mast.
    drawMeshTwoSided(g_quad,
                     f.flagMain * translated(glm::vec3(FLAG_DIMS.x * 0.5f, 0.0f, 0.0f))
                                * scaled(FLAG_DIMS),
                     SAILCLOTH);
}

// ---------------------------------------------------------------------------
// Scene render — draw calls only. No state mutation belongs here.
// ---------------------------------------------------------------------------
static void renderScene(float now)
{
    g_shader.use();

    // --- per-frame uniforms, hoisted out of the object loop (Requirement 11) ---
    g_shader.setMat4("uView", g_camera.view());
    g_shader.setMat4("uProjection", g_camera.projection(g_fbWidth, g_fbHeight));

    // uViewPos is the single most important uniform in this phase. Specular is
    // view-dependent by definition (L8 s44), so a stale uViewPos gives a highlight
    // that is nailed to the surface and does not move when the camera orbits.
    // That symptom means this line, every time (AGENT.md section 5).
    g_shader.setVec3("uViewPos", g_camera.position());

    g_shader.setVec3("uGlobalAmbient", GLOBAL_AMBIENT);
    setLight(g_shader, 0, SUN);

    // Light 1 now rides the hierarchy: its position is the muzzle world point the
    // transform chain produced this frame (L8 s21 point light + attenuation).
    // Phase 12 changes one field here — `enabled` — and nothing else.
    Light muzzleLight     = MUZZLE_LIGHT;
    muzzleLight.position  = g_ship.muzzlePos;
    setLight(g_shader, 1, muzzleLight);

    g_shader.setInt("uShadingMode", g_shadingMode);
    g_shader.setInt("uUseBlinn", g_useBlinn);
    g_shader.setInt("uTermMask", g_termMask);

    // The sea's clock. Simulation time, not wall time, so P freezes the water and
    // the ship together and a single frame can be studied (PRD section 10).
    g_shader.setFloat("uTime", now);

    // Material uniforms are deliberately NOT here any more — they moved into
    // drawMesh below, because they are the only per-object lighting data.

    g_drawCalls = 0;
    g_triangles = 0;

    // --- ocean ---------------------------------------------------------------
    // n_s = 160 and a nearly white k_s. The stored grid stays flat; Phase 8
    // displaces it in the vertex shader and creates the moving sun-streak.
    // uIsOcean is the ONLY per-object flag in the frame, and it is set around a
    // single draw rather than inside drawMesh: exactly one object in this scene
    // is water, and leaving it set would make the hull ripple too.
    g_shader.setInt("uIsOcean", 1);
    drawMesh(g_grid, scaled(glm::vec3(OCEAN_EXTENT, 1.0f, OCEAN_EXTENT)), OCEAN);
    g_shader.setInt("uIsOcean", 0);

    // --- the player ship -----------------------------------------------------
    drawShip(g_ship);

    // --- the muzzle flash ----------------------------------------------------
    // A sphere at the end of the chain, wearing the emissive-only material, at the
    // same point as Light 1. Two separate L8 concepts share one position and must
    // not be confused: the SPHERE is emission (L8 s55) and lights only itself; the
    // orange falloff on the deck and barrel around it is the POINT LIGHT (L8 s21)
    // and lights everything else. A local illumination model has no mechanism
    // connecting the two (L8 s10-11) — which is exactly why the light has to be
    // declared separately at all.
    //
    // It doubles as the Phase 7 verification instrument: if this sphere is not
    // sitting in the barrel's mouth, the muzzle extraction is wrong, and Phase 12
    // would have fired the cannonball from the wrong place.
    drawMesh(g_sphere,
             translated(g_ship.muzzlePos) * scaled(glm::vec3(0.16f)),
             MUZZLE_FLASH);

    // Phase 10 : the same drawShip() call again, with the enemy's matrix
}

// ---------------------------------------------------------------------------
// Frame readout — the Phase 1 checkpoint evidence.
// Window title updates 4x/s; the console prints once a second so dt is
// verifiable without a debugger. Zero cost, and it grows into the HUD in Phase 16.
// ---------------------------------------------------------------------------
static void reportFrame(GLFWwindow* window, float now, float dt)
{
    static double nextTitle    = 0.0;
    static double nextConsole  = 0.0;
    static double windowStart  = 0.0;   // wall time this averaging window opened
    static int    frames       = 0;
    static float  dtAccum      = 0.0f;
    static bool   seeded       = false;

    ++frames;
    dtAccum += dt;

    const double wall = glfwGetTime();

    // Seed the intervals on the first frame — averaging a single startup frame
    // reports a meaningless FPS.
    if (!seeded) {
        seeded      = true;
        nextTitle   = wall + 0.25;
        nextConsole = wall + 1.0;
        windowStart = wall;
        frames      = 0;
        dtAccum     = 0.0f;
        return;
    }

    if (wall < nextTitle)
        return;
    nextTitle = wall + 0.25;

    // dt is simulation time (0 while paused); FPS is wall time, because the
    // renderer keeps working at full rate whether or not the clock is running.
    const double elapsed = wall - windowStart;
    const float  avgDt   = dtAccum / (float)frames;
    const float  fps     = (elapsed > 0.0) ? (float)(frames / elapsed) : 0.0f;
    windowStart = wall;

    // The Phase 4 evidence is the triangle count next to the segment counts:
    // pressing + or - has to move BOTH, and the wireframe has to agree with the
    // number. Phase 16 grows this line into the full HUD.
    char buf[256];
    std::snprintf(buf, sizeof(buf),
                  "Broadside | Shading: %s | %.1f FPS | seg %d | ocean %d | %d tri | %d draws | "
                  "HIER %s%s%s",
                  SHADING_NAMES[g_shadingMode],
                  fps, g_barrelSegments, g_oceanRes, g_triangles, g_drawCalls,
                  g_hierarchy ? "ON" : "OFF",
                  g_wireframe ? " | WIRE" : "",
                  g_clock.paused ? " | PAUSED" : "");
    glfwSetWindowTitle(window, buf);

    if (wall >= nextConsole) {
        nextConsole = wall + 1.0;
        std::printf("[frame] t=%7.2fs  dt=%.4fs  fps=%6.1f  tri=%6d  draws=%2d  "
                    "mode=%-7s cam r=%.2f yaw=%6.1f pitch=%6.1f%s\n",
                    now, avgDt, fps, g_triangles, g_drawCalls, SHADING_NAMES[g_shadingMode],
                    g_camera.radius, glm::degrees(g_camera.yaw), glm::degrees(g_camera.pitch),
                    g_clock.paused ? "  (paused)" : "");
        std::fflush(stdout);
    }

    frames  = 0;
    dtAccum = 0.0f;
}

int main()
{
    glfwSetErrorCallback(glfwErrorCallback);

    if (!glfwInit()) {
        std::fprintf(stderr, "glfwInit failed\n");
        return -1;
    }

    // OpenGL 3.3 Core Profile — forces the modern shader pipeline (guide 0.1)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Broadside", NULL, NULL);
    if (!window) {
        std::fprintf(stderr, "glfwCreateWindow failed\n");
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
    glfwSetScrollCallback(window, scrollCallback);

    // Must happen before ANY gl* call (guide 0.6, common failure #1)
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::fprintf(stderr, "gladLoadGLLoader failed\n");
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    // v-sync — the checkpoint wants a steady dt (one refresh interval), not an uncapped one
    glfwSwapInterval(1);

    std::printf("OpenGL   : %s\n", (const char*)glGetString(GL_VERSION));
    std::printf("GLSL     : %s\n", (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION));
    std::printf("Renderer : %s\n", (const char*)glGetString(GL_RENDERER));

    // Match the viewport to the real framebuffer once at startup — on a HiDPI
    // display it is not the same as the requested window size.
    glfwGetFramebufferSize(window, &g_fbWidth, &g_fbHeight);
    glViewport(0, 0, g_fbWidth, g_fbHeight);

    glEnable(GL_DEPTH_TEST);   // correct occlusion between the drawn objects
    glEnable(GL_CULL_FACE);    // optimization: never rasterise back faces (Requirement 11)
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);       // every generator in Mesh.h must wind counter-clockwise

    // Frame the whole ship: masts reach ~3.3 above the waterline and the hull is
    // 4 long. The default yaw puts the camera off the starboard bow, which is the
    // one angle that shows the hull, both masts, and the cannon at once.
    // The pitch stays low on purpose - specular highlights are strongest near
    // grazing angles, and looking down steeply flattens them out.
    g_camera.target = glm::vec3(0.15f, 1.05f, 0.0f);
    g_camera.radius = 9.5f;
    g_camera.yaw    = glm::radians(50.0f);
    g_camera.pitch  = glm::radians(13.0f);

    if (!initResources()) {
        // Keep console output pure ASCII: the Windows console renders this UTF-8
        // source as CP-1252, so a non-ASCII character here arrives as mojibake.
        std::fprintf(stderr, "resource setup failed - see the shader log above\n");
        destroyResources();
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    std::printf("Controls : 1/2/3 Flat/Gouraud/Phong | H hierarchy on/off | F wireframe\n"
                "           +/- tessellation | left-drag orbit | wheel or W/S zoom\n"
                "           P pause | ESC quit\n");
    std::fflush(stdout);

    // Start the clock at loop entry, not at glfwInit — otherwise the first frame
    // reports every millisecond spent in window and context creation as its dt.
    g_clock.lastWall = glfwGetTime();
    g_clock.lastNow  = (float)g_clock.lastWall;

    while (!glfwWindowShouldClose(window)) {
        // --- clock ---------------------------------------------------------
        // Two deltas, deliberately: realDt keeps ticking while paused and drives
        // the camera; dt is simulation time and goes to 0, freezing the scene.
        const double wall = glfwGetTime();

        float realDt = (float)(wall - g_clock.lastWall);
        if (realDt > MAX_DELTA) realDt = MAX_DELTA;
        if (realDt < 0.0f)      realDt = 0.0f;

        if (g_clock.paused)
            g_clock.pausedTotal += wall - g_clock.lastWall;   // freeze `now` in place
        g_clock.lastWall = wall;

        const float now = (float)(wall - g_clock.pausedTotal);  // <- ALL animation derives from this

        float dt = now - g_clock.lastNow;
        if (dt > MAX_DELTA) dt = MAX_DELTA;
        if (dt < 0.0f)      dt = 0.0f;
        g_clock.lastNow = now;

        // --- frame ---------------------------------------------------------
        processInput(window, realDt);
        updateScene(now, dt);               // all motion computed here

        glClearColor(0.35f, 0.45f, 0.55f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Wireframe is set here rather than inside renderScene, which stays pure
        // draw calls. Culling comes off with it: back-facing edges are part of the
        // polygon count being verified, and hiding half of them would make the
        // Phase 4 checkpoint read low.
        if (g_wireframe) {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            glDisable(GL_CULL_FACE);
        }

        renderScene(now);

        if (g_wireframe) {
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            glEnable(GL_CULL_FACE);
        }

        reportFrame(window, now, dt);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // GL objects must be released while the context is still current.
    destroyResources();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
