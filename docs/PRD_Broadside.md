# PRD — "Broadside": An Age-of-Sail Naval Gunnery Duel
### Computer Graphics Final Project — Product Requirements Document

**Version:** 1.0
**Platform:** OpenGL 3.3 Core Profile (GLFW + GLAD + GLM)
**Target:** Single student, limited timeframe
**Course decks covered:** `L8_Illumination.pptx`, `L9_shading.pptx`

---

## 1. One-Line Pitch

A single warship rides a live wave surface and duels a patrolling enemy vessel with a two-axis aimable cannon firing true parabolic projectiles — presented as a **rigid-body hierarchy and ballistics demonstration** that happens to be set in the age of sail.

---

## 2. Framing (Read This First — It Determines Your Grade)

Your brief says: *"The project should demonstrate concepts rather than just look visually impressive."*

A pirate ship battle reads, on its face, as entertainment-first. **You must actively reframe it.** Throughout your report, viva, and on-screen HUD, this project is presented as:

> A demonstration of hierarchical modelling transformations and the Phong illumination model, using a naval gunnery scenario as the carrier scene because it naturally requires: a deep parent-child transform chain (hull → deck → mount → barrel), a two-axis gimbal, dynamically computed projectile motion, and two extreme material types (polished brass vs. matte sailcloth) under a single light.

Every feature below exists to demonstrate a taught concept. If a feature does not map to a row in Section 4, it does not get built.

---

## 3. Goals and Non-Goals

### 3.1 Goals
- Demonstrate all five required model transformation categories with load-bearing (not decorative) usage.
- Implement the complete Phong illumination model from `L8` in GLSL, including radial attenuation and multiple summed light sources.
- Implement all three surface rendering methods from `L9` (Flat, Gouraud, Phong) as a **live, switchable comparison** on the same geometry.
- Compute 100% of motion inside the render loop from `glfwGetTime()`. No baked keyframes, no animation tables.
- Ship a demonstrable vertical slice early, then layer features.

### 3.2 Explicit Non-Goals (Scope Protection)

These are **rejected by design**. If you find yourself building one, stop.

| Rejected feature | Why |
|---|---|
| Multiple enemy ships / fleet combat | Duplicate objects, no new concept demonstrated |
| Simultaneous multi-cannon broadside | One cannon demonstrates the gimbal; N cannons demonstrate copy-paste |
| Ship damage states, health bars, sinking | Game logic, zero graphics concepts |
| Buoyancy physics / Gerstner or FFT ocean | Simulation research; a sine-sum surface teaches the same normals lesson |
| Enemy AI, pathfinding, evasive maneuvers | Game AI, zero graphics concepts |
| Boarding, crew, characters, animation rigs | Skeletal animation is a separate course |
| Textures, normal maps, skyboxes, HDR, bloom | Not required; hides the shading model you're being graded on |
| Shadows (shadow mapping) | Out of scope; `L8` slide 10-11 explicitly notes OpenGL is *local* illumination |
| Recreating specific AC:BF / PotC ships, characters, logos, UI | IP risk. Build a generic, original three-masted hull |

### 3.3 Scope Ceiling (Hard Limits)
- **1** player ship, **1** enemy ship, **1** cannon, **1** cannonball in flight at a time.
- **≤ 8** unique meshes total.
- **≤ 20** draw calls per frame.
- **2** light sources.

---

## 4. Requirement Traceability Matrix

Every teacher requirement mapped to a concrete, checkable feature.

