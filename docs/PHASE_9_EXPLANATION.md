# Phase 9 Explanation

## Status

Phase 9 is complete and verified.

## What This Phase Does

This phase gives the sails and flag their own motion. The ship now looks alive even when the cannon is not firing.

The fore sail and main sail swing around their yards. The flag turns around the main mast. Each movement is calculated from the current time.

## How the Motion Works

The sails use this simple idea:

```text
angle = amount × sin(speed × time + phase)
```

- `amount` controls how far the part moves.
- `speed` controls how fast it moves.
- `phase` changes when the movement starts.

The two sails have different phase values. This stops them from moving together like one solid object. The flag also moves faster and through a wider angle.

## Why the Parent Matters

Each sail is a child of its yard. The flag is a child of the main rig. Because of this, they follow the ship when it rocks.

The `H` key turns wave rocking off at the ship root. The hull then stays level, but the sails and flag keep moving. This clearly shows that their local animation is separate from the ship's root movement.

## What Was Checked

- Both sails use the required local `R_z` rotation.
- The flag uses the required local `R_y` rotation.
- Each sail turns around its own yard.
- The sails do not move in lockstep.
- The same time value always gives the same pose.
- Turning hierarchy motion off does not stop the rigging animation.
- This phase adds no new mesh and no extra draw call.

## Result

The ship has continuous idle motion. The rigging follows the hierarchy correctly, while keeping its own animation.

