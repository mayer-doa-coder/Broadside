# Phase 1 Explanation

## Status

Phase 1 is complete.

The main loop still runs correctly after all work through Phase 5.

## Goal

The goal was to make a clean loop that runs continuously.

Every frame follows this order:

1. Read the current time.
2. Calculate the frame time.
3. Read input.
4. Update the scene.
5. Clear the colour and depth buffers.
6. Draw the scene.
7. Show the finished frame.
8. Read window events.

## Time Values

| Value | Meaning |
|---|---|
| `now` | Current scene time |
| `dt` | Scene time used by movement |
| `realDt` | Real time used by camera input |

`dt` stops at `0` while the scene is paused. `realDt` keeps running, so the camera can still move around a paused frame.

Large frame times are limited to `0.10` seconds. This prevents one slow frame from causing a large movement jump.

All future animation must be calculated from time inside the loop. The project does not use saved animation frames.

## Main Functions

| Function | Job |
|---|---|
| `processInput()` | Reads keyboard and mouse input |
| `updateScene()` | Calculates scene movement in later phases |
| `renderScene()` | Draws the current scene |
| `reportFrame()` | Reports FPS and useful scene values |

At the end of Phase 1, the update and draw functions were mostly empty. Later phases filled in the drawing code while keeping the same loop structure.

## OpenGL Settings

- Depth testing hides surfaces behind nearer surfaces.
- Back-face culling skips triangles facing away from the camera.
- V-sync keeps frame delivery steady.
- The framebuffer callback updates the viewport after a resize.
- Colour and depth buffers are cleared every frame.

## Controls

| Key | Action |
|---|---|
| `P` | Pause or continue scene time |
| `ESC` | Close the program |

## Checks That Passed

- The loop continued at a steady rate.
- A live frame reported `dt = 0.0164` seconds at about 61 FPS.
- Pausing changed `dt` to `0.0000`.
- The window kept drawing while paused.
- Camera controls still worked while paused.
- Resize, minimize, restore, and exit behavior remained safe.
- The final run exited with code `0` and no runtime error.

The exact FPS depends on the monitor and graphics driver. A 60 Hz display is near `0.0167` seconds per frame. A 144 Hz display is near `0.0069` seconds.

## Result

The Phase 1 checkpoint is passed. The loop is stable and ready for time-based animation in later phases.
