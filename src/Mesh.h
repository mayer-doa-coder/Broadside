#pragma once

// Broadside — Phase 4: the mesh library.
//
// Eight unique primitives (PRD section 6). Everything drawn in this project is one
// of these under a different model matrix and material — that reuse IS the
// optimization claimed in Requirement 11, so the generators must stay generic:
// no generator knows what it will be used for.
//
// Two rules govern every generator below.
//
//   1. UNIT SIZE. Every primitive fits the box [-0.5, +0.5] on each axis it spans.
//      A caller writes S(2.4, 0.5, 0.9) and gets exactly those dimensions, so the
//      scale factors in Scene.h read as real measurements.
//
//   2. COUNTER-CLOCKWISE FROM OUTSIDE. glFrontFace(GL_CCW) and GL_CULL_FACE are on
//      (main.cpp), so winding is not cosmetic. Swap two vertices of a triangle and
//      that triangle silently disappears.
//
// Normals are where this phase earns marks. For the analytic surfaces the exact
// normal is known in closed form and is used directly (L9 s20, continuous case);
// for the non-analytic grid the averaging formula N_v = (Sum N_i) / ||Sum N_i||
// is applied instead. Both paths are exercised, so both can be cited in the report.

#include <glad/glad.h>

#include <glm/glm.hpp>

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <vector>

// ---------------------------------------------------------------------------
// Vertex — the one layout the whole project uses.
// location 0 = position, location 1 = normal. phong.vert already declares this.
// ---------------------------------------------------------------------------
struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
};

// ---------------------------------------------------------------------------
// N_v = (Sum N_i) / ||Sum N_i||        — L9 slide 20
//
// The averaging formula, implemented literally. Used on the ocean grid, whose
// displaced surface has no closed-form normal at generation time. Kept as a free
// function over plain vectors so it can be quoted straight into the report.
// ---------------------------------------------------------------------------
inline void computeSmoothNormals(std::vector<Vertex>& v,
                                 const std::vector<unsigned>& idx)
{
    for (auto& vert : v) vert.normal = glm::vec3(0.0f);
    for (size_t i = 0; i < idx.size(); i += 3) {
        glm::vec3 a = v[idx[i]].position;
        glm::vec3 b = v[idx[i+1]].position;
        glm::vec3 c = v[idx[i+2]].position;
        glm::vec3 faceN = glm::cross(b - a, c - a);

        // A zero-area triangle would make glm::normalize return NaN, and one NaN
        // vertex normal poisons every triangle that shares it. Skipping it is the
        // only deviation from the slide, and it changes nothing for valid input.
        const float faceLen = glm::length(faceN);
        if (faceLen <= 1e-12f) continue;
        faceN /= faceLen;

        v[idx[i]].normal   += faceN;      // accumulate Sum N_i
        v[idx[i+1]].normal += faceN;
        v[idx[i+2]].normal += faceN;
    }
    for (auto& vert : v) {
        const float len = glm::length(vert.normal);
        vert.normal = (len > 1e-12f) ? (vert.normal / len)           // divide by ||Sum||
                                     : glm::vec3(0.0f, 1.0f, 0.0f);  // orphan vertex
    }
}

// ---------------------------------------------------------------------------
// Mesh — one VAO + VBO + EBO, uploaded once, drawn many times.
//
// Move-only: the object owns three GL names, so a copy would delete them twice.
// Move assignment is what makes the Phase 15 idiom `cylinder = makeCylinder(n)`
// safe — the old buffers are released before the new ones are adopted.
// ---------------------------------------------------------------------------
class Mesh {
public:
    Mesh() = default;

    Mesh(const std::vector<Vertex>& verts, const std::vector<unsigned>& idx)
    {
        upload(verts, idx);
    }

    ~Mesh() { destroy(); }

    Mesh(const Mesh&)            = delete;
    Mesh& operator=(const Mesh&) = delete;

