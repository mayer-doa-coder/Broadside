# Phase 7 Explanation

## Status

Phase 7 is complete.

The scene now looks like a simple ship. It has a hull, deck, bowsprit, two masts, two yards, two sails, a flag, and a cannon.

## Main Goal

The goal was to connect the ship parts with parent-child transforms before adding movement.

The project uses these main chains:

```text
hull -> deck -> mount -> yoke -> barrel -> muzzle
hull -> deck -> mast -> yard -> sail
                            \-> flag
```

If the hull moves or rotates, every child moves with it. No child needs separate code to follow the ship.

## Frames Do Not Store Scale

Each parent frame stores only position and rotation. Scale is added only when a mesh is drawn.

This rule is important. The hull is long because its cube is scaled on the Z axis. If that scale was passed to its children, the masts and cannon would also be stretched.

`buildShipFrames()` contains no scale operation. This keeps every parent frame safe to reuse.

## Cannon Movement

The cannon uses two rotations:

| Part | Rotation | Meaning |
|---|---|---|
| Yoke | Y axis | Turns the cannon left or right |
| Barrel | X axis | Raises or lowers the cannon |

The code turns the yoke first. It then raises the barrel inside the turned frame. This gives the required combined transform.

The code uses the negative X axis for elevation. With GLM's right-handed rotations, this makes a positive elevation angle point upward.

## Muzzle Position and Direction

The end of the barrel produces two values:

- `muzzlePos` is a world position and uses `w = 1`;
- `muzzleFwd` is a direction and uses `w = 0`.

The position follows translation. The direction does not. Both still follow the hull and cannon rotations.

The muzzle distance is calculated from the barrel length and offset. This keeps the muzzle at the visible end of the barrel if the barrel size changes later.

The point light and the small emissive sphere both use `muzzlePos`. In the running scene, the sphere sits at the barrel opening.

## Flag Parent Fix

The PRD makes the flag a child of the main yard. The earlier code attached it directly to the main mast.

This has been corrected. The flag is now built from the main-yard frame and moved up to the masthead. The scene graph now matches the PRD.

## Validation

The following checks passed:

- the ship renders as one connected object;
- all ten hierarchy frames have unit-length axes, so no scale leaked into them;
- every deck, mount, mast, yard, and flag offset is correct;
- the flag is a child of the main yard;
- a 45-degree root rotation carries the deck, cannon, masts, yards, sails, flag, and muzzle;
- zero aim points the barrel along the ship's positive Z axis;
- a 90-degree turn points the barrel to starboard;
- a 30-degree elevation gives the expected upward direction;
- moving the ship changes the muzzle position but not its local direction;
- the muzzle direction remains normalized;
- the shared Phase 6 and 7 validation completed 115 checks with 0 failures;
- the live OpenGL program closes normally and reports no runtime error.

The project has seven unique meshes. The current scene uses 15 draw calls and 10,026 triangles. All values are inside the PRD limits.

## What Comes Later

The ship is still static in Phase 7. Phase 8 adds waves and hull rocking. Phase 9 adds sail and flag movement. Phase 11 changes the cannon angles while the program runs.

## Result

Phase 7 is complete. The hierarchy matches the PRD, scale does not leak into child objects, and the whole ship follows one root transform correctly.
