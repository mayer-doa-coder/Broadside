// Broadside — render loop, input, orchestration.
// Phase 1: the loop skeleton and the simulation clock (PRD Requirement 3).
// Phase 2: the shader program and the first triangle.
// Phase 3: MVP matrices and the orbit camera.
// Phase 4: seven generators cover the eight PRD mesh roles; all are indexed.
// Phase 5: the L8 illumination model and the L9 Flat / Gouraud / Phong switch.
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
#include "Mesh.h"
#include "Shader.h"

#include <cstddef>
#include <cstdio>

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
// Lighting (PRD 11.1) — TWO lights, no more, no fewer.
//
// Light 0, the sun, is the only one Phase 5 enables. Light 1 is the muzzle flash:
// its slot exists here and in the shader from the start, but it stays dark until
// Phase 6 gives it a position and Phase 12 gives it something to fire.
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

// Light 1 — muzzle flash. Declared now, enabled in Phase 6.
static const Light MUZZLE_FLASH_OFF = {
    1,
    glm::vec3(0.0f),
    glm::vec3(0.0f),
    glm::vec3(1.00f, 0.75f, 0.35f),        // orange flare
    glm::vec3(1.00f, 0.85f, 0.60f),
    glm::vec3(1.0f, 0.09f, 0.032f),        // a0, a1, a2  — L8 slide 21
    0.0f                                   // OFF
};

// I_a_global — cool sky-scattered fill (L8 slide 57)
static const glm::vec3 GLOBAL_AMBIENT(0.15f, 0.15f, 0.18f);

// ---------------------------------------------------------------------------
// Material — Phase 5 needs exactly one. Phase 6 promotes this to the full
// L8 slide 60 table (Polished Silver, Black Plastic, Ocean, Hull, Sailcloth).
//
// Brass, verbatim from L8 slide 60. Do not "improve" these numbers: they are the
// cited evidence, and n_s = 27.9 is the middle of the 4 -> 160 range the frame is
// built to demonstrate.
// ---------------------------------------------------------------------------
static const glm::vec3 BRASS_KA(0.329412f, 0.223529f, 0.027451f);
static const glm::vec3 BRASS_KD(0.780392f, 0.568627f, 0.113725f);
static const glm::vec3 BRASS_KS(0.992157f, 0.941176f, 0.807843f);
static const glm::vec3 BRASS_EMISSION(0.0f);
static const float     BRASS_SHININESS = 27.8974f;

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

