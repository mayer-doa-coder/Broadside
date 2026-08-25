# AGENT.md — Broadside

Instructions for any AI coding assistant or autonomous agent working in this repository.

**Source of truth:** [docs/PRD_Broadside.md](docs/PRD_Broadside.md) (what & why) and [docs/IMPLEMENTATION_GUIDE_Broadside.md](docs/IMPLEMENTATION_GUIDE_Broadside.md) (how & in what order). If this file disagrees with either, those documents win.

**Development environment:** **Visual Studio Code** with the CMake Tools + C/C++ extensions, on MSVC Build Tools 2022 (Windows) or GCC (Linux). The Visual Studio IDE is **not** used — see implementation guide §0.2.

---

## 1. Project Overview

**Broadside** is an OpenGL 3.3 Core Profile Computer Graphics final project: a single warship rides a live wave surface and duels a patrolling enemy vessel with a two-axis aimable cannon firing true parabolic projectiles.

### Core mission

This is **not** a game. It is a demonstration of **hierarchical modelling transformations** and the **Phong illumination model**, using a naval gunnery scenario as the carrier scene because that scenario naturally requires:

- a deep parent-child transform chain (hull → deck → mount → barrel),
- a two-axis gimbal,
- dynamically computed projectile motion,
- two extreme material types (polished brass vs. matte sailcloth) under a single light.

### The one rule that governs every decision

> **If a feature does not map to a row in PRD §4 (Requirement Traceability Matrix), it does not get built.**

### High-level goals

1. Demonstrate all five model transformation categories (translation, rotation, scaling, composite, hierarchical) with **load-bearing**, not decorative, usage.
2. Implement the complete Phong illumination model in GLSL — ambient + diffuse + specular, radial attenuation, two summed light sources, emission.
3. Implement Flat / Gouraud / Phong surface rendering as a **live, switchable comparison on the same geometry**.
4. Compute **100% of motion inside the render loop from `glfwGetTime()`**. No baked keyframes, no animation tables.
5. Ship a demonstrable vertical slice early, then layer features — the phase order in the implementation guide is demo-first.

---

## 2. Tech Stack & Tools

| Component | Version / Spec | Purpose |
|---|---|---|
| **OpenGL** | 3.3 **Core Profile** | The rendering API. Core profile is mandatory — it forces the modern shader pipeline required for per-fragment Phong. |
| **GLSL** | `#version 330 core` | All shading is hand-written. |
| **GLFW** | 3.x (64-bit) | Window creation + keyboard/mouse input |
| **GLAD** | OpenGL 3.3, Core, C/C++, loader generated | Runtime GL function pointer loading |
| **GLM** | Header-only (`external/glm`) | Vector/matrix math; mirrors GLSL syntax |
| **CMake** | ≥ 3.10 | Build system |
| **C++** | `CMAKE_CXX_STANDARD 17` | Language standard |

### Dependency policy — non-negotiable

- **ALLOWED:** GLFW, GLAD, GLM, the C++ standard library, CMake.
- **FORBIDDEN:** Unity, Unreal, Ogre, Bullet, any physics library, any scene-graph library, any model loader, any GUI toolkit, any texture-loading library.
- GLFW/GLAD/GLM are *a window, a function loader, and a math header* — not graphics engines. This is what satisfies the "OpenGL only" requirement. Do not add anything that renders, simulates, or manages scenes on the project's behalf.
- **Never** use fixed-function OpenGL (`glBegin`/`glEnd`/`glLight`/`glMaterial`). The fixed-function pipeline can only do Gouraud; writing our own GLSL is precisely how the Phong marks are earned (L9 slide 36).

---

## 3. Architecture & Directory Structure

