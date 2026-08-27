# Phase 13 Explanation

## Status

Phase 13 is complete and verified.

## What This Phase Does

This phase decides how a shot ends.

- A ball that reaches the enemy reports `HIT`.
- A ball that reaches the ocean reports `SPLASH`.
- A ball that flies too long reports `LOST` and is removed.

The last result is shown in the window title. The implementation guide accepts the window title as the simple on-screen status display.

## Checking the Enemy

The ball moves quickly. It may travel more than one unit between two frames. A test at only the new point could skip over the enemy.

The code therefore checks the whole line segment from the previous ball position to the new position. This is called a swept test. It gives the same reliable result at low and high frame rates.

## Checking the Ocean

The enemy test runs before the ocean test. This order is important because the enemy hull sits in the water. A shot near the waterline must count as a hit when it reaches the hull, not as a splash.

If the ball misses the enemy and falls below the current wave height, the result becomes `SPLASH`.

## Hit Feedback

After a hit, the enemy hull receives a bright emission colour. The glow fades over 0.4 seconds. Emission is used so the hit stays visible in every shading mode.

The result text also changes to `HIT`, `SPLASH`, or `LOST` in the window title.

Phase 14 now builds on these results. It adds muzzle smoke, orange hit sparks, blue splash spray, and an expanding water ring.

## What Was Checked

- A swept segment detects a target even when both endpoints are outside it.
- A zero-length segment is handled safely.
- A hull hit is checked before a water hit.
- Good auto-aim produces `HIT`.
- A deliberately depressed shot produces `SPLASH`.
- Twelve tracked shots were tested at 30, 60, and 144 Hz; all reported `HIT`.
- A live `SPACE` test reported `HIT` in the visible status readout.
- The hit glow timer lasts 0.4 seconds.
- The ball is removed after every final result.

## Result

Aiming well hits the enemy. A short shot splashes into the sea. The result is clear, deterministic, and visible to the user.