    Mesh(Mesh&& other) noexcept
        : m_vao(other.m_vao), m_vbo(other.m_vbo), m_ebo(other.m_ebo),
          m_indexCount(other.m_indexCount), m_vertexCount(other.m_vertexCount)
    {
        other.m_vao = other.m_vbo = other.m_ebo = 0;
        other.m_indexCount  = 0;
        other.m_vertexCount = 0;
    }

    Mesh& operator=(Mesh&& other) noexcept
    {
        if (this != &other) {
            destroy();
            m_vao         = other.m_vao;
            m_vbo         = other.m_vbo;
            m_ebo         = other.m_ebo;
            m_indexCount  = other.m_indexCount;
            m_vertexCount = other.m_vertexCount;
            other.m_vao = other.m_vbo = other.m_ebo = 0;
            other.m_indexCount  = 0;
            other.m_vertexCount = 0;
        }
        return *this;
    }

    bool valid() const { return m_vao != 0; }

    // Must be called while the GL context is still current — i.e. before
    // glfwTerminate. A global Mesh cannot rely on its destructor for this.
    void destroy()
    {
        if (m_ebo) { glDeleteBuffers(1, &m_ebo);      m_ebo = 0; }
        if (m_vbo) { glDeleteBuffers(1, &m_vbo);      m_vbo = 0; }
        if (m_vao) { glDeleteVertexArrays(1, &m_vao); m_vao = 0; }
        m_indexCount  = 0;
        m_vertexCount = 0;
    }

    void draw() const
    {
        if (m_vao == 0) return;
        glBindVertexArray(m_vao);
        glDrawElements(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }

    // Phase 14 particle batches reuse the same indexed mesh several times in one
    // draw call. Per-instance transforms and colours come from small uniform
    // arrays indexed by gl_InstanceID, so no instance buffer is allocated or
    // uploaded in the render loop.
    void drawInstanced(int instanceCount) const
    {
        if (m_vao == 0 || instanceCount <= 0) return;
        glBindVertexArray(m_vao);
        glDrawElementsInstanced(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, 0,
                                (GLsizei)instanceCount);
        glBindVertexArray(0);
    }

    int vertexCount()   const { return m_vertexCount; }
    int indexCount()    const { return (int)m_indexCount; }
    int triangleCount() const { return (int)m_indexCount / 3; }

private:
    void upload(const std::vector<Vertex>& verts, const std::vector<unsigned>& idx)
    {
        destroy();
        if (verts.empty() || idx.empty()) {
            std::fprintf(stderr, "[mesh] refusing to upload an empty mesh\n");
            return;
        }

        glGenVertexArrays(1, &m_vao);
        glGenBuffers(1, &m_vbo);
        glGenBuffers(1, &m_ebo);

        glBindVertexArray(m_vao);

        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glBufferData(GL_ARRAY_BUFFER,
                     (GLsizeiptr)(verts.size() * sizeof(Vertex)),
                     verts.data(), GL_STATIC_DRAW);

        // The element buffer binding is part of VAO state, so it must be bound
        // while the VAO is bound — and must NOT be unbound before the VAO is.
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     (GLsizeiptr)(idx.size() * sizeof(unsigned)),
                     idx.data(), GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                              (void*)offsetof(Vertex, position));
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                              (void*)offsetof(Vertex, normal));
        glEnableVertexAttribArray(1);

        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

        m_indexCount  = (GLsizei)idx.size();
        m_vertexCount = (int)verts.size();
    }

    GLuint  m_vao = 0, m_vbo = 0, m_ebo = 0;
    GLsizei m_indexCount  = 0;
    int     m_vertexCount = 0;
};

// ---------------------------------------------------------------------------
// Angle convention shared by every round generator:
//
//     x = r * sin(theta),   z = r * cos(theta)
//
// theta = 0 is +Z and theta grows toward +X. Seen from above (+Y looking down)
// that is counter-clockwise on screen, which is what makes the cap, grid and ring
// windings below come out front-facing without a special case.
// ---------------------------------------------------------------------------
static const float MESH_TWO_PI = 6.28318530718f;
static const float MESH_PI     = 3.14159265359f;

