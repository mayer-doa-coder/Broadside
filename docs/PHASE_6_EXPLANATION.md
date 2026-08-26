# Phase 6 Explanation

## Status

Phase 6 is complete.

The project now has several materials and two kinds of light. The material values are stored in one file, and the shader adds both lights correctly.

## What This Phase Adds

Phase 6 adds:

- a material structure;
- seven material presets;
- a directional sun;
- a point light at the cannon muzzle;
- distance falloff for the point light;
- one material choice for each drawn object.

## Materials

The material data is in `src/Material.h`.

Each material has five parts:

```cpp
struct Material {
    glm::vec3 ka;          // ambient colour
    glm::vec3 kd;          // diffuse colour
    glm::vec3 ks;          // shiny colour
    glm::vec3 emission;    // light made by the object
    float shininess;       // size of the shiny highlight
};
```

The project uses these materials:

| Material | Shininess | Used on |
|---|---:|---|
| Sailcloth | 4 | Sails and flag |
| Hull wood | 8 | Hull, deck, masts, and yards |
| Brass | 27.9 | Cannon barrel |
| Black plastic | 32 | Cannon mount |
| Polished silver | 89.6 | Cannon fitting |
| Ocean | 160 | Water |
| Muzzle flash | Emissive | Small light sphere |

A low shininess value makes a wide, soft highlight. A high value makes a small, sharp highlight. This is why the sail looks dull, the brass looks warm and shiny, and the silver has a tighter highlight.

The brass, polished silver, and black plastic values match the required L8 table. The other values match the tuned values in the PRD.

## The Two Lights

There are exactly two light slots.

1. The sun is a directional light. Its rays have one direction and do not fade with distance.
2. The muzzle light is a point light. It starts at the cannon muzzle and becomes weaker over distance.

The point light uses this formula:

```text
1 / (1.0 + 0.09*d + 0.032*d*d)
```

The shader loops over both lights and adds their results. It does not use a separate shader for each light.

The muzzle sphere and the muzzle light are different things. The sphere uses emission, so it can make itself bright. The point light creates the orange light on nearby objects.

## Efficient Uniform Updates

Data shared by the whole frame is sent once per frame. This includes the camera, projection, ambient light, and both lights.

Only object-specific data changes for each draw. This includes the model matrix, normal matrix, and material.

## Validation

The following checks passed:

- all required material numbers are correct;
- the shader contains exactly two light slots;
- the shader sums both lights;
- the point-light falloff values are correct;
- the muzzle light uses the cannon's current muzzle position;
- the real OpenGL shaders compile and link;
- brass, silver, wood, cloth, ocean, black plastic, and emission are visible in the running scene;
- Debug and Release builds succeed;
- a strict Release build succeeds with warnings treated as errors;
- the shared Phase 6 and 7 validation completed 115 checks with 0 failures.

The current scene uses 15 draw calls and 10,026 triangles. Both are inside the PRD limits of 20 draw calls and 25,000 triangles.

## What Comes Later

The muzzle light stays on for now so its colour, position, and falloff can be checked. Phase 12 adds firing and makes this light stay on for only a short time after a shot.

## Result

Phase 6 is complete. The project has the full material table, two working lights, point-light falloff, and clear material differences.
