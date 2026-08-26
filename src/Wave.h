#pragma once

// Broadside — Phase 8: the sea surface, defined once for both processors.
//
// The surface is a sum of two travelling sine waves (PRD 9.1):
//
//   y(x, z, t) = A1 sin(K1 x + W1 t) + A2 sin(K2 z + W2 t + phase)
//
// and its normal is ANALYTIC, taken straight from the partial derivatives rather
// than averaged from neighbouring triangles:
//
//   dy/dx = A1 K1 cos(K1 x + W1 t)
//   dy/dz = A2 K2 cos(K2 z + W2 t + phase)
//   N     = normalize(-dy/dx, 1, -dy/dz)
//
// Two cosines, exact at every point, no matter how coarse the mesh is. That last
// property is what makes Demo A honest: when the grader presses '-' and the ocean
// drops to 8x8, the NORMALS stay perfect and the only thing that degrades is how
// often the illumination equation gets sampled - which is precisely the variable
// L9 slide 28 is about.
//
// ===========================================================================
// THE DRIFT TRAP (AGENT.md section 5, one of the six known bugs in this project)
//
// These seven numbers are needed on BOTH sides:
//   - the GPU displaces the ocean vertices with them, in phong.vert;
//   - the CPU evaluates the same surface under each hull, to make the ship rock.
//
// If the CPU and GPU values disagree by even a little, the ship stops sitting in
// the water and starts hovering above or slicing through it. The ship rocks
// BECAUSE of the wave under it, so the two processors must see identical values.
//
// Each number is therefore written only once below. The C++ constants use those
// literals directly, and the preprocessor turns the same literals into the GLSL
// source string. There is no second numeric table that can drift out of step.
// ===========================================================================

#include <cmath>

// --- the seven numbers, shared by C++ and the GLSL source ------------------
// Keep these literals without an `f` suffix: GLSL uses the text too.
#define BROADSIDE_WAVE_A1    0.22
#define BROADSIDE_WAVE_K1    0.55
#define BROADSIDE_WAVE_W1    1.1
#define BROADSIDE_WAVE_A2    0.14
#define BROADSIDE_WAVE_K2    0.85
#define BROADSIDE_WAVE_W2    1.7
#define BROADSIDE_WAVE_PHASE 1.3

static constexpr float WAVE_A1    = static_cast<float>(BROADSIDE_WAVE_A1);    // amplitude along X
static constexpr float WAVE_K1    = static_cast<float>(BROADSIDE_WAVE_K1);    // wavelength 2*pi/K1 = 11.4
static constexpr float WAVE_W1    = static_cast<float>(BROADSIDE_WAVE_W1);    // angular frequency
static constexpr float WAVE_A2    = static_cast<float>(BROADSIDE_WAVE_A2);    // cross swell along Z
static constexpr float WAVE_K2    = static_cast<float>(BROADSIDE_WAVE_K2);    // wavelength 2*pi/K2 = 7.4
static constexpr float WAVE_W2    = static_cast<float>(BROADSIDE_WAVE_W2);
static constexpr float WAVE_PHASE = static_cast<float>(BROADSIDE_WAVE_PHASE);

// Mean sea level is y = 0. waveHeight returns displacement ABOUT that plane, so
// the ocean grid is drawn at y = 0 and every floating thing adds its own freeboard.

// How much of the water's slope the hull actually takes up. 1.0 would weld the
// deck to the surface; a real hull has mass and lags, and 0.8 reads as a boat
// rather than as a decal (guide 8.2).
static const float WAVE_FOLLOW = 0.8f;

// --- the surface and its slopes, C++ side (guide 8.2) ----------------------
inline float waveHeight(float x, float z, float t)
{
    return WAVE_A1 * std::sin(WAVE_K1 * x + WAVE_W1 * t)
         + WAVE_A2 * std::sin(WAVE_K2 * z + WAVE_W2 * t + WAVE_PHASE);
}

inline float waveSlopeX(float x, float t)
{
    return WAVE_A1 * WAVE_K1 * std::cos(WAVE_K1 * x + WAVE_W1 * t);
}

inline float waveSlopeZ(float z, float t)
{
    return WAVE_A2 * WAVE_K2 * std::cos(WAVE_K2 * z + WAVE_W2 * t + WAVE_PHASE);
}

// --- GLSL side, generated from the same seven literals ---------------------
//
// Spliced into the shader after its #version line, exactly like the lighting
// block. The fragment stage receives this too and simply never calls it; an
// unused uniform is optimised out and costs nothing.
#define BROADSIDE_STRINGIFY_IMPL(value) #value
#define BROADSIDE_STRINGIFY(value) BROADSIDE_STRINGIFY_IMPL(value)

static const char* WAVE_GLSL = R"GLSL(
// ===================== shared wave block (src/Wave.h) =======================
)GLSL"
"const float WAVE_A1    = " BROADSIDE_STRINGIFY(BROADSIDE_WAVE_A1) ";\n"
"const float WAVE_K1    = " BROADSIDE_STRINGIFY(BROADSIDE_WAVE_K1) ";\n"
"const float WAVE_W1    = " BROADSIDE_STRINGIFY(BROADSIDE_WAVE_W1) ";\n"
"const float WAVE_A2    = " BROADSIDE_STRINGIFY(BROADSIDE_WAVE_A2) ";\n"
"const float WAVE_K2    = " BROADSIDE_STRINGIFY(BROADSIDE_WAVE_K2) ";\n"
"const float WAVE_W2    = " BROADSIDE_STRINGIFY(BROADSIDE_WAVE_W2) ";\n"
"const float WAVE_PHASE = " BROADSIDE_STRINGIFY(BROADSIDE_WAVE_PHASE) ";\n"
R"GLSL(

uniform int   uIsOcean;   // 1 for the ocean grid, 0 for every other object
uniform float uTime;      // the simulation clock; freezes with P, so the sea does too

// Displaces a WORLD-space position onto the sea surface and returns the exact
// world-space normal there through `worldNormal`.
//
// World space, not object space, and that is load-bearing: the ocean is a UNIT
// grid scaled up 48x, so evaluating the wave on the raw object coordinates would
// produce one wave 48 times too long AND would not match waveHeight() on the CPU,
// which is called with the ship's world position.
vec3 oceanSurface(vec3 worldPos, out vec3 worldNormal)
{
    // Phases are read BEFORE the displacement, from x and z, which it never touches.
    float p1 = WAVE_K1 * worldPos.x + WAVE_W1 * uTime;
    float p2 = WAVE_K2 * worldPos.z + WAVE_W2 * uTime + WAVE_PHASE;

    worldPos.y += WAVE_A1 * sin(p1) + WAVE_A2 * sin(p2);

    // Analytic normal from the partial derivatives (PRD 9.1).
    float dydx = WAVE_A1 * WAVE_K1 * cos(p1);
    float dydz = WAVE_A2 * WAVE_K2 * cos(p2);
    worldNormal = normalize(vec3(-dydx, 1.0, -dydz));

    return worldPos;
}
// ===================== end shared wave block ================================
)GLSL";

#undef BROADSIDE_STRINGIFY
#undef BROADSIDE_STRINGIFY_IMPL
#undef BROADSIDE_WAVE_A1
#undef BROADSIDE_WAVE_K1
#undef BROADSIDE_WAVE_W1
#undef BROADSIDE_WAVE_A2
#undef BROADSIDE_WAVE_K2
#undef BROADSIDE_WAVE_W2
#undef BROADSIDE_WAVE_PHASE
