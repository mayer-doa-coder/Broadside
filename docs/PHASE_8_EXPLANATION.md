# Phase 8 Explanation

## Status

Phase 8 is complete. Milestone 2 is reached.

The ocean moves, and the ship rises, falls, and tilts with the water below it.

## What This Phase Adds

Phase 8 adds:

- two moving ocean waves;
- ocean movement in the vertex shader;
- correct wave normals;
- ship heave, roll, and pitch;
- an `H` key for the hierarchy demonstration;
- wave movement based only on the current time.

## How the Wave Works

The ocean uses two sine waves:

```text
height = wave 1 along X + wave 2 along Z
```

The full formula is:

```text
y(x, z, t) = A1*sin(K1*x + W1*t)
           + A2*sin(K2*z + W2*t + phase)
```

The waves move in different directions and at different speeds. Together they make the sea look less uniform.

## The GPU Moves the Ocean

The ocean grid is uploaded to the GPU once. It starts flat.

Every frame, the vertex shader calculates a new height for each grid vertex. The CPU does not rebuild or upload the grid again.

The shader uses world coordinates. This is important because the ocean mesh is a small unit grid that is scaled to 48 units. Using its original mesh coordinates would make the GPU wave disagree with the ship's CPU wave.

## Correct Wave Normals

Lighting needs a normal that follows the moving surface.

The normal is calculated from the wave slopes:

```text
normal = normalize(-slopeX, 1, -slopeZ)
```

This normal is exact for the wave formula. It stays smooth even when the ocean grid uses fewer triangles.

The normal is already in world space, so the shader does not transform it a second time.

## One Set of Wave Numbers

The CPU and GPU must use exactly the same wave values. If they differ, the ship can float above the water or cut through it.

The seven wave numbers are defined once in `src/Wave.h`. The same literals create both the C++ constants and the GLSL source. There is no second list of numbers to update.

## How the Ship Follows the Water

The CPU samples the wave at the ship's position.

| Ship movement | Value used |
|---|---|
| Heave | Wave height |
| Roll | Wave slope along X |
| Pitch | Wave slope along Z |

The ship root matrix contains this movement. The deck, cannon, masts, yards, sails, flag, muzzle sphere, and muzzle light all follow because they are children of that root.

A damping value of `0.8` keeps the ship from looking glued to the water.

## The `H` Key

Press `H` to switch wave following off or on.

When it is off, the ocean keeps moving but the ship stays at its normal height and remains level. The window title changes between `HIER ON` and `HIER OFF`.

This makes the effect of the root transform easy to see.

## Time and Pause

The wave and ship pose are calculated directly from the current simulation time. No animation frames are stored.

Pressing `P` freezes simulation time. The ocean and ship then stop on the same frame. Resuming continues the movement.

## Validation

The following checks passed:

- 32,719 automated checks completed with 0 failures;
- the seven required wave values are correct;
- the generated GLSL contains the same values as C++;
- calculated slopes match numerical slopes;
- all tested wave normals are normalized, face upward, and are perpendicular to the surface;
- sampled wave height stayed inside the maximum amplitude of `0.36`;
- asking for the same time twice gives the same ship matrix;
- heave equals freeboard plus the wave height;
- the ship tilts toward the water normal;
- all child transforms keep the same local relationship while the root rocks;
- the muzzle direction changes as the ship rocks;
- `HIER OFF` removes heave, roll, and pitch;
- the real OpenGL shaders compile and link;
- live captures show the ocean geometry, ship pose, and bright water reflection changing;
- Debug and Release builds pass;
- a strict Release build passes with warnings treated as errors;
- the program exits with code `0` and no runtime error.

The current scene still uses 15 draw calls and 10,026 triangles. Both values are inside the PRD limits.

## What Comes Later

Phase 9 adds separate movement to the sails and flag. Phase 10 adds the enemy ship. Phase 11 adds moving cannon aim.

## Result

Phase 8 is complete. The GPU creates the moving sea and its bright reflection, the CPU uses the same wave for the ship, and the full hierarchy follows the rocking root correctly.
