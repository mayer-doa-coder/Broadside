// Broadside — render loop, input, orchestration.
// Phase 1: the loop skeleton and the simulation clock (PRD Requirement 3).
// Phase 2: the shader program and the first triangle.
// Phase 3: MVP matrices and the orbit camera.
//
// The one architectural rule this phase exists to establish:
// every animated quantity in every later phase must be a closed form of `now`
// (Requirement 12 — no pre-computed animation, no keyframe tables, no frame-indexed arrays).
// updateScene / renderScene are the only two places motion is allowed to come from.

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>

#include "Camera.h"
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
// Phase 4 replaces this hand-rolled VAO/VBO pair with Mesh.h. The vertex layout
// here is already the one Mesh.h will use: position at location 0, normal at 1.
// ---------------------------------------------------------------------------
struct Vertex {
    float px, py, pz;   // position
    float nx, ny, nz;   // normal (also the colour source until Phase 5)
};

static Shader g_shader;
static GLuint g_cubeVAO   = 0;
static GLuint g_cubeVBO   = 0;
static GLsizei g_cubeVertexCount = 0;

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

    // W / S — zoom (PRD section 10)
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) g_camera.zoom(-KEY_ZOOM_RATE * realDt);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) g_camera.zoom(+KEY_ZOOM_RATE * realDt);

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

    // Aim, shading-mode and light-toggle keys land here in later phases.
}

// ---------------------------------------------------------------------------
// Scene update — ALL motion is computed here, from `now`, every frame.
// Still empty: in Phase 3 the camera moves, not the scene. Motion starts in Phase 8.
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
    if (!g_shader.loadFromFiles("shaders/phong.vert", "shaders/phong.frag"))
        return false;

    // Unit cube, side 1, centred on the origin. Phase 4 replaces this with
    // makeCube() in Mesh.h; the vertex format is already identical.
    //
    // Each face carries its own face normal, so the six faces come out as six
    // flat colours under the normals-as-RGB debug shader — which makes it obvious
    // at a glance whether the orbit and the depth test are behaving.
    //
    // GL_CULL_FACE is on and glFrontFace is GL_CCW, so the winding is not
    // cosmetic: every face below is counter-clockwise seen from OUTSIDE the cube.
    // Swap any two vertices of a face and that face turns invisible.
    const float h = 0.5f;
    const Vertex cube[36] = {
        // +Z (front)
        { -h, -h,  h,   0, 0, 1 }, {  h, -h,  h,   0, 0, 1 }, {  h,  h,  h,   0, 0, 1 },
        { -h, -h,  h,   0, 0, 1 }, {  h,  h,  h,   0, 0, 1 }, { -h,  h,  h,   0, 0, 1 },
        // -Z (back)
        {  h, -h, -h,   0, 0,-1 }, { -h, -h, -h,   0, 0,-1 }, { -h,  h, -h,   0, 0,-1 },
        {  h, -h, -h,   0, 0,-1 }, { -h,  h, -h,   0, 0,-1 }, {  h,  h, -h,   0, 0,-1 },
        // +X (right)
        {  h, -h,  h,   1, 0, 0 }, {  h, -h, -h,   1, 0, 0 }, {  h,  h, -h,   1, 0, 0 },
        {  h, -h,  h,   1, 0, 0 }, {  h,  h, -h,   1, 0, 0 }, {  h,  h,  h,   1, 0, 0 },
        // -X (left)
        { -h, -h, -h,  -1, 0, 0 }, { -h, -h,  h,  -1, 0, 0 }, { -h,  h,  h,  -1, 0, 0 },
        { -h, -h, -h,  -1, 0, 0 }, { -h,  h,  h,  -1, 0, 0 }, { -h,  h, -h,  -1, 0, 0 },
        // +Y (top)
        { -h,  h,  h,   0, 1, 0 }, {  h,  h,  h,   0, 1, 0 }, {  h,  h, -h,   0, 1, 0 },
        { -h,  h,  h,   0, 1, 0 }, {  h,  h, -h,   0, 1, 0 }, { -h,  h, -h,   0, 1, 0 },
        // -Y (bottom)
        { -h, -h, -h,   0,-1, 0 }, {  h, -h, -h,   0,-1, 0 }, {  h, -h,  h,   0,-1, 0 },
        { -h, -h, -h,   0,-1, 0 }, {  h, -h,  h,   0,-1, 0 }, { -h, -h,  h,   0,-1, 0 },
    };
    g_cubeVertexCount = 36;

    glGenVertexArrays(1, &g_cubeVAO);
    glGenBuffers(1, &g_cubeVBO);

    glBindVertexArray(g_cubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, g_cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cube), cube, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          (void*)offsetof(Vertex, px));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          (void*)offsetof(Vertex, nx));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
    return true;
}

static void destroyResources()
{
    if (g_cubeVBO) { glDeleteBuffers(1, &g_cubeVBO);      g_cubeVBO = 0; }
    if (g_cubeVAO) { glDeleteVertexArrays(1, &g_cubeVAO); g_cubeVAO = 0; }
    g_shader.destroy();
}

// ---------------------------------------------------------------------------
// Scene render — draw calls only. No state mutation belongs here.
// ---------------------------------------------------------------------------
static void renderScene(float now)
{
    g_shader.use();

    // View and projection are per-frame, not per-object. Phase 17 makes a point
    // of hoisting these out of the object loop; the structure is already right.
    g_shader.setMat4("uView", g_camera.view());
    g_shader.setMat4("uProjection", g_camera.projection(g_fbWidth, g_fbHeight));

    g_shader.setVec3("uTint", glm::vec3(1.0f, 0.85f, 0.35f));
    g_shader.setFloat("uTime", now);
    g_shader.setInt("uUseTint", 1);

    // The cube is deliberately stationary: the camera moves, not the model, so
    // the orbit cannot be mistaken for object rotation. Motion arrives in Phase 8.
    g_shader.setMat4("uModel", glm::mat4(1.0f));

    glBindVertexArray(g_cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, g_cubeVertexCount);
    glBindVertexArray(0);

    // Phase 5 : lit meshes through this same single program
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

    char buf[256];
    std::snprintf(buf, sizeof(buf),
                  "Broadside | t %.2fs | dt %.4fs | %.1f FPS | cam r%.1f yaw %.0f deg pitch %.0f deg%s",
                  now, avgDt, fps,
                  g_camera.radius, glm::degrees(g_camera.yaw), glm::degrees(g_camera.pitch),
                  g_clock.paused ? " | PAUSED" : "");
    glfwSetWindowTitle(window, buf);

    if (wall >= nextConsole) {
        nextConsole = wall + 1.0;
        std::printf("[frame] t=%7.2fs  dt=%.4fs  fps=%6.1f  cam r=%.2f yaw=%6.1f pitch=%6.1f%s\n",
                    now, avgDt, fps,
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

    if (!initResources()) {
        // Keep console output pure ASCII: the Windows console renders this UTF-8
        // source as CP-1252, so a non-ASCII character here arrives as mojibake.
        std::fprintf(stderr, "resource setup failed - see the shader log above\n");
        destroyResources();
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    std::printf("Controls : ESC quit | P pause | left-drag orbit | wheel or W/S zoom\n");
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
        renderScene(now);

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
