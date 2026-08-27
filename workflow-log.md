# Workflow Audit Log — Broadside Phases 0 → 8

Sequential verification of every completed phase against its **✅ DONE WHEN**
checkpoint in [docs/IMPLEMENTATION_GUIDE_Broadside.md](docs/IMPLEMENTATION_GUIDE_Broadside.md).

**Rules in force**

1. One phase at a time, 0 → 8.
2. No phase begins until the previous phase's assertions are verified.
3. Every command, check and decision is recorded below.
4. Any failed verification halts the run and is flagged for intervention.

**Environment**

| Item | Value |
|---|---|
| Repo | `d:\Broadside` |
| Branch | `main` |
| Head | `ca23fdd` (working tree contains uncommitted Phases 6–8) |
| Platform | Windows 11, MSVC Build Tools 2022 (VC 14.44, x64) |
| GPU | NVIDIA GeForce RTX 5050 Laptop, OpenGL 3.3.0 / GLSL 3.30 |
| Harness location | scratchpad (never committed to the repo) |

**Status board**

| Phase | Subject | Result |
|---|---|---|
| 0 | Toolchain / build / window | **PASS** 8/8 |
| 1 | Render loop + clock | **PASS** 8/8 |
| 2 | Shader loading + error reporting | **PASS** 15/15 |
| 3 | Camera + MVP | **PASS** 29/29 |
| 4 | Mesh library + normals | **PASS** 37/37 |
| 5 | Phong shader, three modes (M1) | **PASS** 22/22 |
| 6 | Materials + second light | **PASS** 28/28 |
| 7 | Ship hierarchy | **PASS** 38/38 |
| 8 | Ocean waves + rocking (M2) | **PASS** 24/24 |
| 9 | Idle rigging animation | **PASS** 21/21 |
| 10 | Enemy ship on patrol | **PASS** 25/25 |
| 11 | Two-axis cannon aiming | **PASS** 28/28 |
| **12 🎯 M3** | Ballistics — the core | **PASS** 38/38 |
| 13 | Impact resolution | **PASS** 32/32 |
| | **RUN COMPLETE** | **14/14 PASS, 353 assertions** |

---
## Phase 0 — Toolchain, build, window

**Exit criterion (guide):** a window opens with a solid coloured background and
closes cleanly on ESC. Nothing else.

### Commands

| # | Command | Result |
|---|---|---|
| 0.1 | `cmake -S . -B /tmp/p0` (clean tree, no cache) | configure + generate OK |
| 0.2 | `cmake --build /tmp/p0 --config Release` | linked `broadside.exe`, 0 errors, 0 warnings |
| 0.3 | `cmake --build /tmp/p0 --config Debug` | linked `broadside.exe`, 0 errors, 0 warnings |
| 0.4 | `ls /tmp/p0/{Release,Debug}/shaders` | `phong.vert` + `phong.frag` present in BOTH |
| 0.5 | `Start-Process` then `CloseMainWindow()` | exit code 0, stderr empty |

### Assertions

| Assertion | Expected | Observed | Verdict |
|---|---|---|---|
| Configures from scratch with no cache | success | success | PASS |
| Release builds warning-free | 0 warnings | 0 | PASS |
| Debug builds warning-free | 0 warnings | 0 | PASS |
| POST_BUILD copies shaders beside the exe | both configs | both configs | PASS |
| GL context is 3.3 Core | 3.3 | `3.3.0 NVIDIA 596.36` | PASS |
| GLSL version | 3.30 | `3.30 NVIDIA via Cg compiler` | PASS |
| Window closes cleanly | exit 0 | exit 0 | PASS |
| No runtime error output | empty stderr | 0 bytes | PASS |

### Notes

- Built into a throwaway directory rather than `build/`, so the result proves a
  fresh clone configures, not that a warm cache still works.
- Close was driven by `CloseMainWindow()` (WM_CLOSE). GLFW converts that into
  `glfwWindowShouldClose`, which is the identical exit path ESC takes in
  `processInput`, so the clean-shutdown claim is exercised end to end.
- Shader copy verified in both configurations because the `cwd` trap in guide
  0.3.1 only bites in one of them if the POST_BUILD rule is misconfigured.

**PHASE 0 VERDICT: PASS — 8/8 assertions. Proceeding to Phase 1.**

---
## Phase 1 — Render loop skeleton

**Exit criterion (guide):** the loop runs at a steady framerate and `dt` prints
sensible values. Plus Requirement 12: every animated quantity derives from `now`.

### Commands

| # | Command | Result |
|---|---|---|
| 1.1 | 20-second soak run, stdout captured | exit 0, stderr 0 bytes |
| 1.2 | `awk` over the 19 `[frame]` telemetry lines | see assertions |
| 1.3 | `grep` for frame-indexed arrays / keyframe tables | none found |
| 1.4 | `grep` for every mutable file-scope static | 12 found, all classified |
| 1.5 | read `updateScene` body | pose derived from `now` only |

### Assertions

| Assertion | Expected | Observed | Verdict |
|---|---|---|---|
| Simulation clock is monotonic | 0 violations | 0 over 19 samples | PASS |
| dt is steady | one refresh interval | 0.0069 to 0.0070 s, spread 0.0001 | PASS |
| dt never exceeds MAX_DELTA | < 0.10 s | max 0.0070 s | PASS |
| Frame rate is stable | v-synced | 143.6 to 144.1 FPS | PASS |
| Runs continuously without degrading | 20 s clean | t reached 19.44 s | PASS |
| No error text on stdout | 0 matches | 0 | PASS |
| No frame-indexed animation state | none | none | PASS |
| Scene pose is a closed form of `now` | yes | `buildShipFrames(shipMatrix(SHIP_POS, now, ...))` | PASS |

### Notes

- dt reads 0.0069 s rather than the guide-quoted 0.016 s because this display is
  144 Hz, not 60. One refresh interval is the correct expectation; the guide
  quotes 60 Hz hardware. Not a deviation.
- All 12 mutable file-scope statics were classified by hand. Every one is a user
  toggle (`g_wireframe`, `g_shadingMode`, `g_useBlinn`, `g_termMask`,
  `g_hierarchy`, `g_barrelSegments`, `g_oceanRes`), an input latch
  (`g_dragging`, `g_lastCursorX/Y`), an aim constant (`g_azimuth`,
  `g_elevation`), or a counter zeroed every frame (`g_drawCalls`,
  `g_triangles`). None accumulates motion, so none can carry a baked animation.
- Requirement 12 is additionally proven dynamically in Phase 8 by re-rendering an
  earlier timestamp and getting a pixel-identical frame back.

**PHASE 1 VERDICT: PASS — 8/8 assertions. Proceeding to Phase 2.**

---
## Phase 2 — Shader loading + error reporting

**Exit criterion (guide):** a coloured triangle appears, and deliberately
introducing a typo into the shader prints a readable compile error.

### Commands

| # | Command | Result |
|---|---|---|
| 2.1 | build + run scratch harness `p2.exe` | 17 checks, 0 failures |

The harness corrupts COPIES in `%TEMP%/p2shaders/`. The repo shader files are
opened read-only and re-hashed afterwards to prove it.

### Assertions

| # | Assertion | Observed | Verdict |
|---|---|---|---|
| 1 | Shipping shader pair links | program id 3 | PASS |
| 2 | Vertex typo -> `false`, with a log | `0(67) : error C0000: syntax error...` | PASS |
| 3 | Vertex typo leaves no program behind | id = 0 | PASS |
| 4 | Fragment typo -> `false`, with a log | `0(51) : error C0000: syntax error...` | PASS |
| 5 | Fragment typo leaves no program behind | id = 0 | PASS |
| 6 | Stage-interface mismatch is caught | `0(48) : error C1102: incompatible type` | PASS |
| 7 | Link failure leaves no program behind | id = 0 | PASS |
| 8 | Missing file is reported | named path + cwd hint | PASS |
| 9 | Missing file leaves no program behind | id = 0 | PASS |
| 10 | `phong.vert` untouched by the audit | byte-identical | PASS |
| 11 | `phong.frag` untouched by the audit | byte-identical | PASS |
| 12 | `setInt` reaches the driver | read back 1 | PASS |
| 13 | `setFloat` reaches the driver | read back 27.8974 | PASS |
| 14 | Absent uniform is a silent no-op | location -1, no GL error | PASS |
| 15 | No OpenGL error across the run | GL_NO_ERROR | PASS |

