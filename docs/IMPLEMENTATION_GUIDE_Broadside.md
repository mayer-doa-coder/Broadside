# Implementation Guide — "Broadside"
### From zero to a demoable naval gunnery scene, phase by phase

**Read this alongside `PRD_Broadside.md`.** The PRD says *what* and *why*; this document says *how* and *in what order*.

---

## How to Use This Guide

**The order is demo-first, not feature-first.** Each phase ends with a **✅ DONE WHEN** checkpoint you can verify visually in under 30 seconds. Do not start phase N+1 until phase N's checkpoint passes.

Four phases are marked **🎯 MILESTONE** — at each of those you have something you could show your teacher that day.

| Milestone | After phase | What you can demo |
|---|---|---|
| 🎯 M1 | Phase 5 | A lit, shaded 3D object with a visible specular highlight |
| 🎯 M2 | Phase 8 | A ship on an animated ocean, rocking with the waves |
| 🎯 M3 | Phase 12 | **Firing a cannon with real ballistics — the core project** |
| 🎯 M4 | Phase 15 | The full L8/L9 demonstration suite (the grading centrepiece) |

**If you run out of time, you can submit at M3 and still meet every mandatory requirement** except the shading-comparison bonus. Plan backwards from that.

---

# PHASE 0 — Toolchain Setup ✅ COMPLETE

**Goal:** compile and run an empty OpenGL window.
**Time:** half a day. Do not underestimate this; setup is where most students lose their first week.

> **✅ Checkpoint passed.** Verified on Windows 11 / MSVC Build Tools 2022 (VC 14.44, x64) / CMake 4.4.2 / GLFW 3.5.1 / GLAD gl=3.3 core / GLM 1.0.3.
> `broadside.exe` opens a 1280×720 window titled "Broadside" with a solid clear colour, closes cleanly on ESC with exit code 0, and reports:
> ```
> OpenGL   : 3.3.0 NVIDIA 596.36
> GLSL     : 3.30 NVIDIA via Cg compiler
> Renderer : NVIDIA GeForce RTX 5050 Laptop GPU/PCIe/SSE2
> ```
> **Next: Phase 1** — restructure `main()` into the timed render loop (`now` / `dt`, `processInput`, `updateScene`, `renderScene`) and enable depth test + back-face culling.

## 0.1 What You Need

| Component | Purpose | Why this one |
|---|---|---|
| **OpenGL 3.3 Core** | The API | Core profile forces modern shader pipeline — required for per-fragment Phong |
| **GLFW** | Window + input | Simplest cross-platform windowing |
| **GLAD** | Function loader | Loads GL function pointers at runtime |
| **GLM** | Math (vec/mat) | Header-only, mirrors GLSL syntax exactly |
| **CMake** | Build system | Makes your project portable and easy to submit |
| **VS Code** | Editor / IDE | Lightweight; with two extensions it configures, builds, runs, and debugs the project without leaving the window |
| **MSVC Build Tools 2022** (Windows) / **GCC** (Linux) | Compiler | VS Code does not compile anything itself — this is the actual toolchain behind it |

> **Note on "OpenGL only":** GLFW/GLAD/GLM are *not* graphics engines — they are a window, a function loader, and a math header. All rendering is your own OpenGL calls and your own GLSL. This satisfies "OpenGL only." Do **not** add Unity, Unreal, Ogre, Bullet, or any physics/scene-graph library.

> **Why not old-style `glBegin`/`glEnd` + `glLight`?** L9 slide 36 explicitly says Phong shading is *"Not supported in OpenGL"* — meaning the fixed-function pipeline. Writing your own GLSL is precisely how you earn the Phong marks. Fixed-function can only do Gouraud.

## 0.2 Windows (Visual Studio **Code**) — Recommended

> **VS Code is an editor, not a compiler.** You do not need the Visual Studio IDE, but you *do* need a C++ toolchain behind VS Code. On Windows the right one is **MSVC**, installed on its own via Build Tools — roughly 2–3 GB, no IDE.

### Step 0 — Check what you already have

Run this in PowerShell before installing anything:

```powershell
# C++ toolchain?
$vsw = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (Test-Path $vsw) { & $vsw -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property displayName }
else { "NO MSVC toolchain installed" }

# CMake? VS Code? extensions?
cmake --version
code --list-extensions | Select-String "cmake-tools|cpptools$|shader"
```

You need a non-empty result from the first block, a CMake version, and both `ms-vscode.cpptools` and `ms-vscode.cmake-tools`.

> ⚠️ **An existing `gcc` on PATH does not mean you are ready.** Check `gcc -dumpmachine`: if it prints `mingw32` (the old MinGW.org distribution, typically GCC 6.x at `C:\MinGW`), it is **32-bit only** and predates C++17 — `set(CMAKE_CXX_STANDARD 17)` in §0.5 will fail with *"CMake does not know the compile flags to use to enable CXX17"*, and no pre-built 64-bit GLFW library will link against it. Install MSVC as below and simply never select the GCC kit.

### Step 1 — Install the compiler (no IDE)

**With winget (fastest):**

```powershell
winget install --id Microsoft.VisualStudio.2022.BuildTools --override "--passive --wait --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
```

The `--add Microsoft.VisualStudio.Workload.VCTools` part is **not optional** — without it winget installs an empty Build Tools shell containing no compiler at all.

**Or via the GUI:**

Download **Build Tools for Visual Studio 2022** from `visualstudio.microsoft.com/downloads` (under *Tools for Visual Studio*) and select the **"Desktop development with C++"** workload. This installs the MSVC compiler, the linker, and the Windows SDK — and nothing else.

> **Why MSVC and not MinGW/MSYS2?** GLFW's pre-compiled Windows download ships a separate `.lib` per compiler (`lib-vc2022`, `lib-mingw-w64`, …). The `CMakeLists.txt` in §0.5 links `external/glfw/lib-vc2022/glfw3.lib`, which is an **MSVC** library. Staying on MSVC means the build file in this guide works unmodified. If you deliberately choose MinGW instead, you must point the `if(WIN32)` branch of §0.5 at `lib-mingw-w64/libglfw3.a` and link `gdi32` as well — solvable, but it is unpaid setup work in a project where §0 is already the most common place to lose a week.

**Verify:** re-run the Step 0 snippet — the first block must now print `Visual Studio Build Tools 2022`.

### Step 2 — Install CMake

```powershell
winget install --id Kitware.CMake
```

Close and reopen the terminal (PATH changes do not reach open shells), then `cmake --version`. If it is still not found, install from `cmake.org/download` → Windows x64 installer → **tick "Add CMake to the system PATH"**.

