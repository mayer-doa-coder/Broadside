# Phase 2 Explanation

## Status

Phase 2 is complete.

The first triangle was an early checkpoint. Later phases replaced it with the 3D mesh gallery, but the same shader-loading system is still used.

## Goal

The goal was to load GLSL files, compile them, link one shader program, and report useful errors.

## Main Files

| File | Job |
|---|---|
| `src/Shader.h` | Loads, compiles, links, uses, and deletes a shader program |
| `shaders/phong.vert` | Handles vertices |
| `shaders/phong.frag` | Calculates the final pixel colour |

Phase 5 also uses `src/Lighting.h`. The shader loader inserts this shared lighting block into both shader stages.

## Shader Loading

The loader performs these steps:

1. Read both shader files.
2. Compile the vertex shader.
3. Print its error log if it fails.
4. Compile the fragment shader.
5. Print its error log if it fails.
6. Link both stages into one program.
7. Print the link log if linking fails.
8. Delete temporary shader objects.

A failed load leaves no half-built program behind.

## Error Messages

The loader reports:

- a vertex shader compile error;
- a fragment shader compile error;
- a shader link error;
- a missing shader file;
- an empty shader file.

The message includes the file name and the graphics driver's explanation. This prevents a silent black screen.

## Uniform Helpers

The current `Shader` class can send:

| Function | Data |
|---|---|
| `setInt()` | One integer |
| `setFloat()` | One decimal value |
| `setVec3()` | A three-value vector |
| `setMat3()` | A 3 by 3 matrix |
| `setMat4()` | A 4 by 4 matrix |

`setMat3()` was added in Phase 5 for normal matrices. The original Phase 2 helpers remain in use.

## Checks That Passed

- The real Phase 5 vertex and fragment shaders compiled.
- The program linked one valid shader program.
- A broken vertex shader printed a readable error on line 3.
- A broken fragment shader printed a readable error on line 3.
- Mismatched shader stages printed a link error.
- A missing file printed its path and working-folder advice.
- Every failure left the `Shader` object invalid.
- Shader resources were deleted before the OpenGL context closed.

Debug, Release, and strict `/W4` builds passed. The strict build had no project source warning.

## Result

The Phase 2 checkpoint is passed. Shader loading succeeds when files are correct and explains the problem when they are not.
