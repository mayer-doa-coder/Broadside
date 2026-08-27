#pragma once

// Broadside — Phase 6: the material table.
//
// A material is the object's half of the illumination equation: the light
// supplies I_a / I_d / I_s, the material supplies what fraction of each it
// reflects (k_a, k_d, k_s) and how tightly it focuses the reflection (n_s).
//
//   I = I_e + k_a*I_a + SUM f_att * [ k_d*I_d*max(0,N.L) + k_s*I_s*max(0,R.V)^n_s ]
//
// The first three entries are VERBATIM from L8 slide 60. They are cited evidence
// in the report, so they are not tuned, rounded, or "improved" — if brass looks
// too yellow, that is what brass is. Only the three marked "tuned" are ours, and
// each one exists to occupy a specific place in the n_s range.
//
// The n_s spread across a single frame is the point of the whole table:
//
//   sailcloth  4  ->  hull 8  ->  brass 27.9  ->  black plastic 32
//                 ->  polished silver 89.6  ->  ocean 160
//
// 4 -> 160 is a 40x range. L8 slide 46 makes exactly this comparison with test
// spheres; this scene makes it with real objects, in one screenshot. Narrowing
// the range to make the scene prettier throws the demonstration away.

#include <glm/glm.hpp>

// Deliberately the guide's struct, unchanged: four coefficient vectors and one
// exponent. No name string, no id, no shader handle — a material is data, and
// the only thing that ever touches it is setMaterial().
struct Material {
    glm::vec3 ka;          // ambient  reflectivity
    glm::vec3 kd;          // diffuse  reflectivity          — Lambert, L8 s32
    glm::vec3 ks;          // specular reflectivity          — L8 s44
    glm::vec3 emission;    // I_e, light the object makes    — L8 s55
    float     shininess;   // n_s, the specular exponent
};

// ---------------------------------------------------------------------------
// Verbatim — L8 slide 60
// ---------------------------------------------------------------------------

// Cannon barrel. Warm metal: k_d is strongly yellow, k_s is near-white because a
// metal's highlight takes the LIGHT's colour, not the surface's. n_s = 27.9 is a
// medium-width highlight — big enough to sweep visibly along the barrel as the
// cannon traverses, which is the specular demonstration in PRD s13.
static const Material BRASS = {
    glm::vec3(0.329412f, 0.223529f, 0.027451f),
    glm::vec3(0.780392f, 0.568627f, 0.113725f),
    glm::vec3(0.992157f, 0.941176f, 0.807843f),
    glm::vec3(0.0f),
    27.8974f
};

// Cannon fittings. Achromatic, and n_s = 89.6 — over 3x brass. Sitting the two
// side by side on the same ship is a direct n_s comparison a grader can make in
// one glance without touching a key.
static const Material POLISHED_SILVER = {
    glm::vec3(0.23125f,  0.23125f,  0.23125f),
    glm::vec3(0.2775f,   0.2775f,   0.2775f),
    glm::vec3(0.773911f, 0.773911f, 0.773911f),
    glm::vec3(0.0f),
    89.6f
};

// Cannonball, and the cannon mount. k_a is exactly zero and k_d is 0.01: this
// material is almost pure specular. It is the clearest proof in the table that
// the specular term is a separate, independent addition — press K to ambient-only
// and a black plastic object vanishes completely.
static const Material BLACK_PLASTIC = {
    glm::vec3(0.0f,  0.0f,  0.0f),
    glm::vec3(0.01f, 0.01f, 0.01f),
    glm::vec3(0.50f, 0.50f, 0.50f),
    glm::vec3(0.0f),
    32.0f
};

// ---------------------------------------------------------------------------
// Tuned — chosen for where they sit in the n_s range (PRD 11.2)
// ---------------------------------------------------------------------------

// Ocean. n_s = 160, the top of the range: a mirror-like, very tight highlight.
// This number is what makes Demo A work at all. Drop it and the sun-streak grows
// wide enough for Gouraud's per-vertex sampling to catch it, and "Gouraud misses
// the highlight" (L9 s28) stops being demonstrable.
static const Material OCEAN = {
    glm::vec3(0.02f, 0.05f, 0.08f),
    glm::vec3(0.06f, 0.14f, 0.20f),
    glm::vec3(0.90f, 0.94f, 0.98f),
    glm::vec3(0.0f),
    160.0f
};

// Hull. Rough wet timber: low k_s AND low n_s, so it gets a broad, weak sheen
// rather than a highlight. The contrast case that shows what dull looks like.
static const Material HULL_WOOD = {
    glm::vec3(0.12f, 0.08f, 0.05f),
    glm::vec3(0.38f, 0.25f, 0.15f),
    glm::vec3(0.15f, 0.12f, 0.10f),
    glm::vec3(0.0f),
    8.0f
};

// Sailcloth. k_s = 0.04 and n_s = 4 — effectively Lambertian. The bottom of the
// range, and the object that proves diffuse-only appearance: it is bright, it
// shades correctly from light to dark, and it has no highlight anywhere on it.
static const Material SAILCLOTH = {
    glm::vec3(0.20f, 0.19f, 0.17f),
    glm::vec3(0.75f, 0.73f, 0.68f),
    glm::vec3(0.04f, 0.04f, 0.04f),
    glm::vec3(0.0f),
    4.0f
};

// Enemy hull. The same timber as the player's, weathered colder and darker so
// the two vessels read as different ships at 18 units without needing any new
// geometry (guide Phase 10, "a different hull tint"). n_s stays at 8: it is the
// same material, differently aged, and changing the exponent would quietly
// change what the frame demonstrates about the n_s range.
//
// Tuned, not a slide-60 entry. Phase 13 raises its emission briefly on a hit.
static const Material ENEMY_HULL = {
    glm::vec3(0.07f, 0.07f, 0.08f),
    glm::vec3(0.22f, 0.20f, 0.19f),
    glm::vec3(0.15f, 0.14f, 0.13f),
    glm::vec3(0.0f),
    8.0f
};

// ---------------------------------------------------------------------------
// Emissive — L8 slide 55
// ---------------------------------------------------------------------------

// The muzzle flash sphere. Every reflectivity is zero and I_e carries the whole
// colour, so this object is lit by nothing and lights itself. That is the point:
// emission is a constant added before any light is even considered, which is why
// the flash stays visible with every light switched off (L key, Phase 15).
//
// It is NOT a light source itself — emission does not illuminate neighbours in a
// local model (L8 s10-11). The orange glow on nearby surfaces comes from Light 1,
// the point light sitting at the same position.
static const Material MUZZLE_FLASH = {
    glm::vec3(0.0f),
    glm::vec3(0.0f),
    glm::vec3(0.0f),
    glm::vec3(1.0f, 0.7f, 0.3f),
    1.0f
};

// Phase 14 particle batches supply emission per instance in the vertex shader.
// The shared material is otherwise black, so smoke, spray and impact sparks stay
// readable in every shading mode without pretending they are another light.
static const Material PARTICLE_BASE = {
    glm::vec3(0.0f),
    glm::vec3(0.0f),
    glm::vec3(0.0f),
    glm::vec3(0.0f),
    1.0f
};
