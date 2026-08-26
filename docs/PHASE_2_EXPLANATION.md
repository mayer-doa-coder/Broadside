# Phase 2 Explanation

## Status

Phase 2 is complete.

The program now loads two shader files and draws a coloured triangle. It also prints a clear message when a shader has an error.

Phase 2 does not add a camera, lighting, or 3D models. Those parts come later.

## Goal of Phase 2

Phase 2 has two main goals:

1. Load and prepare GLSL shaders.
2. Draw the first shape with OpenGL.

## New Files

| File | Job |
|---|---|
| `src/Shader.h` | Loads, compiles, links, uses, and deletes a shader program |
| `shaders/phong.vert` | Handles each triangle vertex |
| `shaders/phong.frag` | Calculates the colour of each pixel |

The shader files are called `phong` because Phase 5 will add Phong lighting to them.

## How Shader Loading Works

The `Shader` class follows these steps:

1. Open the vertex shader file.
2. Open the fragment shader file.
3. Compile the vertex shader.
4. Check its compile result and log.
5. Compile the fragment shader.
6. Check its compile result and log.
7. Link both shaders into one program.
8. Check the link result and log.

The program stops safely if any step fails. It does not continue with a broken shader.

## Shader Error Messages

Shader errors include the shader name and the driver message. For example:

```text
GLSL compile FAILED: shaders/phong.frag
0(26) : error C1503: undefined variable "sinn"
```

The program also reports missing or empty shader files. A failed shader setup exits with code `-1`.

This prevents a silent black-screen problem.

## How the Triangle Is Stored

The triangle has three vertices. Each vertex has:

- a position,
- a normal value used as colour data in this phase.

The VBO stores the vertex data. The VAO describes how OpenGL should read that data.

The vertices use counter-clockwise order. This is important because back-face culling is enabled.

## What the Shaders Do

The vertex shader:

- reads the position and normal,
- applies the `uTransform` matrix,
- sends the normal to the fragment shader.

The fragment shader:

- changes the normal into a colour,
- adds a warm colour tint,
- uses `uTime` to make the colour change slowly.

The colour change is calculated from time. It does not use saved animation frames.

## Uniform Setters

The `Shader` class has four uniform helpers:

| Function | Sends |
|---|---|
| `setInt()` | An integer |
| `setFloat()` | A decimal number |
| `setVec3()` | Three decimal numbers |
| `setMat4()` | A 4 by 4 matrix |

All four helpers are used when the triangle is drawn.

## Controls

| Key | Action |
|---|---|
| `P` | Pause or continue time and the colour change |
| `ESC` | Close the program |

## Cleanup

The VBO, VAO, and shader program are deleted before the OpenGL window is destroyed. This releases the GPU resources safely.

## Validation

The following checks passed on Windows:

| Check | Result |
|---|---|
| Debug build completes | Passed |
| Release build completes | Passed |
| Strict `/W4` source build completes without source warnings | Passed |
| Vertex and fragment shaders compile | Passed |
| Shader program links | Passed |
| Coloured triangle appears | Passed |
| Running frames change colour | Passed |
| Paused colour stays unchanged | Passed |
| Vertex shader error is reported | Passed |
| Fragment shader error is reported | Passed |
| Shader link error is reported | Passed |
| Missing shader file is reported | Passed |
| Shader failures exit with code `-1` | Passed |
| `ESC` exits with code `0` | Passed |

The successful run printed no runtime errors. The captured OpenGL area was `1280 x 720`. Its corner matched the blue-grey clear colour, and its centre contained the triangle.

The Debug build still has one non-fatal `LNK4098` warning from the pre-built GLFW library. The Release build is clean.

## What Comes Next

Phase 3 adds a camera and model, view, and projection matrices. Its checkpoint is a 3D cube that can be viewed from different angles.