### Step 3 — Install VS Code + three extensions

| Extension | Marketplace ID | Why you need it |
|---|---|---|
| **C/C++** | `ms-vscode.cpptools` | IntelliSense for GLM/GLFW headers, and the debugger |
| **CMake Tools** | `ms-vscode.cmake-tools` | Configure / build / run / debug from the status bar. Auto-detects the MSVC kit and **sets its environment variables for you**, so you never have to open a "Developer Command Prompt". |
| **Shader languages support** | `slevesque.shader` | Syntax highlighting for `.vert` / `.frag`. Optional, but you will be staring at GLSL for the whole project. |

Install from the Extensions pane (`Ctrl+Shift+X`) or:

```bash
code --install-extension ms-vscode.cpptools
code --install-extension ms-vscode.cmake-tools
code --install-extension slevesque.shader
```

### Step 4 — Get the three libraries

4. **GLFW:** pre-compiled 64-bit Windows binaries from `glfw.org/download`. Unzip to `external/glfw/` — you need its `include/` folder and `lib-vc2022/glfw3.lib`.
5. **GLAD:** `glad.dav1d.de` → Language `C/C++`, Specification `OpenGL`, API gl `Version 3.3`, Profile `Core`, tick **Generate a loader** → Generate → download the zip. Its `include/glad/` and `include/KHR/` go to `include/`, and `src/glad.c` goes to `src/`.
6. **GLM:** `github.com/g-truc/glm/releases` (header-only, just unzip to `external/glm/`).

**Scripted equivalent** (no browser; PowerShell). This is how the vendored copies in `external/` were produced:

```powershell
$root = 'd:\Broadside'; $tmp = "$env:TEMP\broadside-deps"
New-Item -ItemType Directory -Force -Path $tmp,"$root\external\glfw","$root\external\glm","$root\include","$root\src" | Out-Null

# GLFW 3.5.1 - pre-built x64 MSVC
Invoke-WebRequest 'https://github.com/glfw/glfw/releases/download/3.5.1/glfw-3.5.1.bin.WIN64.zip' -OutFile "$tmp\glfw.zip"
Expand-Archive "$tmp\glfw.zip" -DestinationPath $tmp -Force
Copy-Item "$tmp\glfw-3.5.1.bin.WIN64\include","$tmp\glfw-3.5.1.bin.WIN64\lib-vc2022" "$root\external\glfw\" -Recurse -Force

# GLM 1.0.3 - header-only
Invoke-WebRequest 'https://github.com/g-truc/glm/releases/download/1.0.3/glm-1.0.3.zip' -OutFile "$tmp\glm.zip"
Expand-Archive "$tmp\glm.zip" -DestinationPath "$tmp\glmx" -Force
Copy-Item "$tmp\glmx\glm\glm" "$root\external\glm\" -Recurse -Force

# GLAD - OpenGL 3.3 Core, loader, NO extensions
pip install --quiet glad
python -m glad --generator=c --spec=gl --api="gl=3.3" --profile=core --extensions="" --out-path="$tmp\gladout"
Copy-Item "$tmp\gladout\include\glad","$tmp\gladout\include\KHR" "$root\include\" -Recurse -Force
Copy-Item "$tmp\gladout\src\glad.c" "$root\src\" -Force
```

The web form at `glad.dav1d.de` is the exact equivalent: Language `C/C++`, Specification `OpenGL`, gl `Version 3.3`, Profile `Core`, Extensions panel left **empty**, and — below the fold — **"Generate a loader" ticked**. Without that checkbox there is no `gladLoadGLLoader`, and the Phase 1 code will not compile.

> **⚠️ `--api="gl=3.3"` and `--extensions=""` are both load-bearing.**
> Generating a higher API declares every 4.x entry point, so calling one compiles silently and resolves at runtime on a permissive desktop driver while failing on the integrated GPU your grader may use. Omitting `--extensions=""` is the same trap by another door: the default pulls in *every* registered extension regardless of API version, and `GL_ARB_direct_state_access` alone re-declares the 4.5 DSA calls such as `glCreateBuffers`.
>
> With both flags set, `glCreateBuffers(...)` is a **compile error** (`C3861: identifier not found`) rather than a portability bug discovered at the demo. The header is ~108 KB; the all-extensions default is ~917 KB.
>
> `pip` may warn that `glad.exe` is not on PATH — harmless, the commands above invoke it as `python -m glad`.

### Step 5 — First configure and build inside VS Code

1. `File → Open Folder…` → select the `broadside/` folder (the one containing `CMakeLists.txt`).
2. `Ctrl+Shift+P` → **CMake: Select a Kit** → choose **`Visual Studio Build Tools 2022 Release - amd64`**.
   ⚠️ It must be **amd64**, not `x86` and not `amd64_x86`. Picking a 32-bit kit is the #1 cause of the `glfw3.lib` linker error in §0.6.
   ⚠️ If you have any MinGW installed, CMake Tools will also list a **GCC** kit here. Do not pick it — see the warning in Step 0.
3. `Ctrl+Shift+P` → **CMake: Configure** (runs once; re-runs automatically when you edit `CMakeLists.txt`).
4. **Build:** `F7`, or click **Build** in the blue status bar at the bottom.
5. **Run:** `Shift+F5` (run without debugging) or `F5` (debug — breakpoints work in `main.cpp` and every header).

The status bar becomes your whole build UI: kit, build type (`Debug`/`Release`), **Build**, **Debug**, **Run**.

> ⚠️ **Build only through CMake Tools.** If you have "Code Runner" (`formulahendry.code-runner`) or "C/C++ Compile Run" (`danielpinto8zz6.c-cpp-compile-run`) installed, ignore their Run/Play buttons for this project. They compile the *single open file* with whatever `gcc` is first on PATH and know nothing about your include paths or the GLFW library — they will fail confusingly on `main.cpp`. `F7` / `F5` are the only build and run commands you need.

## 0.3 Linux (Ubuntu/Debian)

```bash
sudo apt update
sudo apt install build-essential cmake libglfw3-dev libglm-dev
# GLAD still comes from glad.dav1d.de (same settings as above)
```

VS Code works identically here — same three extensions. At **CMake: Select a Kit**, pick the detected **GCC** kit instead of the MSVC one. Everything else in §0.2 Step 5 is unchanged.

## 0.3.1 `.vscode/` — Project Configuration

Create these three files so the project builds, runs, and finds its shaders correctly for anyone who opens the folder.

**`.vscode/extensions.json`** — VS Code will prompt to install these on first open:

```json
{
  "recommendations": [
    "ms-vscode.cpptools",
    "ms-vscode.cmake-tools",
    "slevesque.shader"
  ]
}
```

**`.vscode/settings.json`**:

```json
{
  "cmake.buildDirectory": "${workspaceFolder}/build",
  "cmake.configureOnOpen": true,
  "C_Cpp.default.cppStandard": "c++17",
  "C_Cpp.default.configurationProvider": "ms-vscode.cmake-tools",
  "files.associations": {
    "*.vert": "glsl",
    "*.frag": "glsl"
  }
}
```

> `configurationProvider` is the line that makes IntelliSense work: CMake Tools hands the C/C++ extension the real include paths straight from `CMakeLists.txt`, so `<glad/glad.h>`, `<GLFW/glfw3.h>`, and `<glm/glm.hpp>` all resolve without you hand-maintaining an `includePath` array. If headers show red squiggles, run **CMake: Configure** — not **C/C++: Edit Configurations**.

**`.vscode/launch.json`** — ⚠️ the `cwd` line is the one that matters:

```json
{
  "version": "0.2.0",
  "configurations": [
    {
      "name": "Debug Broadside",
      "type": "cppvsdbg",
      "request": "launch",
      "program": "${command:cmake.launchTargetPath}",
      "args": [],
      "stopAtEntry": false,
      "cwd": "${command:cmake.launchTargetDirectory}",
      "environment": []
    }
  ]
}
```

> **Why `cwd` matters:** the `POST_BUILD` command in §0.5 copies `shaders/` **next to the executable**. Your `Shader` class opens `shaders/phong.vert` as a *relative* path, so the working directory must be the executable's folder. `${command:cmake.launchTargetDirectory}` guarantees that. If you leave the default (`${workspaceFolder}`), the program launches and immediately fails to find its shaders — and if you skipped the compile-log check from Phase 2, it fails **silently** with a black screen.
>
> On Linux, change `"type": "cppvsdbg"` to `"type": "cppdbg"` and add `"MIMode": "gdb"`.

## 0.4 Project Structure

Create this exact layout:

```
broadside/
├── CMakeLists.txt
├── .vscode/                    ← §0.3.1
│   ├── extensions.json
│   ├── settings.json
│   └── launch.json
├── docs/
│   ├── PRD_Broadside.md
│   └── IMPLEMENTATION_GUIDE_Broadside.md
├── include/
│   ├── glad/glad.h
│   └── KHR/khrplatform.h
├── src/
│   ├── glad.c
│   ├── main.cpp
│   ├── Shader.h
│   ├── Mesh.h
│   ├── Camera.h
│   └── Scene.h
├── shaders/
│   ├── phong.vert
│   └── phong.frag
├── external/
│   ├── glm/
│   └── glfw/                   ← Windows only: include/ + lib-vc2022/
└── build/                      ← generated; add to .gitignore
```

## 0.5 CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.10)
project(broadside)
set(CMAKE_CXX_STANDARD 17)

add_executable(broadside
    src/main.cpp
    src/glad.c
)

target_include_directories(broadside PRIVATE
    include
    external/glm
)

find_package(OpenGL REQUIRED)

if(WIN32)
    target_include_directories(broadside PRIVATE external/glfw/include)
    target_link_libraries(broadside
        ${CMAKE_SOURCE_DIR}/external/glfw/lib-vc2022/glfw3.lib
        OpenGL::GL)
else()
    find_package(glfw3 REQUIRED)
    target_link_libraries(broadside glfw OpenGL::GL dl)
endif()

# Copy shaders next to the executable so relative paths work
add_custom_command(TARGET broadside POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/shaders $<TARGET_FILE_DIR:broadside>/shaders)
```

## 0.6 Build

**In VS Code (the normal path):**

| Action | How |
|---|---|
| Configure | `Ctrl+Shift+P` → **CMake: Configure** |
| Build | `F7`, or **Build** in the status bar |
| Run | `Shift+F5` |
| Debug (breakpoints) | `F5` |
| Change Debug ↔ Release | Click the build-variant button in the status bar |
| Clean rebuild | `Ctrl+Shift+P` → **CMake: Delete Cache and Reconfigure** |

**From a terminal** (VS Code's integrated terminal works — ``Ctrl+` ``), if you ever want to bypass the extension:

```bash
cmake -S . -B build
cmake --build build
```

**✅ DONE WHEN:** a window opens with a solid coloured background (`glClearColor`) and closes cleanly on ESC. Nothing else.

**Common failures:**
- *"gladLoadGLLoader failed"* — you called a GL function before `gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)`.
- *Black screen, no errors* — you forgot `glfwSwapBuffers(window)` in the loop.
- *Linker errors on `glfw3.lib`* — architecture mismatch. Make sure you downloaded the 64-bit GLFW binaries **and** selected the **amd64** kit (`Ctrl+Shift+P` → **CMake: Select a Kit**). An `x86` kit produces exactly this error.
- *Shaders fail to load at runtime / black screen after Phase 2* — the working directory is wrong. Check the `cwd` line in `.vscode/launch.json` (§0.3.1).
- *"No kits found"* — the C++ Build Tools are not installed, or CMake Tools has not rescanned. Run `Ctrl+Shift+P` → **CMake: Scan for Kits**.
- *Red squiggles under `<glm/glm.hpp>` but the build succeeds* — IntelliSense only; run **CMake: Configure** to refresh it. The build is the source of truth, not the squiggles.

---

# PHASE 1 — The Render Loop Skeleton ✅ COMPLETE

**Goal:** a correctly structured, continuously running loop (Requirement 3).

> **✅ Checkpoint passed.** A five-minute test produced a steady v-synced frame time (`dt` 0.0068–0.0070 s, average 144.1 FPS). Pause, resume, resize, minimize, restore, and `ESC` all worked. The program exited with code 0 and printed no runtime errors. Depth testing and back-face culling are enabled. Debug and Release both build. Debug has one non-fatal `LNK4098` warning from the pre-built GLFW library; Release and the strict `/W4` source check are warning-free. See [PHASE_1_EXPLANATION.md](PHASE_1_EXPLANATION.md).

```cpp
int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Broadside", NULL, NULL);
    glfwMakeContextCurrent(window);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);          // optimization: cull back faces

    float lastFrame = 0.0f;

    while (!glfwWindowShouldClose(window)) {
        float now = (float)glfwGetTime();   // ← ALL animation derives from this
        float dt  = now - lastFrame;
        lastFrame = now;

        processInput(window, dt);
        updateScene(now, dt);               // all motion computed here
        
        glClearColor(0.35f, 0.45f, 0.55f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        renderScene(now);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    glfwTerminate();
}
```