| # | Requirement | Feature that satisfies it | Verify by |
|---|---|---|---|
| 1 | Simple 3D scene | 8 meshes, ~11 drawn objects, single environment | Object count in report |
| 2 | OpenGL only | GLFW/GLAD/GLM only; no engine, no physics lib | Dependency list |
| 3 | Runs continuously in render loop | All state advances from `glfwGetTime()` each frame | Leave it running 5 min |
| 4 | Visible motion/animation | Waves, ship rocking, sail flutter, cannon tracking, projectile flight, particles | Visual |
| 5 | Clear indications | On-screen HUD: aim angles, muzzle velocity, flight time, HIT/MISS, shading mode name | Screenshot |
| 6a | Translation | Cannonball flight, enemy patrol, ship heave | Section 8 |
| 6b | Rotation | Cannon azimuth + elevation, hull roll + pitch, sail flutter | Section 8 |
| 6c | Scaling | Smoke/splash growth, unit-mesh → object dimensions | Section 8 |
| 6d | Composite | `T·R_y·R_x·S` on cannon; `T·R_z·R_x` on hull | Section 8 |
| 6e | Hierarchical | 4-level chain hull→deck→mount→barrel; toggle key proves it | `H` key demo |
| 7 | Lighting maintained | 2 lights summed every frame, every object, every mode | Visual |
| 8 | Specular mandatory | Brass cannon (`n_s`=27.9) + ocean (`n_s`=160) highlights | Visual |
| 9 | Phong preferred | Per-fragment Phong is the default mode; Flat/Gouraud are comparison modes | `1`/`2`/`3` keys |
| 10 | Concepts over looks | Shading comparison mode, hierarchy-disable toggle, HUD physics readout | Demo script §16 |
| 11 | Optimized | Mesh reuse table, pooled particles, GPU-side waves, conditional draws | Section 14 |
| 12 | No pre-computed animation | Every formula in Section 9 is a closed form of `t` | Code review |

### 4.1 Lecture Content Traceability

| Deck topic | Where it appears |
|---|---|
| L8: Ambient / Diffuse / Specular terms | Fragment shader, three separate summed terms |
| L8: Point light + `1/(a₀+a₁d+a₂d²)` attenuation | Muzzle-flash light (Light 1) |
| L8: Directional light (`L = −direction`) | Sun (Light 0) |
| L8: Material coefficient table (slide 60) | Brass cannon, Polished Silver fittings, Black Plastic |
| L8: Specular exponent `n_s` range | Brass 27.9 vs sailcloth 4 vs ocean 160 in one frame |
| L8: Blinn-Phong half-vector `(N·H)^n` | `B` key toggles `(R·V)^n` ↔ `(N·H)^n` |
| L8: Emission | Muzzle flash sphere emissive term |
| L8: Multiple lights summed | Shader loops `for(i=0;i<2;i++)` |
| L8: Local vs Global illumination | Report: why no shadows/reflections |
| L9: Flat surface rendering | Mode 1, `flat` qualifier |
| L9: Gouraud (intensity interpolation) | Mode 2, lighting in vertex shader |
| L9: Phong (normal interpolation) | Mode 3, lighting in fragment shader |
| L9: Vertex normal averaging `N_v=ΣN_i/‖ΣN_i‖` | Mesh generator, documented |
| L9: Mach banding | Mode 1 on low-tessellation barrel |
| L9: "Gouraud misses highlights" | Mode 2 on ocean plane — highlight vanishes |
| L9: Polygon count vs highlight quality | `+`/`−` tessellation keys |

---

## 5. Core Concept and Scene Description

### 5.1 Setting
Open ocean, mid-afternoon, low sun angle. Two vessels: the player's ship (foreground, camera-side) and an enemy vessel patrolling laterally at range. No land, no sky geometry, no other objects.

### 5.2 The Story the Scene Tells
The player's ship rocks on live waves. Its cannon continuously computes a firing solution against the moving enemy. On fire, a cannonball leaves the muzzle along the barrel's true current direction and arcs under gravity. It either strikes the enemy hull or falls short into the sea. Both outcomes are computed, never scripted.

### 5.3 Why This Is Meaningful (Beyond Spectacle)
The scene visualises a real historical engineering problem: **naval gunnery is hard because the gun platform itself is moving.** The cannon must aim relative to a hull that is rolling and pitching — which is exactly why hierarchical transforms exist. The `H` key (disable hull rocking) makes this pedagogically explicit: with the hierarchy off, the cannon floats free of its ship and the absurdity is immediately visible.

