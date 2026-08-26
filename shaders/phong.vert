#version 330 core

// Broadside — Phase 3 vertex stage.
// The attribute layout is already the Phase 4 layout (struct Vertex { vec3 position; vec3 normal; }),
// so Mesh.h will drop straight in without touching this file.
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;

// The three matrices are kept separate rather than pre-multiplied on the CPU:
// Phase 5 needs uModel on its own to compute world-space fragment positions for
// the lighting terms, and uNormalMatrix joins them there.
uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

out vec3 vNormal;

void main()
{
    vNormal     = aNormal;
    gl_Position = uProjection * uView * uModel * vec4(aPos, 1.0);
}