**The golden rule for Requirement 12:** every animated quantity must be derivable from `now`. If you ever write an array of positions indexed by frame number, you have violated "no pre-computed animation."

**✅ DONE WHEN:** the loop runs at a steady framerate and `dt` prints sensible values (~0.016 at 60 FPS).

---

# PHASE 2 — Shader Loading + First Triangle ✅ COMPLETE

**Goal:** get GLSL compiling and a triangle on screen.

> **✅ Checkpoint passed.** `src/Shader.h` compiles and links `shaders/phong.vert` and `shaders/phong.frag`, checks both logs, and draws a coloured triangle. Captured running frames change colour, while the paused triangle stays visually unchanged. Vertex errors, fragment errors, link errors, and missing files print clear messages and exit with code `-1`. All four uniform setters are used. Debug and Release build successfully, and the strict `/W4` source build is warning-free. See [PHASE_2_EXPLANATION.md](PHASE_2_EXPLANATION.md).

Write a minimal `Shader` class: read the `.vert`/`.frag` files, `glCreateShader`, `glCompileShader`, **check the compile log** (do this from day one — silent shader failures will cost you hours), `glCreateProgram`, `glLinkProgram`, and helper setters (`setMat4`, `setVec3`, `setFloat`, `setInt`).

**✅ DONE WHEN:** a coloured triangle appears, and deliberately introducing a typo into the shader prints a readable compile error.

---

# PHASE 3 — Camera + MVP Matrices ✅ COMPLETE

**Goal:** a 3D perspective view you can orbit.

> **✅ Checkpoint passed.** `src/Camera.h` holds a spherical orbit camera (radius / yaw / pitch) with drag, wheel and W/S zoom. Verified against the perspective formula by framebuffer readback at seven camera settings: the face-on cube measures exactly 248x248 px in a 16:9 window (predicted 248.3) and 310x310 after a live resize to 600x900 (predicted 310.4), so there is no stretching. Pitch clamps at 89°, radius at 1.2, the press-latch prevents view snap, and the camera keeps working while `P` is paused. See [PHASE_3_EXPLANATION.md](PHASE_3_EXPLANATION.md).

```cpp
glm::mat4 projection = glm::perspective(glm::radians(45.0f),
                                        (float)W/(float)H, 0.1f, 300.0f);
glm::mat4 view = glm::lookAt(cameraPos, cameraTarget, glm::vec3(0,1,0));
glm::mat4 model = glm::mat4(1.0f);
```

Implement an **orbit camera**: keep `radius`, `yaw`, `pitch`; compute position with spherical coordinates. Bind mouse drag to yaw/pitch and scroll to radius. This matters more than it sounds — your grader will want to move the camera to inspect specular highlights from different angles, and specular is view-dependent by definition.

**✅ DONE WHEN:** you can orbit around a cube in 3D and it looks correct (no stretching, near/far clipping sane).

---

# PHASE 4 — Mesh Library (Do This Properly Once)

**Goal:** generate all 8 primitives with **correct normals**, parameterised by tessellation.

This phase is the foundation for everything. Do it carefully.

```cpp
struct Vertex { glm::vec3 position; glm::vec3 normal; };

class Mesh {
    unsigned int VAO, VBO, EBO;
    int indexCount;
public:
    Mesh(const std::vector<Vertex>& verts, const std::vector<unsigned>& idx);
    void draw() const;   // glBindVertexArray + glDrawElements
};
```

## 4.1 Meshes to Generate

| Function | Notes |
|---|---|
| `makeCube()` | 24 vertices (4 per face) so each face gets its own flat normal |
| `makeCylinder(int segments)` | **Parameterised** — this is your tessellation demo object |
| `makeSphere(int stacks, int slices)` | **Parameterised** — analytic normal = `normalize(position)` |
| `makeQuad()` | 4 vertices, normal `(0,0,1)` |
| `makeGrid(int N)` | **Parameterised** — the ocean; flat at generation, displaced in the shader |
| `makeCone(int segments)` | Optional |
| `makeRing(int segments)` | Optional, splash effect |

## 4.2 Normals — The L9 Connection

For **analytic surfaces** the exact normal is known:
- Sphere: `normal = normalize(position)`
- Cylinder side: `normal = normalize(vec3(x, 0, z))`
- Cylinder caps: `(0, ±1, 0)`

For **non-analytic** meshes, implement the L9 slide 20 averaging formula so you can cite it:

```cpp
// N_v = (Σ N_i) / ||Σ N_i||        — L9 slide 20
void computeSmoothNormals(std::vector<Vertex>& v,
                          const std::vector<unsigned>& idx) {
    for (auto& vert : v) vert.normal = glm::vec3(0.0f);
    for (size_t i = 0; i < idx.size(); i += 3) {
        glm::vec3 a = v[idx[i]].position;
        glm::vec3 b = v[idx[i+1]].position;
        glm::vec3 c = v[idx[i+2]].position;
        glm::vec3 faceN = glm::normalize(glm::cross(b - a, c - a));
        v[idx[i]].normal   += faceN;      // accumulate Σ N_i
        v[idx[i+1]].normal += faceN;
        v[idx[i+2]].normal += faceN;
    }
    for (auto& vert : v) vert.normal = glm::normalize(vert.normal);  // divide by ||Σ||
}
```

**Put this function in your report.** It is a direct implementation of a lecture slide.

**✅ DONE WHEN:** a sphere, cylinder, and cube all render as solid silhouettes, and you can pass different segment counts and see the polygon count change (render in wireframe with `glPolygonMode(GL_FRONT_AND_BACK, GL_LINE)` to verify).

---

# PHASE 5 — 🎯 MILESTONE 1: The Phong Shader

**Goal:** one lit object with a visible, moving specular highlight. **This is the heart of your grade — build it early and build it right.**

## 5.1 `phong.vert`