### Notes

- Every diagnostic carries the offending FILE PATH and a GLSL line number, which
  is what makes it readable rather than merely present. This is the failure mode
  the guide calls out as costing students hours.
- The missing-file message additionally prints the working-directory hint from
  guide 0.3.1 - the single most common runtime failure in this project.
- Four distinct failure paths were exercised, not just the one the guide asks
  for: vertex compile, fragment compile, stage-interface link, and missing file.
  All four fail closed, returning `false` and leaving `id == 0`, so a caller can
  never accidentally bind a half-built program.
- Compile logs appear ahead of the PASS lines in the transcript because they go
  to stderr while the checks go to stdout. Cosmetic ordering only.

**PHASE 2 VERDICT: PASS — 17/17 assertions. Proceeding to Phase 3.**

---
## Phase 3 — Camera + MVP matrices

**Exit criterion (guide):** you can orbit around a cube in 3D and it looks
correct (no stretching, near/far clipping sane).

### Commands

| # | Command | Result |
|---|---|---|
| 3.1 | build + run scratch harness `p3.exe` | 26/27, one FAILED |
| 3.2 | analytic check of the perspective depth mapping | assertion was wrong, not the camera |
| 3.3 | corrected the assertion, re-ran `p3.exe` | 29 checks, 0 failures |

### Assertions

| Group | Assertion | Observed | Verdict |
|---|---|---|---|
| 3.1 | yaw=0 pitch=0 sits on target +Z | exact | PASS |
| 3.1 | yaw=90 swings to +X | exact | PASS |
| 3.1 | distance to target equals radius | 7.0000 | PASS |
| 3.1 | radius invariant over 57x16 angles | drift 9.5e-07 | PASS |
| 3.2 | pitch clamps below the lookAt singularity | 89.00 deg | PASS |
| 3.2 | negative pitch clamps symmetrically | -1.5533 rad | PASS |
| 3.2 | view matrix finite at the limit | no NaN | PASS |
| 3.2 | radius clamps low / high | 1.20 / 120.00 | PASS |
| 3.2 | zoom is multiplicative | 0.6065 near and far | PASS |
| 3.2 | yaw wraps into [0, 2pi) | 6.0178 after 1000 drags | PASS |
| 3.3 | target lands on the view axis | (0.00000, 0.00000) | PASS |
| 3.3 | target is `radius` in front | view z = -9.0000 | PASS |
| 3.4 | 4:3 - X and Y pixel scale equal | 120.71 / 120.71 | PASS |
| 3.4 | 16:9 wide - equal | 144.85 / 144.85 | PASS |
| 3.4 | 2:3 portrait - equal | 181.07 / 181.07 | PASS |
| 3.5 | nearZ positive and tight | 0.10 | PASS |
| 3.5 | farZ covers the scene | 300 | PASS |
| 3.5 | point ON the near plane -> NDC -1 | -1.000005 | PASS |
| 3.5 | point ON the far plane -> NDC +1 | 1.000000 | PASS |
| 3.5 | point inside near is clipped | -3.0007 | PASS |
| 3.5 | point beyond far is clipped | > +1 | PASS |
| 3.5 | target inside the frustum | NDC z 0.9673 | PASS |
| 3.5 | point behind the camera is clipped | outside | PASS |
| 3.6 | cube renders solid | 55,081 px | PASS |
| 3.6 | still solid after a 50 deg orbit | 47,794 px | PASS |
| 3.6 | silhouette area stable across orbit | 13.2% change | PASS |
| 3.6 | zooming enlarges it | 55,081 -> 186,604 px | PASS |
| 3.6 | no OpenGL error | GL_NO_ERROR | PASS |

### Instrument defect found and corrected

The first run FAILED on `a point at the near plane maps to NDC -1`. Investigated
before touching anything: the harness sampled 1 mm BEYOND the near plane and
expected NDC < -0.999. Solving the projection by hand gives

| eye z | NDC z |
|---|---|
| -0.050 (inside near) | -3.000667 |
| -0.100 (ON near) | -1.000000 |
| -0.101 (1 mm beyond) | -0.980191 |
| -300.000 (ON far) | +1.000000 |

so 1 mm past a 0.1 near plane is already at -0.98. The camera was correct; the
assertion was measuring the nonlinearity of perspective depth, not the position
of the plane. Rewritten to sample exactly ON each plane and to check clipping
just outside both. No project code was changed.

### Notes

- The stretch test is the load-bearing one for the guide criterion. It projects a
  point 1 unit off-axis in X and one 1 unit off-axis in Y and requires the same
  pixel distance, across three very different aspect ratios including a portrait
  framebuffer. Equal to 0.01 px in all three.
- The 13.2% silhouette change across a 50 degree orbit is a cube presenting a
  different number of faces to the camera, not a projection error.
- Note for later phases: the target at NDC z 0.9673 shows how much of the depth
  range a 0.1 near plane consumes. Fine here; worth remembering if depth fighting
  ever appears.

**PHASE 3 VERDICT: PASS — 29/29 assertions after instrument correction. Proceeding to Phase 4.**

---
## Phase 4 — Mesh library

**Exit criterion (guide):** a sphere, cylinder and cube all render as solid
silhouettes, and you can pass different segment counts and see the polygon count
change.

### Commands

| # | Command | Result |
|---|---|---|
| 4.1 | build + run scratch harness `p4.exe` | 38 checks, 0 failures |

### Assertions

| Group | Assertion | Observed | Verdict |
|---|---|---|---|
| 4.1 | seven generators upload valid meshes | cube 12, cyl 128, sphere 960, quad 2, grid 8192, ring 64, cone 64 tri | PASS x8 |
| 4.3 | cylinder tri count rises with segments | 24, 32, 64, 128, 256 | PASS |
| 4.3 | sphere tri count rises with segments | 24, 48, 224, 960, 3968 | PASS |
| 4.3 | grid is exactly 2*N*N | at N = 8, 16, 32, 64, 128 | PASS |
| 4.3 | the minus-to-plus span is real | 24 tri at 6 segments vs 256 at 64 | PASS |
| 4.4 | `makeCylinder(0)` clamps | 12 tri, valid | PASS |
| 4.4 | `makeCylinder(-5)` clamps | 12 tri, valid | PASS |
| 4.4 | `makeSphere(0,0)` clamps | 6 tri, valid | PASS |
| 4.4 | `makeGrid(0)` clamps | 2 tri, valid | PASS |
| 4.5 | cube / cylinder / sphere / cone render solid | 131k / 98k / 64k / 52k px | PASS x4 |
| 4.5 | screen extent matches a UNIT mesh | 409, 302, 285, 269 px wide | PASS x4 |
| 4.5 | silhouette identical culled vs unculled | exact match on all four | PASS x4 |
| 4.5 | shading identical culled vs unculled | front faces wind CCW | PASS x4 |
| 4.6 | `computeSmoothNormals` on an octahedron | equals normalize(position), error 0.00e+00 | PASS |
| 4.6 | `computeSmoothNormals` on a flat grid | all exactly +Y, error 0.00e+00 | PASS |
| 4.7 | 200 build/destroy cycles | no GL error, state unchanged | PASS x2 |
| — | no OpenGL error across the run | GL_NO_ERROR | PASS |

### Honest accounting

Check 4.2 (`unit-size claim deferred to the rendered-extent test in 4.5`) is a
PLACEHOLDER that passes unconditionally. `Mesh.h` exposes only uploaded GPU
meshes, not the CPU vertex arrays, so object-space bounds cannot be read back
directly without duplicating the generators. The claim is genuinely tested in
4.5 instead, by measuring rendered screen extent against the analytic prediction
(700 px / (2 * 3 * tan 22.5 deg) = 282 px per world unit, so a unit-diameter
solid must land near 280 px, and a radius-1 mesh would land near 560).

