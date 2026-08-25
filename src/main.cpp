// Broadside — Phase 0: toolchain checkpoint.
// Goal: open a window with a solid clear colour, close cleanly on ESC. Nothing else.
// The render loop proper (timing, updateScene/renderScene) arrives in Phase 1.

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <cstdio>

static void glfwErrorCallback(int code, const char* description)
{
    std::fprintf(stderr, "[GLFW error %d] %s\n", code, description);
}

static void framebufferSizeCallback(GLFWwindow* /*window*/, int width, int height)
{
    glViewport(0, 0, width, height);
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

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Broadside", NULL, NULL);
    if (!window) {
        std::fprintf(stderr, "glfwCreateWindow failed\n");
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);

    // Must happen before ANY gl* call (guide 0.6, common failure #1)
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::fprintf(stderr, "gladLoadGLLoader failed\n");
        glfwTerminate();
        return -1;
    }

    std::printf("OpenGL   : %s\n", (const char*)glGetString(GL_VERSION));
    std::printf("GLSL     : %s\n", (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION));
    std::printf("Renderer : %s\n", (const char*)glGetString(GL_RENDERER));

    while (!glfwWindowShouldClose(window)) {
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, GLFW_TRUE);

        glClearColor(0.35f, 0.45f, 0.55f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
