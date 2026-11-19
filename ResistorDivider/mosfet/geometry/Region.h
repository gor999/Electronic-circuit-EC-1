#pragma once

#include <cmath>
#include <cstdint>
#include <functional>
#include <vector>

// Plain geometry types (no Qt, no OpenGL) so the structure can be tested on its own.
namespace mosfet {

struct Vec3f
{
    float x = 0.0f, y = 0.0f, z = 0.0f;
};

inline Vec3f operator+(Vec3f a, Vec3f b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline Vec3f operator-(Vec3f a, Vec3f b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline Vec3f operator*(Vec3f a, float s) { return {a.x * s, a.y * s, a.z * s}; }
inline float dot(Vec3f a, Vec3f b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3f cross(Vec3f a, Vec3f b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
inline float length(Vec3f a) { return std::sqrt(dot(a, a)); }
inline Vec3f normalized(Vec3f a)
{
    const float l = length(a);
    return l > 0.0f ? a * (1.0f / l) : a;
}

// A solid made by extruding a cross-section along z (the channel width).
// The cross-section is the area between two curves over [xs.front(), xs.back()]:
// bottom(x) <= y <= top(x). Every part of the MOSFET is one of these, which
// makes meshing, containment and picking exact and simple.
struct Region
{
    std::vector<float> xs;       // strictly increasing
    std::vector<float> top;
    std::vector<float> bottom;
    float z0 = 0.0f;
    float z1 = 0.0f;

    bool empty() const { return xs.size() < 2; }
    float xMin() const { return xs.front(); }
    float xMax() const { return xs.back(); }
    float topAt(float x) const;
    float bottomAt(float x) const;
    bool contains(Vec3f p, float tolerance = 0.0f) const;
    float area() const;          // of the cross-section
    float volume() const { return area() * (z1 - z0); }
};

Region boxRegion(float x0, float x1, float y0, float y1, float z0, float z1);

// Triangle mesh with outward-facing counter-clockwise triangles.
// vertices: px py pz nx ny nz per vertex. edges: pairs of points (xyz xyz)
// along the visible outline, drawn as lines.
struct MeshData
{
    std::vector<float> vertices;
    std::vector<uint32_t> indices;
    std::vector<float> edges;
    std::size_t triangleCount() const { return indices.size() / 3; }
};

MeshData buildMesh(const Region &region);

// Volume enclosed by the mesh (divergence theorem). Positive when the mesh is
// closed and its triangles face outwards.
double signedVolume(const MeshData &mesh);

// Nearest intersection t in (0, tMax) of origin + t*dir with the mesh whose
// hit point passes `accept`. Returns false if there is none.
bool intersectRay(const MeshData &mesh, Vec3f origin, Vec3f dir, float tMax,
                  const std::function<bool(Vec3f)> &accept, float &t);

} // namespace mosfet