---

## 6. Object Inventory

**Unique meshes: 8.** Everything else is a reuse under a different model matrix and material.

| # | Mesh | Primitive | Reused for | Instances |
|---|---|---|---|---|
| M1 | Unit cube | box | Hull (tapered), deck, cannon mount, enemy hull | 4 |
| M2 | Unit cylinder (parameterised segments) | cylinder | Cannon barrel, masts ×2, yards ×2 | 5 |
| M3 | Unit sphere (parameterised stacks/slices) | sphere | Cannonball, muzzle flash, smoke puffs, splash puffs | 1 + pools |
| M4 | Unit quad | quad | Sails ×2, flag | 3 |
| M5 | Ocean grid | subdivided quad | Ocean surface | 1 |
| M6 | Ring / annulus | flat ring | Splash ring | pool |
| M7 | Unit cone | cone | Bowsprit tip, optional | 1 |
| M8 | HUD quad | screen quad | Text/indicator background | 1 |

**Total drawn objects per frame:** ~11 static + up to 8 pooled particles = **≤ 20 draw calls.**

---

## 7. Scene Graph (Hierarchy Specification)

```
WORLD
├── Sun (directional light, no geometry)
├── Ocean surface                      [M5]  (vertex-shader displaced)
│
├── PLAYER SHIP  ── T(pos) · R_z(roll(t)) · R_x(pitch(t))
│   ├── Hull                           [M1, scaled/tapered]
│   ├── Deck                           [M1]
│   ├── Mast_fore ── Yard_fore ── Sail_fore   [M2, M2, M4]
│   ├── Mast_main ── Yard_main ── Sail_main   [M2, M2, M4]
│   │                          └── Flag       [M4]
│   └── CANNON MOUNT              [M1]
│       └── Cannon Yoke ── R_y(azimuth)
│           └── Cannon Barrel ── R_x(elevation)   [M2, BRASS]
│               └── Muzzle Point (empty transform — spawn origin)
│                   └── Muzzle Flash [M3, emissive, Light 1 position]
│
├── ENEMY SHIP   ── T(patrol(t)) · R_z(roll(t)) · R_x(pitch(t))
│   ├── Hull                           [M1]
│   ├── Mast ── Yard ── Sail           [M2, M2, M4]
│   └── (identical sub-structure, reduced)
│
└── PARTICLE POOLS (world-space, not parented)
    ├── Smoke pool     [M3 ×4]
    ├── Splash pool    [M3 ×4 + M6 ×1]
    └── Cannonball     [M3 ×1, drawn only when in flight]
```

**Critical hierarchy property:** the cannon barrel's world transform is
`M_hull · M_deck · M_mount · R_y(az) · R_x(el) · S(barrel)`
so the barrel inherits hull roll and pitch **for free**. The muzzle spawn point inherits all of it, which is why the projectile's launch direction is automatically correct without special-case code.

---

## 8. Model Transformation Specification

| Category | Object | Transform | Purpose |
|---|---|---|---|
| **Translation** | Cannonball | `T(p(t))` from ballistic equation | Projectile flight |
| | Enemy ship | `T(patrol(t))` | Lateral patrol |
| | Both ships | `T(0, waveHeight(x,z,t), 0)` | Heave with sea |
| | Particles | `T(p₀ + v·age)` | Smoke drift, splash rise |
| **Rotation** | Cannon yoke | `R_y(azimuth)` | Traverse (gimbal axis 1) |
| | Cannon barrel | `R_x(elevation)` | Elevate (gimbal axis 2) |
| | Ship hull | `R_z(roll(t))`, `R_x(pitch(t))` | Wave-driven rocking |
| | Sails | `R_z(A·sin(ωt + φ))` about yard | Wind flutter |
| | Flag | `R_y(A·sin(ωt))` | Wind indication |
| **Scaling** | Smoke/splash | `S(s₀ + k·age)` | Expansion over lifetime |
| | All objects | `S(dims)` from unit mesh | Mesh reuse mechanism |
| | Muzzle flash | `S(1 − age/life)` | Flash decay |
| **Composite** | Cannon barrel | `T(mount) · R_y(az) · R_x(el) · S(dims)` | Two-axis gimbal |
| | Ship | `T(pos) · R_z(roll) · R_x(pitch) · S(dims)` | Rocking platform |
| | Particles | `T(p) · R_y(spin) · S(size)` | Full TRS |
| **Hierarchical** | Barrel ← mount ← deck ← hull | 4-level chain | Cannon inherits ship motion |
| | Sail ← yard ← mast ← hull | 4-level chain | Rigging inherits ship motion |