```glsl
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;

uniform mat4 uModel, uView, uProjection;
uniform mat3 uNormalMatrix;      // (M^-1)^T — correct under non-uniform scale
uniform int  uShadingMode;       // 0=Flat, 1=Gouraud, 2=Phong

// ---- lighting uniforms (shared with fragment shader) ----
struct Light {
    int   type;            // 0 = directional, 1 = point
    vec3  direction;       // for directional:  L = -direction
    vec3  position;        // for point
    vec3  diffuse, specular;
    vec3  attenuation;     // (a0, a1, a2)  — L8 slide 21
    float enabled;
};
uniform Light uLights[2];
uniform vec3  uGlobalAmbient;
uniform vec3  uViewPos;
uniform vec3  uKa, uKd, uKs, uEmission;
uniform float uShininess;
uniform int   uUseBlinn;
uniform int   uTermMask;         // bit 0 ambient, 1 diffuse, 2 specular

out vec3 vFragPos;
out vec3 vNormal;
out vec3 vGouraudColor;

vec3 computeLighting(vec3 N, vec3 fragPos);   // shared function, see frag shader

void main() {
    vFragPos = vec3(uModel * vec4(aPos, 1.0));
    vNormal  = uNormalMatrix * aNormal;

    // ---- GOURAUD: evaluate illumination HERE, at the vertex (L9 slide 19) ----
    if (uShadingMode == 1)
        vGouraudColor = computeLighting(normalize(vNormal), vFragPos);

    gl_Position = uProjection * uView * vec4(vFragPos, 1.0);
}
```

## 5.2 `phong.frag` — The Core

```glsl
#version 330 core
in vec3 vFragPos;
in vec3 vNormal;
in vec3 vGouraudColor;
out vec4 FragColor;

// ... same uniform block as vertex shader ...

// ===== The L8 illumination model =====
vec3 computeLighting(vec3 N, vec3 fragPos) {
    vec3 V = normalize(uViewPos - fragPos);

    // Ambient — L8 slide 26 (no direction, no position dependence)
    vec3 result = vec3(0.0);
    if ((uTermMask & 1) != 0) result += uKa * uGlobalAmbient;
    result += uEmission;                          // L8 slide 55

    for (int i = 0; i < 2; ++i) {                 // L8 slide 56: sum the lights
        if (uLights[i].enabled < 0.5) continue;

        vec3  L;
        float attenuation = 1.0;

        if (uLights[i].type == 0) {
            L = normalize(-uLights[i].direction);         // L8 slide 19
        } else {
            vec3 toLight = uLights[i].position - fragPos;
            float d = length(toLight);
            L = toLight / d;
            // Radial attenuation — L8 slide 21: 1/(a0 + a1*d + a2*d^2)
            vec3 a = uLights[i].attenuation;
            attenuation = 1.0 / (a.x + a.y * d + a.z * d * d);
        }

        // Diffuse — Lambert, L8 slide 32
        float NdotL = max(dot(N, L), 0.0);
        if ((uTermMask & 2) != 0)
            result += attenuation * uKd * uLights[i].diffuse * NdotL;

        // Specular — L8 slide 44 / 49
        if (NdotL > 0.0 && (uTermMask & 4) != 0) {
            float spec;
            if (uUseBlinn == 1) {
                vec3 H = normalize(L + V);                // Blinn-Phong, L8 s49
                spec = pow(max(dot(N, H), 0.0), uShininess);
            } else {
                vec3 R = reflect(-L, N);                  // R = 2(N·L)N - L, L8 s47
                spec = pow(max(dot(R, V), 0.0), uShininess);
            }
            result += attenuation * uKs * uLights[i].specular * spec;
        }
    }
    return result;
}

void main() {
    vec3 color;

    if (uShadingMode == 0) {
        // ---- FLAT: one normal per face (L9 slides 9-16) ----
        vec3 faceN = normalize(cross(dFdx(vFragPos), dFdy(vFragPos)));
        color = computeLighting(faceN, vFragPos);
    }
    else if (uShadingMode == 1) {
        // ---- GOURAUD: interpolated INTENSITY from the vertex shader (L9 s19-21) ----
        color = vGouraudColor;
    }
    else {
        // ---- PHONG: interpolated NORMAL, lighting per fragment (L9 s30-32) ----
        color = computeLighting(normalize(vNormal), vFragPos);
    }

    FragColor = vec4(color, 1.0);
}
```

> **Implementation note:** GLSL has no `#include`. Keep `computeLighting` and the uniform block in a plain C++ string constant and prepend it to both shader sources at load time, so there is exactly one copy of the lighting math.

## 5.3 The Normal Matrix

```cpp
glm::mat3 normalMatrix = glm::mat3(glm::transpose(glm::inverse(model)));
```
Compute this **on the CPU, once per object per frame**. It keeps normals correct under non-uniform scaling — which you will use heavily (the hull is a stretched cube). Mention it in your report; it is an advanced detail that costs one line.

**✅ DONE WHEN 🎯 M1:** a sphere with brass material under one directional light shows a clear bright side, dark side, and a **specular highlight that moves as you orbit the camera**. Press `1`/`2`/`3` and see three visibly different results.

**If the highlight doesn't move when you orbit,** you forgot to update `uViewPos` — specular is view-dependent, that's the whole point.

---

# PHASE 6 — Materials and the Second Light

**Goal:** satisfy L8's material table and multi-light requirements.

```cpp
struct Material {
    glm::vec3 ka, kd, ks, emission;
    float shininess;
};

// Values verbatim from L8 slide 60
const Material BRASS = {
    {0.329412f, 0.223529f, 0.027451f},
    {0.780392f, 0.568627f, 0.113725f},
    {0.992157f, 0.941176f, 0.807843f},
    {0,0,0}, 27.8974f };

const Material POLISHED_SILVER = {
    {0.23125f, 0.23125f, 0.23125f},
    {0.2775f,  0.2775f,  0.2775f},
    {0.773911f,0.773911f,0.773911f},
    {0,0,0}, 89.6f };

const Material BLACK_PLASTIC = {
    {0.0f, 0.0f, 0.0f},
    {0.01f,0.01f,0.01f},
    {0.5f, 0.5f, 0.5f},
    {0,0,0}, 32.0f };

// Tuned materials
const Material OCEAN     = {{0.02,0.05,0.08},{0.06,0.14,0.20},{0.90,0.94,0.98},{0,0,0},160.0f};
const Material HULL_WOOD = {{0.12,0.08,0.05},{0.38,0.25,0.15},{0.15,0.12,0.10},{0,0,0},  8.0f};
const Material SAILCLOTH = {{0.20,0.19,0.17},{0.75,0.73,0.68},{0.04,0.04,0.04},{0,0,0},  4.0f};
```

Set up the two lights from PRD §11.1. Hoist view/projection/light uniforms **outside** the per-object loop.

**✅ DONE WHEN:** three spheres side by side with Brass / Polished Silver / Sailcloth materials look clearly different — the silver has a tiny sharp highlight, the brass a medium warm one, the sailcloth almost none. That single screenshot goes straight into your report as your `n_s` comparison.

---