// ---------------------------------------------------------------------------
// M1 — Unit cube. Hull, deck, cannon mount, enemy hull.
//
// 24 vertices, not 8: each face needs its own normal, and a shared corner vertex
// can only carry one. Sharing them would average three face normals into a
// rounded-off corner and destroy the flat shading a box is supposed to have.
// ---------------------------------------------------------------------------
inline Mesh makeCube()
{
    const float h = 0.5f;

    const glm::vec3 faceNormal[6] = {
        {  0,  0,  1 }, {  0,  0, -1 },
        {  1,  0,  0 }, { -1,  0,  0 },
        {  0,  1,  0 }, {  0, -1,  0 },
    };
    // tangent and bitangent are chosen so that cross(tangent, bitangent) == normal.
    // That makes the shared 0,1,2 / 0,2,3 index pattern counter-clockwise seen from
    // outside on all six faces, with no per-face special case.
    const glm::vec3 faceTangent[6] = {
        {  1,  0,  0 }, { -1,  0,  0 },
        {  0,  0, -1 }, {  0,  0,  1 },
        {  1,  0,  0 }, { -1,  0,  0 },
    };
    const glm::vec3 faceBitangent[6] = {
        {  0,  1,  0 }, {  0,  1,  0 },
        {  0,  1,  0 }, {  0,  1,  0 },
        {  0,  0, -1 }, {  0,  0, -1 },
    };

    std::vector<Vertex>   verts;
    std::vector<unsigned> idx;
    verts.reserve(24);
    idx.reserve(36);

    for (int f = 0; f < 6; ++f) {
        const glm::vec3 n = faceNormal[f];
        const glm::vec3 t = faceTangent[f];
        const glm::vec3 b = faceBitangent[f];
        const glm::vec3 c = n * h;                 // face centre

        const unsigned base = (unsigned)verts.size();
        verts.push_back({ c - t * h - b * h, n });
        verts.push_back({ c + t * h - b * h, n });
        verts.push_back({ c + t * h + b * h, n });
        verts.push_back({ c - t * h + b * h, n });

        idx.push_back(base + 0); idx.push_back(base + 1); idx.push_back(base + 2);
        idx.push_back(base + 0); idx.push_back(base + 2); idx.push_back(base + 3);
    }

    return Mesh(verts, idx);
}

// ---------------------------------------------------------------------------
// M2 — Unit cylinder, axis along +Y, radius 0.5, height 1. Barrel, masts, yards.
//
// PARAMETERISED — this is the Demo B object (L9 s27). At 6 segments in Flat mode
// it facets and Mach-bands; at 64 it is smooth. The whole point of the + / - keys
// is that this number changes at runtime.
//
//   side normal = normalize(vec3(x, 0, z))   — analytic (guide 4.2)
//   cap normal  = (0, +-1, 0)
//
// The side and the caps therefore CANNOT share their rim vertices: the same rim
// position carries two different normals depending on which surface you are on.
// ---------------------------------------------------------------------------
inline Mesh makeCylinder(int segments)
{
    if (segments < 3) segments = 3;

    const float r  = 0.5f;
    const float hy = 0.5f;

    std::vector<Vertex>   verts;
    std::vector<unsigned> idx;
    verts.reserve((size_t)segments * 4 + 2);
    idx.reserve((size_t)segments * 12);

    // --- side ---------------------------------------------------------------
    // No seam duplication here: theta = 0 and theta = 2*pi give an identical
    // position AND an identical normal, so the ring closes on index wrapping.
    const unsigned sideBottom = 0;
    const unsigned sideTop    = (unsigned)segments;
    for (int ring = 0; ring < 2; ++ring) {
        const float y = (ring == 0) ? -hy : hy;
        for (int i = 0; i < segments; ++i) {
            const float th = MESH_TWO_PI * (float)i / (float)segments;
            const float s = std::sin(th), c = std::cos(th);
            verts.push_back({ glm::vec3(r * s, y, r * c), glm::vec3(s, 0.0f, c) });
        }
    }
    for (int i = 0; i < segments; ++i) {
        const unsigned i0 = (unsigned)i;
        const unsigned i1 = (unsigned)((i + 1) % segments);
        // seen from outside: lower-left, lower-right, upper-right / then upper-left
        idx.push_back(sideBottom + i0); idx.push_back(sideBottom + i1); idx.push_back(sideTop + i1);
        idx.push_back(sideBottom + i0); idx.push_back(sideTop    + i1); idx.push_back(sideTop + i0);
    }

    // --- caps ---------------------------------------------------------------
    // +Y cap: seen from above, increasing theta runs counter-clockwise, so
    // (centre, rim[i], rim[i+1]) is front-facing. The -Y cap reverses the pair.
    for (int cap = 0; cap < 2; ++cap) {
        const float     y      = (cap == 0) ? hy : -hy;
        const glm::vec3 n      = (cap == 0) ? glm::vec3(0, 1, 0) : glm::vec3(0, -1, 0);
        const unsigned  centre = (unsigned)verts.size();

        verts.push_back({ glm::vec3(0.0f, y, 0.0f), n });
        for (int i = 0; i < segments; ++i) {
            const float th = MESH_TWO_PI * (float)i / (float)segments;
            verts.push_back({ glm::vec3(r * std::sin(th), y, r * std::cos(th)), n });
        }

        const unsigned rim = centre + 1;
        for (int i = 0; i < segments; ++i) {
            const unsigned i0 = rim + (unsigned)i;
            const unsigned i1 = rim + (unsigned)((i + 1) % segments);
            if (cap == 0) { idx.push_back(centre); idx.push_back(i0); idx.push_back(i1); }
            else          { idx.push_back(centre); idx.push_back(i1); idx.push_back(i0); }
        }
    }

    return Mesh(verts, idx);
}