Substantive assertions: **37**. Placeholders: **1**, disclosed above.

### Notes

- The winding test is the one worth keeping. It renders each closed solid twice,
  once with `GL_CULL_FACE` on and once off, and requires both the silhouette area
  and the total shading to be identical. A clockwise-wound generator would cull
  its front faces and show either nothing or a darker interior. All four closed
  solids match exactly, which confirms the CCW contract `main.cpp` relies on via
  `glFrontFace(GL_CCW)`.
- The L9 slide-20 averaging formula is verified on two independent cases, both to
  0.00e+00: an octahedron (where the correct answer is `normalize(position)`) and
  a flat grid (where it must be exactly +Y). The grid is the case `makeGrid`
  actually uses, so this exercises the shipping code path.
- Sphere triangle counts rise faster than the cylinder because `makeSphere` takes
  both stacks and slices from the same tessellation knob. Expected, not a defect.

**PHASE 4 VERDICT: PASS — 37/37 substantive assertions (1 disclosed placeholder). Proceeding to Phase 5.**

---
## Phase 5 — 🎯 Milestone 1: the Phong shader

**Exit criterion (guide):** a sphere with brass material under one directional
light shows a clear bright side, dark side, and a specular highlight that MOVES
as you orbit the camera. `1`/`2`/`3` give three visibly different results.

### Commands

| # | Command | Result |
|---|---|---|
| 5.1 | build + run scratch harness `p5.exe` | 20/22, two FAILED |
| 5.2 | inspect `Lighting.h` term gating and GL depth func | both failures traced to the harness |
| 5.3 | corrected both assertions, re-ran `p5.exe` | 22 checks, 0 failures |

### Assertions

| Group | Assertion | Observed | Verdict |
|---|---|---|---|
| 5.1 | one program serves all three modes | id 3 throughout | PASS |
| 5.1 | mode switching never relinks | still program 3 | PASS |
| 5.1 | mode is a live uniform | `uShadingMode` location valid | PASS |
| 5.2 | brass sphere renders | 101,586 lit px | PASS |
| 5.2 | genuinely bright region | peak luminance 0.992 | PASS |
| 5.2 | genuinely dark region | min luminance 0.036 | PASS |
| 5.2 | bright-to-dark ratio | 27.3x | PASS |
| **5.3** | **a specular highlight exists** | **peak 0.941** | **PASS** |
| **5.3** | **orbiting 25 deg MOVES it** | **38.0 px** | **PASS** |
| **5.3** | **yaw+pitch orbit moves it** | **38.5 px** | **PASS** |
| 5.3 | returning the camera returns the highlight | back to (377,382) | PASS |
| 5.4 | Flat vs Gouraud differ | 78,336 px at 6x10 | PASS |
| 5.4 | Flat vs Phong differ | 74,996 px | PASS |
| 5.4 | Gouraud vs Phong differ | 47,922 px | PASS |
| 5.5 | ambient-only equals k_a * I_a | (0.0510 0.0353 0.0039) | PASS |
| 5.5 | each term adds light | 0.0510 < 0.5356 < 0.5955 | PASS |
| 5.5 | terms reconstruct per pixel | worst 1/255 over 298,195 channels | PASS |
| 5.6 | (M^-1)^T keeps normals perpendicular | dot 1.49e-08 | PASS |
| 5.6 | naive M would shear badly | dot 0.946 | PASS |
| 5.6 | the shader really uses uNormalMatrix | swapping it changes 248,986 px | PASS |
| 5.7 | Blinn toggle changes the highlight | 8,438 px | PASS |
| — | no OpenGL error | GL_NO_ERROR | PASS |

### Two instrument defects found and corrected

**(a) `the terms are additive` FAILED.** The harness averaged each term mask over
its OWN set of lit pixels. Those sets differ - specular-only lights far fewer
pixels - so the means are not additive even when the shader is. Rewritten to
compare PER PIXEL over one fixed mask, skipping channels whose sum would clamp
at 255. Result: worst error 1/255 across 298,195 channels. Emission was checked
first and is ungated by `uTermMask` (`Lighting.h:65`), but brass emission is
zero so it does not enter the algebra.

**(b) `substituting the naive matrix changes 0 px` FAILED.** The harness redrew
identical geometry into a framebuffer it had not cleared. At identical depth the
default `GL_LESS` test rejects every fragment, so nothing was written and the
comparison reported "no difference" regardless of the uniform. Added the clear.
Result: 248,986 px change, confirming `uNormalMatrix` is live.

Neither defect was in project code. No project file was modified.

### Notes

- 5.3 is the Milestone 1 criterion and the one the guide singles out: "if the
  highlight does not move when you orbit, you forgot to update `uViewPos`". It is
  measured with the term mask set to SPECULAR ONLY, so nothing but the view
  vector can move the peak, and re-tested for return-to-origin to rule out drift.
- 5.4 uses a deliberately coarse 6x10 sphere, the condition under which L9 s27-28
  says the three modes must diverge. All three pairs differ on tens of thousands
  of pixels.
- 5.6 tests the normal matrix three ways: the geometric property, the fact that
  the naive alternative is badly wrong, and that the shipping shader is actually
  receiving the correct one.

**PHASE 5 VERDICT: PASS — 22/22 assertions after instrument correction. Milestone 1 confirmed. Proceeding to Phase 6.**

---
## Phase 6 — Materials and the second light

**Exit criterion (guide):** three spheres side by side with Brass / Polished
Silver / Sailcloth look clearly different — the silver has a tiny sharp
highlight, the brass a medium warm one, the sailcloth almost none.

### Commands

| # | Command | Result |
|---|---|---|
| 6.1 | build + run scratch harness `p6.exe` | 27/28, one FAILED |
| 6.2 | inspected the failure | sample-count threshold, not the claim |
| 6.3 | lowered the threshold, re-ran | 28 checks, 0 failures |

### Assertions

| Group | Assertion | Observed | Verdict |
|---|---|---|---|
| 6.1 | Brass matches slide 60 in all 10 numbers | exact | PASS |
| 6.1 | Polished Silver matches slide 60 | exact | PASS |
| 6.1 | Black Plastic matches slide 60 | exact | PASS |
| 6.1 | tuned n_s: ocean 160, hull 8, sailcloth 4 | exact | PASS |
| 6.1 | muzzle flash I_e = (1.0, 0.7, 0.3) | exact | PASS |
| 6.1 | n_s spans 4 -> 160 in one frame | 40x | PASS |
| **6.2** | **three crops identical in size** | **8,649 px each** | **PASS** |
| **6.2** | **Brass vs Polished Silver differ** | **8,536 px, mean dE 0.470** | **PASS** |
| **6.2** | **Brass vs Sailcloth differ** | **8,627 px, mean dE 0.406** | **PASS** |
| **6.2** | **Polished Silver vs Sailcloth differ** | **8,236 px, mean dE 0.372** | **PASS** |
| 6.2 | sailcloth has almost no highlight | peak 0.039 | PASS |
| 6.2 | polished silver has a strong one | peak 0.767 | PASS |
| 6.2 | silver is TIGHTER than brass | 47 px vs 141 px | PASS |
| 6.2 | sailcloth is BROADER than brass | 966 px vs 141 px | PASS |
| 6.3 | attenuation at d = 1.5 / 3.5 / 5.5 / 8.5 | within 0.002 of formula | PASS x4 |
| 6.3 | intensity falls monotonically | 0.827 > 0.584 > 0.404 > 0.247 | PASS |
| 6.4 | the point light alone lights the sphere | 31,782 channels | PASS |
| 6.4 | sun + point reconstructs both-on | worst 1/255 over 46,333 channels | PASS |
| 6.5 | emissive sphere visible with all lights off | 15,628 px | PASS |
| 6.5 | its colour is exactly I_e | (1.000 0.698 0.298) | PASS |
| 6.5 | black plastic vanishes in ambient-only | 0 px | PASS |
| 6.6 | light slot 0 exists | yes | PASS |
| 6.6 | light slot 1 exists | yes | PASS |
| 6.6 | light slot 2 does NOT exist | location -1 | PASS |
| — | no OpenGL error | GL_NO_ERROR | PASS |

