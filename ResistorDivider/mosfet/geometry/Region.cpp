#include "Region.h"

#include <algorithm>

namespace mosfet {

namespace {

const float kSmoothCos = 0.766f;   // cos 40 deg: adjacent faces closer than this share a normal
const float kCreaseCos = 0.866f;   // cos 30 deg: sharper corners get an outline edge

float interpolate(const std::vector<float> &xs, const std::vector<float> &ys, float x)
{
    if (x <= xs.front())
        return ys.front();
    if (x >= xs.back())
        return ys.back();
    const auto upper = std::upper_bound(xs.begin(), xs.end(), x);
    const std::size_t i = static_cast<std::size_t>(upper - xs.begin());
    const float t = (x - xs[i - 1]) / (xs[i] - xs[i - 1]);
    return ys[i - 1] + (ys[i] - ys[i - 1]) * t;
}

class MeshBuilder
{
public:
    explicit MeshBuilder(MeshData &mesh) : m_mesh(mesh) {}

    uint32_t vertex(Vec3f p, Vec3f n)
    {
        const float data[6] = {p.x, p.y, p.z, n.x, n.y, n.z};
        m_mesh.vertices.insert(m_mesh.vertices.end(), data, data + 6);
        return static_cast<uint32_t>(m_mesh.vertices.size() / 6 - 1);
    }

    // a, b, c, d counter-clockwise when seen from outside.
    void quad(uint32_t a, uint32_t b, uint32_t c, uint32_t d)
    {
        const uint32_t data[6] = {a, b, c, a, c, d};
        m_mesh.indices.insert(m_mesh.indices.end(), data, data + 6);
    }

