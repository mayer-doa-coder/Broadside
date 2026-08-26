# Phase 0 Explanation

## Status

Phase 0 is complete.

Later phases added shapes and lighting. The basic window and tool setup from Phase 0 are still working.

## Goal

The goal was simple: build the program, open an OpenGL window, show a clear colour, and close safely.

## Main Tools

| Tool | Job |
|---|---|
| CMake | Creates the build files |
| MSVC | Compiles the C++ code on Windows |
| GLFW | Creates the window and reads input |
| GLAD | Loads OpenGL functions |
| GLM | Provides vector and matrix maths |
| OpenGL 3.3 Core | Draws the scene |

Only GLFW, GLAD, GLM, OpenGL, and the C++ standard library are used. No game engine or physics library is used.

## Startup Order

The program starts in this order:

1. Start GLFW.
2. Request OpenGL 3.3 Core.
3. Create a `1280 x 720` window named `Broadside`.
4. Make the OpenGL context active.
5. Load OpenGL functions with GLAD.
6. Set the viewport and OpenGL options.
7. Run the main loop.
8. Delete OpenGL resources before closing the context.

This order matters. OpenGL functions cannot be used before GLAD is ready.

## Build and Run

From a terminal:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
```

The VS Code CMake settings also use the correct executable folder as the working folder. This lets the program find its shader files.

## Checks That Passed

- CMake configured with the x64 MSVC compiler.
- Debug and Release builds completed.
- The window opened at `1280 x 720`.
- OpenGL 3.3 and GLSL 3.30 were reported.
- A blue-grey clear colour was visible.
- `ESC` closed the program.
- The program exited with code `0`.
- No forbidden dependency was found.

The Debug build prints one `LNK4098` warning from the supplied GLFW library. It is not a project source error. The Release build is clean.

## Result

The Phase 0 checkpoint is passed. The project has a working OpenGL toolchain and window.