### Instrument defect found and corrected

`sun + point reconstructs both-on` FAILED with `worst 1/255` printed in its own
message. The substantive claim had passed; the compound condition also required
`compared > 50000` and the sphere yielded 46,333 unclamped channels. An arbitrary
sample-count floor set too high, nothing more. Lowered to 20,000. No project code
touched.

### Notes

- 6.2 is the guide checkpoint. All three spheres are the SAME geometry at the
  same size, so every measured difference is attributable to the material alone.
  The half-peak-area figures put numbers on the guide's qualitative claim:
  sailcloth 966 px (broad and invisibly dim), brass 141 px, silver 47 px. The
  ordering is exactly what n_s 4 / 27.9 / 89.6 predicts.
- 6.6 is worth noting as a scope check: the shader declares `uLights[2]`, so the
  two-light ceiling in the PRD is enforced by the GLSL itself rather than by
  convention. A third light cannot be added by accident.
- The attenuation figures reproduce the Phase 6 explanation doc exactly, which
  is a useful cross-check that the doc was not written from memory.

**PHASE 6 VERDICT: PASS — 28/28 assertions after instrument correction. Proceeding to Phase 7.**

---
## Phase 7 — Static ship hierarchy

**Exit criterion (guide):** the ship looks like a ship, and temporarily
hardcoding `shipMatrix = rotate(45 deg)` visibly carries the masts, sails, cannon
and barrel with it.

### Commands

| # | Command | Result |
|---|---|---|
| 7.1 | build + run scratch harness `p7.exe` | 38 checks, 0 failures, first run |

### Assertions

| Group | Assertion | Observed | Verdict |
|---|---|---|---|
| 7.1 | all 10 frames are rigid (no scale leaked) | 10/10 unit basis vectors | PASS |
| 7.1 | fore mast offset is as specified | 0.95 ahead of hull centre | PASS |
| 7.1 | the leak would be 4x, and is not present | wrong path gives 3.80 | PASS |
| 7.2 | L1 hull -> L2 deck | (0.00 0.38 0.00) | PASS |
| 7.2 | L2 deck -> L3 mount | (0.48 0.14 0.55) | PASS |
| 7.2 | L3 mount -> L4 yoke | (0.00 0.24 0.00) | PASS |
| 7.2 | L4 yoke -> barrel is pure rotation | zero offset | PASS |
| 7.2 | L2 deck -> L3 main mast | (0.00 0.00 -0.55) | PASS |
| 7.2 | L3 mast -> L4 yard | (0.00 2.05 0.00) | PASS |
| 7.2 | L4 yard -> flag | (0.00 0.80 0.00), child of the YARD per PRD s7 | PASS |
| **7.3** | **deck / mount / yoke / barrel land on R * offset** | **exact, 4 nodes** | **PASS x4** |
| **7.3** | **both masts and both yards land on R * offset** | **exact, 4 nodes** | **PASS x4** |
| **7.3** | **flag lands on R * offset** | **moved 2.468 units** | **PASS** |
| **7.3** | **muzzle POINT rotates with the hull** | **moved 1.256 units** | **PASS** |
| **7.3** | **muzzle DIRECTION rotates with the hull** | **exact** | **PASS** |
| 7.4 | az=0 el=0 fires down ship +Z | (0.000 0.000 1.000) | PASS |
| 7.4 | az=90 fires to starboard | (1,0,0) | PASS |
| 7.4 | positive elevation RAISES the muzzle | dir.y = +0.500 = sin 30 | PASS |
| 7.4 | az=138 el=16 matches R_y * R_x | (0.643 0.276 -0.714) | PASS |
| 7.4 | traversing does not move the masts | 0 movement | PASS |
| 7.4 | traversing does move the muzzle | 2.26 units | PASS |
| 7.5 | forward vector is unit length | 1.000000 | PASS |
| 7.5 | w=1 gives the world-origin direction | (0.69 0.71 -0.10) | PASS |
| 7.5 | w=0 is translation-invariant | unchanged over 42 units | PASS |
| 7.5 | w=1 position does translate | > 40 units | PASS |
| 7.5 | muzzle sits at BARREL_OFFSET + half barrel | 1.175 | PASS |
| 7.6 | draw calls within ceiling | 15 of 20 | PASS |
| 7.6 | triangles within budget | 10,026 of 25,000 | PASS |
| 7.6 | the ship is on screen | 38,878 px | PASS |
| 7.6 | heeling 45 deg visibly moves the whole body | 46,388 px changed | PASS |
| 7.6 | sails survive a full orbit | 26,732 px from astern | PASS |
| — | no OpenGL error | GL_NO_ERROR | PASS |

### Notes

- No instrument defects this phase. 38/38 on the first run.
- 7.3 is the guide checkpoint, executed as an exact numerical test rather than by
  eye: every one of nine child frames plus the muzzle point AND the muzzle
  direction lands on `R * offset` to within 1e-4. The rendered form of the same
  check (7.6) shows 46,388 pixels changing, so it is confirmed both analytically
  and visually.
- 7.2 confirms the flag is parented to the main YARD, matching PRD section 7.
  This was corrected from an earlier main-mast parenting.
- 7.4 records the elevation-axis correction: a positive angle raises the muzzle
  (`dir.y = +0.500 = sin 30`). The guide snippet rotates about `+X`, which would
  depress it.
- Draw-call headroom is 5. Phase 10 adds a reduced enemy ship, so this is the
  budget line to watch.

**PHASE 7 VERDICT: PASS — 38/38 assertions, no corrections needed. Proceeding to Phase 8.**

---
## Phase 8 — 🎯 Milestone 2: ocean waves + ship rocking

**Exit criterion (guide):** the ocean ripples, the ship rides it convincingly,
and the specular sun-streak glitters on the moving water.

### Commands

| # | Command | Result |
|---|---|---|
| 8.1 | build + run scratch harness `p8.exe` | 23/24, one FAILED |
| 8.2 | inspected the failure | badly chosen sample instant, not a defect |
| 8.3 | replaced the point sample with a scan, re-ran | 24 checks, 0 failures |

### Assertions

| Group | Assertion | Observed | Verdict |
|---|---|---|---|
| 8.1 | `waveSlopeX` is d/dx of `waveHeight` | worst 6.61e-06 | PASS |
| 8.1 | `waveSlopeZ` is d/dz of `waveHeight` | worst 7.58e-06 | PASS |
| **8.2** | **648 depth probes taken over 4 instants** | **648** | **PASS** |
| **8.2** | **GPU sea matches `waveHeight()`** | **worst 0.00072 vs 0.00005 noise floor** | **PASS** |
| 8.3 | sea GEOMETRY moves, not just shading | 615,727 of 617,238 depth pixels changed | PASS |
| 8.3 | returning to t=0.0 is pixel-identical | 640,000 / 640,000 | PASS |
| 8.4 | hull renders at both instants | 33,932 and 36,602 px | PASS |
| 8.4 | hull silhouette stable across t | 7.9% change, from pose only | PASS |
| 8.5 | heave = freeboard + waveHeight | exact at 5 instants | PASS x5 |
| 8.6 | hull better aligned with the water normal than level | 39/39 instants | PASS |
| 8.6 | starboard rail rises with the cross-ship slope | 64/64 | PASS |
| 8.6 | bow rises with the fore-aft slope | 62/62 | PASS |
| 8.7 | H off: hull dead level at mean sea level | 3 instants | PASS x3 |
| 8.7 | child stays fixed in SHIP space | worst 2.53e-07 over 93 instants | PASS |
| 8.7 | child separates in WORLD space | up to 0.388 units | PASS |
| **8.8** | **specular sun-path present on the water** | **1,584 near-white px** | **PASS** |
| **8.8** | **the streak breaks up as crests pass** | **2,821 px changed state** | **PASS** |
| — | no OpenGL error | GL_NO_ERROR | PASS |

### Instrument defect found and corrected