    void edge(Vec3f a, Vec3f b)
    {
        const float data[6] = {a.x, a.y, a.z, b.x, b.y, b.z};
        m_mesh.edges.insert(m_mesh.edges.end(), data, data + 6);
    }

private:
    MeshData &m_mesh;
};

// Outward normals of a boundary curve's segments in the x-y plane.
// `up` selects the top boundary (normal points to +y side) or the bottom one.
std::vector<Vec3f> segmentNormals(const std::vector<float> &xs, const std::vector<float> &ys, bool up)
{
    std::vector<Vec3f> normals(xs.size() - 1);
    for (std::size_t k = 0; k + 1 < xs.size(); ++k) {
        const float dx = xs[k + 1] - xs[k];
        const float dy = ys[k + 1] - ys[k];
        normals[k] = up ? normalized(Vec3f{-dy, dx, 0.0f}) : normalized(Vec3f{dy, -dx, 0.0f});
    }
    return normals;
}

// Normal used at end `k` of segment `segment`: smoothed with the neighbouring
// segment when the boundary is curved, the segment's own normal at a corner.
Vec3f vertexNormal(const std::vector<Vec3f> &normals, std::size_t segment, std::size_t k)
{
    const Vec3f own = normals[segment];
    const std::size_t neighbour = (k == segment) ? segment - 1 : segment + 1;
    if ((k == segment && segment == 0) || (k != segment && segment + 1 >= normals.size()))
        return own;
    const Vec3f other = normals[neighbour];
    if (dot(own, other) < kSmoothCos)
        return own;
    return normalized(own + other);
}

void addBoundary(MeshBuilder &b, const Region &r, const std::vector<float> &ys, bool up)
{
    const std::vector<Vec3f> normals = segmentNormals(r.xs, ys, up);
    for (std::size_t k = 0; k + 1 < r.xs.size(); ++k) {
        const Vec3f n0 = vertexNormal(normals, k, k);
        const Vec3f n1 = vertexNormal(normals, k, k + 1);
        const uint32_t a = b.vertex({r.xs[k], ys[k], r.z1}, n0);
        const uint32_t bb = b.vertex({r.xs[k + 1], ys[k + 1], r.z1}, n1);
        const uint32_t c = b.vertex({r.xs[k + 1], ys[k + 1], r.z0}, n1);
        const uint32_t d = b.vertex({r.xs[k], ys[k], r.z0}, n0);
        if (up)
            b.quad(a, bb, c, d);
        else
            b.quad(a, d, c, bb);
    }
    // Lengthwise outline edges at the corners of the cross-section.
    for (std::size_t k = 1; k + 1 < r.xs.size(); ++k) {
        if (dot(normals[k - 1], normals[k]) < kCreaseCos)
            b.edge({r.xs[k], ys[k], r.z0}, {r.xs[k], ys[k], r.z1});
    }
}

} // namespace

float Region::topAt(float x) const { return interpolate(xs, top, x); }
float Region::bottomAt(float x) const { return interpolate(xs, bottom, x); }

bool Region::contains(Vec3f p, float tolerance) const
{
    if (empty() || p.z < z0 - tolerance || p.z > z1 + tolerance)
        return false;
    if (p.x < xMin() - tolerance || p.x > xMax() + tolerance)
        return false;
    return p.y <= topAt(p.x) + tolerance && p.y >= bottomAt(p.x) - tolerance;
}

float Region::area() const
{
    double sum = 0.0;
    for (std::size_t k = 0; k + 1 < xs.size(); ++k) {
        const double t0 = top[k] - bottom[k], t1 = top[k + 1] - bottom[k + 1];
        sum += 0.5 * (t0 + t1) * (xs[k + 1] - xs[k]);
    }
    return static_cast<float>(sum);
}

Region boxRegion(float x0, float x1, float y0, float y1, float z0, float z1)
{
    Region r;
    r.xs = {x0, x1};
    r.top = {y1, y1};
    r.bottom = {y0, y0};
    r.z0 = z0;
    r.z1 = z1;
    return r;
}

MeshData buildMesh(const Region &r)
{
    MeshData mesh;
    if (r.empty())
        return mesh;
    MeshBuilder b(mesh);
    const std::size_t n = r.xs.size();

    // Front (z1) and back (z0) faces: strips between the bottom and top curves.
    for (int side = 0; side < 2; ++side) {
        const bool front = side == 0;
        const float z = front ? r.z1 : r.z0;
        const Vec3f normal{0.0f, 0.0f, front ? 1.0f : -1.0f};
        std::vector<uint32_t> lower(n), upper(n);
        for (std::size_t k = 0; k < n; ++k) {
            lower[k] = b.vertex({r.xs[k], r.bottom[k], z}, normal);
            upper[k] = b.vertex({r.xs[k], r.top[k], z}, normal);
        }
        for (std::size_t k = 0; k + 1 < n; ++k) {
            if (front)
                b.quad(lower[k], lower[k + 1], upper[k + 1], upper[k]);
            else
                b.quad(lower[k], upper[k], upper[k + 1], lower[k + 1]);
            b.edge({r.xs[k], r.top[k], z}, {r.xs[k + 1], r.top[k + 1], z});
            b.edge({r.xs[k], r.bottom[k], z}, {r.xs[k + 1], r.bottom[k + 1], z});
        }
    }

    addBoundary(b, r, r.top, true);
    addBoundary(b, r, r.bottom, false);

    // End walls.
    for (int side = 0; side < 2; ++side) {
        const std::size_t k = side == 0 ? 0 : n - 1;
        const float x = r.xs[k], y0 = r.bottom[k], y1 = r.top[k];
        if (y1 - y0 < 1.0e-6f)
            continue;
        const Vec3f normal{side == 0 ? -1.0f : 1.0f, 0.0f, 0.0f};
        const uint32_t p1 = b.vertex({x, y0, r.z1}, normal);
        const uint32_t p2 = b.vertex({x, y1, r.z1}, normal);
        const uint32_t p3 = b.vertex({x, y1, r.z0}, normal);
        const uint32_t p4 = b.vertex({x, y0, r.z0}, normal);
        if (side == 0)
            b.quad(p1, p2, p3, p4);
        else
            b.quad(p1, p4, p3, p2);
        b.edge({x, y0, r.z1}, {x, y1, r.z1});
        b.edge({x, y0, r.z0}, {x, y1, r.z0});
        b.edge({x, y0, r.z0}, {x, y0, r.z1});
        b.edge({x, y1, r.z0}, {x, y1, r.z1});
    }
    return mesh;
}

double signedVolume(const MeshData &mesh)
{
    double volume = 0.0;
    const auto point = [&](uint32_t i) {
        return Vec3f{mesh.vertices[i * 6], mesh.vertices[i * 6 + 1], mesh.vertices[i * 6 + 2]};
    };
    for (std::size_t t = 0; t + 2 < mesh.indices.size(); t += 3) {
        const Vec3f a = point(mesh.indices[t]), b = point(mesh.indices[t + 1]), c = point(mesh.indices[t + 2]);
        volume += static_cast<double>(dot(a, cross(b, c))) / 6.0;
    }
    return volume;
}

bool intersectRay(const MeshData &mesh, Vec3f origin, Vec3f dir, float tMax,
                  const std::function<bool(Vec3f)> &accept, float &tHit)
{
    bool found = false;
    float best = tMax;
    const auto point = [&](uint32_t i) {
        return Vec3f{mesh.vertices[i * 6], mesh.vertices[i * 6 + 1], mesh.vertices[i * 6 + 2]};
    };
    for (std::size_t t = 0; t + 2 < mesh.indices.size(); t += 3) {
        // Moller-Trumbore, double-sided.
        const Vec3f a = point(mesh.indices[t]);
        const Vec3f e1 = point(mesh.indices[t + 1]) - a;
        const Vec3f e2 = point(mesh.indices[t + 2]) - a;
        const Vec3f p = cross(dir, e2);
        const float det = dot(e1, p);
        if (std::fabs(det) < 1.0e-12f)
            continue;
        const float inv = 1.0f / det;
        const Vec3f s = origin - a;
        const float u = dot(s, p) * inv;
        if (u < 0.0f || u > 1.0f)
            continue;
        const Vec3f q = cross(s, e1);
        const float v = dot(dir, q) * inv;
        if (v < 0.0f || u + v > 1.0f)
            continue;
        const float distance = dot(e2, q) * inv;
        if (distance <= 1.0e-5f || distance >= best)
            continue;
        if (accept && !accept(origin + dir * distance))
            continue;
        best = distance;
        found = true;
    }
    if (found)
        tHit = best;
    return found;
}

} // namespace mosfet
