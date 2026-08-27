# Phase 11 Explanation

## Status

Phase 11 is complete and verified.

## What This Phase Does

This phase makes the cannon aim in two directions:

- azimuth turns the cannon left and right;
- elevation moves the barrel up and down.

There are two aiming modes. Press `TAB` to switch between them.

## Auto-Track Mode

Auto-track is the default mode. It aims at the moving enemy.

The player ship is always rocking. For this reason, the enemy position is first changed from world space into the player's local ship space. In simple words, the code asks: "Where is the enemy when viewed from this moving deck?"

The answer gives the cannon's target azimuth and elevation. No extra roll or pitch correction is needed because the hierarchy already carries the cannon with the ship.

## Smooth Turning

The cannon does not jump to the target angle. It has a maximum turning speed of 50 degrees per second. This makes the movement look mechanical and easy to see.

Angles wrap from `+180` degrees to `-180` degrees. The code uses `wrapAngle()` to choose the short way across this boundary. Without it, the cannon could turn almost a full circle in the wrong direction.

## Manual Mode

Press `TAB` to enter manual mode. Then use:

- left and right arrows for azimuth;
- up and down arrows for elevation.

Elevation is limited between `-5` and `+45` degrees. Azimuth can turn all the way around.

The current final code also adds a small calculated ballistic lift. That part was completed in Phase 12 so the cannonball can reach the enemy instead of passing below it.

## What Was Checked

- Auto-track uses the enemy position in ship-local space.
- The cannon stops on the correct local-space solution.
- Turning never moves faster than the set slew rate.
- The `+180`/`-180` boundary uses the shortest turn.
- All four arrow keys move the correct axis.
- The elevation limits work in both modes.
- A zero time step causes no aiming movement, so pause works correctly.

## Result

The cannon follows the enemy smoothly from a rocking deck. The user can also switch to manual control at any time.