`but N units apart in WORLD space` FAILED at 0.025 units against a 0.05
threshold. Cause: the test sampled a single instant, t = 3.4, where the wave
height happens to be -0.0238 — very nearly zero. With no wave under the hull
there is almost nothing for the H key to remove, so the separation is genuinely
small there. The assertion was measuring the wave phase, not the mechanism.
Replaced with a scan over a full period: the ship-space invariance now holds at
all 93 instants (worst 2.53e-07) and the world-space separation peaks at 0.388
units. No project code touched.

### Notes

- 8.2 is the load-bearing test of the phase and the one that guards the drift
  trap in `src/Wave.h`. It is calibrated against a CONTROL run on a known-flat
  surface (true y = 0 everywhere), so the tolerance is set by the resolution of
  the depth-readback method rather than guessed. The measured agreement, 0.00072,
  sits a factor of 140 below the smallest error a wrong constant could produce.
- The probe uses a 256x256 grid, not the shipping 64x64. The rasteriser draws a
  linear chord across each cell, which sits up to 0.0236 below the true sine at
  shipping resolution — enough to swamp the quantity being measured. This is a
  property of mesh resolution, not of the constants.
- 8.3 second assertion is the strongest available evidence for Requirement 12:
  re-rendering an earlier timestamp reproduces the frame in all 640,000 pixels,
  so no state accumulates between frames.
- 8.6 is deliberately damping-agnostic. It asserts the hull ends up BETTER aligned
  with the water normal than a level hull would be, which fails if either the roll
  or the pitch sign is inverted regardless of the value of `WAVE_FOLLOW`.
- 8.8 confirms both halves of the guide wording: the streak exists (1,584
  near-white pixels) and it GLITTERS, i.e. changes as the crests move under it.

**PHASE 8 VERDICT: PASS — 24/24 assertions after instrument correction. Milestone 2 confirmed.**

---

# RUN COMPLETE — ALL 9 PHASES PASS

| Phase | Subject | Substantive assertions | Result |
|---|---|---:|---|
| 0 | Toolchain / build / window | 8 | PASS |
| 1 | Render loop + clock | 8 | PASS |
| 2 | Shader loading + error reporting | 15 | PASS |
| 3 | Camera + MVP | 29 | PASS |
| 4 | Mesh library + normals | 37 | PASS |
| 5 | Phong shader, three modes (M1) | 22 | PASS |
| 6 | Materials + second light | 28 | PASS |
| 7 | Ship hierarchy | 38 | PASS |
| 8 | Ocean waves + rocking (M2) | 24 | PASS |
| | **Total** | **209** | **9/9 PASS** |

## Defects found

**In project code: none.** No file under `src/`, `shaders/`, or `CMakeLists.txt`
was modified during this audit.

**In the audit instruments: five**, all found, diagnosed and corrected before the
phase was allowed to advance. Each was verified to be a fault in the measurement
rather than in the thing measured:

| Phase | Instrument defect | Root cause |
|---|---|---|
| 3 | near-plane NDC assertion | sampled 1 mm past the plane, where depth is already -0.98 |
| 5 | term additivity | averaged each term over its OWN lit-pixel set; sets differ |
| 5 | normal-matrix substitution | redrew identical geometry without clearing; `GL_LESS` rejected every fragment |
| 6 | light summation | arbitrary sample-count floor set above what the sphere yields |
| 8 | H-key world separation | sampled one instant where the wave was near zero |

## Standing observations for later phases

- Draw-call headroom is **5** (15 of 20 used). Phase 10 adds a reduced enemy ship
  and Phases 12–14 add the cannonball, muzzle flash and particle pools. This is
  the budget line most likely to break first; Phase 17 levers are the flag, the
  bowsprit and the trunnion.
- Two-sided sails light their back faces with the front normal. Documented in the
  Phase 7 notes; the fix is `gl_FrontFacing` in the fragment shader. Neither L9
  demo is affected.
- The `H` key binding is nominally Phase 15 work but is wired now, because the
  branch it drives lives in the Phase 8 `shipMatrix` snippet and is untestable
  otherwise. Phase 15 only needs to leave it alone.

## Provenance of the audited tree

The tree changed under the audit and this is recorded rather than smoothed over.

- At Phase 0 the head was `ca23fdd` and Phases 6–8 were UNCOMMITTED working-tree
  changes (`AGENT.md`, `CLAUDE.md`, the guide, `shaders/phong.vert`,
  `src/main.cpp` modified; `src/Material.h`, `src/Wave.h` and three phase docs
  untracked).
- Between the Phase 8 run and this summary those changes were committed as
  `a3a04eb feat: material table, ship hierarchy, and the wave-driven sea`,
  touching 10 files, +1113 / -131.
- The commit contents are byte-identical to what was audited: every phase was
  verified against those same files, and no source file was modified by the audit
  at any point. `git status` after the run shows only `workflow-log.md` as new.

So the audit result applies to `a3a04eb` exactly. The one caveat worth stating:
Phases 0–5 were verified against source that already carried the Phase 6–8
changes, because that is the only tree that exists. They were not re-verified
against the historical `ca23fdd` snapshot.

---

## Phase 9 — Idle rigging animation (appended after the original 0–8 run)

**Exit criterion (guide):** sails and flag move independently and the scene has
visible life even with no shot fired.

### Commands

| # | Command | Result |
|---|---|---|
| 9.1 | implement `R_z` sail flutter and `R_y` flag wave below the root | `src/main.cpp` only |
| 9.2 | build Release / Debug / strict `/W4` | 0 errors, 0 warnings |
| 9.3 | build + run scratch harness `p9.exe` | 21 checks, 0 failures, first run |
| 9.4 | re-run `p2` through `p8` | 196 checks, 0 failures |

### Assertions

| Group | Assertion | Observed | Verdict |
|---|---|---|---|
| 9.1 | fore sail angle == A sin(wt+phi) about +Z | worst 2.98e-08 over 540 samples | PASS |
| 9.1 | main sail angle == A sin(wt+phi) about +Z | worst 2.98e-08 | PASS |
| 9.1 | flag angle == A sin(wt+phi) about +Y (PRD s8) | worst 5.96e-08 | PASS |
| **9.2** | **the two sails are not in lockstep** | **up to 0.163 rad / 9.3 deg apart** | **PASS** |
| **9.2** | **neither sail permanently leads** | **fore 49%, main 50%** | **PASS** |
| **9.2** | **the flag runs at a different frequency** | **3.1 vs 2.2 rad/s** | **PASS** |
| **9.2** | **the scene is alive with no shot fired** | **24,959 px over 0.55 s** | **PASS** |
| 9.3 | with H off the root is frozen | bit-identical, 239/239 | PASS |
| 9.3 | the sail keeps moving anyway | 237/239 | PASS |
| 9.3 | the flag keeps moving anyway | 238/239 | PASS |
| 9.3 | with H on the hull moves too | 98/99 | PASS |
| 9.4 | rigging survives H, on screen | 17,660 px still change | PASS |
| 9.5 | same t twice gives identical frames | bit-identical | PASS |
| 9.5 | a different t gives a different pose | yes | PASS |
| 9.5 | whole frame at t=2.5 reproduces | 640,000 / 640,000 px | PASS |
| 9.6 | the foot swings far more than the head | 0.115 vs 0.0000 units | PASS |
| 9.6 | the head stays on the yard | 0.0000 units | PASS |
| 9.6 | the foot swing is visible | 0.115 units | PASS |
| 9.7 | draw calls unchanged | 15 | PASS |
| 9.7 | triangles within budget | 10,026 | PASS |
| — | no OpenGL error | GL_NO_ERROR | PASS |

### Notes

- No instrument defects. 21/21 on the first run.
- 9.6 reports the head moving `0.0000` units, giving a degenerate ratio. That is
  correct rather than suspicious: the sail is hung by `translate(0, -h/2, 0)` from
  the yard frame, so its head lands exactly ON the rotation axis and cannot move
  at all. The check confirms the pivot is at the yard, which is the distinction
  the guide draws.
- 9.3 and 9.4 are the phase's real payoff. Phase 8 could only half-demonstrate
  the `H` key because once the hull stopped, nothing on the ship was moving.
  Now the root freezes bit-identically while 17,660 pixels of rigging carry on.