# PHASE 7 — Static Ship Hierarchy (No Animation Yet)

**Goal:** build the transform chain and verify it before adding motion.

```cpp
void drawShip(const glm::mat4& shipMatrix, float t) {
    // Level 1: hull
    glm::mat4 hull = shipMatrix * glm::scale(glm::mat4(1), {1.2f, 0.6f, 4.0f});
    drawMesh(cubeMesh, hull, HULL_WOOD);

    // Level 2: deck (child of hull's frame, not of hull's scale!)
    glm::mat4 deck = shipMatrix * glm::translate(glm::mat4(1), {0, 0.35f, 0});

    // Level 3: cannon mount
    glm::mat4 mount = deck * glm::translate(glm::mat4(1), {0.6f, 0.0f, 0.5f});
    drawMesh(cubeMesh, mount * glm::scale(glm::mat4(1), {0.3f,0.2f,0.3f}), BLACK_PLASTIC);

    // Level 4: yoke (azimuth) then barrel (elevation) — COMPOSITE
    glm::mat4 yoke   = mount * glm::rotate(glm::mat4(1), azimuth,   glm::vec3(0,1,0));
    glm::mat4 barrel = yoke  * glm::rotate(glm::mat4(1), elevation, glm::vec3(1,0,0));
    drawMesh(cylinderMesh,
             barrel * glm::scale(glm::mat4(1), {0.08f, 0.08f, 0.9f}), BRASS);

    // Masts / yards / sails — same pattern
    // ...
}
```

**⚠️ The single most common bug:** applying a parent's *scale* to its children. Keep an unscaled "frame" matrix for each node and apply `scale` only in the final `drawMesh` call, as above. If you scale the hull to 4× length and pass that to the mast, your mast becomes a 4×-stretched noodle.

**✅ DONE WHEN:** the ship looks like a ship, and temporarily hardcoding `shipMatrix = rotate(45°)` visibly carries the masts, sails, cannon, and barrel with it.

---

# PHASE 8 — 🎯 MILESTONE 2: Ocean Waves + Ship Rocking

**Goal:** a living sea and a ship that responds to it.

## 8.1 Wave in the Vertex Shader (GPU-side — optimization)

Add to `phong.vert`:

```glsl
uniform int   uIsOcean;
uniform float uTime;
const float A1 = 0.22, K1 = 0.55, W1 = 1.1;
const float A2 = 0.14, K2 = 0.85, W2 = 1.7;

// ... inside main(), before computing vFragPos:
vec3 pos = aPos;
vec3 nrm = aNormal;
if (uIsOcean == 1) {
    pos.y = A1 * sin(K1 * pos.x + W1 * uTime)
          + A2 * sin(K2 * pos.z + W2 * uTime + 1.3);
    // Analytic normal from the partial derivatives
    float dydx = A1 * K1 * cos(K1 * pos.x + W1 * uTime);
    float dydz = A2 * K2 * cos(K2 * pos.z + W2 * uTime + 1.3);
    nrm = normalize(vec3(-dydx, 1.0, -dydz));
}
```

**Debug tip:** temporarily output `FragColor = vec4(N * 0.5 + 0.5, 1.0)` to visualise normals as RGB. A correct wave surface shows smooth colour bands rolling across it. If it's a flat single colour, your normals aren't being recomputed.

## 8.2 Matching CPU Function for Ship Rocking

```cpp
// MUST match the GLSL constants exactly
float waveHeight(float x, float z, float t) {
    return 0.22f*sinf(0.55f*x + 1.1f*t) + 0.14f*sinf(0.85f*z + 1.7f*t + 1.3f);
}
float waveSlopeX(float x, float t) { return 0.22f*0.55f*cosf(0.55f*x + 1.1f*t); }
float waveSlopeZ(float z, float t) { return 0.14f*0.85f*cosf(0.85f*z + 1.7f*t + 1.3f); }

glm::mat4 shipMatrix(glm::vec3 pos, float t, bool hierarchyEnabled) {
    float y     = waveHeight(pos.x, pos.z, t);
    float roll  = atanf(waveSlopeX(pos.x, t)) * 0.8f;
    float pitch = atanf(waveSlopeZ(pos.z, t)) * 0.8f;
    if (!hierarchyEnabled) { y = pos.y; roll = 0; pitch = 0; }   // 'H' key demo

    glm::mat4 M = glm::translate(glm::mat4(1), {pos.x, y, pos.z});
    M = glm::rotate(M, roll,  glm::vec3(0,0,1));
    M = glm::rotate(M, pitch, glm::vec3(1,0,0));
    return M;
}
```

**✅ DONE WHEN 🎯 M2:** the ocean ripples, the ship rides it convincingly, and the specular sun-streak glitters on the moving water. **This already looks impressive** — a good moment to show your teacher for early feedback.

---

# PHASE 9 — Idle Rigging Animation

**Goal:** continuous ambient motion so the scene is never static.

```cpp
float sailFlutter = 0.10f * sinf(2.2f * t + phaseOffset);
glm::mat4 sail = yard * glm::rotate(glm::mat4(1), sailFlutter, glm::vec3(0,0,1));
```

Give each sail a different `phaseOffset` so they don't move in lockstep.

**✅ DONE WHEN:** sails and flag move independently and the scene has visible life even with no shot fired.

---

# PHASE 10 — Enemy Ship on Patrol

**Goal:** a moving target. Reuse everything.

```cpp
glm::vec3 enemyPos(float t) {
    return glm::vec3(sinf(0.25f * t) * 14.0f, 0.0f, -18.0f);
}
```

Call the **same** `drawShip()` with a different matrix, slightly different scale, and a different hull tint. Zero new meshes — note this in your optimization section.

**✅ DONE WHEN:** two ships, both rocking correctly, one patrolling laterally.

---

# PHASE 11 — Two-Axis Cannon Aiming

**Goal:** the gimbal (same technique as a radar dish mount).

```cpp
void updateAim(float dt, const glm::mat4& shipM, glm::vec3 enemyWorld) {
    if (autoTrack) {
        // Bring the target into the SHIP'S local space — the platform is moving
        glm::vec3 local = glm::vec3(glm::inverse(shipM) * glm::vec4(enemyWorld, 1.0f));
        glm::vec3 d = glm::normalize(local - MOUNT_LOCAL_POS);

        float targetAz = atan2f(d.x, d.z);
        float targetEl = asinf(glm::clamp(d.y, -1.0f, 1.0f)) + BALLISTIC_LIFT;
        targetEl = glm::clamp(targetEl, glm::radians(-5.0f), glm::radians(45.0f));

        // Rate-limited slew — makes the motion visibly mechanical
        azimuth   += glm::clamp(targetAz - azimuth,   -SLEW*dt, SLEW*dt);
        elevation += glm::clamp(targetEl - elevation, -SLEW*dt, SLEW*dt);
    } else {
        if (keyLeft)  azimuth   -= SLEW * dt;
        if (keyRight) azimuth   += SLEW * dt;
        if (keyUp)    elevation += SLEW * dt;
        if (keyDown)  elevation -= SLEW * dt;
        elevation = glm::clamp(elevation, glm::radians(-5.0f), glm::radians(45.0f));
    }
}
```