// ---------------------------------------------------------------------------
// M3 — Unit sphere, radius 0.5 (diameter 1, so S(d) gives a d-wide ball).
// Cannonball, muzzle flash, smoke puffs, splash puffs.
//
// PARAMETERISED. The normal is exactly normalize(position), so any highlight
// artefact seen on this mesh is the shading model's doing and not the geometry's.
// That is what makes the Gouraud-vs-Phong comparison a fair test.
// ---------------------------------------------------------------------------
inline Mesh makeSphere(int stacks, int slices)
{
    if (stacks < 2) stacks = 2;
    if (slices < 3) slices = 3;

    const float r = 0.5f;

    std::vector<Vertex>   verts;
    std::vector<unsigned> idx;
    verts.reserve((size_t)(stacks + 1) * (size_t)(slices + 1));
    idx.reserve((size_t)stacks * (size_t)slices * 6);

    // The seam column IS duplicated here (slices + 1 columns). Positions and
    // normals match at theta = 0 and 2*pi, so it costs one strip of vertices and
    // buys uniform row-major index arithmetic with no modulo wrap.
    for (int i = 0; i <= stacks; ++i) {
        const float phi = MESH_PI * (float)i / (float)stacks;   // 0 at the +Y pole
        const float sp = std::sin(phi), cp = std::cos(phi);
        for (int j = 0; j <= slices; ++j) {
            const float th = MESH_TWO_PI * (float)j / (float)slices;
            const glm::vec3 dir(sp * std::sin(th), cp, sp * std::cos(th));
            // |dir| == 1 by construction, so this IS normalize(position)
            verts.push_back({ dir * r, dir });
        }
    }

    const int stride = slices + 1;
    for (int i = 0; i < stacks; ++i) {
        for (int j = 0; j < slices; ++j) {
            const unsigned upperL = (unsigned)(i * stride + j);
            const unsigned upperR = upperL + 1;
            const unsigned lowerL = (unsigned)((i + 1) * stride + j);
            const unsigned lowerR = lowerL + 1;

            // Both pole rows collapse to a point, so one triangle of each quad
            // there is degenerate. Emitting it would hand the rasteriser a
            // zero-area face for nothing.
            if (i != stacks - 1) {
                idx.push_back(lowerL); idx.push_back(lowerR); idx.push_back(upperR);
            }
            if (i != 0) {
                idx.push_back(lowerL); idx.push_back(upperR); idx.push_back(upperL);
            }
        }
    }

    return Mesh(verts, idx);
}

