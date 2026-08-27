# Phase 14 Explanation

## Status

Phase 14 is complete and verified.

## What This Phase Does

This phase adds three visible effects:

- grey smoke comes from the muzzle after a shot;
- orange sparks appear when the enemy is hit;
- blue spray and an expanding ring appear when the ball hits the sea.

## Fixed Particle Pools

The project has two small arrays:

- four smoke slots;
- four impact slots.

There is also one reusable splash-ring slot.

These slots are created before the render loop. A new shot or impact fills the same slots again. The arrays never grow, and the program does not create particle objects while running.

## Particle Life

Each particle stores its start point, speed, age, lifetime, colour, starting size, and growth amount.

Every frame, only its age changes. The visible values are calculated from that age:

```text
position = origin + velocity × age
size     = start size + growth × progress
opacity  = 1 - progress
progress = age / lifetime
```

This makes every puff move, grow, and fade. When its age reaches its lifetime, the slot becomes inactive and can be reused.

Pressing `P` gives a time step of zero. Particle ages then stop with the ocean, ships, cannon, and ball.

## How Events Use the Pools

Pressing `SPACE` fills the smoke pool at the current world-space muzzle position. The smoke moves forward with the shot and slowly rises.

A `HIT` fills the impact pool with orange sparks at the enemy centre. The existing enemy-hull glow still works.

A `SPLASH` fills the same impact pool with blue spray. It also starts the ring at the current animated wave height, not below the water.

## Staying Inside 20 Draw Calls

The idle scene already uses 18 draw calls. Drawing eight puffs one by one would break the limit.

All active smoke and impact spheres therefore share one instanced draw call. The splash ring uses one more draw call.

| Scene state | Draw calls |
|---|---:|
| Idle | 18 |
| Ball, after-flash smoke | 20 |
| Hit smoke and sparks | 19 |
| Smoke, spray, and ring | 20 |
| Worst tested overlap | 20 |

Particles remain in their pools during the short muzzle flash, but are drawn after the flash ends. An old ring is hidden while a new ball is flying. These two simple rules prevent temporary overlaps from reaching 21 draws.

## Transparency

Blending is enabled only while particles are drawn. Particles still use depth testing, but they do not write to the depth buffer. After the effects are drawn, blending is disabled and depth writing is restored.

## What Was Checked

- All pool sizes are fixed.
- Smoke starts at the real muzzle position.
- Smoke moves forward and upward.
- Splash spray starts on the animated water surface.
- Hit sparks start at the enemy.
- Particles grow, fade, expire, and reuse their slots.
- Fifty direct smoke spawns caused zero C++ allocations and safely reused four slots.
- Fifty repeated fire, update, and render calls caused zero C++ allocations.
- Every tested render state stayed at or below 20 draws and 25,000 triangles.
- OpenGL blending and depth state were restored correctly.
- A pixel test proved that the blue splash is easy to see against the ocean.
- The live application showed smoke, hit sparks, and automatic cleanup.
- The focused Phase 14 check passed all 68 checks.
- All 292 earlier Phase 9–13 checks still pass.
- Debug, Release, and strict warning builds all pass.

## Result

Firing now produces visible smoke and impact effects. The implementation is reusable, deterministic, allocation-free during play, and inside the project budget.