```
broadside/
├── CMakeLists.txt
├── .vscode/
│   ├── extensions.json   # recommends cpptools, cmake-tools, shader
│   ├── settings.json     # build dir, C++17, CMake Tools as IntelliSense provider
│   └── launch.json       # cwd MUST be the executable's directory (shader paths)
├── docs/
│   ├── PRD_Broadside.md
│   └── IMPLEMENTATION_GUIDE_Broadside.md
├── include/
│   ├── glad/glad.h
│   └── KHR/khrplatform.h
├── src/
│   ├── glad.c
│   ├── main.cpp          # render loop, input, orchestration
│   ├── Shader.h          # GLSL load/compile/link + uniform setters
│   ├── Mesh.h            # Vertex struct, Mesh class, primitive generators
│   ├── Camera.h          # orbit camera (radius, yaw, pitch)
│   └── Scene.h           # scene graph, materials, ship/cannon/ballistics
├── shaders/
│   ├── phong.vert
│   └── phong.frag
├── external/
│   ├── glm/
│   └── glfw/             # Windows only: include/ + lib-vc2022/
└── build/                # generated — gitignored
```

### Where things go

| Concern | File |
|---|---|
| `glfwInit`, window hints, render loop, `glfwGetTime()` clock, input polling | `src/main.cpp` |
| Shader compile/link, compile-log checking, `setMat4`/`setVec3`/`setFloat`/`setInt` | `src/Shader.h` |
| `struct Vertex { vec3 position; vec3 normal; }`, `class Mesh { VAO, VBO, EBO }`, `makeCube` / `makeCylinder` / `makeSphere` / `makeQuad` / `makeGrid` / `makeCone` / `makeRing`, `computeSmoothNormals` | `src/Mesh.h` |
| Orbit camera (spherical coords), view matrix, mouse drag → yaw/pitch, scroll → radius | `src/Camera.h` |
| `struct Material`, material constants, `drawShip()`, aim solver, `Projectile`, particle pools, CPU wave functions | `src/Scene.h` |
| Illumination equation, wave displacement, shading-mode branch | `shaders/phong.vert`, `shaders/phong.frag` |

### Scene graph — the architecture that matters most

```
WORLD
├── Sun (directional light, no geometry)
├── Ocean surface                      [M5]  (vertex-shader displaced)
├── PLAYER SHIP  ── T(pos) · R_z(roll(t)) · R_x(pitch(t))
│   ├── Hull [M1]   ├── Deck [M1]
│   ├── Mast ── Yard ── Sail (×2)  [M2, M2, M4]   └── Flag [M4]
│   └── CANNON MOUNT [M1]
│       └── Yoke ── R_y(azimuth)
│           └── Barrel ── R_x(elevation)  [M2, BRASS]
│               └── Muzzle Point (empty transform — spawn origin)
│                   └── Muzzle Flash [M3, emissive, Light 1 position]
├── ENEMY SHIP   ── T(patrol(t)) · R_z(roll(t)) · R_x(pitch(t))
└── PARTICLE POOLS (world-space, NOT parented)
    ├── Smoke pool [M3 ×4]   ├── Splash pool [M3 ×4 + M6]
    └── Cannonball [M3 ×1, drawn only when in flight]
```

**Critical property:** the barrel's world transform is

```
M_hull · M_deck · M_mount · R_y(az) · R_x(el) · S(barrel)
```

so the barrel inherits hull roll and pitch **for free**, and the muzzle spawn point inherits all of it. This is why the projectile's launch direction is automatically correct with **no special-case code**. Never shortcut this by computing the muzzle position independently of the hierarchy.

### Mesh inventory — 8 unique meshes, everything else is reuse

| # | Mesh | Reused for |
|---|---|---|
| M1 | Unit cube | Hull, deck, cannon mount, enemy hull |
| M2 | Unit cylinder (parameterised segments) | Cannon barrel, masts ×2, yards ×2 |
| M3 | Unit sphere (parameterised stacks/slices) | Cannonball, muzzle flash, smoke, splash |
| M4 | Unit quad | Sails ×2, flag |
| M5 | Ocean grid (subdivided quad) | Ocean surface |
| M6 | Ring / annulus | Splash ring (optional) |
| M7 | Unit cone | Bowsprit tip (optional) |
| M8 | HUD quad | Indicator background (optional) |

---

## 4. Coding Standards & Rules

### Naming conventions