**✅ DONE WHEN:** the cannon smoothly tracks the patrolling enemy, and manual mode responds to arrow keys. Watch the brass highlight sweep along the barrel as it turns — that's your specular demo working.

---

# PHASE 12 — 🎯 MILESTONE 3: Ballistics (The Core)

**Goal:** fire a real projectile. **This is the phase that makes it *this* project.**

## 12.1 Extract the Muzzle Transform From the Hierarchy

This is the trickiest ten lines in the project. Build it in isolation.

```cpp
struct Projectile {
    bool  active = false;
    glm::vec3 p0, v0;
    float fireTime;
};

void fire(const glm::mat4& barrelMatrix, float now) {
    // Muzzle sits at the barrel's local +Z tip
    glm::vec3 muzzlePos = glm::vec3(barrelMatrix * glm::vec4(0, 0, MUZZLE_Z, 1.0f));
    // Forward direction: transform the local +Z AXIS (w = 0, not 1!)
    glm::vec3 forward = glm::normalize(
                          glm::vec3(barrelMatrix * glm::vec4(0, 0, 1, 0.0f)));

    ball.p0       = muzzlePos;
    ball.v0       = forward * MUZZLE_SPEED;
    ball.fireTime = now;
    ball.active   = true;

    spawnSmoke(muzzlePos, forward);
    muzzleLightUntil = now + 0.15f;   // Light 1 turns on
}
```

**⚠️ The `w` component matters.** Use `w=1` for the *position*, `w=0` for the *direction*. Getting this backwards makes the ball fly toward the world origin instead of along the barrel.

**Debug aid:** before firing anything, render a bright line from `muzzlePos` along `forward` for 5 units. If that line doesn't visually come out of the barrel's mouth and point where the cannon aims, fix it before continuing.

## 12.2 Ballistic Update (Requirement 12 — the closed form)

```cpp
void updateProjectile(float now) {
    if (!ball.active) return;
    float tau = now - ball.fireTime;

    // p(τ) = p0 + v0·τ + ½·g·τ²      ← evaluated fresh, never stored
    glm::vec3 g(0.0f, -9.8f, 0.0f);
    ballPos = ball.p0 + ball.v0 * tau + 0.5f * g * tau * tau;

    if (tau > MAX_FLIGHT_TIME) ball.active = false;
}
```

## 12.3 Testing Order (Do Not Skip)

1. **Ship stationary, target stationary.** Fire straight ahead. Does it arc?
2. **Ship rocking, target stationary.** Fire at the top vs. the bottom of a roll — the arcs should differ. *This proves the hierarchy feeds the ballistics.*
3. **Everything moving.** Only now enable the patrol.

**✅ DONE WHEN 🎯 M3:** pressing SPACE launches a ball that visibly arcs under gravity along the barrel's true direction, and firing at different points in the ship's roll produces different trajectories.

**At this point you have a complete, submittable project** meeting every mandatory requirement. Everything after this raises the grade.

---

# PHASE 13 — Impact Resolution

```cpp
void checkImpact(float now) {
    if (!ball.active) return;

    if (ballPos.y <= waveHeight(ballPos.x, ballPos.z, now)) {
        spawnSplash(ballPos);
        lastResult = "SPLASH";
        ball.active = false;
    }
    else if (glm::distance(ballPos, enemyPos(now)) < HIT_RADIUS) {
        spawnBurst(ballPos);
        enemyFlashUntil = now + 0.4f;   // visual confirmation
        lastResult = "HIT";
        ball.active = false;
    }
}
```

The `enemyFlashUntil` timer briefly boosts the enemy hull's emission — a clear, unmissable hit indication (Requirement 5).

**✅ DONE WHEN:** aiming well produces HIT, aiming short produces SPLASH, and the HUD reports which.

---

# PHASE 14 — Pooled Particles

**Goal:** smoke and splash with zero per-frame allocation.

```cpp
struct Particle { glm::vec3 origin, velocity; float age, life; bool active; };
Particle smokePool[4];
Particle splashPool[4];

void updateParticles(float dt) {
    for (auto& p : smokePool) {
        if (!p.active) continue;
        p.age += dt;
        if (p.age > p.life) { p.active = false; continue; }
    }
    // identical for splashPool
}

void drawParticle(const Particle& p) {
    if (!p.active) return;
    float k = p.age / p.life;
    glm::vec3 pos   = p.origin + p.velocity * p.age;   // pure function of age
    float     scale = 0.15f + 0.9f * k;
    float     alpha = 1.0f - k;
    // draw sphere with emission scaled by alpha
}
```

Enable blending for particles only, then disable it again:
```cpp
glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
glDepthMask(GL_FALSE);    // don't write depth for transparent puffs
// ... draw particles ...
glDepthMask(GL_TRUE); glDisable(GL_BLEND);
```

**✅ DONE WHEN:** firing repeatedly produces smoke and splashes, and firing 50 times in a row never allocates memory or breaks.

---

# PHASE 15 — 🎯 MILESTONE 4: The Demonstration Suite

**Goal:** the features that convert a nice scene into a high-scoring *Computer Graphics* project. **Do not skip this phase — this is where the marks are.**

## 15.1 Runtime Tessellation Control

```cpp
int barrelSegments = 32;
int oceanRes       = 64;

void rebuildMeshes() {                 // called ONLY on keypress, never per frame
    cylinderMesh = makeCylinder(barrelSegments);
    sphereMesh   = makeSphere(barrelSegments/2, barrelSegments);
    oceanMesh    = makeGrid(oceanRes);
}
// '+' : barrelSegments = min(64, barrelSegments*2); oceanRes = min(128, oceanRes*2); rebuildMeshes();
// '-' : barrelSegments = max(6,  barrelSegments/2); oceanRes = max(8,   oceanRes/2);  rebuildMeshes();
```

## 15.2 The Key Bindings (PRD §10)