// ---------------------------------------------------------------------------
// M4 — Unit quad in the XY plane, normal (0, 0, 1). Sails x2, flag.
//
// Single-sided by design: a sail seen from behind is culled, which is correct and
// free. Phase 9 gives it a billow by displacing it, not by adding geometry.
// ---------------------------------------------------------------------------
inline Mesh makeQuad()
{
    const float h = 0.5f;
    const glm::vec3 n(0.0f, 0.0f, 1.0f);

    const std::vector<Vertex> verts = {
        { glm::vec3(-h, -h, 0.0f), n },
        { glm::vec3( h, -h, 0.0f), n },
        { glm::vec3( h,  h, 0.0f), n },
        { glm::vec3(-h,  h, 0.0f), n },
    };
    const std::vector<unsigned> idx = { 0, 1, 2, 0, 2, 3 };

    return Mesh(verts, idx);
}

// ---------------------------------------------------------------------------
// M5 — Ocean grid: N x N cells in the XZ plane, unit extent, normal (0, 1, 0).
//
// PARAMETERISED — the Demo A object (L9 s28). Flat at generation; Phase 8
// displaces it in the VERTEX SHADER (the GPU-side wave, Requirement 11) and
// recomputes the normal there from the analytic wave derivative.
//
// This is the one generator that uses computeSmoothNormals instead of a closed
// form. On a flat grid every averaged normal comes out exactly (0, 1, 0), which
// is precisely why it is the safe place to prove the L9 s20 code path: any other
// answer means the accumulation is wrong, and it is visible immediately in the
// normals-as-RGB debug view.
// ---------------------------------------------------------------------------
inline Mesh makeGrid(int N)
{
    if (N < 1) N = 1;

    std::vector<Vertex>   verts;
    std::vector<unsigned> idx;
    verts.reserve((size_t)(N + 1) * (size_t)(N + 1));
    idx.reserve((size_t)N * (size_t)N * 6);

    for (int i = 0; i <= N; ++i) {            // i runs along +Z
        const float z = -0.5f + (float)i / (float)N;
        for (int j = 0; j <= N; ++j) {        // j runs along +X
            const float x = -0.5f + (float)j / (float)N;
            verts.push_back({ glm::vec3(x, 0.0f, z), glm::vec3(0.0f, 1.0f, 0.0f) });
        }
    }

    const int stride = N + 1;
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < N; ++j) {
            // seen from above, -Z is screen-up, so row i is the far (upper) row
            const unsigned upperL = (unsigned)(i * stride + j);
            const unsigned upperR = upperL + 1;
            const unsigned lowerL = (unsigned)((i + 1) * stride + j);
            const unsigned lowerR = lowerL + 1;

            idx.push_back(lowerL); idx.push_back(lowerR); idx.push_back(upperR);
            idx.push_back(lowerL); idx.push_back(upperR); idx.push_back(upperL);
        }
    }

    computeSmoothNormals(verts, idx);   // N_v = (Sum N_i) / ||Sum N_i||  — L9 slide 20
    return Mesh(verts, idx);
}