| Kind | Convention | Example |
|---|---|---|
| GLSL uniforms | `u` + PascalCase | `uModel`, `uNormalMatrix`, `uShadingMode`, `uTermMask` |
| GLSL varyings | `v` + PascalCase | `vFragPos`, `vNormal`, `vGouraudColor` |
| GLSL attributes | `a` + PascalCase | `aPos`, `aNormal` |
| C++ types | PascalCase | `Mesh`, `Material`, `Projectile`, `Particle` |
| C++ functions / variables | camelCase | `waveHeight`, `updateAim`, `barrelSegments` |
| Material / tuning constants | SCREAMING_SNAKE | `BRASS`, `MUZZLE_SPEED`, `HIT_RADIUS`, `SLEW` |

### Do

- **Derive every animated quantity from `now = (float)glfwGetTime()`.** Every formula must be a closed form of `t`.
- **Check every shader compile and link log** and print it. Silent shader failures cost hours — wire this up on day one.
- Keep **one** shader program with a `uniform int uShadingMode` branch.
- Compute the normal matrix `glm::mat3(glm::transpose(glm::inverse(model)))` **on the CPU, once per object per frame**.
- Keep an **unscaled "frame" matrix** for every hierarchy node; apply `glm::scale` only in the final `drawMesh` call.
- Use `w = 1` when transforming a **position**, `w = 0` when transforming a **direction / axis**.
- Keep the CPU `waveHeight` / `waveSlopeX` / `waveSlopeZ` constants **identical** to the GLSL constants `A1, K1, W1, A2, K2, W2` and the phase `1.3`.
- Hoist view / projection / light uniforms **outside** the per-object loop — set once per frame.
- Use fixed-size particle pools (`Particle smokePool[4]`, `splashPool[4]`); position, scale, and alpha are pure functions of `age`.
- Keep `computeLighting` and the shared uniform block in **one** C++ string constant prepended to both shader sources — GLSL has no `#include`, and there must be exactly one copy of the lighting math.
- Cite the lecture slide in a comment wherever a formula comes from one (e.g. `// L8 slide 21: 1/(a0 + a1*d + a2*d^2)`). These comments are graded report evidence — never strip them.

### Do NOT

- ❌ **No pre-computed animation.** Never store an array of positions indexed by frame number. Only `age` counters and the current firing state may persist between frames.
- ❌ **No parent scale leaking to children.** The single most common bug: scaling the hull 4× and passing that matrix to the mast turns the mast into a 4×-stretched noodle.
- ❌ **No writing three shader programs** for the three shading modes. One program, one branch — cheaper, and it is what makes the "optimized" claim defensible.
- ❌ **No `new` / `malloc` inside the render loop.** Zero allocation after startup.
- ❌ **No mesh regeneration per frame.** Rebuild only on `+` / `−` keypress.
- ❌ **No textures, normal maps, skyboxes, HDR, bloom, or post-processing.** They hide the shading model being graded.
- ❌ **No shadow mapping.** Deliberately out of scope — this is a *local* illumination model (L8 slides 10–11).
- ❌ **No recreating specific AC:BF / PotC ships, characters, logos, or UI.** IP risk. Build a generic, original three-masted hull.

### Rejected features (scope protection — PRD §3.2)

If you find yourself building one of these, **stop**:

| Rejected | Why |
|---|---|
| Multiple enemy ships / fleet combat | Duplicate objects, no new concept demonstrated |
| Simultaneous multi-cannon broadside | One cannon demonstrates the gimbal; N cannons demonstrate copy-paste |
| Damage states, health bars, sinking | Game logic, zero graphics concepts |
| Buoyancy physics / Gerstner or FFT ocean | Simulation research; a sine-sum surface teaches the same normals lesson |
| Enemy AI, pathfinding, evasive maneuvers | Game AI, zero graphics concepts |
| Boarding, crew, characters, animation rigs | Skeletal animation is a separate course |

### Hard scope ceiling