**Demonstration feature:** pressing `H` sets the hull's roll/pitch to identity **for the parent node only**. The children retain their own local animation. The cannon and masts visibly detach from the rocking hull, proving the parent-child relationship is real and load-bearing. This is the single clearest "hierarchical transformation" evidence you can show a grader.

---

## 9. Motion Specification — What Moves, How, Why

Every formula is a closed-form function of time, evaluated fresh each frame. Nothing is stored except `age` counters and the current firing state.

### 9.1 Ocean Surface
**What:** ocean grid vertices.
**How:** sum of 2 sine waves, in the **vertex shader**:
```
y(x,z,t) = A₁·sin(k₁·x + ω₁·t) + A₂·sin(k₂·z + ω₂·t + φ)
```
**Normal (analytic, not averaged):**
```
∂y/∂x = A₁k₁·cos(k₁x + ω₁t)
∂y/∂z = A₂k₂·cos(k₂z + ω₂t + φ)
N = normalize(−∂y/∂x, 1, −∂y/∂z)
```
**Why:** wind-driven sea state. Analytic normals are exact, cost 2 cosines, and are the correct answer to "how do you light a deforming surface" — a direct application of the normal-vector discussion in `L9`.

### 9.2 Ship Rocking
**What:** hull roll and pitch of both ships.
**How:** evaluate the *same* wave function's slope at the hull's `(x,z)` on the CPU:
```
roll(t)  = atan(∂y/∂x at hull position) · dampening
pitch(t) = atan(∂y/∂z at hull position) · dampening
heave(t) = y(hull.x, hull.z, t)
```
**Why:** the ship tilts to match the water surface beneath it. This is *physical consistency* — the hull rocks **because of** the wave under it, not from an unrelated sine. State this in the report; it is a small detail graders notice.

### 9.3 Cannon Aiming
**What:** cannon yoke (azimuth) and barrel (elevation).
**How:** two modes, `TAB` switches.
- *Auto-track:* compute direction to enemy in the **ship's local space** (so the solution is relative to the rocking platform):
  ```
  d = normalize(inverse(M_ship) · enemy_world_pos − mount_local_pos)
  azimuth   = atan2(d.x, d.z)
  elevation = clamp(asin(d.y) + ballistic_lift, −5°, +45°)
  ```
  Then slew toward target at a max rate: `az += clamp(az_target − az, ±rate·dt)`.
- *Manual:* arrow keys directly adjust azimuth/elevation.
**Why:** a gun crew tracks the target. The slew rate limit makes the motion visibly mechanical and proves it is computed, not snapped.

### 9.4 Projectile Flight
**What:** the cannonball.
**How:** on `SPACE`, capture the muzzle's **current world position and forward direction** from the hierarchy, then:
```
p₀ = muzzle_world_position
v₀ = muzzle_world_forward · MUZZLE_SPEED
p(τ) = p₀ + v₀·τ + ½·g·τ²        where τ = now − fireTime, g = (0,−9.8,0)
```
**Why:** gravity acts on the ball. The trajectory automatically differs every shot because the muzzle transform includes the ship's current roll/pitch — firing at the top vs. bottom of a roll produces visibly different arcs. **This is the project's best single argument that nothing is pre-computed.**

