# Phase 12 Explanation

## Status

Phase 12 is complete and verified. Milestone 3 is complete.

## What This Phase Does

Press `SPACE` to fire one cannonball. The ball starts at the real muzzle position and travels in the real barrel direction.

The shot is not a saved animation. Its position is calculated again from time on every frame.

## Reading the Muzzle Transform

The barrel matrix already contains the ship's position, wave rocking, cannon turn, and barrel elevation. The code reads two things from this matrix:

| Value | `w` | Meaning |
|---|---:|---|
| Muzzle position | 1 | Includes the barrel's world translation |
| Forward direction | 0 | Uses rotation, but ignores translation |

This is why the ball leaves the end of the barrel and travels where the barrel points.

## The Flight Formula

The ball uses the required formula:

```text
position = start + velocity × flightTime + 0.5 × gravity × flightTime²
```

Gravity is `(0, -9.8, 0)`. It pulls the ball down and creates the visible arc.

The code stores only the launch position, launch velocity, and fire time. It does not add a small movement every frame. This keeps the result stable at different frame rates.

## Ballistic Lift

The enemy is far away, so pointing directly at it would make gravity pull the ball too low. The code calculates a small low-arc lift from the current range.

At a range of 20 units, the lift is about 3.2 degrees. It changes as the enemy patrols.

## Muzzle Flash and Budget

The muzzle light and flash sphere are active for 0.15 seconds after firing. They are not drawn while idle.

The scene uses 18 draws while idle, 19 with the ball, and 20 during the short time when the ball and flash are both visible. It never passes the hard limit of 20.

## What Was Checked

- The launch point exactly matches the hierarchy's muzzle point.
- Launch speed is 42 units per second.
- Launch direction matches the barrel's world forward direction.
- The flight position matches the closed formula.
- Asking for the same flight time twice gives the same position.
- Different ship roll poses create different launch points and directions.
- The flash lasts 0.15 seconds.
- The ball and flash are drawn only when active.

## Result

`SPACE` launches a real ballistic shot from the moving cannon. The ball visibly arcs under gravity and stays inside the project budget.