- **1** player ship, **1** enemy ship, **1** cannon, **1** cannonball in flight at a time
- **≤ 8** unique meshes
- **≤ 20** draw calls per frame
- **2** light sources
- **≤ 25k** triangles at default tessellation; 60 FPS at 1280×720 on integrated graphics

---

## 5. Workflow & Testing

### Build — VS Code is the development environment

| Action | How |
|---|---|
| Configure | `Ctrl+Shift+P` → **CMake: Configure** |
| Build | `F7` |
| Run | `Shift+F5` |
| Debug | `F5` |
| Clean rebuild | `Ctrl+Shift+P` → **CMake: Delete Cache and Reconfigure** |

Terminal equivalent, when the extension is not wanted:

```bash
cmake -S . -B build
cmake --build build
```

Shaders are copied next to the executable by a `POST_BUILD` custom command, so relative paths (`shaders/phong.vert`) resolve from the build output directory. **This is why `.vscode/launch.json` must set `"cwd": "${command:cmake.launchTargetDirectory}"`** — the default workspace-root cwd makes every shader load fail.

**Required extensions:** `ms-vscode.cpptools` (IntelliSense + debugger), `ms-vscode.cmake-tools` (configure/build/debug; also supplies IntelliSense config via `C_Cpp.default.configurationProvider`), `slevesque.shader` (GLSL highlighting, optional).

**Windows toolchain:** **Build Tools for Visual Studio 2022** with the "Desktop development with C++" workload — the compiler only, **not** the Visual Studio IDE. CMake on PATH. In **CMake: Select a Kit**, pick the **amd64** kit; an `x86` kit produces the classic `glfw3.lib` linker error. Libraries: 64-bit GLFW from `glfw.org/download` (use `lib-vc2022/glfw3.lib` — MSVC ABI, which is why MinGW is not the recommended path), GLAD from `glad.dav1d.de` (C/C++, OpenGL, gl 3.3, Core, *Generate a loader*), GLM from `github.com/g-truc/glm/releases`.

**Linux toolchain:**

```bash
sudo apt update
sudo apt install build-essential cmake libglfw3-dev libglm-dev
```

Same VS Code extensions; select the **GCC** kit. In `launch.json` use `"type": "cppdbg"` with `"MIMode": "gdb"` instead of `cppvsdbg`.

### Testing model — visual checkpoints, not a unit test suite

There is no test framework. Each phase ends with a **✅ DONE WHEN** checkpoint verifiable by eye in under 30 seconds. **Do not start phase N+1 until phase N's checkpoint passes.**

| Phase | Status | ✅ DONE WHEN |
|---|---|---|
| 0 | **✅ DONE** | Window opens with a solid clear colour, closes cleanly on ESC |
| 1 | ← next | Loop runs at a steady framerate; `dt` prints ~0.016 at 60 FPS |
| 2 |  | Coloured triangle appears; a deliberate shader typo prints a readable compile error |
| 3 |  | Camera orbits a cube in 3D; no stretching, sane near/far clipping |
| 4 |  | Sphere, cylinder, cube render as solid silhouettes; segment counts visibly change polygon count in wireframe |
| **5 🎯 M1** |  | Brass sphere under one directional light: bright side, dark side, and a **specular highlight that moves as you orbit**. `1`/`2`/`3` give three visibly different results |
| 6 |  | Brass / Polished Silver / Sailcloth spheres side by side look clearly different |
| 7 |  | Ship looks like a ship; hardcoding `shipMatrix = rotate(45°)` carries masts, sails, cannon, and barrel with it |
| **8 🎯 M2** |  | Ocean ripples, ship rides it convincingly, specular sun-streak glitters on the moving water |
| 9 |  | Sails and flag move independently; the scene has life with no shot fired |
| 10 |  | Two ships, both rocking correctly, one patrolling laterally |
| 11 |  | Cannon smoothly tracks the enemy; manual mode responds to arrow keys |
| **12 🎯 M3** |  | SPACE launches a ball that visibly arcs under gravity along the barrel's true direction; firing at different roll points produces different trajectories |
| 13 |  | Good aim → HIT, short aim → SPLASH, HUD reports which |
| 14 |  | Repeated firing produces smoke and splashes; 50 shots never allocate or break |
| **15 🎯 M4** |  | The entire 3-minute demo script (PRD §16.2) runs without touching code |
| 16 |  | A grader can read the current shading mode and aim angles without asking |
| 17 |  | Optimization checklist measured and recorded |