### 9.5 Impact Resolution
**What:** hit/splash decision.
**How:** each frame while in flight:
```
if (p.y <= waveHeight(p.x, p.z, t))        → SPLASH   (spawn splash pool)
else if (distance(p, enemyCenter) < R_hit) → HIT      (spawn burst, flash enemy hull)
else if (τ > MAX_FLIGHT_TIME)              → DESPAWN
```
**Why:** deterministic, per-frame collision. No scripted outcome.

### 9.6 Particles (Smoke, Splash)
**What:** pooled spheres.
**How:** each slot holds `{origin, velocity, age, active}`. Per frame: `age += dt`; position `= origin + velocity·age`; scale `= s₀ + k·age`; alpha `= 1 − age/life`; when `age > life`, mark inactive and reuse.
**Why:** muzzle smoke and water splash. **Stateless recycling** — no allocation after startup.

### 9.7 Rigging
**What:** sails, flag.
**How:** `R_z(A·sin(ωt + φ))` with per-sail phase offset `φ`.
**Why:** wind. Also provides continuous ambient motion so the scene never looks frozen between shots.

---

## 10. Interaction and On-Screen Indications

Requirement 5 demands "clear indications." The HUD is not decoration — it is graded evidence.

| Key | Action | Why it earns marks |
|---|---|---|
| `1` / `2` / `3` | Flat / Gouraud / Phong shading | **The L9 demonstration** |
| `+` / `−` | Increase/decrease mesh tessellation | Shows highlight quality vs polygon count |
| `B` | Toggle Phong `(R·V)^n` ↔ Blinn-Phong `(N·H)^n` | L8 slide 49–50 |
| `H` | Disable/enable hull rocking in hierarchy | **Proves hierarchical transforms** |
| `L` | Cycle: both lights / sun only / muzzle light only | Isolates each light's contribution |
| `K` | Cycle: ambient only / +diffuse / +specular / full | **Recreates L8 slide 54 live** |
| `TAB` | Auto-track ↔ manual aim | Interactivity |
| `←→↑↓` | Manual azimuth / elevation | Interactivity |
| `SPACE` | Fire | Triggers the ballistic demo |
| `W`/`S`/mouse | Camera orbit / zoom | Lets grader inspect highlights from any angle |
| `P` | Pause time (freeze `t`) | Lets grader study a single frame |

### HUD Text (top-left, always visible)
```
SHADING: Phong (per-fragment)     [1/2/3]
SPECULAR: (R·V)^n                 [B]
LIGHTS:  Sun + Muzzle             [L]
TERMS:   Ambient+Diffuse+Specular [K]
BARREL SEGMENTS: 32               [+/-]
HIERARCHY: ON                     [H]
---
AZIMUTH:   -12.4°
ELEVATION:  18.7°
MUZZLE VEL: 42.0 m/s
FLIGHT TIME: 1.83 s
LAST SHOT:  HIT
```

**Why the HUD matters:** it converts invisible internal state into visible, checkable evidence. A grader can read "ELEVATION 18.7°" and watch the barrel match it.

---

## 11. Lighting Design (per `L8_Illumination.pptx`)

### 11.1 Light Sources — 2 total, summed

**Light 0 — The Sun (Directional)**
- Type: directional. `L = −normalize(direction)`. No attenuation (L8 slide 19: distant source, parallel rays).
- Direction: `(−0.4, −0.35, −0.5)` — deliberately low grazing angle to maximise the ocean's specular streak.
- `I_diffuse = (1.0, 0.96, 0.86)` warm afternoon
- `I_specular = (1.0, 1.0, 0.98)`

**Light 1 — Muzzle Flash (Point, transient)**
- Type: positional, at the muzzle world position (inherited from the hierarchy).
- Active only for ~0.15 s after firing; intensity decays with `age`.
- **Radial attenuation (L8 slide 21):** `f(d) = 1/(a₀ + a₁·d + a₂·d²)` with `a₀=1.0, a₁=0.09, a₂=0.032`.
- `I_diffuse = (1.0, 0.75, 0.35)` orange muzzle flare
- **Why this light exists:** it is the pedagogical excuse to implement attenuation, a second light, and emission — three L8 concepts — in a way that is visually dramatic and narratively justified.

