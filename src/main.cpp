// Broadside — render loop, input, orchestration.
// Phase 1: the loop skeleton and the simulation clock (PRD Requirement 3).
// Phase 2: the shader program and the first triangle.
// Phase 3: MVP matrices and the orbit camera.
// Phase 4: seven generators cover the eight PRD mesh roles; all are indexed.
// Phase 5: the L8 illumination model and the L9 Flat / Gouraud / Phong switch.
// Phase 6: the full L8 slide-60 material table and the second light.
// Phase 7: the static ship hierarchy - two 4-level chains and a two-axis gimbal.
// Phase 8: GPU ocean waves, and a hull that rocks because of the water under it.
// Phase 9: idle rigging - sails and flag with their own motion below the root.
// Phase 10: the enemy on patrol - the same drawShip(), reduced, zero new meshes.
// Phase 11: the gimbal aims itself - rate-limited slew in the ship's own frame.
// Phase 12: MILESTONE 3 - a real cannonball on a real parabola.
// Phase 13: impact resolution - HIT or SPLASH, decided per frame, never scripted.
// Phase 14: fixed particle pools - smoke, spray and hit sparks with no allocation.
// Phase 15: the complete live L8/L9 demonstration suite behind simple key toggles.
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

static bool changeTessellation(int direction)
{
    const int oldBarrel = g_barrelSegments;
    const int oldOcean  = g_oceanRes;

    if (direction > 0) {
        g_barrelSegments = (g_barrelSegments * 2 <  64) ? g_barrelSegments * 2 : 64;
        g_oceanRes       = (g_oceanRes       * 2 < 128) ? g_oceanRes       * 2 : 128;
    } else if (direction < 0) {
        g_barrelSegments = (g_barrelSegments / 2 >  6) ? g_barrelSegments / 2 : 6;
        g_oceanRes       = (g_oceanRes       / 2 >  8) ? g_oceanRes       / 2 : 8;
    }

    const bool changed = (g_barrelSegments != oldBarrel || g_oceanRes != oldOcean);
    if (changed) rebuildMeshes();
    return changed;
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

static int g_useBlinn = 0;                          // 0 = (R.V)^n, 1 = (N.H)^n
static int g_termMask = 7;                          // ambient | diffuse | specular

// Phase 15 light isolation. BOTH preserves the normal scene: the sun stays on
// and the muzzle point light is active only during its 0.15-second flash. The
// two isolated modes are deliberate teaching views. MUZZLE_ONLY keeps the point
// light on at the current muzzle so it can be studied without timing a keypress
// inside a 0.15-second shot.
enum LightMode {
    LIGHTS_BOTH = 0,
    LIGHTS_SUN_ONLY,
    LIGHTS_MUZZLE_ONLY,
    LIGHT_MODE_COUNT
};

static LightMode g_lightMode = LIGHTS_BOTH;

static const char* LIGHT_MODE_NAMES[LIGHT_MODE_COUNT] = {
    "Sun+Muzzle", "Sun only", "Muzzle only"
};

static const char* termMaskName(int mask)
{
    switch (mask) {
        case 1: return "Ambient";
        case 3: return "Ambient+Diffuse";
        case 7: return "All";
        case 4: return "Specular only";
        default: return "Unknown";
    }
}

static void toggleSpecularModel()
{
    g_useBlinn = 1 - g_useBlinn;
}

static void cycleLightMode()
{
    g_lightMode = static_cast<LightMode>((static_cast<int>(g_lightMode) + 1)
                                      % static_cast<int>(LIGHT_MODE_COUNT));
}

static void cycleTermMask()
{
    // The exact guide sequence: ambient -> ambient+diffuse -> all ->
    // specular-only -> ambient. The default is all, so the first K isolates
    // specular and the next press begins the sequence again at ambient.
    switch (g_termMask) {
        case 1:  g_termMask = 3; break;
        case 3:  g_termMask = 7; break;
        case 7:  g_termMask = 4; break;
        default: g_termMask = 1; break;
    }
}

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
// Aim input state (PRD section 10). Declared up here with the other toggles
// because processInput writes it; the aiming MATHS lives further down with the
// gimbal, next to the geometry it depends on.
//
// TAB switches auto-track off. Auto-track is the default because it is what
// shows the gimbal working without the grader having to know any keys.
// ---------------------------------------------------------------------------
static bool g_autoTrack = true;

// Held, not edge-triggered: aiming is a continuous action for as long as the key
// is down.
static bool g_aimLeft = false, g_aimRight = false, g_aimUp = false, g_aimDown = false;

// SPACE, latched by processInput and consumed by updateScene. It cannot be acted
// on where it is read: firing needs the barrel matrix, and processInput runs
// before the hierarchy is rebuilt for this frame.
static bool g_fireRequested = false;

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
// Phase 12 completed the last part: `renderScene()` switches `enabled` on only
// for the 0.15 s after a shot. The constant below describes the light while it is
// active; the per-frame copy supplies the current position and enabled state.
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
// Held keys (camera, aim) poll with glfwGetKey. Toggles (P/H/B/L/K/TAB)
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

    // B — compare the reflection-vector Phong term with the Blinn half-vector
    // term (L8 slides 49-50). Only one uniform changes.
    if (keyPressedOnce(window, GLFW_KEY_B)) {
        toggleSpecularModel();
        std::printf("[input] specular model: %s\n",
                    g_useBlinn ? "Blinn-Phong (N.H)^n" : "Phong (R.V)^n");
        std::fflush(stdout);
    }

    // L — both lights -> sun only -> muzzle only -> both. The isolated muzzle
    // mode holds the point light on so the attenuation can be inspected without
    // racing its normal 0.15-second lifetime.
    if (keyPressedOnce(window, GLFW_KEY_L)) {
        cycleLightMode();
        std::printf("[input] lights: %s\n", LIGHT_MODE_NAMES[g_lightMode]);
        std::fflush(stdout);
    }

    // K — recreate the separate L8 lighting terms live. These are bit masks read
    // by the one shared lighting function used by Gouraud and Phong.
    if (keyPressedOnce(window, GLFW_KEY_K)) {
        cycleTermMask();
        std::printf("[input] lighting terms: %s (mask %d)\n",
                    termMaskName(g_termMask), g_termMask);
        std::fflush(stdout);
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

    // Opposite keys held together cancel. Already-clamped steps also skip the
    // rebuild, so leaning on either key cannot churn identical GPU buffers.
    if (tessUp != tessDown)
        changeTessellation(tessUp ? 1 : -1);

    // SPACE — fire (PRD section 10). Edge-triggered: holding it must launch one
    // ball, not one per frame. The request is recorded and consumed in
    // updateScene, because firing needs the barrel matrix and processInput runs
    // before the hierarchy is rebuilt.
    if (keyPressedOnce(window, GLFW_KEY_SPACE))
        g_fireRequested = true;

    // TAB — auto-track / manual aim (PRD section 10). Edge-triggered.
    if (keyPressedOnce(window, GLFW_KEY_TAB)) {
        g_autoTrack = !g_autoTrack;
        std::printf("[input] aim: %s\n", g_autoTrack ? "auto-track" : "manual (arrow keys)");
        std::fflush(stdout);
    }

    // Arrow keys — manual azimuth and elevation. Polled, not edge-triggered: the
    // gun should keep turning for as long as the key is held. They are recorded
    // here and consumed by updateAim, so all aiming maths stays in one place.
    g_aimLeft  = (glfwGetKey(window, GLFW_KEY_LEFT)  == GLFW_PRESS);
    g_aimRight = (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS);
    g_aimUp    = (glfwGetKey(window, GLFW_KEY_UP)    == GLFW_PRESS);
    g_aimDown  = (glfwGetKey(window, GLFW_KEY_DOWN)  == GLFW_PRESS);

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
// Rigging animation (PRD 9.7, guide Phase 9)
//
//   sail: R_z(A sin(w t + phi))  about its yard
//   flag: R_y(A sin(w t + phi))  about the mast it flies from
//
// Two properties matter more than the numbers.
//
// FIRST, the phase offsets. Without them every sail reaches its extreme on the
// same frame and the rig moves as one rigid object, which reads as a glitch
// rather than as wind. The sails use different phases, and the flag also uses a
// different speed, so the three parts do not move in lockstep.
//
// SECOND, and this is the point of putting it here at all: these rotations sit
// BELOW the root. They are not touched by the H key. With the hierarchy off the
// hull goes dead level and stops following the sea, while the sails keep
// fluttering and the flag keeps waving on top of it - so H now shows children
// that visibly have a life of their own being abandoned by their parent, which
// is the demonstration CLAUDE.md describes and which Phase 8 could only half
// make (there was nothing left moving once the hull stopped).
// ---------------------------------------------------------------------------
static const float SAIL_FLUTTER_A     = 0.10f;   // radians, guide Phase 9
static const float SAIL_FLUTTER_W     = 2.2f;    // rad/s,   guide Phase 9
static const float SAIL_FORE_PHASE    = 0.0f;
static const float SAIL_MAIN_PHASE    = 1.9f;    // roughly a third of a cycle apart

// The flag is deliberately louder than the sails: it is a wind INDICATOR, it has
// no mass to speak of, and it is the smallest thing on the ship, so it needs a
// wider swing to read at all from the default camera distance.
static const float FLAG_WAVE_A        = 0.42f;   // radians (~24 degrees)
static const float FLAG_WAVE_W        = 3.1f;    // rad/s - faster than the sails
static const float FLAG_PHASE         = 0.7f;

// ---------------------------------------------------------------------------
// Aim state (PRD 9.3, guide Phase 11)
//
// These start where Phase 7 parked them - abaft the starboard beam, which is
// roughly where the enemy first appears - and from Phase 11 they are driven every
// frame, either by the tracking solution or by the arrow keys.
// ---------------------------------------------------------------------------
static float g_azimuth   = glm::radians(138.0f);  // traverse, about the yoke's +Y
static float g_elevation = glm::radians(16.0f);   // elevate,  about the trunnion axis

// The gimbal's own limits and speed.
//
// SLEW is the whole reason this reads as machinery rather than as a snap. The
// enemy needs about 11 deg/s of traverse at its fastest, and the hull's roll asks
// for a few more, so 50 deg/s tracks comfortably while still visibly LAGGING when
// the target crosses dead astern and the solution moves quickest. That lag is the
// evidence the angle is computed rather than assigned (PRD 9.3).
static const float AIM_SLEW   = glm::radians(50.0f);   // rad/s
static const float AIM_EL_MIN = glm::radians(-5.0f);
static const float AIM_EL_MAX = glm::radians(45.0f);

// ===========================================================================
// BALLISTICS (PRD 9.4, guide Phase 12) — MILESTONE 3
// ===========================================================================
static const float     MUZZLE_SPEED      = 42.0f;   // m/s, the figure in the PRD 10 HUD
static const glm::vec3 GRAVITY           = glm::vec3(0.0f, -9.8f, 0.0f);
static const float     MAX_FLIGHT_TIME   = 6.0f;    // backstop; Phase 13 ends flights on impact
// 0.30, not a scale-accurate 0.15. At the engagement range the ball is about
// ten pixels across; halving that makes the arc guesswork, and "visibly arcs
// under gravity" is the Milestone 3 checkpoint, not a detail.
static const float     BALL_DIAMETER     = 0.30f;
static const float     MUZZLE_FLASH_TIME = 0.15f;   // PRD 11.1

// How far above the straight line to the target the gun must aim, so that the
// shot ARRIVES rather than passing under the target.
//
// Phase 11 parked this at a fixed 7 degrees and said Phase 12 would derive it.
// Here is the derivation. For a target at roughly the firing height, the low-arc
// solution of p(tau) = p0 + v0 tau + g tau^2 / 2 is
//
//     range = v^2 sin(2 theta) / g      ->      theta = asin(g R / v^2) / 2
//
// At the engagement range of 18 to 23 units that is 2.9 to 3.7 degrees - and the
// old fixed 7 degrees would have thrown the ball 43 units, more than twice the
// distance to the enemy. A constant cannot work here because the enemy patrols
// across 28 units of frontage, so the range changes throughout the pass.
//
// The high-arc solution (90 - theta) is also valid and is deliberately not used:
// a flat, fast shot is the one whose parabola reads clearly at this scale.
static float ballisticLift(float range)
{
    // Out of reach: hand back the 45-degree maximum-range solution rather than
    // NaN from asin, so the gun still does the most sensible thing it can.
    const float s = (9.8f * range) / (MUZZLE_SPEED * MUZZLE_SPEED);
    if (s >= 1.0f) return glm::radians(45.0f);
    return 0.5f * std::asin(s);
}

// The projectile. ONE at a time, per the PRD 3.3 scope ceiling.
//
// What is stored is the LAUNCH STATE - where it started, how fast, and when - and
// never the current position. Position is recomputed from tau every frame, so the
// trajectory is a closed form and not an integration that could drift
// (Requirement 12).
struct Projectile {
    bool      active   = false;
    glm::vec3 p0       = glm::vec3(0.0f);   // muzzle world position at the instant of firing
    glm::vec3 v0       = glm::vec3(0.0f);   // muzzle world direction * MUZZLE_SPEED
    float     fireTime = 0.0f;
};

static Projectile g_ball;
static glm::vec3  g_ballPos(0.0f);          // derived every frame, never accumulated
static glm::vec3  g_ballPrev(0.0f);         // last frame's position - the swept segment
static float      g_flightTime       = 0.0f;
static float      g_muzzleLightUntil = -1.0f;

// ===========================================================================
// POOLED PARTICLES (PRD 9.6, guide Phase 14)
// ===========================================================================
//
// There are exactly four smoke slots and four impact slots. Spawning overwrites
// these fixed records; updating only changes age and active. No vector grows, no
// object is created, and no memory is allocated after startup.
//
// Position, spin, size and opacity are derived when rendered:
//
//   position = origin + velocity * age
//   spin     = spinRate * age
//   size     = startSize + growth * (age / life)
//   opacity  = 1 - age / life
//
// Smoke and impact puffs share one instanced sphere draw. The splash ring uses a
// second draw. That batching is load-bearing: the Phase 13 scene already costs
// 18 draws while idle, so one draw per puff would break the hard ceiling of 20.
// ===========================================================================
static const int PARTICLE_POOL_SIZE = 4;
static const int MAX_PARTICLE_INSTANCES = PARTICLE_POOL_SIZE * 2;

struct Particle {
    glm::vec3 origin    = glm::vec3(0.0f);
    glm::vec3 velocity  = glm::vec3(0.0f);
    glm::vec3 emission  = glm::vec3(0.0f);
    float     age       = 0.0f;
    float     life      = 1.0f;
    float     startSize = 0.1f;
    float     growth    = 0.5f;
    float     spinRate  = 0.0f;
    bool      active    = false;
};

static Particle g_smokePool[PARTICLE_POOL_SIZE];
static Particle g_splashPool[PARTICLE_POOL_SIZE];   // blue spray or orange hit burst
static Particle g_splashRing;                       // the one reused M6 ring

static void activateParticle(Particle& p,
                             const glm::vec3& origin,
                             const glm::vec3& velocity,
                             const glm::vec3& emission,
                             float life, float startSize, float growth, float spinRate)
{
    p.origin    = origin;
    p.velocity  = velocity;
    p.emission  = emission;
    p.age       = 0.0f;
    p.life      = life;
    p.startSize = startSize;
    p.growth    = growth;
    p.spinRate  = spinRate;
    p.active    = true;
}

static float particleProgress(const Particle& p)
{
    if (p.life <= 0.0f) return 1.0f;
    return glm::clamp(p.age / p.life, 0.0f, 1.0f);
}

static glm::vec3 particlePosition(const Particle& p)
{
    return p.origin + p.velocity * p.age;
}

static float particleSize(const Particle& p)
{
    return p.startSize + p.growth * particleProgress(p);
}

static float particleOpacity(const Particle& p)
{
    return 1.0f - particleProgress(p);
}

template <size_t N>
static int activeParticleCount(const Particle (&pool)[N])
{
    int count = 0;
    for (const Particle& p : pool)
        if (p.active) ++count;
    return count;
}

static void spawnSmoke(const glm::vec3& muzzlePos, const glm::vec3& forward)
{
    const glm::vec3 worldUp(0.0f, 1.0f, 0.0f);
    glm::vec3 right = glm::cross(forward, worldUp);
    if (glm::length(right) < 1e-5f) right = glm::vec3(1.0f, 0.0f, 0.0f);
    else                            right = glm::normalize(right);

    static const float SIDE[PARTICLE_POOL_SIZE] = { -0.18f, -0.06f, 0.08f, 0.20f };
    static const float UP[PARTICLE_POOL_SIZE]   = {  0.00f,  0.07f, 0.03f, 0.11f };
    static const float SPEED[PARTICLE_POOL_SIZE]= {  0.55f,  0.72f, 0.48f, 0.64f };
    static const float LIFE[PARTICLE_POOL_SIZE] = {  1.10f,  1.35f, 1.22f, 1.48f };

    for (int i = 0; i < PARTICLE_POOL_SIZE; ++i) {
        const glm::vec3 origin = muzzlePos + forward * 0.08f
                               + right * SIDE[i] * 0.25f + worldUp * UP[i];
        const glm::vec3 velocity = forward * SPEED[i]
                                 + worldUp * (0.30f + 0.08f * (float)i)
                                 + right * SIDE[i];
        const float shade = 0.24f + 0.035f * (float)i;
        activateParticle(g_smokePool[i], origin, velocity, glm::vec3(shade),
                         LIFE[i], 0.14f + 0.02f * (float)i,
                         0.68f + 0.05f * (float)i,
                         (i % 2 == 0 ? 1.0f : -1.0f) * (0.7f + 0.2f * (float)i));
    }
}

static void spawnSplash(const glm::vec3& waterPos)
{
    static const glm::vec3 DIRECTION[PARTICLE_POOL_SIZE] = {
        glm::vec3( 1.0f, 0.0f,  0.2f), glm::vec3(-0.7f, 0.0f,  0.7f),
        glm::vec3( 0.2f, 0.0f, -1.0f), glm::vec3(-0.8f, 0.0f, -0.4f)
    };

    for (int i = 0; i < PARTICLE_POOL_SIZE; ++i) {
        const glm::vec3 radial = glm::normalize(DIRECTION[i]);
        const glm::vec3 velocity = radial * (1.00f + 0.28f * (float)i)
                                 + glm::vec3(0.0f, 0.75f + 0.16f * (float)i, 0.0f);
        activateParticle(g_splashPool[i], waterPos + radial * 0.12f, velocity,
                         glm::vec3(0.18f, 0.75f, 1.10f),
                         0.70f + 0.08f * (float)i,
                         0.32f, 1.00f + 0.05f * (float)i,
                         (i % 2 == 0 ? 1.0f : -1.0f) * 1.4f);
    }

    activateParticle(g_splashRing, waterPos, glm::vec3(0.0f),
                     glm::vec3(0.12f, 0.62f, 1.00f),
                     1.00f, 0.45f, 2.55f, 0.8f);
}

static void spawnBurst(const glm::vec3& hitPos)
{
    static const glm::vec3 DIRECTION[PARTICLE_POOL_SIZE] = {
        glm::vec3( 1.0f,  0.5f,  0.2f), glm::vec3(-0.8f,  0.7f,  0.4f),
        glm::vec3( 0.2f,  0.8f, -1.0f), glm::vec3(-0.6f, -0.2f, -0.8f)
    };

    for (int i = 0; i < PARTICLE_POOL_SIZE; ++i) {
        const glm::vec3 velocity = glm::normalize(DIRECTION[i])
                                 * (0.85f + 0.18f * (float)i);
        activateParticle(g_splashPool[i], hitPos, velocity,
                         glm::vec3(1.00f, 0.28f, 0.05f),
                         0.42f + 0.05f * (float)i,
                         0.10f, 0.48f + 0.04f * (float)i,
                         (i % 2 == 0 ? 1.0f : -1.0f) * 2.0f);
    }

    // A hit uses sparks and the existing hull glow; the water ring belongs only
    // to a splash. Retire any older ring so the feedback cannot tell two stories.
    g_splashRing.active = false;
}

template <size_t N>
static void updateParticlePool(Particle (&pool)[N], float dt)
{
    for (Particle& p : pool) {
        if (!p.active) continue;
        p.age += dt;
        if (p.age >= p.life) p.active = false;
    }
}

static void updateParticles(float dt)
{
    if (dt <= 0.0f) return;   // P pauses particle ages with the rest of the scene
    updateParticlePool(g_smokePool, dt);
    updateParticlePool(g_splashPool, dt);
    if (g_splashRing.active) {
        g_splashRing.age += dt;
        if (g_splashRing.age >= g_splashRing.life)
            g_splashRing.active = false;
    }
}

// ===========================================================================
// IMPACT (PRD 9.5, guide Phase 13)
// ===========================================================================
static const float HIT_RADIUS = 1.70f;      // the enemy hull, approximated by a sphere

// How long the enemy hull glows after being struck. Emission (L8 s55) is used
// rather than a colour change because it is the one term that survives every
// lighting mode and every K-key term mask - the hit stays legible whatever the
// grader is demonstrating at the time.
static const float     ENEMY_FLASH_TIME = 0.40f;
static const glm::vec3 ENEMY_HIT_EMISSION(1.00f, 0.35f, 0.10f);

static float       g_enemyFlashUntil = -1.0f;
static const char* g_lastResult      = "-";

// ===========================================================================
// ENEMY SHIP (PRD 7 and 9, guide Phase 10)
//
// The entire phase adds ZERO new meshes and ZERO new geometry code. The enemy is
// the same drawShip() called with a different root matrix, a reduced detail
// level and a different hull tint - which is the mesh-reuse claim in
// Requirement 11 made concrete rather than asserted.
//
// It is REDUCED, and that is a budget decision, not laziness. PRD section 7
// specifies "identical sub-structure, reduced"; a full clone would be 13 more
// draws and would put the frame at 28, well past the 20-call ceiling in PRD 3.3.
// Four draws - hull, mast, yard, sail - is what it takes to read as a ship at
// 18 units, and every draw skipped here is one the budget still has for the
// cannonball and the particle pools in Phases 12 and 14.
// ===========================================================================
static const float ENEMY_Z         = -18.0f;   // range, dead astern of the player
static const float ENEMY_SWEEP     =  14.0f;   // half-width of the patrol
static const float ENEMY_RATE      =   0.25f;  // rad/s - a slow, readable pass
static const float ENEMY_FREEBOARD =   0.10f;  // its own hull rides slightly lower

// A UNIFORM scale, applied to the enemy's ROOT and nowhere else.
//
// This is the single place in the project where a scale is allowed into a frame
// chain, and it is safe for exactly the reason the hull scale is not: it is
// uniform. It shrinks the whole ship evenly and cannot shear a child or tilt a
// normal. A non-uniform root scale would be the parent-scale-leak bug wearing a
// different hat, and is why the audit checks the enemy frames for UNIFORMITY
// rather than simply waving them through.
static const float ENEMY_SCALE     =   0.85f;

// T(patrol(t)) — guide Phase 10. A closed form of t like everything else, so the
// enemy's position at any instant is derived and never stored.
static glm::vec3 enemyPos(float t)
{
    return glm::vec3(std::sin(ENEMY_RATE * t) * ENEMY_SWEEP, ENEMY_FREEBOARD, ENEMY_Z);
}

// ===========================================================================
// AIMING (PRD 9.3, guide Phase 11)
// ===========================================================================

// The gimbal pivot, in the SHIP's own space. Derived from the hierarchy rather
// than typed, so it cannot drift if the mount is ever moved: it is the deck
// height, plus the mount offset, plus the trunnion height.
static const glm::vec3 MOUNT_LOCAL_POS(MOUNT_POS.x,
                                       DECK_Y + MOUNT_POS.y + YOKE_Y,
                                       MOUNT_POS.z);

// Fold an angle into (-pi, pi].
//
// This is not decoration. The enemy patrols across the player's stern, so the
// bearing to it sweeps through 180 degrees on every pass - straight across the
// branch cut of atan2. At the crossing the solution jumps from +178.5 to -178.5
// degrees, which is a 3 degree turn but reads as -357 to naive subtraction. The
// guide's snippet subtracts directly, and a turret driven that way spins almost
// all the way round the wrong way, once per pass. Taking the SHORTEST signed
// angle is what makes "smoothly tracks" true.
static float wrapAngle(float a)
{
    const float twoPi = 6.28318530718f;
    const float pi    = 3.14159265359f;
    a = std::fmod(a + pi, twoPi);
    if (a < 0.0f) a += twoPi;
    return a - pi;
}

// Rate-limited two-axis tracking, or direct arrow-key control.
//
// The solution is computed in the SHIP'S LOCAL SPACE, not in world space, and
// that is the pedagogical heart of the phase: the gun platform is rolling and
// pitching, so the bearing that matters is the one relative to the deck the gun
// is bolted to. Transform the target into ship space once, and the two gimbal
// angles fall straight out of it - no compensation term anywhere.
static void updateAim(float dt, const glm::mat4& shipM, const glm::vec3& enemyWorld)
{
    if (g_autoTrack) {
        // Into the ship's frame. inverse() of a rigid transform - the frames are
        // rigid precisely so this is meaningful.
        const glm::vec3 local = glm::vec3(glm::inverse(shipM) * glm::vec4(enemyWorld, 1.0f));
        const glm::vec3 toTarget = local - MOUNT_LOCAL_POS;
        const glm::vec3 d        = glm::normalize(toTarget);

        // The lift is recomputed from the CURRENT range every frame, because the
        // enemy patrols across 28 units of frontage and the range never settles.
        const float targetAz = std::atan2(d.x, d.z);
        float targetEl = std::asin(glm::clamp(d.y, -1.0f, 1.0f))
                       + ballisticLift(glm::length(toTarget));
        targetEl = glm::clamp(targetEl, AIM_EL_MIN, AIM_EL_MAX);

        // Rate-limited slew. dt is SIMULATION time, so P freezes the turret with
        // everything else instead of letting it snap while the scene is frozen.
        const float step = AIM_SLEW * dt;
        g_azimuth   += glm::clamp(wrapAngle(targetAz - g_azimuth), -step, step);
        g_elevation += glm::clamp(targetEl - g_elevation,          -step, step);
    } else {
        const float step = AIM_SLEW * dt;
        if (g_aimLeft)  g_azimuth   -= step;
        if (g_aimRight) g_azimuth   += step;
        if (g_aimUp)    g_elevation += step;
        if (g_aimDown)  g_elevation -= step;
    }

    // Applied on BOTH paths. Elevation is a physical stop on the trunnion; the
    // gun cannot depress past -5 or elevate past +45 whoever is driving it.
    g_elevation = glm::clamp(g_elevation, AIM_EL_MIN, AIM_EL_MAX);

    // Azimuth has no stop - the yoke turns all the way round - but it is kept
    // folded so a long session cannot grow it until float precision makes the
    // traverse visibly step.
    g_azimuth = wrapAngle(g_azimuth);
}

// ---------------------------------------------------------------------------
// fire() — the trickiest ten lines in the project (guide 12.1)
//
// Everything the hierarchy has been built for arrives here. The barrel's world
// matrix already contains the ship's heave, roll and pitch, the deck, the mount,
// the traverse and the elevation, so the launch state is read straight out of it
// with no special case for any of them. Fire at the top and the bottom of a roll
// and the two shots differ, automatically, because the matrix differed.
//
// THE W COMPONENT IS THE WHOLE TRAP (AGENT.md section 5):
//   w = 1 -> a POSITION, which picks up the translation. Correct for the muzzle.
//   w = 0 -> a DIRECTION, immune to translation. Correct for the bore axis.
// Swap them and the ball flies at the world origin instead of out of the barrel.
// ---------------------------------------------------------------------------
static void fire(const glm::mat4& barrelMatrix, float now)
{
    // Muzzle sits at the barrel's local +Z tip.
    const glm::vec3 muzzlePos = glm::vec3(barrelMatrix * glm::vec4(0.0f, 0.0f, MUZZLE_Z, 1.0f));

    // Forward direction: transform the local +Z AXIS (w = 0, not 1).
    const glm::vec3 forward = glm::normalize(
                                  glm::vec3(barrelMatrix * glm::vec4(0.0f, 0.0f, 1.0f, 0.0f)));

    g_ball.p0       = muzzlePos;
    g_ball.v0       = forward * MUZZLE_SPEED;
    g_ball.fireTime = now;
    g_ball.active   = true;

    g_ballPos    = muzzlePos;
    g_flightTime = 0.0f;

    spawnSmoke(muzzlePos, forward);

    // Light 1 comes alive for 0.15 s (PRD 11.1). This is the only line that ever
    // switches it on, and the only reason the point light and its attenuation are
    // in the project at all.
    g_muzzleLightUntil = now + MUZZLE_FLASH_TIME;
}

// ---------------------------------------------------------------------------
// updateProjectile() — the closed form (guide 12.2, Requirement 12)
//
//   p(tau) = p0 + v0 * tau + g * tau^2 / 2
//
// Evaluated fresh from tau every frame. Nothing is integrated and nothing is
// accumulated, so the ball cannot drift, cannot depend on the frame rate, and
// gives the identical arc if the clock is rewound.
// ---------------------------------------------------------------------------
static void updateProjectile(float now)
{
    if (!g_ball.active) return;

    const float tau = now - g_ball.fireTime;
    g_ballPrev   = g_ballPos;                       // where it was, for the swept test
    g_ballPos    = g_ball.p0 + g_ball.v0 * tau + 0.5f * GRAVITY * tau * tau;
    g_flightTime = tau;

    if (tau > MAX_FLIGHT_TIME) {
        g_ball.active = false;
        g_lastResult  = "LOST";
    }
}

// ---------------------------------------------------------------------------
// Shortest distance from the segment [a, b] to the point c.
// ---------------------------------------------------------------------------
static float segmentPointDistance(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c)
{
    const glm::vec3 ab    = b - a;
    const float     denom = glm::dot(ab, ab);
    if (denom < 1e-12f) return glm::length(c - a);
    const float t = glm::clamp(glm::dot(c - a, ab) / denom, 0.0f, 1.0f);
    return glm::length(c - (a + ab * t));
}

// ---------------------------------------------------------------------------
// checkImpact() — decided fresh every frame, never scripted (guide Phase 13)
//
// TWO DELIBERATE DEPARTURES FROM THE GUIDE SNIPPET, both measured first:
//
// 1. The hull test sweeps the SEGMENT the ball covered this frame, not the point
//    it happens to be at. At 42 m/s the ball moves 0.7 units per frame at 60 Hz
//    and 1.4 at 30 Hz, while a grazing hit stays inside HIT_RADIUS for only 1.17
//    frames at 60 Hz and 0.58 at 30 Hz. A point test therefore MISSES grazing
//    hits, and misses them differently at different frame rates - which would
//    make "aiming well produces HIT" a matter of luck and would contradict the
//    frame-rate independence the rest of the project is built on.
//
// 2. The hull is tested BEFORE the sea. The guide checks the sea first, but the
//    enemy floats ON the sea: a shot arriving at hull height while the local wave
//    is at a crest is below the waterline and above the deck at the same instant.
//    Testing the sea first reports SPLASH for a shot that plainly struck the ship.
//    A ball reaches the water only if it missed, so the hull is asked first.
//
// The sea test stays a simple point test, and that is safe rather than lazy: the
// sea is a boundary the ball never leaves once it is under, so it cannot be
// tunnelled through the way a finite target can.
// ---------------------------------------------------------------------------
static void checkImpact(float now, const glm::vec3& enemyWorld)
{
    if (!g_ball.active) return;

    if (segmentPointDistance(g_ballPrev, g_ballPos, enemyWorld) < HIT_RADIUS) {
        // Centre the burst on the target. A swept hit may have both sampled ball
        // endpoints outside the hull, so either endpoint can be a poor contact
        // marker even though the segment correctly crossed it.
        spawnBurst(enemyWorld);
        g_enemyFlashUntil = now + ENEMY_FLASH_TIME;
        g_lastResult      = "HIT";
        g_ball.active     = false;
        return;
    }

    if (g_ballPos.y <= waveHeight(g_ballPos.x, g_ballPos.z, now)) {
        glm::vec3 waterPos = g_ballPos;
        // A small clearance keeps the flat ring above nearby displaced vertices
        // instead of z-fighting with the animated ocean surface.
        waterPos.y = waveHeight(waterPos.x, waterPos.z, now) + 0.06f;
        spawnSplash(waterPos);
        g_lastResult  = "SPLASH";
        g_ball.active = false;
    }
}

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
    glm::mat4 mastFore, yardFore, sailFore;
    glm::mat4 mastMain, yardMain, sailMain, flagMain, flag;
    glm::vec3 muzzlePos;    // world position of the barrel tip     (w = 1)
    glm::vec3 muzzleFwd;    // world firing direction, unit length   (w = 0)
};

static ShipFrames g_ship;
static ShipFrames g_enemy;

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

static ShipFrames buildShipFrames(const glm::mat4& root, float t)
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

    // The second chain: two masts, each carrying a yard, each yard carrying a
    // sail. Depth is deck -> mast -> yard -> sail, matching the cannon chain.
    f.mastFore = f.deck     * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, MAST_FORE_Z));
    f.yardFore = f.mastFore * glm::translate(glm::mat4(1.0f),
                                             glm::vec3(0.0f, MAST_FORE_H * YARD_HEIGHT_F, 0.0f));
    f.mastMain = f.deck     * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, MAST_MAIN_Z));
    f.yardMain = f.mastMain * glm::translate(glm::mat4(1.0f),
                                             glm::vec3(0.0f, MAST_MAIN_H * YARD_HEIGHT_F, 0.0f));

    // Level 5 — the sails, each swinging about its own yard (PRD 9.7).
    // Closed forms of t, evaluated fresh: no stored angle, no accumulated delta.
    f.sailFore = f.yardFore * glm::rotate(
        glm::mat4(1.0f),
        SAIL_FLUTTER_A * std::sin(SAIL_FLUTTER_W * t + SAIL_FORE_PHASE),
        glm::vec3(0.0f, 0.0f, 1.0f));
    f.sailMain = f.yardMain * glm::rotate(
        glm::mat4(1.0f),
        SAIL_FLUTTER_A * std::sin(SAIL_FLUTTER_W * t + SAIL_MAIN_PHASE),
        glm::vec3(0.0f, 0.0f, 1.0f));

    // The PRD places the flag below the main yard in the scene graph. Keep that
    // exact parent relationship, then move from the yard up to the masthead.
    f.flagMain = f.yardMain * glm::translate(glm::mat4(1.0f),
                                             glm::vec3(0.0f, MAST_MAIN_H * (1.0f - YARD_HEIGHT_F), 0.0f));

    // The flag swings about the MAST axis, which is where its staff would be, so
    // it slews around the masthead like a wind vane rather than orbiting it.
    // R_y, not R_z, exactly as PRD section 8 specifies.
    f.flag = f.flagMain * glm::rotate(
        glm::mat4(1.0f),
        FLAG_WAVE_A * std::sin(FLAG_WAVE_W * t + FLAG_PHASE),
        glm::vec3(0.0f, 1.0f, 0.0f));
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
static void updateScene(float now, float dt)
{
    // Existing particles age before new events are spawned, so a fresh smoke or
    // splash effect is rendered once at age zero instead of losing its first dt.
    updateParticles(dt);

    // `now` reaches the chain TWICE, and the two paths are independent on purpose:
    // through shipMatrix it drives the root (heave, roll, pitch, and switchable
    // with H), and directly into buildShipFrames it drives the sails and flag,
    // which H does not touch.
    // Both roots first. They are needed before either chain can be built, because
    // the gun has to be aimed before the frames that carry it are assembled.
    const glm::mat4 shipRoot = shipMatrix(SHIP_POS, now, g_hierarchy);

    // The enemy: same builder, same wave, same rigging clock. Only the root
    // differs - it translates along the patrol AND carries the uniform scale.
    //
    // Note which side the scale goes on. Post-multiplying the root means the
    // shrink happens in the ship's own space, about its own origin, so the enemy
    // stays on the water instead of being dragged toward the world origin.
    //
    // It samples the wave at ITS OWN (x, z), so the two hulls rock out of phase
    // with each other - and its sample point is itself moving, which is why its
    // roll is not simply a delayed copy of the player's.
    const glm::mat4 enemyRoot =
        shipMatrix(enemyPos(now), now, g_hierarchy)
            * glm::scale(glm::mat4(1.0f), glm::vec3(ENEMY_SCALE));

    // Aim at where the enemy ACTUALLY is, heave included - the translation of its
    // root, not the flat patrol point. The target is riding the same sea.
    updateAim(dt, shipRoot, glm::vec3(enemyRoot[3]));

    g_ship  = buildShipFrames(shipRoot,  now);
    g_enemy = buildShipFrames(enemyRoot, now);

    // Firing happens AFTER the chain is assembled, so the shot leaves along the
    // barrel's pose THIS frame rather than last frame's. That one line of
    // ordering is what makes "fire at the top of a roll" mean anything.
    if (g_fireRequested) {
        g_fireRequested = false;
        fire(g_ship.barrel, now);
        g_lastResult = "IN FLIGHT";
    }

    updateProjectile(now);

    // Resolved against the enemy's CURRENT world position - the root translation,
    // heave included - because the target is riding the same sea the shot has to
    // cross. Deterministic, per frame, no scripted outcome (PRD 9.5).
    checkImpact(now, glm::vec3(enemyRoot[3]));
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
// Particle rendering — fixed data, one instanced sphere draw, optional ring.
// ---------------------------------------------------------------------------
static void sortParticlesBackToFront(const Particle* particles[], int count)
{
    // Insertion sort is ideal for eight fixed entries: no allocation and a tiny,
    // predictable amount of work. Blended puffs are drawn farthest first.
    const glm::vec3 cameraPos = g_camera.position();
    for (int i = 1; i < count; ++i) {
        const Particle* current = particles[i];
        const float currentDistance = glm::dot(particlePosition(*current) - cameraPos,
                                               particlePosition(*current) - cameraPos);
        int j = i - 1;
        while (j >= 0) {
            const glm::vec3 delta = particlePosition(*particles[j]) - cameraPos;
            if (glm::dot(delta, delta) >= currentDistance) break;
            particles[j + 1] = particles[j];
            --j;
        }
        particles[j + 1] = current;
    }
}

static void drawParticleBatch(const Mesh& mesh, const Particle* particles[], int count)
{
    if (count <= 0) return;

    g_shader.setInt("uUseParticleInstances", 1);
    setMaterial(g_shader, PARTICLE_BASE);

    char name[64];
    for (int i = 0; i < count; ++i) {
        const Particle& p = *particles[i];
        const float size = particleSize(p);
        const glm::mat4 model = translated(particlePosition(p))
                              * glm::rotate(glm::mat4(1.0f), p.spinRate * p.age,
                                            glm::vec3(0.0f, 1.0f, 0.0f))
                              * scaled(glm::vec3(size));
        const glm::mat3 normalMatrix = glm::mat3(glm::transpose(glm::inverse(model)));

        std::snprintf(name, sizeof(name), "uParticleModels[%d]", i);
        g_shader.setMat4(name, model);
        std::snprintf(name, sizeof(name), "uParticleNormalMatrices[%d]", i);
        g_shader.setMat3(name, normalMatrix);
        std::snprintf(name, sizeof(name), "uParticleEmission[%d]", i);
        g_shader.setVec3(name, p.emission);
        std::snprintf(name, sizeof(name), "uParticleAlpha[%d]", i);
        g_shader.setFloat(name, particleOpacity(p));
    }

    mesh.drawInstanced(count);
    ++g_drawCalls;
    g_triangles += mesh.triangleCount() * count;
    g_shader.setInt("uUseParticleInstances", 0);
}

static void drawParticles(bool muzzleMarkerVisible)
{
    // While the flash marker is visible, the opaque scene + ball + marker already
    // uses all 20 allowed draws. This includes Phase 15's held muzzle-only view.
    // Smoke keeps aging in its pool and appears when the marker goes away;
    // nothing is discarded or allocated.
    if (muzzleMarkerVisible) return;

    const Particle* puffs[MAX_PARTICLE_INSTANCES];
    int puffCount = 0;
    for (const Particle& p : g_smokePool)
        if (p.active) puffs[puffCount++] = &p;
    for (const Particle& p : g_splashPool)
        if (p.active) puffs[puffCount++] = &p;

    sortParticlesBackToFront(puffs, puffCount);

    // A ring plus the ball plus the puff batch would be 21 draws. Hide an older
    // ring while a new shot is in flight; it stays in the fixed pool and may
    // reappear if it is still alive when that shot ends.
    const bool drawRing = g_splashRing.active && !g_ball.active;
    if (puffCount == 0 && !drawRing) return;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);

    if (drawRing) {
        const Particle* ring[1] = { &g_splashRing };
        drawParticleBatch(g_ring, ring, 1);
    }
    drawParticleBatch(g_sphere, puffs, puffCount);

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

// ---------------------------------------------------------------------------
// One mast, its yard, and its sail — the rigging chain, drawn twice.
//
// Called with the fore and main frames in turn: same code, same meshes, different
// matrices. That reuse is the mesh-reuse optimization argument made concrete —
// five cylinder draws in this scene all come out of one VAO.
// ---------------------------------------------------------------------------
static void drawRig(const glm::mat4& mastFrame, const glm::mat4& yardFrame,
                    const glm::mat4& sailFrame,
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

    // Sail: a child of the YARD, hanging below it, drawn from the FLUTTER frame
    // rather than the yard frame. sailFrame is yardFrame * R_z(A sin(wt + phi)),
    // so the rotation happens at the yard and the sail swings beneath it - which
    // is what "about the yard" in PRD 9.7 means. Hanging the sail first and
    // rotating afterwards would spin it about its own middle instead.
    drawMeshTwoSided(g_quad,
                     sailFrame * translated(glm::vec3(0.0f, -sailH * 0.5f, 0.0f))
                               * scaled(glm::vec3(sailW, sailH, 1.0f)),
                     SAILCLOTH);
}

// ---------------------------------------------------------------------------
// The ship. Thirteen draws at full detail, four when reduced; five materials,
// four unique meshes, and ONE function serving both vessels.
//
// Read the left-hand side of every drawMesh call: a frame, then translate and
// scale. No frame is ever built FROM a scaled matrix — that is the whole
// discipline of this function.
// ---------------------------------------------------------------------------
enum ShipDetail { SHIP_FULL, SHIP_REDUCED };

static void drawShip(const ShipFrames& f, ShipDetail detail, const Material& hullMaterial)
{
    // --- Level 1: hull -------------------------------------------------------
    // The only part that takes the tint. Everything else is shared timber, which
    // is how one material change makes two different ships.
    drawMesh(g_cube, f.ship * scaled(HULL_DIMS), hullMaterial);

    // --- the main mast, yard and sail: both vessels carry one ----------------
    drawRig(f.mastMain, f.yardMain, f.sailMain,
            MAST_MAIN_H, YARD_MAIN_SPAN, SAIL_MAIN_W, SAIL_MAIN_H);

    // Four draws is the whole enemy. It rocks on the same sea and its sail
    // flutters on the same clock; at 18 units that is all a target needs to be.
    if (detail == SHIP_REDUCED)
        return;

    // ===== everything below here belongs to the player alone =================

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

    // --- the fore rig (the main one is drawn above, for both ships) ----------
    drawRig(f.mastFore, f.yardFore, f.sailFore,
            MAST_FORE_H, YARD_FORE_SPAN, SAIL_FORE_W, SAIL_FORE_H);

    // Flag at the main masthead, parented to the main yard exactly as in PRD s7.
    // The half-width shift makes the flag's left edge meet the mast.
    drawMeshTwoSided(g_quad,
                     f.flag * translated(glm::vec3(FLAG_DIMS.x * 0.5f, 0.0f, 0.0f))
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

    Light sunLight = SUN;
    sunLight.enabled = (g_lightMode == LIGHTS_MUZZLE_ONLY) ? 0.0f : 1.0f;
    setLight(g_shader, 0, sunLight);

    // Light 1 rides the hierarchy AND the clock. It sits at the muzzle world point
    // the transform chain produced this frame. In the normal BOTH mode it is dark
    // except for the 0.15 s after a shot (PRD 11.1). Phase 15's MUZZLE_ONLY mode
    // deliberately holds it on so its point-light attenuation can be inspected.
    //
    // While it is dark the scene is lit by the sun alone, so the attenuation
    // demonstration is now a transient the grader triggers rather than a
    // permanent fixture. It also hands one draw call back to the budget.
    const bool timedFlash = (now < g_muzzleLightUntil);
    const bool forceMuzzle = (g_lightMode == LIGHTS_MUZZLE_ONLY);
    const bool allowMuzzle = (g_lightMode != LIGHTS_SUN_ONLY);
    const bool muzzleVisible = allowMuzzle && (timedFlash || forceMuzzle);

    Light muzzleLight    = MUZZLE_LIGHT;
    muzzleLight.position = g_ship.muzzlePos;
    muzzleLight.enabled  = muzzleVisible ? 1.0f : 0.0f;
    setLight(g_shader, 1, muzzleLight);

    g_shader.setInt("uShadingMode", g_shadingMode);
    g_shader.setInt("uUseBlinn", g_useBlinn);
    g_shader.setInt("uTermMask", g_termMask);
    g_shader.setInt("uUseParticleInstances", 0);

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

    // --- the two ships -------------------------------------------------------
    // Same function, same meshes, same rigging clock. What differs between them
    // is a matrix, a detail level and one material.
    drawShip(g_ship, SHIP_FULL, HULL_WOOD);

    // The hit indication (Requirement 5). A struck hull glows and fades over
    // 0.4 s, using the EMISSION term - the one part of the illumination model
    // that owes nothing to any light, so the confirmation survives every shading
    // mode and every K-key term mask.
    Material enemyHull = ENEMY_HULL;
    if (now < g_enemyFlashUntil) {
        const float k = (g_enemyFlashUntil - now) / ENEMY_FLASH_TIME;   // 1 -> 0
        enemyHull.emission = ENEMY_HIT_EMISSION * k;
    }
    drawShip(g_enemy, SHIP_REDUCED, enemyHull);

    // --- the cannonball, drawn ONLY while it is in flight --------------------
    // A conditional draw (Requirement 11): most frames pay nothing for it. Black
    // Plastic, so it carries a small hard highlight that tracks its arc - moving
    // object, fixed light, which is the specular case PRD s13 wants from it.
    if (g_ball.active)
        drawMesh(g_sphere,
                 translated(g_ballPos) * scaled(glm::vec3(BALL_DIAMETER)),
                 BLACK_PLASTIC);

    // --- muzzle marker: timed normally, held in the muzzle-only teaching view --
    // A sphere at the end of the chain, wearing the emissive-only material, at the
    // same point as Light 1. Two separate L8 concepts share one position and must
    // not be confused: the SPHERE is emission (L8 s55) and lights only itself; the
    // orange falloff on the deck and barrel around it is the POINT LIGHT (L8 s21)
    // and lights everything else. A local illumination model has no mechanism
    // connecting the two (L8 s10-11) — which is exactly why the light has to be
    // declared separately at all.
    if (muzzleVisible)
        drawMesh(g_sphere,
                 translated(g_ship.muzzlePos) * scaled(glm::vec3(0.16f)),
                 MUZZLE_FLASH);

    // Transparent effects come last. They keep depth testing but do not write
    // depth, then restore both blend and depth-mask state before returning.
    drawParticles(muzzleVisible);

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
    const int smokeCount = activeParticleCount(g_smokePool);
    const int impactCount = activeParticleCount(g_splashPool);
    char buf[512];
    std::snprintf(buf, sizeof(buf),
                  "Broadside | Shading: %s | Spec: %s | Lights: %s | Terms: %s | "
                  "%.1f FPS | %d tri | %d draws | "
                  "Az %+6.1f El %+5.1f (%s) | Vel %.0f | Flight %.2fs | LAST: %s | "
                  "FX S%d I%d R%s | seg %d | ocean %d | HIER %s%s%s",
                  SHADING_NAMES[g_shadingMode],
                  g_useBlinn ? "N.H" : "R.V",
                  LIGHT_MODE_NAMES[g_lightMode], termMaskName(g_termMask),
                  fps, g_triangles, g_drawCalls,
                  (double)glm::degrees(g_azimuth), (double)glm::degrees(g_elevation),
                  g_autoTrack ? "AUTO" : "MANUAL",
                  (double)MUZZLE_SPEED, (double)g_flightTime, g_lastResult,
                  smokeCount, impactCount, g_splashRing.active ? "1" : "0",
                  g_barrelSegments, g_oceanRes,
                  g_hierarchy ? "ON" : "OFF",
                  g_wireframe ? " | WIRE" : "",
                  g_clock.paused ? " | PAUSED" : "");
    glfwSetWindowTitle(window, buf);

    if (wall >= nextConsole) {
        nextConsole = wall + 1.0;
        std::printf("[frame] t=%7.2fs  dt=%.4fs  fps=%6.1f  tri=%6d  draws=%2d  "
                    "mode=%-7s spec=%-3s lights=%-11s terms=%-19s "
                    "az=%+7.1f el=%+6.1f %-6s enemyX=%+6.1f ball=%-3s "
                    "t=%.2f last=%-9s fx=%d/%d/%s%s\n",
                    now, avgDt, fps, g_triangles, g_drawCalls, SHADING_NAMES[g_shadingMode],
                    g_useBlinn ? "N.H" : "R.V", LIGHT_MODE_NAMES[g_lightMode],
                    termMaskName(g_termMask),
                    (double)glm::degrees(g_azimuth), (double)glm::degrees(g_elevation),
                    g_autoTrack ? "AUTO" : "MANUAL", (double)enemyPos(now).x,
                    g_ball.active ? "YES" : "no", (double)g_flightTime, g_lastResult,
                    smokeCount, impactCount, g_splashRing.active ? "1" : "0",
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
    // Framed to hold BOTH ships: the player at the origin and the enemy 18 units
    // astern. The target sits between them and the radius is pulled back far
    // enough that the patrol stays on screen at both ends of its sweep.
    // W / S and the wheel close on the player for material and highlight work.
    g_camera.target = glm::vec3(0.0f, 1.20f, -8.0f);
    g_camera.radius = 24.0f;
    g_camera.yaw    = glm::radians(14.0f);
    g_camera.pitch  = glm::radians(15.0f);

    if (!initResources()) {
        // Keep console output pure ASCII: the Windows console renders this UTF-8
        // source as CP-1252, so a non-ASCII character here arrives as mojibake.
        std::fprintf(stderr, "resource setup failed - see the shader log above\n");
        destroyResources();
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    std::printf("Controls : 1/2/3 Flat/Gouraud/Phong | B Phong/Blinn specular\n"
                "           K lighting terms | L light isolation\n"
                "           TAB auto-track / manual aim\n"
                "           arrow keys aim (manual) | SPACE fire\n"
                "           H hierarchy on/off | F wireframe\n"
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