- Regression: `p2`–`p8` re-run clean. Note that the `p7` and `p8` harnesses carry
  their own pre-Phase-9 copy of `buildShipFrames` without the sail frames. Their
  assertions cover the hull, gimbal and wave, none of which Phase 9 touches, so
  they remain valid; `p9` is the authority for the rigging chain.

**PHASE 9 VERDICT: PASS — 21/21 assertions, no corrections needed.**

---

## Phase 10 — Enemy ship on patrol

**Exit criterion (guide):** two ships, both rocking correctly, one patrolling
laterally.

### Commands

| # | Command | Result |
|---|---|---|
| 10.1 | add `ENEMY_HULL`, `enemyPos(t)`, `ShipDetail`, enemy root | `src/Material.h`, `src/main.cpp` |
| 10.2 | build Release / Debug / strict `/W4` | 0 errors, 0 warnings |
| 10.3 | build + run scratch harness `p10.exe` | 24/25, one FAILED |
| 10.4 | project the full sweep for 4 camera settings | framing defect confirmed and solved |
| 10.5 | reframe default camera, re-run `p10.exe` | 25 checks, 0 failures |
| 10.6 | re-run `p2` through `p9` | 217 checks, 0 failures |

### Assertions

| Group | Assertion | Observed | Verdict |
|---|---|---|---|
| 10.1 | `enemyPos.x == sin(0.25t)*14` | exact over 4,600 samples | PASS |
| 10.1 | the sweep is full width | -14.0 to +14.0 | PASS |
| 10.1 | same t gives the same position | yes | PASS |
| 10.1 | one period | 25.13 s | PASS |
| **10.2** | **both hulls are tilted** | **429/429 instants** | **PASS** |
| **10.2** | **and tilted DIFFERENTLY** | **429/429** | **PASS** |
| **10.2** | **up-vectors diverge visibly** | **up to 0.262** | **PASS** |
| 10.2 | enemy heave tracks the wave at its moving x | worst 0.00e+00 | PASS |
| 10.3 | draw calls | 19 (1+13+4+1) | PASS |
| 10.3 | inside the PRD 3.3 ceiling | 19 of 20 | PASS |
| 10.3 | triangles | 10,296 of 25,000 | PASS |
| 10.3 | the reduced enemy is 4 draws | 4 | PASS |
| 10.3 | a full clone would have broken the ceiling | 13 draws, frame at 28 | PASS |
| 10.4 | all enemy frames uniformly scaled | 12/12, worst anisotropy 1.19e-07 | PASS |
| 10.4 | player frames still exactly unit | yes | PASS |
| 10.4 | enemy sits at its patrol point | (11.78, -18.00) | PASS |
| 10.5 | the enemy is on screen | 13,618 px | PASS |
| 10.5 | its hull is darker | kd.x 0.22 vs 0.38 | PASS |
| 10.5 | and colder | R-B 0.03 vs 0.23 | PASS |
| 10.5 | n_s unchanged | 8 | PASS |
| **10.6** | **on screen across the whole patrol** | **104/104 instants** | **PASS** |
| **10.6** | **the lateral pass is unmistakable** | **646 px traversed** | **PASS** |
| 10.7 | H levels the enemy too | 120/120 | PASS |
| 10.7 | but it keeps patrolling | yes | PASS |
| — | no OpenGL error | GL_NO_ERROR | PASS |

### One REAL defect found and fixed

Unlike the five instrument bugs in the 0–8 run, this one was in the project.

`the enemy stays inside the viewport` FAILED. The enemy travels 28 units laterally
at 18 units of range; with the default camera at yaw 28 degrees the far end of the
sweep projected to screen x **1062** on a 1000-pixel viewport, so the ship sailed
off the right edge. The checkpoint explicitly requires the lateral patrol to be
visible, so this was a genuine framing defect.

Solved by projecting the full sweep analytically for several camera settings
rather than guessing:

| Camera | Enemy screen x | All on screen |
|---|---|---|
| `z=-7, r=21, yaw=28` | 349 to 1062 | no |
| `z=-8, r=24, yaw=14` | 265 to 910 | **yes** |
| `z=-8, r=26, yaw=10` | 254 to 861 | yes |
| `z=-9, r=28, yaw=8` | 247 to 836 | yes |

Chose `z=-8, r=24, yaw=14`: the smallest radius that fits the whole patrol, so the
player ship stays as large as possible.

### Standing budget warning — escalated

19 of 20 draw calls are now used. This is the line flagged as at-risk in the
Phase 7 notes and in the original run summary, and it is now one call from the
ceiling. Remaining phases add: cannonball +1 (Phase 12), smoke pool up to +4 and
splash pool up to +4 (Phase 14). Making the muzzle flash conditional in Phase 12
returns one call, but the peak during a shot still passes 20.

Levers, in the guide's own descope order: bowsprit, flag, trunnion (1 draw each),
or shrinking the particle pools from 4+4 to 3+3. The decision belongs to Phase 17
and is recorded in `docs/PHASE_10_EXPLANATION.md` so it is not met by surprise.

**PHASE 10 VERDICT: PASS — 25/25 assertions after one genuine fix.**

---

## Phase 11 — Two-axis cannon aiming

**Exit criterion (guide):** the cannon smoothly tracks the patrolling enemy, and
manual mode responds to arrow keys.

### Commands

| # | Command | Result |
|---|---|---|
| 11.1 | check the enemy bearing range for an atan2 wrap | branch cut confirmed on every pass |
| 11.2 | add `updateAim`, `wrapAngle`, TAB and arrow keys | `src/main.cpp` only |
| 11.3 | build Release / Debug / strict `/W4` | 0 errors, 0 warnings |
| 11.4 | build + run scratch harness `p11.exe` | 27/28, one FAILED |
| 11.5 | work the roll/pitch geometry by hand | assertion picked the wrong axis |
| 11.6 | correct the assertion, re-run | 28 checks, 0 failures |
| 11.7 | re-run `p2` through `p10` | 242 checks, 0 failures |

### Assertions

| Group | Assertion | Observed | Verdict |
|---|---|---|---|
| 11.1 | on-target azimuth is a fixed point | -0.0000 deg drift | PASS |
| 11.1 | on-target elevation is a fixed point | yes | PASS |
| 11.1 | solution sits exactly BALLISTIC_LIFT high | 7.0 deg | PASS |
| **11.2** | **bearing crossed the branch cut** | **3 times in 36 s** | **PASS** |
| **11.2** | **turret never exceeded the slew limit** | **0.0921 vs 0.4167 deg/step** | **PASS** |
| 11.3 | azimuth rate limited | worst 0.417 deg/step | PASS |
| 11.3 | elevation rate limited | worst 0.417 deg/step | PASS |
| 11.3 | converges from 170 deg off | within 0.00 deg | PASS |
| **11.4** | **world bearing tracks the enemy while both roll** | **within 0.97 deg, 49 samples** | **PASS** |
| **11.4** | **world elevation tracks target + lift** | **within 0.47 deg** | **PASS** |
| 11.5 | stationary target still swings elevation | 13.99 deg | PASS |
| 11.5 | and azimuth, as the minor axis | 0.35 deg | PASS |
| 11.5 | hull level + fixed target -> settles and stops | dead stop | PASS |
| 11.6 | no key held: nothing moves | yes | PASS |
| 11.6 | RIGHT / LEFT traverse | +25.0 / -25.0 deg | PASS x2 |
| 11.6 | UP / DOWN elevate and depress | +25.0 / -5.0 deg | PASS x2 |
| 11.6 | manual ignores a target in view | yes | PASS |
| 11.7 | manual UP stops at +45 | exact | PASS |
| 11.7 | manual DOWN stops at -5 | exact | PASS |
| 11.7 | auto-track respects the stop | -5.00 deg | PASS |
| 11.7 | 30 s of live tracking stays in range | 3601/3601 | PASS |
| 11.8 | dt = 0 freezes the turret | no movement | PASS |
| 11.9 | draw calls unchanged | 19 | PASS |
| 11.9 | triangles within budget | 10,296 | PASS |
| — | no OpenGL error | GL_NO_ERROR | PASS |

