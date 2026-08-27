#version 330 core

// Broadside — vertex stage.
// Phase 5: the illumination model, evaluated here in Gouraud mode.
// Phase 8: the ocean displacement, computed on the GPU.
//
// NOTE: this file is not standalone GLSL. src/Shader.h splices two shared blocks
// in immediately after the #version line above — the lighting model
// (src/Lighting.h) and the wave surface (src/Wave.h) — so `computeLighting`,
// `oceanSurface`, `struct Light`, `uIsOcean`, `uTime` and every material/light
// uniform are already declared by the time this file's own code begins. GLSL has
// no #include; that splice IS the include (guide 5.2, implementation note).

// Matches Mesh.h exactly: every indexed vertex is a position and a normal.
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;

// The matrices stay separate rather than pre-multiplied, because lighting needs
// uModel on its own: the illumination equation works in WORLD space, so it needs
// the world-space fragment position, not the clip-space one.
uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

// (M^-1)^T, computed once per object per frame on the CPU (guide 5.3).
// Non-uniform scale — and the hull is a stretched cube — shears a naively
// transformed normal off the true surface. This is the correction.
uniform mat3 uNormalMatrix;

// Phase 14: a fixed upper bound, matching smokePool[4] + splashPool[4].
// Uniform arrays keep all active puffs in one sphere draw. There is no dynamic
// buffer, allocation, or upload object created in the frame loop.
const int MAX_PARTICLE_INSTANCES = 8;
uniform int   uUseParticleInstances;
uniform mat4  uParticleModels[MAX_PARTICLE_INSTANCES];
uniform mat3  uParticleNormalMatrices[MAX_PARTICLE_INSTANCES];
uniform vec3  uParticleEmission[MAX_PARTICLE_INSTANCES];
uniform float uParticleAlpha[MAX_PARTICLE_INSTANCES];

out vec3 vFragPos;
out vec3 vNormal;
out vec3 vGouraudColor;
out vec3 vParticleEmission;
out float vParticleAlpha;

void main()
{
    mat4 model = uModel;
    mat3 normalMatrix = uNormalMatrix;
    vParticleEmission = vec3(0.0);
    vParticleAlpha = 1.0;

    if (uUseParticleInstances == 1) {
        model = uParticleModels[gl_InstanceID];
        normalMatrix = uParticleNormalMatrices[gl_InstanceID];
        vParticleEmission = uParticleEmission[gl_InstanceID];
        vParticleAlpha = uParticleAlpha[gl_InstanceID];
    }

    vec3 worldPos = vec3(model * vec4(aPos, 1.0));   // w = 1: this is a POSITION
    vec3 worldNrm = normalMatrix * aNormal;          // mat3: directions carry no translation

    // ---- OCEAN: displace the surface here, on the GPU (PRD 14) ----
    // The vertex buffer is never touched and nothing is re-uploaded: the grid is
    // uploaded flat, once, and every frame of sea after that costs two sines per
    // vertex on hardware built for exactly that. Zero per-frame CPU work, zero
    // bandwidth — this is the single largest optimization in the project.
    //
    // oceanSurface OVERWRITES worldNrm rather than feeding into it, and that is
    // deliberate. The analytic normal it returns is already in WORLD space, so
    // pushing it through uNormalMatrix a second time would tilt it by the ocean's
    // 48x scale and land the sun-streak in the wrong place.
    if (uIsOcean == 1)
        worldPos = oceanSurface(worldPos, worldNrm);

    vFragPos = worldPos;
    vNormal  = worldNrm;

    // Always written, even in the modes that ignore it. Leaving a varying
    // undefined on some paths is the kind of thing that works on one driver and
    // produces garbage on another.
    vGouraudColor = vec3(0.0);

    // ---- GOURAUD: evaluate the illumination equation HERE, per vertex ----
    // L9 s19. The rasteriser then interpolates the resulting COLOUR across the
    // triangle. That is precisely why Gouraud loses a highlight that falls
    // between two vertices — it was never computed anywhere (L9 s28).
    if (uShadingMode == 1)
        vGouraudColor = computeLighting(normalize(vNormal), vFragPos);

    gl_Position = uProjection * uView * vec4(vFragPos, 1.0);
}