**Global ambient (L8 slide 57):** `I_a_global = (0.15, 0.15, 0.18)` — cool sky-scattered fill.

### 11.2 Material Properties

Values taken **verbatim from L8 slide 60** where available. Cite the table in your report.

| Object | `k_a` | `k_d` | `k_s` | `n_s` | Source |
|---|---|---|---|---|---|
| **Cannon barrel** | (0.329, 0.224, 0.027) | (0.780, 0.569, 0.114) | (0.992, 0.941, 0.808) | **27.9** | L8 slide 60 "Brass" |
| **Cannon fittings** | (0.231, 0.231, 0.231) | (0.278, 0.278, 0.278) | (0.774, 0.774, 0.774) | **89.6** | L8 slide 60 "Polished Silver" |
| **Cannonball** | (0.0, 0.0, 0.0) | (0.01, 0.01, 0.01) | (0.50, 0.50, 0.50) | **32** | L8 slide 60 "Black Plastic" |
| **Ocean** | (0.02, 0.05, 0.08) | (0.06, 0.14, 0.20) | (0.90, 0.94, 0.98) | **160** | Tuned (high `n_s` = mirror-like) |
| **Hull (wood)** | (0.12, 0.08, 0.05) | (0.38, 0.25, 0.15) | (0.15, 0.12, 0.10) | **8** | Tuned (rough → low `n_s`) |
| **Sailcloth** | (0.20, 0.19, 0.17) | (0.75, 0.73, 0.68) | (0.04, 0.04, 0.04) | **4** | Tuned (near-Lambertian) |
| **Muzzle flash** | — | — | — | — | **Emissive only**, `I_e = (1.0, 0.7, 0.3)` |

**Design intent:** the frame deliberately contains `n_s` values spanning **4 → 160** (a 40× range). L8 slide 46 shows exactly this comparison; your scene reproduces it with real objects instead of test spheres.

### 11.3 The Illumination Equation (implemented in GLSL)

```
I = I_e                                             // emission (L8 s55)
  + k_a · I_a_global                                // ambient  (L8 s26)
  + Σ_lights  f_att(d) · [ k_d · I_d · max(0, N·L)  // diffuse  (L8 s32)
                         + k_s · I_s · max(0, R·V)^n_s ]  // specular (L8 s44)

where R = reflect(−L, N) = 2(N·L)N − L              (L8 s47-48)
   or, in Blinn-Phong mode:  (N·H)^n_s, H = normalize(L+V)   (L8 s49)
   and f_att = 1/(a₀ + a₁d + a₂d²) for point lights, 1.0 for directional
```

---

## 12. Shading Model Design (per `L9_shading.pptx`)

### 12.1 The Three Modes — One Shader Program

A single shader program with `uniform int uShadingMode`. **Do not write three programs** — one program with a branch is cheaper, easier to maintain, and easier to defend as "optimized."

| Mode | Key | Implementation | L9 reference |
|---|---|---|---|
| **0 — Flat** | `1` | Fragment shader derives the face normal via `normalize(cross(dFdx(FragPos), dFdy(FragPos)))`, then evaluates lighting once per face. Requires no extra buffers. | Slides 9–16 |
| **1 — Gouraud** | `2` | Vertex shader evaluates the **full illumination equation** and outputs an RGB colour. Rasteriser linearly interpolates it. Fragment shader just passes it through. | Slides 17–29 |
| **2 — Phong** | `3` | Vertex shader outputs the **interpolated normal**; fragment shader normalizes it and evaluates the illumination equation per fragment. | Slides 30–37 |

### 12.2 Vertex Normal Averaging

All curved meshes (sphere, cylinder) compute smooth vertex normals at generation time using the L9 formula:
```
N_v = (Σ N_i) / ‖Σ N_i‖        (L9 slide 20)
```
For the analytic primitives (sphere, cylinder), the exact normal is known in closed form and is used directly; document in your report that this is the analytic equivalent of the averaged normal, and that averaging is used for the ocean grid's non-analytic case if needed.

