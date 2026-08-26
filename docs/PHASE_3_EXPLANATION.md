# Phase 3 Explanation

## Status

Phase 3 is complete.

The program now draws a real 3D cube. You can move the camera around it, look at it from above, and move closer or further away.

Phase 3 does not add lights or ship models yet. Those come later.

## What Changed From Phase 2

Phase 2 drew a flat triangle. The triangle was already in screen positions, so no real 3D was needed.

Phase 3 draws a cube in a 3D world. To do that, the program must answer three questions every frame:

1. Where is the object in the world?
2. Where is the camera, and where is it looking?
3. How does 3D depth turn into a flat picture?

Each question is answered by one matrix.

## The Three Matrices

| Uniform | Question it answers |
|---|---|
| `uModel` | Where the object sits in the world |
| `uView` | Where the camera is and where it looks |
| `uProjection` | How depth becomes a flat picture |

The vertex shader multiplies them together:

```glsl
gl_Position = uProjection * uView * uModel * vec4(aPos, 1.0);
```

They are sent as three separate matrices instead of one combined matrix. Phase 5 will need `uModel` on its own to work out where each point sits in the world for lighting.

The projection settings are:

| Setting | Value | Meaning |
|---|---|---|
| Field of view | 45 degrees | How wide the view is |
| Near plane | 0.1 | Closer than this is not drawn |
| Far plane | 300 | Further than this is not drawn |

## The Orbit Camera

The new file is `src/Camera.h`.

The camera does not store an X, Y, Z position. Instead it stores three simple numbers:

| Value | Meaning |
|---|---|
| `radius` | How far the camera is from the object |
| `yaw` | How far around the object it has turned |
| `pitch` | How high above the object it is |

The position is worked out from these three numbers each frame.

This is like a camera on an invisible arm pointing at the object. Because the arm always points at the target, the object can never slide out of view. That is exactly what is needed when someone wants to inspect an object.

## Two Safety Limits

**Pitch stops at 89 degrees, not 90.**

At exactly 90 degrees the camera looks straight down. The "up" direction and the "looking" direction would point along the same line. The maths that builds the view matrix cannot handle that and the picture breaks. Stopping one degree early avoids this completely.

**Radius stays between 1.2 and 120.**

This stops the camera from going inside the object, and stops it from drifting past the far plane where nothing is drawn.

## Controls

| Key or action | Result |
|---|---|
| Left mouse drag | Turn the camera around the object |
| Mouse wheel | Zoom in or out |
| `W` | Zoom in |
| `S` | Zoom out |
| `P` | Pause or continue time |
| `ESC` | Close the program |

Dragging feels like turning the object with your hand. Drag right and the front of the object turns right. Drag down and the top tips toward you.

Zoom multiplies the distance instead of adding to it. This makes one zoom step feel the same whether you are close or far away.

## Two Problems That Were Fixed

### Problem 1: the camera froze when the scene was paused

Phase 1 gave the loop one time value called `dt`. When `P` pauses the scene, `dt` becomes `0`. Everything that used `dt` stopped, including the camera.

That is wrong. Pausing is meant to let someone freeze a frame and then look around it.

So the loop now keeps two time values:

| Value | Used by | While paused |
|---|---|---|
| `dt` | Scene animation | `0` |
| `realDt` | Camera controls | Keeps running |

Now pressing `P` freezes the scene but the camera still works.

### Problem 2: the view could jump when you clicked

If the program compared the cursor to wherever it was last frame, then clicking in a new part of the window would look like a huge sudden drag. The view would jump.

The fix is to remember the cursor position at the moment the button is pressed. The first frame of a drag then has a movement of zero, so nothing jumps.

## The Cube

The cube is one unit wide and sits at the centre of the world. It is built from 36 vertices, which is 6 faces made of 2 triangles each.

Every face has its own normal value. The debug colouring turns normals into colours, so each of the six faces gets its own flat colour. This makes it very easy to see that the camera and the depth test are working.

All faces are wound counter-clockwise when seen from outside. Back-face culling is switched on, so a face wound the wrong way would simply disappear.

**The cube never moves. Only the camera moves.** This is deliberate. If both moved, it would be hard to tell whether the camera or the object was turning.

Phase 4 will replace this hand-written cube with a proper `makeCube()` function in `Mesh.h`.

## How This Was Tested

The picture was not judged by eye. The program was made to read back its own finished image and the drawn cube was measured in pixels.

The size a cube should appear at can be worked out with a formula. The measured size was then compared to it.

| View | Window size | Size it should be | Size it was | Result |
|---|---|---|---|---|
| Front, distance 4 | 1280 x 720 | 248.3 px | 248 x 248 px | Passed |
| Corner, distance 4 | 1280 x 720 | 3 faces visible | 3 faces visible | Passed |
| Front, distance 4 | 600 x 900 | 310.4 px | 310 x 310 px | Passed |

**Why the square shape matters.** The window is 1280 by 720, which is a wide shape. If the aspect ratio were handled wrongly, the cube would come out as a rectangle. It measured exactly 248 by 248, a perfect square, so the picture is not stretched.

**Why the second size matters.** During the test the window was resized to 600 by 900, a tall shape. The cube stayed square. This proves the program rebuilds the projection when the window changes.

The controls were tested as well:

| Check | Should be | Was | Result |
|---|---|---|---|
| Drag right 100 px | yaw `0.62` degrees | `0.62` | Passed |
| Drag down 50 px | pitch `39.19` degrees | `39.19` | Passed |
| Click far from the last point | No jump | No jump | Passed |
| Drag down a long way | pitch stops at `89` | `89.00` | Passed |
| Hold `W` | Gets closer, stops at `1.2` | `1.20` | Passed |
| Hold `S` | Gets further away | `6.57` | Passed |
| Mouse wheel | Distance changes | Changed correctly | Passed |
| Zoom while paused | Still works | Works | Passed |
| `ESC` | Exit code `0` | `0` | Passed |

Both the Debug and Release builds completed from a clean folder.

The Debug build still shows one `LNK4098` warning. This comes from the ready-made GLFW library and does not affect the program.

## One Thing To Check By Hand

The drag maths was tested by feeding the program a scripted list of cursor positions, and every result was correct.

However, Windows would not let the test move the real mouse pointer. So it is worth doing one real mouse drag yourself to check the speed feels right.

If it feels too fast or too slow, change `MOUSE_SENSITIVITY` in `src/main.cpp`. It is currently `0.006`.

## What Comes Next

Phase 4 adds `src/Mesh.h`. This builds a proper shape library with a cube, cylinder, sphere, quad, and grid, plus a function that works out smooth normals.

Its goal is that these shapes appear as solid objects, and that changing the detail level visibly changes the number of triangles in wireframe mode.