**The project is submittable at M3 (Phase 12).** Everything after that raises the grade; M3 already meets every mandatory requirement except the shading-comparison bonus.

### Mandatory testing order for ballistics (Phase 12 — do not skip)

1. **Ship stationary, target stationary.** Fire straight ahead — does it arc?
2. **Ship rocking, target stationary.** Fire at the top vs. the bottom of a roll — the arcs must differ. *This proves the hierarchy feeds the ballistics.*
3. **Everything moving.** Only now enable the patrol.

### Debugging techniques

| Symptom | Technique |
|---|---|
| Lighting looks flat or wrong | Temporarily `FragColor = vec4(N * 0.5 + 0.5, 1.0)` to view normals as RGB. A correct wave surface shows smooth colour bands rolling across it; a flat single colour means normals are not being recomputed. |
| Ball fires sideways or toward the origin | Render a bright debug line from `muzzlePos` along `forward` for 5 units before firing anything. Then check the `w` component. |
| Highlight does not move when orbiting | `uViewPos` is not being updated — specular is view-dependent, that is the whole point. |
| Gouraud looks identical to Phong | Needs **low tessellation + high `n_s`**. Test the ocean at 8×8 with `n_s` = 160. |
| `gladLoadGLLoader failed` | A GL call happened before `gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)`. |
| Black screen, no errors | Missing `glfwSwapBuffers(window)` in the loop. |
| Shaders not found at runtime | Working directory is wrong. `.vscode/launch.json` needs `"cwd": "${command:cmake.launchTargetDirectory}"`, because `POST_BUILD` copies `shaders/` next to the exe. |
| `glfw3.lib` linker error | Wrong CMake kit — reselect the **amd64** kit, not `x86`. |
| Red squiggles on `<glm/glm.hpp>` while the build succeeds | IntelliSense only. Run **CMake: Configure**; the build is the source of truth. |
| Children detach from their parent | Hierarchy matrix order. Build the static hierarchy (Phase 7) before any animation and verify by manually rotating the hull. |
| Verifying polygon count | `glPolygonMode(GL_FRONT_AND_BACK, GL_LINE)` |

### Validation checklist before declaring work done (Phase 17)

- [ ] Unique meshes ≤ 8; reuse table documented
- [ ] Draw calls per frame ≤ 20 (add a counter and print it)
- [ ] `glEnable(GL_CULL_FACE)` and `glEnable(GL_DEPTH_TEST)` are on
- [ ] View / projection / light uniforms set **once per frame**, not per object
- [ ] Meshes rebuilt only on `+` / `−`, never in the loop
- [ ] Cannonball and muzzle flash skipped when inactive
- [ ] No `new` / `malloc` inside the render loop
- [ ] FPS measured (`1.0/dt` averaged) at default and max tessellation

### Deliverables

1. Source code (`/src`, `/shaders`, `CMakeLists.txt`)
2. Compiled executable
3. Report containing: the L8/L9 traceability matrix, the illumination equation as implemented, the material table with cited slide-60 values, a Flat/Gouraud/Phong screenshot triptych, a Mach-banding screenshot, the ocean-highlight comparison, the optimization table with measured numbers, and one paragraph on local vs. global illumination
4. 3-minute demo (PRD §16.2)

### Emergency descope order (cut the last item first)

1. Splash rings, cone/bowsprit, second sail — cosmetic
2. Blinn-Phong toggle (`B`) — bonus only
3. Particle pools → replace with a simple expanding sphere
4. Enemy patrol → make the target stationary (ballistics still works)
5. Auto-track aiming → manual-only is still a full gimbal demo

**Never cut:** the three shading modes, the two lights with attenuation, the 4-level hierarchy, the ballistic projectile, or the material variety. Those are the graded requirements.
