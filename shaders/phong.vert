#version 330 core

// Broadside — Phase 5 vertex stage.
//
// NOTE: this file is not standalone GLSL. src/Shader.h splices the shared
// lighting block (src/Lighting.h) in immediately after the #version line above,
// so `computeLighting`, `struct Light` and every material/light uniform are
// already declared by the time this file's own code begins. GLSL has no
// #include; that splice IS the include (guide 5.2, implementation note).

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

out vec3 vFragPos;
out vec3 vNormal;
out vec3 vGouraudColor;

void main()
{
    vFragPos = vec3(uModel * vec4(aPos, 1.0));   // w = 1: this is a POSITION
    vNormal  = uNormalMatrix * aNormal;          // mat3: directions carry no translation

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
