# Phase 4 Explanation

## Status

Phase 4 is complete.

The mesh library is still correct after Phase 5 added lighting.

## Goal

The goal was to generate reusable shapes with correct normals and changeable detail.

## Mesh Data

Each vertex contains:

```cpp
struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
};
```

The `Mesh` class owns one VAO, one VBO, and one EBO. It uploads data once and draws indexed triangles with `glDrawElements`.

The class cannot be copied. It can be moved safely, and old OpenGL objects are deleted when a mesh is replaced.

## Shape Generators

Seven generators cover the eight PRD mesh roles. The HUD reuses the quad.

| Function | Shape | Later use |
|---|---|---|
| `makeCube()` | Cube | Hull, deck, mount |
| `makeCylinder(segments)` | Cylinder | Barrel, masts, yards |
| `makeSphere(stacks, slices)` | Sphere | Cannonball and effects |
| `makeQuad()` | Quad | Sails, flag, HUD |
| `makeGrid(N)` | Grid | Ocean |
| `makeRing(segments)` | Ring | Splash |
| `makeCone(segments)` | Cone | Bowsprit tip |

Every shape fits inside a unit box from `-0.5` to `+0.5`. Model matrices scale these unit shapes to the needed size.

## Normals

A normal tells the lighting shader which way a surface faces.

| Surface | Normal rule |
|---|---|
| Sphere | Normalized position from the centre |
| Cylinder side | Outward from the axis |
| Cylinder caps | Exactly up or down |
| Quad | Forward along positive Z |
| Grid and ring | Up along positive Y |

The flat grid also passes through `computeSmoothNormals()`. The function adds nearby face normals and normalizes the result. Every grid normal becomes exactly `(0, 1, 0)`.

## Triangle Order

Triangles use counter-clockwise order when viewed from outside. This matches OpenGL back-face culling.

The audit confirmed that every tested triangle has a real area and that its order agrees with its vertex normals.

## Detail Controls

| Level | Barrel segments | Ocean resolution | Scene triangles |
|---|---:|---:|---:|
| Minimum | 6 | 8 | 214 |
| Default | 32 | 64 | 9,422 |
| Maximum | 64 | 128 | 37,262 |

`+` increases detail. `-` decreases it. `F` changes between solid and wireframe drawing.

Meshes rebuild only when a detail value changes. They do not rebuild every frame. The default scene stays below the PRD limit of 25,000 triangles and uses 7 draw calls.

## Checks That Passed

The combined OpenGL audit checked:

- vertex, index, and triangle counts;
- index ranges;
- finite positions and normals;
- unit-length normals;
- unit-size bounds;
- non-empty triangles;
- triangle order;
- sphere and cylinder analytic normals;
- cylinder cap normals;
- averaged grid normals;
- minimum, default, and maximum detail levels.

All checks passed. The test also rebuilt a mesh 200 times. Old VAO, VBO, and EBO objects were deleted, the final mesh stayed valid, and OpenGL reported no error.

The live program showed solid shapes, a low-detail wireframe, 214 triangles at minimum detail, and 9,422 triangles at default detail.

## Result

The Phase 4 checkpoint is passed. The mesh library is correct and ready for all later scene objects.
