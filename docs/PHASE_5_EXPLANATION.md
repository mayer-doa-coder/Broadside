# Phase 5 Explanation

## Status

Phase 5 is complete. Milestone 1 is reached.

The scene now uses real lighting. The brass sphere has a bright side, a dark side, and a highlight that moves when the camera moves.

## Goal

The goal was to build the main lighting and shading system before building the ships.

Phase 5 needed:

- ambient, diffuse, and specular lighting;
- one active directional light;
- support for a point light and distance falloff;
- emission support;
- Flat, Gouraud, and Phong shading;
- one shader program for all three modes;
- correct normals after non-uniform scaling;
- a camera-dependent specular highlight.

## Lighting Equation

The shader adds four parts:

```text
final colour = emission + ambient + diffuse + specular
```

| Part | Simple meaning |
|---|---|
| Emission | Light produced by the object itself |
| Ambient | Small base light from the environment |
| Diffuse | Light based on how directly the surface faces the light |
| Specular | Shiny reflection based on the light, surface, and camera |

Point lights use this distance falloff:

```text
1 / (a0 + a1*d + a2*d*d)
```

A directional light does not fade with distance.

## One Shared Lighting Function

The lighting equation lives once in `src/Lighting.h`.

GLSL has no normal `#include`. `src/Shader.h` inserts the shared text into both shader stages after their `#version` line.

This matters because:

- Gouraud shading calculates lighting in the vertex shader;
- Phong shading calculates lighting in the fragment shader.

Both modes therefore use exactly the same equation.

## Three Shading Modes

All modes use one program and one `uShadingMode` value.

| Key | Mode | Where lighting is calculated |
|---|---|---|
| `1` | Flat | Uses one face normal for each triangle |
| `2` | Gouraud | At vertices, then colours are blended |
| `3` | Phong | At every fragment using blended normals |

Flat looks clearly faceted. Gouraud is smoother but can miss a small highlight between vertices. Phong calculates the most accurate smooth highlight and is the default mode.

## Normal Matrix

Normals use this matrix:

```cpp
glm::mat3(glm::transpose(glm::inverse(model)))
```

It is calculated on the CPU once for each drawn object.

Normal model-matrix multiplication fails when an object is stretched differently on different axes. The inverse-transpose matrix keeps the normal perpendicular to the surface.

## Light and Material

Phase 5 uses the directional sun and one brass material.

The sun provides warm diffuse light and a bright specular reflection. The brass values come from the L8 material table, including shininess `27.8974`.

A second point-light slot is already supported. It is disabled in this phase. Later phases use it for the muzzle flash.

## Checks That Passed

The independent OpenGL audit used the real mesh and shader files. It completed 234 checks with 0 failures.

Important results:

- Ambient matched `ka * global ambient`.
- Diffuse matched `kd * light * max(0, N dot L)`.
- Specular matched the mirror direction.
- Emission worked with all lights and terms disabled.
- Point-light falloff was correct at distances 2, 4, and 8.
- The inverse-transpose normal stayed perpendicular after a `20 x 1 x 20` scale.
- A normal transformed the simple way was badly wrong under the same scale.
- The sphere had a solid outline and a clear bright-to-dark range.
- The strongest specular point moved 21 pixels after the camera orbited.
- Flat, Gouraud, and Phong differed by more than 17,000 pixels each in the focused test.
- The real shaders compiled into one valid program.
- OpenGL reported no error.

The live program also confirmed:

- `1`, `2`, and `3` changed the mode and window title;
- the three full-scene images were visibly different;
- Phong showed a small bright highlight on the sphere;
- Flat showed clear polygon faces;
- the scene remained interactive while paused;
- the default scene used 9,422 triangles and 7 draw calls;
- the program exited with code `0` and no runtime error.

## What Is Not Part of Phase 5

Phase 5 does not need the full material table or an active muzzle-flash light. Those belong to Phase 6.

The Blinn-Phong and term-mask uniforms are supported, but their `B` and `K` controls are added later with the full demonstration controls.

## Result

The Phase 5 checkpoint is passed.

The project has one lit brass object with a moving specular highlight. Flat, Gouraud, and Phong shading give three different results on the same geometry.
