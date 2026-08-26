# Phase 0 Explanation

## Status

Phase 0 is complete.

The project can build and open a basic OpenGL window. The window has a solid blue-grey background. Pressing `ESC` closes it safely.

## Purpose of Phase 0

Phase 0 prepares the tools and the project folder. It does not draw ships, waves, or other 3D objects yet.

This phase proves that these parts work together:

- Visual Studio Code
- MSVC Build Tools 2022
- CMake
- OpenGL 3.3 Core
- GLFW 3.5.1
- GLAD for OpenGL 3.3 Core
- GLM 1.0.3
- C++17

## Important Files

- `CMakeLists.txt` tells CMake how to build the program.
- `src/main.cpp` creates the window and runs the basic loop.
- `src/glad.c` loads OpenGL functions.
- `include/` contains the GLAD headers.
- `external/glfw/` contains GLFW for the window and keyboard input.
- `external/glm/` contains the math library for later phases.
- `.vscode/` contains the VS Code build, run, debug, and extension settings.
- `shaders/` is ready for shader files. The real shaders are added in Phase 2.

## What the Program Does

The program follows these simple steps:

1. Start GLFW.
2. Ask for OpenGL 3.3 Core.
3. Create a `1280 x 720` window named `Broadside`.
4. Load the OpenGL functions with GLAD.
5. Print the OpenGL, GLSL, and graphics card information.
6. Clear the window with one solid colour every frame.
7. Check the keyboard and window events.
8. Close the window when `ESC` is pressed.
9. Destroy the window and stop GLFW.

## How to Build and Run

In Visual Studio Code:

1. Open the `Broadside` folder.
2. Select the `Visual Studio Build Tools 2022 - amd64` kit.
3. Run `CMake: Configure` from the Command Palette.
4. Press `F7` to build.
5. Press `Shift+F5` to run.
6. Press `ESC` to close the window.

You can also use the terminal:

```powershell
cmake -S . -B build
cmake --build build --config Release
```

If `cmake` is not found just after installation, close and reopen Visual Studio Code or the terminal. This reloads the system `PATH`.

## Validation Result

The following checks passed on Windows:

| Check | Result |
|---|---|
| CMake configuration with the x64 MSVC compiler | Passed |
| Debug build | Passed |
| Release build | Passed |
| OpenGL 3.3 Core window opens | Passed |
| Window title is `Broadside` | Passed |
| Solid clear colour is visible | Passed |
| `ESC` closes the program | Passed |
| Program exits with code `0` | Passed |
| Required VS Code settings are valid | Passed |
| Only the allowed libraries are used | Passed |

The program reported OpenGL 3.3, GLSL 3.30, and an NVIDIA GeForce RTX 5050 Laptop GPU during the validation.

## What Comes Next

Phase 1 adds time values, frame timing, input functions, depth testing, and back-face culling. Those features are not part of Phase 0.