// ---------------------------------------------------------------------------
// Scene update — ALL motion is computed here, from `now`, every frame.
// Still empty through Phase 5: the camera moves, not the scene. Motion starts in
// Phase 8, and every value it produces must be a closed form of `now`.
// ---------------------------------------------------------------------------
static void updateScene(float /*now*/, float /*dt*/)
{
    // Phase 8  : ocean wave phase, hull roll and pitch
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
    if (!g_shader.loadFromFiles("shaders/phong.vert", "shaders/phong.frag", LIGHTING_GLSL))
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
// The Phase 4 checkpoint scene — the mesh gallery.
//
// One instance of each generator, in a row, on the ocean grid. Not the final
// scene: it exists so the checkpoint can be judged by eye. The three meshes the
// guide names (sphere, cylinder, cube) sit in the middle where they are easiest
// to read, and the row is spaced so no two silhouettes overlap from the default
// camera angle.
//
// Phase 7 replaces this function with drawShip(); the generators do not change.
// ---------------------------------------------------------------------------
static const float GALLERY_SPACING = 1.6f;
static const float GALLERY_Y       = 0.0f;    // unit meshes are centred, so this is their centre
static const float OCEAN_Y         = -1.2f;
static const float OCEAN_EXTENT    = 20.0f;

// x position of gallery slot i, with the six slots centred on the origin
static float gallerySlot(int i)
{
    return ((float)i - 2.5f) * GALLERY_SPACING;
}

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
// Scene render — draw calls only. No state mutation belongs here.
// ---------------------------------------------------------------------------
static void renderScene(float /*now*/)
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
    setLight(g_shader, 1, MUZZLE_FLASH_OFF);       // Phase 6 lights this one

    g_shader.setInt("uShadingMode", g_shadingMode);
    g_shader.setInt("uUseBlinn", g_useBlinn);
    g_shader.setInt("uTermMask", g_termMask);

    // One material for the whole frame in Phase 5. Phase 6 moves this inside
    // drawMesh so each object can carry its own.
    g_shader.setVec3("uKa", BRASS_KA);
    g_shader.setVec3("uKd", BRASS_KD);
    g_shader.setVec3("uKs", BRASS_KS);
    g_shader.setVec3("uEmission", BRASS_EMISSION);
    g_shader.setFloat("uShininess", BRASS_SHININESS);

    g_drawCalls = 0;
    g_triangles = 0;

    // drawMesh is where glm::scale is applied and NOWHERE else. Phase 7 keeps
    // this rule when the hierarchy arrives: parent frames stay unscaled so a
    // parent's scale can never leak into a child (AGENT.md, the common bug).
    auto drawMesh = [](const Mesh& mesh, const glm::mat4& frame, const glm::vec3& dims) {
        const glm::mat4 model = frame * glm::scale(glm::mat4(1.0f), dims);

        // (M^-1)^T on the CPU, once per object per frame (guide 5.3). The ocean is
        // scaled 20x on X and Z but 1x on Y — under that non-uniform scale a
        // naively transformed normal tilts off the true surface and the specular
        // streak lands in the wrong place.
        const glm::mat3 normalMatrix = glm::mat3(glm::transpose(glm::inverse(model)));

        g_shader.setMat4("uModel", model);
        g_shader.setMat3("uNormalMatrix", normalMatrix);
        mesh.draw();
        ++g_drawCalls;
        g_triangles += mesh.triangleCount();
    };

    // Ocean grid — flat for now; Phase 8 displaces it in the vertex shader.
    drawMesh(g_grid,
             glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, OCEAN_Y, 0.0f)),
             glm::vec3(OCEAN_EXTENT, 1.0f, OCEAN_EXTENT));

    // The checkpoint sphere sits first, at its native unit size. Everything in
    // the row wears brass so the ONLY variable between them is geometry, which is
    // what makes the L9 mode comparison read cleanly. Phase 6 hands each object
    // its own material.
    drawMesh(g_sphere,
             glm::translate(glm::mat4(1.0f), glm::vec3(gallerySlot(0), GALLERY_Y, 0.0f)),
             glm::vec3(1.0f));
    drawMesh(g_cylinder,
             glm::translate(glm::mat4(1.0f), glm::vec3(gallerySlot(1), GALLERY_Y, 0.0f)),
             glm::vec3(1.0f));
    drawMesh(g_cube,
             glm::translate(glm::mat4(1.0f), glm::vec3(gallerySlot(2), GALLERY_Y, 0.0f)),
             glm::vec3(1.0f));
    drawMesh(g_cone,
             glm::translate(glm::mat4(1.0f), glm::vec3(gallerySlot(3), GALLERY_Y, 0.0f)),
             glm::vec3(1.0f));
    drawMesh(g_quad,
             glm::translate(glm::mat4(1.0f), glm::vec3(gallerySlot(4), GALLERY_Y, 0.0f)),
             glm::vec3(1.0f));
    // The ring stays flat in XZ, its real orientation as a splash ring. Now that
    // objects are lit rather than coloured by normal, it reads against the ocean
    // on its own.
    drawMesh(g_ring,
             glm::translate(glm::mat4(1.0f), glm::vec3(gallerySlot(5), GALLERY_Y, 0.0f)),
             glm::vec3(1.0f));

    // Phase 6 : one material per object, and the muzzle flash lit
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
                  "Broadside | Shading: %s | %.1f FPS | seg %d | ocean %d | %d tri | %d draws%s%s",
                  SHADING_NAMES[g_shadingMode],
                  fps, g_barrelSegments, g_oceanRes, g_triangles, g_drawCalls,
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

    // Frame the whole gallery row. Phase 3 defaulted to r=4 around a single cube;
    // six objects at 1.6 spacing need the camera pulled back and lifted enough to
    // read the flat ring and the grid.
    g_camera.target = glm::vec3(0.0f, -0.2f, 0.0f);
    g_camera.radius = 11.0f;
    g_camera.yaw    = glm::radians(12.0f);
    g_camera.pitch  = glm::radians(16.0f);

    if (!initResources()) {
        // Keep console output pure ASCII: the Windows console renders this UTF-8
        // source as CP-1252, so a non-ASCII character here arrives as mojibake.
        std::fprintf(stderr, "resource setup failed - see the shader log above\n");
        destroyResources();
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    std::printf("Controls : 1/2/3 Flat/Gouraud/Phong | F wireframe | +/- tessellation\n"
                "           left-drag orbit | wheel or W/S zoom | P pause | ESC quit\n");
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