### A REAL correction to the guide's snippet

This is the fourth of its kind, after the gun elevation axis (Phase 7), the hull
pitch axis (Phase 8) and the pixel-centre sampling (Phase 8).

The guide writes `azimuth += clamp(targetAz - azimuth, ±SLEW*dt)`. The enemy
patrols across the player's stern, so the bearing sweeps through 180 degrees on
every pass - straight across the branch cut of `atan2`:

| Enemy x | Bearing |
|---:|---:|
| +2 | +175.3 |
| 0 | -178.5 |
| -2 | -172.4 |

At the crossing the solution steps 3 degrees, but plain subtraction reads it as
-357 and drives the turret almost all the way round the wrong way, once per pass.
Replaced with the shortest signed angle, `wrapAngle(targetAz - g_azimuth)`.

### One corrected assertion

`with a STATIONARY target the azimuth still swings` FAILED at 0.35 deg against an
expected 1.0. The code was right; the assertion picked the wrong axis. Roll turns
about the bow axis and pitch about the beam axis, so for a target near the
horizontal plane the rocking tips it up and down in the ship frame far more than
it swings it sideways. Verified by hand at +/-5 deg of roll and pitch: azimuth
spans 155.1 to 155.5 while elevation spans -5.8 to +3.3. The check now asserts on
elevation, with a smaller assertion kept on azimuth as the minor axis.

**PHASE 11 VERDICT: PASS — 28/28 assertions.**

---

## Phase 12 — 🎯 Milestone 3: ballistics

**Exit criterion (guide):** pressing SPACE launches a ball that visibly arcs
under gravity along the barrel's true direction, and firing at different points
in the ship's roll produces different trajectories.

### Commands

| # | Command | Result |
|---|---|---|
| 12.1 | derive the lift from MUZZLE_SPEED before writing code | 7 deg placeholder throws 43 units at a 20-unit target |
| 12.2 | add `Projectile`, `fire()`, `updateProjectile()`, SPACE | `src/main.cpp` only |
| 12.3 | make the muzzle flash conditional | one draw call returned to the budget |
| 12.4 | build Release / Debug / strict `/W4` | 0 errors, 0 warnings |
| 12.5 | build + run scratch harness `p12.exe` | 35/36, one FAILED |
| 12.6 | rewrite that assertion as an exact statement | 38 checks, 0 failures |
| 12.7 | enlarge the ball for visibility, re-verify | 38/38 held |
| 12.8 | re-run `p2` through `p11` | 270 checks, 0 failures |

### Assertions

| Group | Assertion | Observed | Verdict |
|---|---|---|---|
| 12.1 | SPACE launches a ball | yes | PASS |
| 12.1 | p0 == the hierarchy muzzle point | 0.00e+00 | PASS |
| 12.1 | \|v0\| == MUZZLE_SPEED | 42.0000 | PASS |
| 12.1 | v0 along the barrel forward axis | exact | PASS |
| 12.1 | w=1 on the direction is wrong | visibly | PASS |
| 12.1 | w=0 on the position returns a bare direction | length == MUZZLE_Z | PASS |
| 12.1 | the error equals the discarded translation | 0.992 units | PASS |
| 12.1 | 72 units out the same slip costs 72.1 units | yes | PASS |
| 12.1 | the ball starts above the waterline | y = 0.70 | PASS |
| 12.2 | p(tau) == the closed form | worst 1.87e-05 over 600 samples | PASS |
| 12.2 | it rises then falls | parabola | PASS |
| 12.2 | apex at v0.y / g | 0.195 s | PASS |
| 12.2 | horizontal motion is linear in t | exact | PASS |
| 12.3 | same tau after a different tau | identical | PASS |
| 12.3 | 30 Hz and 300 Hz agree | 0.00e+00 apart | PASS |
| **12.4** | **6 shots, all different muzzle points** | **0.048 to 0.317 units apart** | **PASS** |
| **12.4** | **all different launch directions** | **0.57 to 2.47 deg apart** | **PASS** |
| **12.4** | **widest pair** | **2.47 deg** | **PASS** |
| **12.4** | **landing spread from the roll alone** | **4.98 units** | **PASS** |
| 12.5 | level hull, 20 deg: it climbs | 10.53 units | PASS |
| 12.5 | and returns to sea level | 3.02 s | PASS |
| 12.5 | within 0.086 s of 2 v sin(th)/g | yes | PASS |
| 12.6 | ballisticLift(20) | 3.19 deg | PASS |
| 12.6 | the old 7 deg maps to ~43 units | yes | PASS |
| 12.6 | the solution inverts exactly, 5 to 60 units | worst 3.81e-06 | PASS |
| 12.6 | beyond reach returns 45 deg, not NaN | 45.0 | PASS |
| 12.7 | a tracked shot passes close | 0.82 units | PASS |
| 12.7 | after a readable flight | 0.490 s | PASS |
| 12.8 | flash ends exactly 0.15 s after the shot | exact | PASS |
| 12.8 | lit at +0.05, +0.14; dark at +0.16 | yes | PASS x3 |
| 12.9 | idle draw calls | 18 | PASS |
| 12.9 | during the flash | 20 | PASS |
| 12.9 | peak is AT the ceiling, not over | 20 of 20 | PASS |
| 12.9 | ball up, flash dead | 19 | PASS |
| 12.9 | triangles | 10,296 | PASS |
| — | no OpenGL error | GL_NO_ERROR | PASS |

### A second correction to the guide chain

Phase 11 left `BALLISTIC_LIFT` at a fixed 7 degrees, flagged as Phase 12's job.
Working it out before writing any code: at 42 m/s a 7 degree lift throws the ball
`v^2 sin(2 theta) / g` = **43.5 units** at a target **20** away. Every shot would
have sailed over. Replaced with the low-arc solution `theta = asin(gR/v^2)/2`,
recomputed each frame because the enemy patrols across 28 units of frontage.

### One corrected assertion

`w=0 on the POSITION loses the translation entirely` FAILED at 0.99 units against
an arbitrary 1.0 threshold. The claim was right; the threshold was meaningless,
because the size of that mistake is just however far the ship is from the world
origin, and this ship sits almost on it. Rewritten as exact statements: `w = 0`
returns a bare direction of length `MUZZLE_Z`, the error equals the discarded
translation exactly, and 72 units out the same slip misplaces the muzzle by 72.1.

### Budget status — improved

Making the flash conditional returned one draw call. The frame now breathes
between **18 idle** and **20 at peak**, exactly at the ceiling rather than over
it. Phase 14's particle pools remain the outstanding risk; the levers recorded in
`docs/PHASE_10_EXPLANATION.md` are unchanged.

**PHASE 12 VERDICT: PASS — 38/38 assertions. Milestone 3 reached; the project is submittable.**

---

## Phase 13 — Impact resolution

**Exit criterion (guide):** aiming well produces HIT, aiming short produces
SPLASH, and the HUD reports which.

### Commands

| # | Command | Result |
|---|---|---|
| 13.1 | measure whether a point test can tunnel | grazing hit inside for 0.58 frames at 30 Hz |
| 13.2 | add `segmentPointDistance`, `checkImpact`, hull glow | `src/main.cpp` only |
| 13.3 | build Release / Debug / strict `/W4` | 0 errors, 0 warnings |
| 13.4 | build + run scratch harness `p13.exe` | 31 checks, 0 failures, first run |
| 13.5 | strengthen a weak shading-mode check | 32 checks, 0 failures |
| 13.6 | re-run `p2` through `p12` | 308 checks, 0 failures |

### Assertions

