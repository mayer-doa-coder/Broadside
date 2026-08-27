# Phase 15 Explanation

## Status

Phase 15 is complete and verified.

This phase turns the project into a live graphics demonstration. You can now show the important lighting, shading, hierarchy, and animation ideas by pressing keys. You do not need to change the code during the demonstration.

## Demonstration Keys

| Key | What it does |
|---|---|
| `1` | Uses Flat shading. |
| `2` | Uses Gouraud shading. |
| `3` | Uses Phong shading. |
| `B` | Changes between Phong specular and Blinn-Phong specular. |
| `K` | Shows different lighting terms. |
| `L` | Shows the sun, muzzle light, or both light modes. |
| `+` / `-` | Changes mesh detail. |
| `H` | Turns the ship hierarchy motion on or off. |
| `TAB` | Changes between automatic and manual aiming. |
| Arrow keys | Aim the cannon in manual mode. |
| `SPACE` | Fires the cannon. |
| `P` | Pauses or resumes the scene. |
| Mouse / `W` / `S` | Orbits or zooms the camera. |

The window title shows the current setting after every change.

## Lighting Terms

The `K` key uses this cycle:

```text
All
  -> Specular only
  -> Ambient
  -> Ambient + Diffuse
  -> All
```

This lets you see what each part adds to the final colour.

## Light Modes

The `L` key uses this cycle:

```text
Sun + Muzzle
  -> Sun only
  -> Muzzle only
  -> Sun + Muzzle
```

The muzzle light is normally active for only 0.15 seconds after firing. In the `Muzzle only` teaching mode, it stays on at the real muzzle position. This gives you enough time to inspect its colour and distance falloff.

The project still has exactly two lights. The teaching mode does not add another light.

## Mesh Detail

Press `-` three times to reach the demonstration setting:

```text
Barrel segments: 6
Ocean resolution: 8
```

Press `+` to add detail again. Meshes are rebuilt only when one of these keys changes the setting. They are not rebuilt every frame.

## Money Shot A: Ocean Highlight

1. Press `-` three times.
2. Orbit the camera toward the bright path of the sun on the water.
3. Press `2` for Gouraud.
4. Press `3` for Phong.

With Gouraud, the narrow highlight breaks up or disappears because lighting is calculated only at the grid points. With Phong, lighting is calculated for every visible fragment, so the bright streak returns.

The automated visual check found:

- Gouraud bright highlight pixels: `0`
- Phong bright highlight pixels: `427`
- Pixels that changed: `36,144`

## Money Shot B: Low-Detail Barrel

1. Keep the barrel at six segments.
2. Look closely at the brass cannon barrel.
3. Press `1`, then `2`, then `3`.

Flat shading clearly shows the six faces and bright bands near their edges. Gouraud smooths colours between the corners. Phong gives the smoothest and tightest highlight.

All three framebuffer comparisons were clearly different.

## What Was Checked

- Every Phase 15 state cycles in the correct order.
- Every state reaches the shader.
- The real application receives every demonstration key.
- The window title reports every changed state.
- Tessellation stops at `6/8` and `64/128`.
- Repeated keys at a limit do not rebuild the meshes.
- Sun-only and muzzle-only views look different.
- Phong and Blinn-Phong look different.
- All four lighting-term views look different from the relevant comparison view.
- Both required money shots are visible.
- The full live sequence remains at or below 20 draw calls.
- The focused Phase 15 audit passed all 34 checks.
- Earlier project regression checks still pass.

## Result

Milestone 4 is complete. The full three-minute demonstration can be performed with keys and the camera. No code changes are needed during the demonstration.
