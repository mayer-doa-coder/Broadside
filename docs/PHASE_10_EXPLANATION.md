# Phase 10 Explanation

## Status

Phase 10 is complete and verified.

## What This Phase Does

This phase adds one enemy ship. The enemy moves from side to side and rocks on the same ocean as the player.

## The Enemy Reuses Existing Work

The project does not create a second set of ship meshes. Both ships use the same `drawShip()` function and the same mesh objects.

The enemy looks different because it uses:

- a smaller root scale;
- a darker hull material;
- a reduced detail mode.

The reduced enemy has only four drawn parts: hull, mast, yard, and sail. It is still easy to recognize as a ship from a distance.

## The Patrol

The enemy's horizontal position is:

```text
x = sin(0.25 × time) × 14
```

This moves the enemy smoothly between `-14` and `+14`. Its height and forward position are handled separately.

## Rocking on the Sea

The enemy reads the wave under its own current position. The player reads the wave under the player position. Therefore, both ships use the same sea formula but usually have different heave, roll, and pitch.

Pressing `H` stops the wave rocking, but it does not stop the patrol. These are separate movements.

## Draw Budget

The current Phase 13 scene uses:

| State | Draw calls |
|---|---:|
| Idle | 18 |
| Cannonball in flight | 19 |
| Ball and short muzzle flash | 20 |

The hard limit is 20. The current scene stays inside it.

## What Was Checked

- The patrol reaches both ends of its path.
- The enemy stays at `z = -18`.
- The enemy follows the wave at its own moving position.
- The two ships do not rock in lockstep.
- The enemy scale is uniform, so it does not stretch the hierarchy.
- The reduced enemy uses exactly four draw calls.
- No new mesh generator or geometry type was added.

## Result

There are now two ships. They share the same code and meshes, but the enemy is smaller, darker, reduced, and moving.