### 12.3 The Money Shot — Two Guaranteed Demonstrations

**Demo A — "Gouraud misses the highlight" (L9 slide 28).**
The ocean is a large surface with a very high `n_s` (160). Set tessellation low with `−`. In **Gouraud** mode the specular streak on the water **breaks into diamond patches or disappears entirely**, because the highlight falls between vertices. Switch to **Phong** — the streak reappears, coherent and bright, on *identical geometry*. This is the single most convincing demonstration in the project.

**Demo B — "Highlights distort on coarse polygons" (L9 slide 27).**
The brass cannon barrel is a cylinder with adjustable segments. At 8 segments in **Flat** mode: textbook faceting and Mach bands. In **Gouraud**: the highlight smears into a polygon-shaped blob ("the funny shape"). In **Phong**: it stays a clean, tight, round highlight that glides smoothly as the barrel traverses.

### 12.4 Mach Banding
Set the barrel or ocean to low tessellation in **Flat** mode and point out the perceived bright bands at facet edges — the human-visual-system effect described on L9 slides 13–15. Include a screenshot in your report.

---

## 13. Specular Reflection Design (Requirement 8 — Mandatory)

Specular is not an afterthought here; it is the visual centre of the scene.

| Surface | Effect | Why it's noticeable |
|---|---|---|
| **Ocean** | Glittering sun-path streak toward the camera | Low sun angle + `n_s`=160 + moving waves = constantly shifting bright streak. Impossible to miss. |
| **Cannon barrel** | Tight brass glint sweeping along the barrel | The barrel *rotates* to track the target, so the highlight sweeps continuously — the reflection vector changes every frame. |
| **Cannon fittings** | Very tight silver pinpoints (`n_s`=89.6) | Adjacent to the brass (`n_s`=27.9) — a side-by-side `n_s` comparison in one glance. |
| **Cannonball** | Small hard highlight tracking its arc | Moving object, fixed light: proves specular is view/position dependent. |
| **Hull** | Broad, weak sheen | Contrast case: shows what *low* `k_s` and *low* `n_s` look like. |
| **Sailcloth** | Essentially none | Contrast case: near-Lambertian, proves diffuse-only appearance. |

**Guaranteed visibility check:** at any camera angle, at least two surfaces with visible specular highlights must be on screen. The ocean guarantees one; the cannon guarantees the second.

---

## 14. Optimization Strategy (Requirement 11)

| Technique | Implementation | Saving |
|---|---|---|
| **Mesh reuse** | 8 unique meshes → ~11 objects + particles. One cylinder VAO serves barrel, 2 masts, 2 yards. | ~60% fewer VBOs |
| **Single shader program** | One program, `uShadingMode` branch, instead of 3 programs | No state-change cost on mode switch |
| **GPU-side wave displacement** | Ocean `y` and normals computed in the vertex shader | **Zero** per-frame CPU cost, zero buffer uploads |
| **Stateless particle pools** | Fixed arrays; position/scale/alpha are pure functions of `age` | **Zero** allocation after startup |
| **Conditional draws** | Cannonball drawn only while in flight; muzzle flash only for 0.15 s; particles only when `active` | Skips draw calls most frames |
| **Back-face culling** | `glEnable(GL_CULL_FACE)` | ~50% fragment work on closed meshes |
| **Depth testing** | `glEnable(GL_DEPTH_TEST)` | Early-Z rejection |
| **Uniform hoisting** | View/projection matrices, light data set **once per frame**, not per object | Fewer `glUniform` calls |
| **Tessellation regeneration on demand** | Barrel/sphere rebuilt only on `+`/`−` keypress, never per frame | No per-frame geometry work |
| **Normal matrix on CPU** | `(M⁻¹)ᵀ` computed once per object per frame, not per vertex | Avoids per-vertex inverse |
| **No textures / no post-processing** | Deliberate omission | Lower bandwidth, keeps focus on shading model |

