#version 330 core

// Broadside — Phase 5 fragment stage.
//
// NOTE: this file is not standalone GLSL. src/Shader.h splices the shared
// lighting block (src/Lighting.h) in immediately after the #version line above,
// so `computeLighting` and every material/light uniform are already declared
// here. There is exactly one copy of the illumination maths, shared with
// phong.vert, so Gouraud and Phong cannot drift apart and the L9 comparison
// stays a fair test on identical geometry.
//
// The only lighting code that lives HERE rather than in the shared block is the
// Flat-mode face normal, because dFdx / dFdy exist only in a fragment shader.

in vec3 vFragPos;
in vec3 vNormal;
in vec3 vGouraudColor;

out vec4 FragColor;

void main()
{
    vec3 color;

    if (uShadingMode == 0) {
        // ---- FLAT: one normal for the whole face (L9 s9-16) ----
        // The screen-space derivatives of the world position span the triangle's
        // plane, so their cross product IS the true geometric face normal. It is
        // constant across the face, which is what produces the faceting and the
        // Mach bands at facet edges (L9 s13-15).
        //
        // Deriving it here rather than passing a per-face normal costs no extra
        // buffers and works on every mesh in the project unchanged.
        vec3 faceN = normalize(cross(dFdx(vFragPos), dFdy(vFragPos)));
        color = computeLighting(faceN, vFragPos);
    }
    else if (uShadingMode == 1) {
        // ---- GOURAUD: interpolated INTENSITY (L9 s19-21) ----
        // Nothing is computed here at all. The colour was evaluated per vertex
        // and linearly interpolated by the rasteriser on the way in.
        color = vGouraudColor;
    }
    else {
        // ---- PHONG: interpolated NORMAL, lighting per fragment (L9 s30-32) ----
        // The normal must be re-normalised: linear interpolation of two unit
        // vectors does not produce a unit vector, and an un-normalised N makes
        // the specular power term wrong across the whole triangle.
        color = computeLighting(normalize(vNormal), vFragPos);
    }

    FragColor = vec4(color, 1.0);
}
