#pragma once

// Broadside — Phase 5: the L8 illumination model, as GLSL source.
//
// GLSL has no #include. The vertex stage needs this maths (Gouraud evaluates the
// full equation per vertex, L9 s19) and so does the fragment stage (Phong
// evaluates it per fragment, L9 s30), so the guide's rule is to keep exactly ONE
// copy in a C++ string and splice it into both stages at load time.
//
// Two copies of an illumination equation is how Gouraud and Phong silently drift
// apart and the whole L9 comparison stops being a fair test. There is one copy.
//
// What may NOT go in here: anything stage-specific. dFdx / dFdy are fragment-only,
// so the Flat-mode face normal is derived in phong.frag's main, not here.

// The equation implemented below (PRD 11.3):
//
//   I = I_e                                            emission        L8 s55
//     + k_a * I_a_global                               ambient         L8 s26
//     + SUM_lights f_att(d) * [ k_d * I_d * max(0, N.L)      diffuse   L8 s32
//                             + k_s * I_s * max(0, R.V)^n_s ] specular L8 s44
//
//   R     = reflect(-L, N) = 2(N.L)N - L                                L8 s47
//   or Blinn-Phong (N.H)^n_s with H = normalize(L + V)                  L8 s49
//   f_att = 1 / (a0 + a1*d + a2*d^2) for point lights, 1.0 directional  L8 s21

static const char* LIGHTING_GLSL = R"GLSL(
// ===================== shared lighting block (src/Lighting.h) ===============

struct Light {
    int   type;          // 0 = directional, 1 = point
    vec3  direction;     // directional only: L = -normalize(direction)
    vec3  position;      // point only
    vec3  diffuse;       // I_d
    vec3  specular;      // I_s
    vec3  attenuation;   // (a0, a1, a2)   — L8 slide 21
    float enabled;       // float, not bool: one uniform setter covers every field
};

uniform Light uLights[2];        // exactly two (AGENT.md scope ceiling)
uniform vec3  uGlobalAmbient;    // I_a_global                     — L8 slide 57
uniform vec3  uViewPos;          // camera world position; specular is view-dependent

uniform vec3  uKa;               // material ambient  reflectivity — L8 slide 60
uniform vec3  uKd;               // material diffuse  reflectivity
uniform vec3  uKs;               // material specular reflectivity
uniform vec3  uEmission;         // I_e                            — L8 slide 55
uniform float uShininess;        // n_s

uniform int   uShadingMode;      // 0 = Flat, 1 = Gouraud, 2 = Phong
uniform int   uUseBlinn;         // 0 = (R.V)^n, 1 = Blinn-Phong (N.H)^n
uniform int   uTermMask;         // bit 0 ambient, bit 1 diffuse, bit 2 specular

vec3 computeLighting(vec3 N, vec3 fragPos)
{
    vec3 V = normalize(uViewPos - fragPos);

    vec3 result = vec3(0.0);

    // Ambient — no direction and no position dependence at all (L8 slide 26).
    // That is exactly why it alone cannot describe a shape: press K and the
    // ambient-only pass shows a flat silhouette (L8 slide 54).
    if ((uTermMask & 1) != 0) result += uKa * uGlobalAmbient;

    result += uEmission;                                  // L8 slide 55

    for (int i = 0; i < 2; ++i) {                         // L8 slide 56: sum the lights
        if (uLights[i].enabled < 0.5) continue;

        vec3  L;
        float attenuation = 1.0;

        if (uLights[i].type == 0) {
            // Directional: parallel rays from infinitely far away, so there is no
            // distance to attenuate over (L8 slide 19).
            L = normalize(-uLights[i].direction);
        } else {
            vec3  toLight = uLights[i].position - fragPos;
            float d       = length(toLight);
            L = toLight / max(d, 1e-6);
            // Radial attenuation — L8 slide 21: 1/(a0 + a1*d + a2*d^2)
            vec3 a = uLights[i].attenuation;
            attenuation = 1.0 / max(a.x + a.y * d + a.z * d * d, 1e-6);
        }

        // Diffuse — Lambert's cosine law, L8 slide 32
        float NdotL = max(dot(N, L), 0.0);
        if ((uTermMask & 2) != 0)
            result += attenuation * uKd * uLights[i].diffuse * NdotL;

        // Specular — L8 slide 44 / 49.
        // Gated on NdotL > 0: a face turned away from the light must not pick up a
        // highlight through its back, which is what (R.V)^n alone would allow.
        if (NdotL > 0.0 && (uTermMask & 4) != 0) {
            float spec;
            if (uUseBlinn == 1) {
                vec3 H = normalize(L + V);                // Blinn-Phong   — L8 s49
                spec = pow(max(dot(N, H), 0.0), uShininess);
            } else {
                vec3 R = reflect(-L, N);                  // 2(N.L)N - L   — L8 s47
                spec = pow(max(dot(R, V), 0.0), uShininess);
            }
            result += attenuation * uKs * uLights[i].specular * spec;
        }
    }
    return result;
}
// ===================== end shared lighting block ============================
)GLSL";
