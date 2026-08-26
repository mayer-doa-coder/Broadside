# Phase 3 Explanation

## Status

Phase 3 is complete.

The orbit camera still works correctly with the Phase 5 lit scene.

## Goal

The goal was to create a 3D perspective view that can move around the scene.

## The Three Matrices

| Matrix | Simple meaning |
|---|---|
| Model | Places an object in the world |
| View | Places and points the camera |
| Projection | Turns the 3D scene into a perspective picture |

The vertex shader uses them in this order:

```glsl
gl_Position = uProjection * uView * uModel * vec4(aPos, 1.0);
```

They stay separate because lighting needs the world position and model matrix.

## Orbit Camera

The camera is in `src/Camera.h`.

It stores:

| Value | Meaning |
|---|---|
| `target` | Point being viewed |
| `radius` | Distance from the target |
| `yaw` | Angle around the target |
| `pitch` | Angle above or below the target |

These values produce the camera position. `glm::lookAt` builds the view matrix.

## Perspective Settings

| Setting | Value |
|---|---:|
| Field of view | 45 degrees |
| Near plane | 0.1 |
| Far plane | 300 |

The projection uses the current framebuffer width and height. This prevents stretching after a window resize.

## Controls and Limits

| Input | Action |
|---|---|
| Left mouse drag | Orbit |
| Mouse wheel | Zoom |
| `W` | Zoom in |
| `S` | Zoom out |

- Pitch stops at 89 degrees to keep `glm::lookAt` valid.
- Radius stays between 1.2 and 120.
- Yaw wraps after a full turn.
- The first mouse position is saved when dragging starts. A normal click does not move the camera.
- Camera input uses real time, so it keeps working while the scene is paused.

## Checks That Passed

- A separate click test kept radius, yaw, and pitch unchanged.
- A mouse drag changed yaw and pitch.
- The mouse wheel changed radius from `11.00` to `7.67`.
- Holding `W` reached the `1.20` minimum.
- `S` zoomed out again.
- Camera input worked while scene time stayed frozen.
- Wide `1280 x 720` and portrait `600 x 900` views worked.
- The focused camera audit also reached the 120 maximum radius and 89-degree pitch limit.

## Result

The Phase 3 checkpoint is passed. The camera can safely orbit, zoom, resize, and inspect view-dependent highlights.