// ---------------------------------------------------------------------------
// M7 — Unit cone, base radius 0.5 at y = -0.5, apex at y = +0.5. Bowsprit tip.
//
// The side normal is constant along a slant line and works out to
// normalize(vec3(sin(theta), r, cos(theta))) with r = 0.5. That comes from the
// cross product of the two surface tangents:
//
//   dP/dtheta x dP/dt = (1-t)*r * (sin(theta), r, cos(theta))
//
// The apex is duplicated per segment: a cone tip has no single normal, so one
// shared apex vertex would have to pick an arbitrary direction and smear it.
// ---------------------------------------------------------------------------
inline Mesh makeCone(int segments)
{
    if (segments < 3) segments = 3;

    const float r  = 0.5f;
    const float hy = 0.5f;

    std::vector<Vertex>   verts;
    std::vector<unsigned> idx;
    verts.reserve((size_t)segments * 4 + 1);
    idx.reserve((size_t)segments * 6);

    // --- side ---------------------------------------------------------------
    for (int i = 0; i < segments; ++i) {
        const float th0 = MESH_TWO_PI * (float)i       / (float)segments;
        const float th1 = MESH_TWO_PI * (float)(i + 1) / (float)segments;
        const float thM = 0.5f * (th0 + th1);          // apex normal: facet midpoint

        const glm::vec3 n0 = glm::normalize(glm::vec3(std::sin(th0), r, std::cos(th0)));
        const glm::vec3 n1 = glm::normalize(glm::vec3(std::sin(th1), r, std::cos(th1)));
        const glm::vec3 nM = glm::normalize(glm::vec3(std::sin(thM), r, std::cos(thM)));

        const unsigned base = (unsigned)verts.size();
        verts.push_back({ glm::vec3(r * std::sin(th0), -hy, r * std::cos(th0)), n0 });
        verts.push_back({ glm::vec3(r * std::sin(th1), -hy, r * std::cos(th1)), n1 });
        verts.push_back({ glm::vec3(0.0f, hy, 0.0f),                            nM });

        idx.push_back(base + 0); idx.push_back(base + 1); idx.push_back(base + 2);
    }

    // --- base cap (normal -Y, so the theta pair is reversed to face down) ----
    const unsigned  centre = (unsigned)verts.size();
    const glm::vec3 down(0.0f, -1.0f, 0.0f);
    verts.push_back({ glm::vec3(0.0f, -hy, 0.0f), down });
    for (int i = 0; i < segments; ++i) {
        const float th = MESH_TWO_PI * (float)i / (float)segments;
        verts.push_back({ glm::vec3(r * std::sin(th), -hy, r * std::cos(th)), down });
    }
    const unsigned rim = centre + 1;
    for (int i = 0; i < segments; ++i) {
        const unsigned i0 = rim + (unsigned)i;
        const unsigned i1 = rim + (unsigned)((i + 1) % segments);
        idx.push_back(centre); idx.push_back(i1); idx.push_back(i0);
    }

    return Mesh(verts, idx);
}

// ---------------------------------------------------------------------------
// M6 — Flat ring / annulus in the XZ plane, normal (0, 1, 0), outer radius 0.5.
// The expanding splash ring (Phase 14).
//
// Single-sided and upward-facing: the splash is always seen from above the
// waterline, so the back face is work nobody would have seen.
// ---------------------------------------------------------------------------
inline Mesh makeRing(int segments, float innerRadius = 0.35f)
{
    if (segments < 3) segments = 3;

    const float outer = 0.5f;
    const float inner = glm::clamp(innerRadius, 0.0f, outer - 1e-3f);

    std::vector<Vertex>   verts;
    std::vector<unsigned> idx;
    verts.reserve((size_t)segments * 2);
    idx.reserve((size_t)segments * 6);

    const glm::vec3 up(0.0f, 1.0f, 0.0f);
    for (int i = 0; i < segments; ++i) {
        const float th = MESH_TWO_PI * (float)i / (float)segments;
        const float s = std::sin(th), c = std::cos(th);
        verts.push_back({ glm::vec3(outer * s, 0.0f, outer * c), up });   // even = outer
        verts.push_back({ glm::vec3(inner * s, 0.0f, inner * c), up });   // odd  = inner
    }

    for (int i = 0; i < segments; ++i) {
        const unsigned o0 = (unsigned)(2 * i);
        const unsigned n0 = o0 + 1;
        const unsigned o1 = (unsigned)(2 * ((i + 1) % segments));
        const unsigned n1 = o1 + 1;

        // Same handedness as the +Y cap: increasing theta is CCW seen from above.
        idx.push_back(o0); idx.push_back(o1); idx.push_back(n1);
        idx.push_back(o0); idx.push_back(n1); idx.push_back(n0);
    }

    return Mesh(verts, idx);
}