**Performance budget:** ≤ 20 draw calls, ≤ 25k triangles at default tessellation, 60 FPS at 1280×720 on integrated graphics.

**Report note:** explicitly state that this is a **local illumination** model (L8 slides 10–11) — one bounce, no inter-reflection, no shadows — and that this is the correct, intentional choice for a raster OpenGL pipeline, not a limitation you failed to overcome. This pre-empts the most likely viva question.

---

## 15. Risks and Mitigations

| Risk | Severity | Mitigation |
|---|---|---|
| Scope creep toward a real game | **High** | Section 3.2 is a contract. Re-read it weekly. |
| Muzzle world-transform extraction is wrong (ball fires sideways) | **High** | Build and test this in isolation (Phase 12) with the ship *stationary* first. Draw a debug line along the barrel's forward axis. |
| Combining moving target + projectile at once → unclear which is broken | **High** | Get projectile working against a **fixed** target before adding patrol motion. |
| Ocean normals wrong → lighting looks flat/wrong | Medium | Derive analytically, verify by rendering `N` as RGB colour first. |
| Gouraud mode looks identical to Phong (demo fails) | Medium | Requires **low tessellation + high `n_s`**. Test the ocean at 8×8 with `n_s`=160 early. |
| Hierarchy matrix order wrong (children detach) | Medium | Build the static hierarchy (Phase 6) before any animation. Verify by manually rotating the hull. |
| Time overrun | Medium | Phase order in the implementation guide is strictly demo-first. MVP is complete at Phase 12. |

---

## 16. Acceptance Criteria & Demo Script

### 16.1 Minimum Viable Product (must exist to submit)
- Ocean with animated waves and correct lighting
- Player ship with 4-level hierarchy, rocking with the sea
- Cannon with two-axis aiming
- Enemy ship patrolling
- Projectile with true ballistic motion and hit/splash resolution
- Full Phong illumination: 2 lights, ambient/diffuse/specular, attenuation, ≥ 4 distinct materials
- Three shading modes switchable live
- HUD showing shading mode and aim angles

### 16.2 The 3-Minute Demo Script (rehearse this)
1. **(0:00)** Scene running. "Everything you see is computed in the render loop from elapsed time. Nothing is pre-recorded."
2. **(0:20)** Press `K` four times: ambient → +diffuse → +specular → full. "This is L8 slide 54, live."
3. **(0:45)** Press `L`: sun only → muzzle only → both. "Two light sources, summed per fragment."
4. **(1:00)** Press `−` a few times, then `1`/`2`/`3`. "Flat: Mach banding. Gouraud: the highlight breaks up. Phong: it's correct. This is the entire L9 lecture in one keypress."
5. **(1:30)** Orbit camera to the sun-path on the water in Gouraud, then Phong. "Gouraud misses the specular highlight completely — slide 28."
6. **(2:00)** Press `H`. "The cannon just detached from the ship. That's the hierarchy — the barrel's transform is a child of the hull's."
7. **(2:20)** `TAB` to manual, aim, `SPACE`. "The ball launches along the barrel's actual world direction, including the ship's current roll. Trajectory is `p₀ + v₀τ + ½gτ²`. Watch the flight time in the HUD."
8. **(2:45)** Fire at the top vs. bottom of a roll. "Different arcs — because the muzzle transform genuinely inherits the ship's motion."

### 16.3 Optional (only if time remains)
- Blinn-Phong toggle (`B`)
- Splash ring effect
- Second enemy ship (reuses everything — genuinely free)
- Spotlight (a ship's lantern) to demonstrate L8's angular attenuation

---

## 17. Deliverables

1. Source code (`/src`, `/shaders`, `CMakeLists.txt`)
2. Compiled executable
3. Report containing: the L8/L9 traceability matrix (Section 4.1), the illumination equation as implemented, the material table with cited slide-60 values, screenshots of Flat/Gouraud/Phong comparison, a Mach-banding screenshot, and the optimization table (Section 14)
4. 3-minute demo (Section 16.2)