Wire up `1`/`2`/`3` (shading mode), `B` (Blinn toggle), `L` (light cycle), `K` (term mask), `H` (hierarchy), `TAB`, `P` (pause).

**The `K` term mask is worth special attention** — it recreates L8 slide 54 (ambient / diffuse / specular / final) live, in your own scene. That is a very strong demo moment.

```cpp
// K cycles: 1 (ambient) → 3 (ambient+diffuse) → 7 (all) → 4 (specular only) → 1
```

## 15.3 Verify the Two Money Shots

**Demo A — Gouraud misses the ocean highlight.** Press `−` until `oceanRes` is 8 or 16. Orbit until the sun-streak is prominent. Press `2` (Gouraud) — the streak should break into patches or vanish. Press `3` (Phong) — it returns. *If it doesn't vanish in Gouraud, your ocean `n_s` is too low or your tessellation is too high.*

**Demo B — Mach banding on the barrel.** Set `barrelSegments` to 6–8, press `1` (Flat). Photograph the faceting and the perceived bright bands at the edges.

**✅ DONE WHEN 🎯 M4:** you can perform the entire 3-minute demo script from PRD §16.2 without touching the code.

---

# PHASE 16 — HUD / On-Screen Indications

**Goal:** satisfy Requirement 5 with visible evidence.

**Simplest approach that works:** render the HUD as a small set of textured quads using a bitmap font atlas, OR — much simpler and perfectly acceptable — **update the window title** every frame:

```cpp
char buf[256];
snprintf(buf, sizeof(buf),
    "Broadside | Shading: %s | Spec: %s | Az %.1f° El %.1f° | Flight %.2fs | %s",
    modeNames[shadingMode], useBlinn ? "N·H" : "R·V",
    glm::degrees(azimuth), glm::degrees(elevation), flightTime, lastResult);
glfwSetWindowTitle(window, buf);
```

The window title is legitimate, zero-cost, and instantly readable in a screen recording. If you have spare time, upgrade to on-screen bitmap text — but **do this only after Phase 15**, because a font renderer is a rabbit hole.

**✅ DONE WHEN:** a grader can read the current shading mode and aim angles without asking you.

---

# PHASE 17 — Optimization Pass

**Goal:** produce the evidence for Requirement 11.

Checklist:
- [ ] Count unique meshes — should be ≤ 8. Document the reuse table.
- [ ] Count draw calls per frame — should be ≤ 20. Add a counter and print it.
- [ ] Confirm `glEnable(GL_CULL_FACE)` and `GL_DEPTH_TEST` are on.
- [ ] Confirm view/projection/light uniforms are set **once per frame**, not per object.
- [ ] Confirm meshes are rebuilt only on `+`/`−`, never in the loop.
- [ ] Confirm the cannonball and muzzle flash are skipped when inactive.
- [ ] Confirm no `new`/`malloc` inside the render loop.
- [ ] Measure FPS (`1.0/dt` averaged) and record it at default and max tessellation.

Put this checklist, with your measured numbers, directly into your report.

---

# PHASE 18 — Report and Viva Preparation

Your report must contain:

1. **The traceability matrix** (PRD §4 and §4.1) — this alone answers most "did you cover X" questions.
2. **The illumination equation as implemented**, with slide citations.
3. **The material table** with the L8 slide-60 values you used verbatim.
4. **The `computeSmoothNormals` function** with the L9 slide-20 formula.
5. **Screenshot triptych:** the same frame in Flat, Gouraud, Phong. Annotate what differs.
6. **Screenshot:** Mach banding on the low-tessellation barrel.
7. **Screenshot:** the ocean highlight present in Phong, absent in Gouraud.
8. **The optimization table** with your measured draw-call count and FPS.
9. **One paragraph on local vs. global illumination** — explaining that the absence of shadows and inter-reflection is the correct, intentional behaviour of a local illumination model (L8 slides 10–11), not a shortcoming.

## Likely Viva Questions — Prepare These

| Question | Your answer |
|---|---|
| "Why no shadows?" | L8 slides 10–11: OpenGL's raster pipeline is *local* illumination, one bounce, each polygon rendered independently. Shadows require global techniques. |
| "Is the animation pre-computed?" | No — demonstrate by firing at different points in the ship's roll and showing different trajectories. Every value is a closed form of `glfwGetTime()`. |
| "Where is scaling used?" | Particle growth over lifetime, and every object is a unit mesh scaled to its dimensions — that's the mesh-reuse optimization. |
| "Show me hierarchical transforms." | Press `H`. The cannon detaches from the rocking hull. |
| "Why is Phong slower than Gouraud?" | L9 slide 36: the illumination model is re-evaluated per fragment instead of per vertex. Demonstrate by comparing FPS at high tessellation. |
| "Why does Gouraud miss the highlight?" | L9 slide 28: intensity is interpolated linearly between vertices, so a highlight falling *between* vertices is never computed. Demonstrate on the ocean. |
| "What is the normal matrix for?" | Non-uniform scaling (my hull is a stretched cube) breaks naive normal transformation; `(M⁻¹)ᵀ` corrects it. |

---

# Suggested Schedule

Adjust to your actual deadline, but keep the **proportions** — setup and the core always take longer than expected.

| Block | Phases | Outcome |
|---|---|---|
| **Block 1** | 0–3 | Window, shaders, camera working |
| **Block 2** | 4–6 | 🎯 **M1** — lit objects with specular; materials done |
| **Block 3** | 7–8 | 🎯 **M2** — ship on animated ocean *(good early demo)* |
| **Block 4** | 9–11 | Rigging, enemy, aiming |
| **Block 5** | 12–13 | 🎯 **M3** — **ballistics working; project is submittable** |
| **Block 6** | 14–15 | 🎯 **M4** — particles + demonstration suite *(the marks)* |
| **Block 7** | 16–18 | HUD, optimization pass, report, rehearsal |

**Reserve the last 20% of your time for Blocks 6–7.** Students routinely spend everything on getting the scene working and submit with no shading comparison and no report evidence — losing exactly the marks the course is actually assessing.

---

# Emergency Descope Plan

If you fall behind, cut in this order (last item is cut first):

1. Splash rings, cone/bowsprit, second sail — cosmetic
2. Blinn-Phong toggle (`B`) — bonus only
3. Particle pools — replace with a simple expanding sphere
4. Enemy patrol — make the target stationary (ballistics still works)
5. Auto-track aiming — manual-only aiming is still a full gimbal demo

**Never cut:** the three shading modes, the two lights with attenuation, the 4-level hierarchy, the ballistic projectile, or the material variety. Those are the graded requirements.