| Group | Assertion | Observed | Verdict |
|---|---|---|---|
| 13.1 | a through-segment reports distance 0 | exact | PASS |
| 13.1 | both endpoints outside the radius | 3.00 and 3.00 | PASS |
| 13.1 | perpendicular / endpoint / degenerate cases | all exact | PASS x3 |
| **13.2** | **the auto-track solution scores HIT** | **HIT** | **PASS** |
| 13.2 | the hull is flagged to glow | yes | PASS |
| 13.2 | the ball is retired on impact | yes | PASS |
| **13.3** | **a depressed shot SPLASHES** | **SPLASH at 3.8 units** | **PASS** |
| 13.3 | ending at the water surface | yes | PASS |
| 13.4 | a 45 deg shot terminates | SPLASH | PASS |
| **13.5** | **30/60/144 Hz agree** | **HIT / HIT / HIT** | **PASS** |
| **13.5** | **and agree at every firing instant** | **16/16** | **PASS** |
| 13.6 | the ambiguous hull-vs-wave case | HIT, not SPLASH | PASS |
| 13.7 | flash full / half / dark | exact | PASS x3 |
| 13.7 | the base material is untouched | yes | PASS |
| 13.8 | the flash reads on screen | 937 px | PASS |
| 13.8 | Flat / Gouraud / Phong | 937 px each | PASS x3 |
| 13.8 | ambient-only (K term mask) | 937 px | PASS |
| 13.9 | on the solution -> HIT | HIT | PASS |
| 13.9 | depressed -> SPLASH | SPLASH | PASS |
| 13.9 | 35 deg off -> miss | SPLASH | PASS |
| 13.10 | draw calls idle | 18 | PASS |
| 13.10 | triangles | 9,336 | PASS |
| — | no OpenGL error | GL_NO_ERROR | PASS |

### Two measured departures from the guide snippet

**(a) The hull test sweeps a segment, not a point.** Measured before writing any
code: the ball covers 0.70 units per frame at 60 Hz and 1.40 at 30 Hz, while a
grazing hit (closest approach 1.65 against a 1.70 radius) stays inside for only
**1.17 frames at 60 Hz and 0.58 at 30 Hz**. A point test misses those, and misses
different ones at different frame rates - which would make the checkpoint a
matter of luck and contradict the frame-rate independence proven in Phase 12.
The swept test makes 30/60/144 Hz agree at 16 of 16 firing instants.

**(b) The hull is tested BEFORE the sea.** The enemy floats ON the sea, so a shot
arriving at hull height while the local wave crests is below the waterline and
above the deck at the same instant. The guide's order reports SPLASH for a shot
that struck the ship. The audit constructs that exact case - ball at 0.10, wave at
0.32 - and confirms the new order scores it HIT.

### One weak test, corrected

The first shading-mode check looped over three modes but never passed the mode to
the renderer. It rendered Phong three times, reported the same 937 px each time,
and tested nothing while looking like a pass. The renderer now takes the mode as a
parameter. All three still report 937 px - the correct answer rather than a
coincidence, since emission is added identically in all three paths - and a check
on the `K` ambient-only mask was added alongside.

**PHASE 13 VERDICT: PASS — 32/32 assertions.**

---

## Phases 9–13 — Final combined revalidation

The five phases were checked together again after the explanation files were
simplified and the guide samples were corrected.

### Final checks

| Check | Result |
|---|---|
| Requirements mapped to the PRD and implementation guide | PASS |
| Focused logic, hierarchy, input-state, projectile, and impact checks | PASS |
| Hidden OpenGL rendering and draw-budget checks | PASS |
| Full enemy bounds across 361 patrol samples | inside the viewport |
| Auto-track shots at 30, 60, and 144 Hz | 12/12 HIT |
| Deliberately depressed shot | SPLASH |
| Flight timeout | LOST |
| Idle / flight / flash draw calls | 18 / 19 / 20 |
| Default triangle peak | below 25,000 |
| Live SPACE test | HIT after 0.45 s |
| Debug build | PASS |
| Release build | PASS |
| Release `/W4 /WX /permissive-` build | PASS |
| Combined focused audit | **292 checks, 0 failures** |
| Markdown and local-link checks | PASS |

### Documentation corrections

- Replaced the five long phase explanations with short, plain-language files.
- Removed the old Phase 11 claim that the final code still uses a fixed 7-degree lift.
- Updated the Phase 10 budget note to the current 18/19/20 draw counts.
- Updated the Phase 11 guide sample to use `wrapAngle()` and calculated lift.
- Updated the Phase 13 guide sample to use the swept hull-first impact test.
- Clarified that the current visible status is the window title, which the guide
  accepts as the simple HUD approach.

**FINAL VERDICT: PHASES 9, 10, 11, 12, AND 13 ARE COMPLETE.**

---

## Phase 14 — Final implementation and validation

Phase 14 adds fixed, reusable effects without adding a frame-loop allocation.
The implementation uses four smoke slots, four impact slots, and one splash-ring
slot. Smoke starts at the hierarchy's world-space muzzle. A hit reuses the impact
slots for orange sparks. A water impact reuses them for blue spray and starts the
ring at the current wave height.

All active sphere effects use one instanced draw. The optional ring uses one more
draw. This keeps the old 18-draw idle scene inside the PRD ceiling even when
effects overlap.

### Final checks

| Check | Result |
|---|---|
| Fixed pool sizes | 4 smoke + 4 impact + 1 ring |
| Fifty direct smoke spawns | zero C++ allocations |
| Fifty fire/update/render calls | zero C++ allocations |
| Age-based position, growth, fade, and expiry | PASS |
| Smoke wired to `fire()` | PASS |
| Orange burst wired to `HIT` | PASS |
| Blue spray and ring wired to `SPLASH` | PASS |
| Splash framebuffer visibility | PASS |
| Blend and depth state restoration | PASS |
| Maximum measured draw calls | 20 |
| Maximum measured triangles | 17,976 |
| OpenGL errors | none |
| Focused Phase 14 audit | **68 checks, 0 failures** |
| Phase 9–13 regression audit | **292 checks, 0 failures** |
| Debug build | PASS |
| Release build | PASS |
| Release `/W4 /WX /permissive-` build | PASS |

**FINAL VERDICT: PHASE 14 IS COMPLETE.**

---

## Phase 15 — Demonstration suite and Milestone 4

The current project already had runtime tessellation, three shading modes,
hierarchy control, pause, camera control, manual aiming, and firing. The missing
Phase 15 controls were implemented:

- `B` switches reflection-vector Phong and Blinn-Phong.
- `K` cycles the four required lighting-term masks.
- `L` isolates the sun, holds the muzzle point light for inspection, or returns
  to the normal combined mode.
- The title and console now report all three states.

### Focused results

| Check | Result |
|---|---|
| `B` state and shader uniform | PASS |
| `K` cycle `1 -> 3 -> 7 -> 4 -> 1` | PASS |
| `L` cycle both -> sun -> muzzle -> both | PASS |
| Tessellation minimum / maximum | `6/8` and `64/128` |
| Rebuild at an existing clamp | skipped |
| Sun-only vs muzzle-only | 10,292 changed pixels |
| Phong vs Blinn-Phong | 56,584 changed pixels |
| Ambient vs ambient+diffuse | 196,371 changed pixels |
| Ambient+diffuse vs all terms | 31,603 changed pixels |
| Low-grid Gouraud vs Phong | 36,144 changed pixels |
| Gouraud / Phong bright ocean pixels | 0 / 427 |
| Six-segment Flat vs Gouraud barrel | 3,736 changed pixels |
| Six-segment Gouraud vs Phong barrel | 3,402 changed pixels |
| Six-segment Flat vs Phong barrel | 3,508 changed pixels |
| Focused Phase 15 audit | **34 checks, 0 failures** |
| Phase 14 regression audit | **68 checks, 0 failures** |
| Phase 9–13 regression audit | **292 checks, 0 failures** |
| Debug / Release builds | PASS / PASS |
| Release `/W4 /WX /permissive-` build | PASS |

### Live three-minute-script path

The Release executable received `B`, `K` four times, `L` three times, `-` three
times, `1`/`2`/`3`, `H`, `TAB`, `P`, `SPACE`, resume, and `ESC`. The title showed
every expected state. The low-detail frame used 388 triangles and 18 draws. A
paused shot used 20 draws, then resumed and produced a computed `SPLASH`. The
application exited cleanly.

**FINAL VERDICT: PHASE 15 AND MILESTONE 4 ARE COMPLETE.**
