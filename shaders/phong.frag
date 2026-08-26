#version 330 core

// Broadside — Phase 2 fragment stage.
// No illumination yet: Phase 5 replaces this body with the L8 ambient + diffuse +
// specular sum and the uShadingMode branch. Phase 2 only has to prove that GLSL
// compiles, links, and receives uniforms.

in vec3 vNormal;

uniform vec3  uTint;      // exercises setVec3
uniform float uTime;      // exercises setFloat — the render loop's `now`
uniform int   uUseTint;   // exercises setInt   — foreshadows uShadingMode

out vec4 FragColor;

void main()
{
    // Normals as RGB — the standard debug view for this project, and the colour
    // source until Phase 5 replaces it with real illumination. On the Phase 3
    // cube it gives each of the six faces its own flat colour, which makes the
    // orbit obviously correct at a glance.
    vec3 base = normalize(vNormal) * 0.5 + 0.5;

    if (uUseTint == 1) {
        // Closed form of elapsed time (Requirement 12). Pressing P freezes the
        // render loop's clock, so this pulse must stop dead — that is the visible
        // proof that Phase 1's simulation clock actually reaches the GPU.
        float pulse = 0.5 + 0.5 * sin(uTime * 2.0);
        base = mix(base, uTint, pulse * 0.30);
    }

    FragColor = vec4(base, 1.0);
}
