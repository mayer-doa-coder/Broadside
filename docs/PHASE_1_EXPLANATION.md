# Phase 1 Explanation

## Status

Phase 1 is complete.

The program now has a clear and continuous render loop. It reads input, updates the scene, clears the window, draws the scene, and shows the finished frame.

Phase 1 does not draw a ship or a triangle. Those parts come later.

## Goal of Phase 1

The goal is to prepare the main loop for all later work.

The loop must:

- run until the user closes the program,
- read time from `glfwGetTime()`,
- calculate the time between frames,
- handle keyboard input,
- update the scene,
- draw the scene,
- show the new frame.

## Time Values

The program uses two important time values:

| Value | Simple meaning |
|---|---|
| `now` | How long the scene has been running |
| `dt` | How long the last frame took |

All later animation must use these time values. The project must not use recorded animation or a list of saved positions.

The program limits `dt` to `0.10` seconds. This stops one long frame from causing a very large movement.

## Main Loop

Every frame follows the same order:

1. Read the current time.
2. Calculate `now` and `dt`.
3. Read keyboard input.
4. Update the scene.
5. Clear the colour and depth buffers.
6. Draw the scene.
7. Show the completed frame.
8. Read window events.

This order keeps the code easy to understand and extend.

## Main Functions

| Function | Job |
|---|---|
| `processInput()` | Reads keys such as `ESC` and `P` |
| `updateScene()` | Will calculate movement in later phases |
| `renderScene()` | Will draw objects in later phases |
| `reportFrame()` | Shows time and frame-rate information |

`updateScene()` and `renderScene()` are empty now. This is correct for Phase 1.

## Pause and Exit Controls

| Key | Action |
|---|---|
| `P` | Pause or continue scene time |
| `ESC` | Close the program |

When the program is paused, `now` stops changing and `dt` becomes `0`. The window still refreshes. Pressing `P` again continues time without a jump.

## OpenGL Settings

Phase 1 turns on these settings:

- Depth testing makes near objects hide far objects.
- Back-face culling skips faces that point away from the camera.
- The colour and depth buffers are cleared every frame.
- The viewport changes when the window size changes.
- V-sync keeps the frame rate close to the monitor refresh rate.

These settings prepare the project for 3D drawing.

## Frame Information

The window title and console show values like these:

```text
Broadside | t 12.34s | dt 0.0069s | 144.0 FPS
```

A 60 Hz monitor normally gives a `dt` near `0.0167` seconds. A 144 Hz monitor normally gives a `dt` near `0.0069` seconds. Both values are correct.

## Validation

The following checks passed on Windows:

| Check | Result |
|---|---|
| Debug build completes | Passed |
| Release build completes | Passed |
| Window stays open and keeps drawing | Passed |
| Frame time stays steady | Passed |
| `P` pauses time | Passed |
| `P` continues time without a jump | Passed |
| Window resize works | Passed |
| Minimize and restore work | Passed |
| `ESC` closes the program | Passed |
| Program exits with code `0` | Passed |

A five-minute test stayed near 144 FPS. The measured `dt` stayed between `0.0068` and `0.0070` seconds after the input and window tests. No runtime errors were printed.

The Debug build may show one `LNK4098` warning from the pre-built GLFW library. The build still completes. The Release build is clean.

## What Comes Next

Phase 2 adds shader loading and the first coloured triangle. It will also print clear shader errors when a shader cannot compile.
