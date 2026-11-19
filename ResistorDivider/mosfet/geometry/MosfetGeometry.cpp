#include "MosfetGeometry.h"

#include "physics/DeviceState.h"
#include "physics/MosfetParameters.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace mosfet {

namespace {

const float kHalfPi = 1.57079632679f;
const float kGridStep = 0.02f;
const int kCornerSamples = 28;

// Depth of a diffused region that reaches the surface at `left` and `right`,
// has a flat bottom at `depth`, and quarter-ellipse corners `corner` wide.
float diffusionDepth(float x, float left, float right, float depth, float corner)
{
    if (x <= left || x >= right)
        return 0.0f;
    float u = 0.0f;
    if (x < left + corner)
        u = (left + corner - x) / corner;
    else if (x > right - corner)
        u = (x - (right - corner)) / corner;
    else
        return depth;
    return depth * std::sqrt(std::max(0.0f, 1.0f - u * u));
}

// Sample positions that resolve the steep sides of the elliptical corners.
void addCornerSamples(std::vector<float> &xs, float left, float right, float corner)
{
    for (int k = 0; k <= kCornerSamples; ++k) {
        const float c = 1.0f - std::cos(kHalfPi * k / kCornerSamples);
        xs.push_back(left + corner * c);
        xs.push_back(right - corner * c);
    }
}

std::vector<float> sortedUnique(std::vector<float> xs)
{
    std::sort(xs.begin(), xs.end());
    std::vector<float> out;
    for (float x : xs) {
        if (out.empty() || x - out.back() > 1.0e-6f)
            out.push_back(x);
    }
    return out;
}

// Grid points strictly inside (a, b), plus a and b themselves.
std::vector<float> subset(const std::vector<float> &grid, float a, float b)
{
    std::vector<float> out{a};
    for (float x : grid) {
        if (x > a + 1.0e-6f && x < b - 1.0e-6f)
            out.push_back(x);
    }
    out.push_back(b);
    return out;
}

float interpolate(const std::vector<double> &xs, const std::vector<double> &ys, double x)
{
    if (xs.empty())
        return 0.0f;
    if (x <= xs.front())
        return static_cast<float>(ys.front());
    if (x >= xs.back())
        return static_cast<float>(ys.back());
    const auto upper = std::upper_bound(xs.begin(), xs.end(), x);
    const std::size_t i = static_cast<std::size_t>(upper - xs.begin());
    const double t = (x - xs[i - 1]) / (xs[i] - xs[i - 1]);
    return static_cast<float>(ys[i - 1] + (ys[i] - ys[i - 1]) * t);
}

} // namespace

void MosfetGeometry::addPart(PartId id, Region region, float explodeWeight, bool translucent)
{
    Part part;
    part.id = id;
    part.region = std::move(region);
    part.mesh = buildMesh(part.region);
    part.explodeWeight = explodeWeight;
    part.translucent = translucent;

    const Region &r = part.region;
    part.boundsMin = {r.xMin(), *std::min_element(r.bottom.begin(), r.bottom.end()), r.z0};
    part.boundsMax = {r.xMax(), *std::max_element(r.top.begin(), r.top.end()), r.z1};

    // Label anchors on the front face, at the thickest of a few positions;
    // the top face centre as a last resort.
    for (float f : {0.5f, 0.3f, 0.7f, 0.15f, 0.85f}) {
        const float x = r.xMin() + f * (r.xMax() - r.xMin());
        const float top = r.topAt(x), bottom = r.bottomAt(x);
        if (top - bottom > 0.02f)
            part.anchors.push_back({x, 0.5f * (top + bottom), r.z1});
    }
    const float xc = 0.5f * (r.xMin() + r.xMax());
    part.anchors.push_back({xc, r.topAt(xc), 0.5f * (r.z0 + r.z1)});
    m_parts.push_back(std::move(part));
}

MosfetGeometry MosfetGeometry::build(const MosfetParameters &p, const DeviceState &state, const DisplayScale &base)
{
    MosfetGeometry g;
    DisplayScale s = base;
    const float um = 1.0e6f;
    auto micro = [&](double metres) { return float(metres) * um; };

    // Depth scale: x2 normally, reduced so the deepest feature stays under ~3.2 units.
    double deepestGate = 0.0;
    for (double d : state.gateDepletion)
        deepestGate = std::max(deepestGate, d);
    const float deepest = std::max({micro(p.junctionDepth + std::max(state.sourceJunctionDepletion, state.drainJunctionDepletion)),
                                    micro(deepestGate), 0.05f});
    s.siliconDepth = std::min(base.siliconDepth, 3.2f / deepest);

    const float half = 0.5f * micro(p.channelLength) * s.lateral;
    const float sourceLength = micro(p.sourceLength) * s.lateral;
    const float drainLength = micro(p.drainLength) * s.lateral;
    const float overlap = micro(p.gateOverlap()) * s.lateral;
    const float xj = micro(p.junctionDepth) * s.siliconDepth;
    const float corner = micro(p.lateralDiffusion()) * s.lateral;
    // Oxide: exaggerated but monotonic, visible even at 1 nm.
    const float tox = std::min(0.05f + 7.0f * micro(p.oxideThickness), 1.0f);
    s.oxide = tox / micro(p.oxideThickness);
    const float tGate = std::clamp(micro(p.gateThickness) * s.gate, 0.12f, 1.5f);
    const float width = std::clamp(micro(p.channelWidth) * s.width, 0.3f, 12.0f);
    const float z1 = 0.5f * width, z0 = -z1;

    // Junction depletion on the p side; the n+ side is negligibly thin.
    const float wxS = micro(state.sourceJunctionDepletion) * s.lateral;
    const float wyS = micro(state.sourceJunctionDepletion) * s.siliconDepth;
    const float wxD = micro(state.drainJunctionDepletion) * s.lateral;
    const float wyD = micro(state.drainJunctionDepletion) * s.siliconDepth;
    s.siliconMargin = std::max(base.siliconMargin, 1.3f * std::max(wxS, wxD) + 0.2f);

    const float aS = -(half + sourceLength), bS = -half, aD = half, bD = half + drainLength;
    // Either junction's depletion can reach past the other one (punch-through).
    const float depletionLeft = std::min(aS - wxS, aD - wxD), depletionRight = std::max(bS + wxS, bD + wxD);
    const float xLeft = std::min(aS - s.siliconMargin, depletionLeft - 0.3f);
    const float xRight = std::max(bD + s.siliconMargin, depletionRight + 0.3f);

    // Substrate drawn deeper for thicker wafers (log scale), always below the deepest feature.
    const float wafer = 1.2f + 0.5f * std::log10(std::max(1.0f, micro(p.substrateThickness) / 50.0f));
    s.substrateDepth = std::max(wafer, std::max(deepest * s.siliconDepth, xj) + 0.6f);
    const float yBottom = -s.substrateDepth;
    g.m_scale = s;
    g.drawnWidth = width;
    g.sourceEdge = bS;

    auto sourceDepth = [&](float x) { return diffusionDepth(x, aS, bS, xj, corner); };
    auto drainDepth = [&](float x) { return diffusionDepth(x, aD, bD, xj, corner); };
    auto wellDepth = [&](float x) { return std::max(sourceDepth(x), drainDepth(x)); };
    auto shellDepth = [&](float x) {
        return std::max(diffusionDepth(x, aS - wxS, bS + wxS, xj + wyS, corner + wxS),
                        diffusionDepth(x, aD - wxD, bD + wxD, xj + wyD, corner + wxD));
    };
    // Profiles along the channel, mapped from metres-from-source to display x.
    auto channelX = [&](float x) { return (static_cast<double>(x) + half) / (double(um) * s.lateral); };
    auto gateDepletion = [&](float x) {
        if (x < -half - overlap || x > half + overlap)
            return 0.0f;
        return interpolate(state.x, state.gateDepletion, channelX(std::clamp(x, -half, half))) * um * s.siliconDepth;
    };
    // Drawn thickness follows the calculated inversion charge, saturating so
    // very large charges stay drawable: t = tmax Q / (Q + Qref). Beyond the
    // pinch-off point there is no inversion layer.
    const double channelEnd = state.pinchedOff ? state.pinchOffPosition : p.channelLength;
    auto channelThickness = [&](float x) {
        if (!state.channelExists || x < -half || x > half)
            return 0.0f;
        const double xm = channelX(x);
        if (xm > channelEnd)
            return 0.0f;
        const double q = std::max(0.0f, interpolate(state.x, state.inversionCharge, xm));
        return s.inversionMax * float(q / (q + kInversionChargeReference));
    };
    auto depletionDepth = [&](float x) { return std::max(shellDepth(x), gateDepletion(x)); };

    // Shared sample grid: uniform, refined on every curved corner.
    std::vector<float> grid;
    for (float x = xLeft; x <= xRight + 1.0e-5f; x += kGridStep)
        grid.push_back(x);
    addCornerSamples(grid, aS, bS, corner);
    addCornerSamples(grid, aD, bD, corner);
    for (int k = 0; k <= kCornerSamples; ++k) {
        const float c = std::cos(kHalfPi * k / kCornerSamples);
        grid.push_back(aS + corner - (corner + wxS) * c);
        grid.push_back(bS - corner + (corner + wxS) * c);
        grid.push_back(aD + corner - (corner + wxD) * c);
        grid.push_back(bD - corner + (corner + wxD) * c);
    }
    for (double xm : state.x)
        grid.push_back(static_cast<float>(xm * um * s.lateral) - half);
    if (state.pinchedOff)
        grid.push_back(static_cast<float>(state.pinchOffPosition * um * s.lateral) - half);
    for (float x : {-half - overlap, half + overlap, -half, half})
        grid.push_back(x);
    grid = sortedUnique(grid);
    grid.erase(std::remove_if(grid.begin(), grid.end(), [&](float x) { return x < xLeft || x > xRight; }), grid.end());

    auto makeRegion = [&](float a, float b, auto top, auto bottom) {
        Region r;
        r.xs = subset(grid, a, b);
        for (float x : r.xs) {
            r.top.push_back(top(x));
            r.bottom.push_back(std::min(bottom(x), top(x)));
        }
        r.z0 = z0;
        r.z1 = z1;
        return r;
    };
    auto above = [&](float x) { return std::max(wellDepth(x), channelThickness(x)); };

    // --- Silicon: four regions that tile the cross-section without overlap ---
    float channelMax = 0.0f;
    for (float x : grid)
        channelMax = std::max(channelMax, channelThickness(x));
    if (channelMax > 1.0e-4f) {
        const float end = std::min(half, static_cast<float>(channelEnd * um * s.lateral) - half);
        g.addPart(PartId::Channel,
                  makeRegion(-half, end, [](float) { return 0.0f; }, [&](float x) { return -channelThickness(x); }),
                  0.45f);
    }
    g.addPart(PartId::Source, makeRegion(aS, bS, [](float) { return 0.0f; }, [&](float x) { return -sourceDepth(x); }),
              0.3f);
    g.addPart(PartId::Drain, makeRegion(aD, bD, [](float) { return 0.0f; }, [&](float x) { return -drainDepth(x); }),
              0.3f);
    g.addPart(PartId::Depletion,
              makeRegion(std::max(xLeft, depletionLeft), std::min(xRight, depletionRight), [&](float x) { return -above(x); },
                         [&](float x) { return -std::max(depletionDepth(x), above(x)); }),
              0.12f);
    g.addPart(PartId::Substrate,
              makeRegion(xLeft, xRight, [&](float x) { return -std::max(depletionDepth(x), above(x)); },
                         [&](float) { return yBottom; }),
              0.0f);

    // --- Gate stack and contacts ---------------------------------------------
    const float gl = -half - overlap, gr = half + overlap;
    g.addPart(PartId::GateOxide, boxRegion(gl, gr, 0.0f, tox, z0, z1), 0.8f, true);
    g.addPart(PartId::Gate, boxRegion(gl, gr, tox, tox + tGate, z0, z1), 1.5f);
    const float gc = std::min(0.45f, 0.4f * gr);
    g.addPart(PartId::GateContact, boxRegion(-gc, gc, tox + tGate, tox + tGate + 0.18f, z0 * 0.6f, z1 * 0.6f), 2.0f);
    const float inset = 0.25f * std::min(sourceLength, drainLength);
    g.addPart(PartId::SourceContact, boxRegion(aS + inset, bS - inset, 0.0f, 0.26f, z0 * 0.66f, z1 * 0.66f), 0.8f);
    g.addPart(PartId::DrainContact, boxRegion(aD + inset, bD - inset, 0.0f, 0.26f, z0 * 0.66f, z1 * 0.66f), 0.8f);
    g.addPart(PartId::BodyContact, boxRegion(xLeft, xRight, yBottom - 0.1f, yBottom, z0, z1), -0.6f);

    // Order: opaque silicon first, then the rest (the renderer sorts translucent parts).
    g.zFront = z1;
    g.zBack = z0;
    g.siliconLeft = xLeft;
    g.siliconRight = xRight;
    g.siliconBottom = yBottom;
    g.drainEdge = aD;
    g.gateLeft = gl;
    g.gateRight = gr;
    g.gateTop = tox + tGate;
    g.boundsMin = {xLeft, yBottom - 0.1f, z0};
    g.boundsMax = {xRight, tox + tGate + 0.18f, z1};
    return g;
}

float MosfetGeometry::channelThicknessAt(double metres) const
{
    const Part *channel = part(PartId::Channel);
    if (!channel)
        return 0.0f;
    const float x = toDisplayX(metres);
    if (x < channel->region.xMin() || x > channel->region.xMax())
        return 0.0f;
    return -channel->region.bottomAt(x);
}

const Part *MosfetGeometry::part(PartId id) const
{
    for (const Part &p : m_parts) {
        if (p.id == id)
            return &p;
    }
    return nullptr;
}

int MosfetGeometry::pick(Vec3f origin, Vec3f dir, float explode, const ClipPlane &clip, const PartFlags &visible,
                         bool includeTranslucent, float *tHit) const
{
    float best = std::numeric_limits<float>::max();
    int found = -1;
    for (const Part &part : m_parts) {
        if (!visible[index(part.id)] || (part.translucent && !includeTranslucent))
            continue;
        const Vec3f off = offset(part, explode);
        const Vec3f local = origin - off;

        // Slab test against the bounding box first.
        float tNear = 0.0f, tFar = best;
        bool miss = false;
        const float o[3] = {local.x, local.y, local.z}, d[3] = {dir.x, dir.y, dir.z};
        const float lo[3] = {part.boundsMin.x, part.boundsMin.y, part.boundsMin.z};
        const float hi[3] = {part.boundsMax.x, part.boundsMax.y, part.boundsMax.z};
        for (int a = 0; a < 3 && !miss; ++a) {
            if (std::fabs(d[a]) < 1.0e-9f) {
                miss = o[a] < lo[a] - 1.0e-4f || o[a] > hi[a] + 1.0e-4f;
                continue;
            }
            float t0 = (lo[a] - 1.0e-4f - o[a]) / d[a], t1 = (hi[a] + 1.0e-4f - o[a]) / d[a];
            if (t0 > t1)
                std::swap(t0, t1);
            tNear = std::max(tNear, t0);
            tFar = std::min(tFar, t1);
            miss = tNear > tFar;
        }
        if (miss)
            continue;

        float t = 0.0f;
        const auto kept = [&](Vec3f p) { return clip.distance(p + off) >= -1.0e-4f; };
        if (intersectRay(part.mesh, local, dir, best, kept, t)) {
            best = t;
            found = index(part.id);
        }
    }

    // Cut faces: where the ray meets the section plane inside a solid part.
    if (clip.enabled) {
        const float denominator = dot(clip.normal, dir);
        if (std::fabs(denominator) > 1.0e-9f) {
            const float t = -(dot(clip.normal, origin) + clip.offset) / denominator;
            if (t > 0.0f && t < best) {
                const Vec3f q = origin + dir * t;
                for (const Part &part : m_parts) {
                    if (!visible[index(part.id)] || part.translucent)
                        continue;
                    if (part.region.contains(q - offset(part, explode), 1.0e-4f)) {
                        best = t;
                        found = index(part.id);
                        break;
                    }
                }
            }
        }
    }
    if (found >= 0 && tHit)
        *tHit = best;
    return found;
}

int MosfetGeometry::siliconPartAt(Vec3f p) const
{
    for (PartId id : {PartId::Channel, PartId::Source, PartId::Drain, PartId::Depletion, PartId::Substrate}) {
        const Part *candidate = part(id);
        if (candidate && candidate->region.contains(p))
            return index(id);
    }
    return -1;
}

Lattice MosfetGeometry::lattice(const ClipPlane &clip, float explode, const PartFlags &visible) const
{
    Lattice lattice;
    const float spacing = 0.15f;
    lattice.radius = 0.032f;
    const bool acrossChannel = clip.enabled && std::fabs(clip.normal.x) > 0.5f;
    const float plane = clip.enabled ? clip.offset : zFront;
    const float lift = lattice.radius * 0.55f;   // sit just in front of the face

    const float firstColumn = acrossChannel ? zBack : siliconLeft;
    const float lastColumn = acrossChannel ? zFront : siliconRight;
    const int columns = static_cast<int>((lastColumn - firstColumn) / spacing);
    const int rows = static_cast<int>(-siliconBottom / spacing);
    std::vector<int> slot(static_cast<std::size_t>(columns * rows), -1);
    std::vector<int> owner(slot.size(), -1);

    for (int j = 0; j < rows; ++j) {
        for (int i = 0; i < columns; ++i) {
            const float u = firstColumn + spacing * (i + 0.5f);
            const float y = -spacing * (j + 0.5f);
            const Vec3f probe = acrossChannel ? Vec3f{plane - 0.01f, y, u} : Vec3f{u, y, plane - 0.01f};
            const int id = siliconPartAt(probe);
            if (id < 0 || !visible[id])
                continue;
            const unsigned hash = (static_cast<unsigned>(i) * 73856093u) ^ (static_cast<unsigned>(j) * 19349663u);
            Lattice::Kind kind = Lattice::Silicon;
            if (id == index(PartId::Source) || id == index(PartId::Drain))
                kind = (hash % 7u == 0u) ? Lattice::Donor : Lattice::Silicon;
            else
                kind = (hash % 13u == 0u) ? Lattice::Acceptor : Lattice::Silicon;
            Vec3f position = acrossChannel ? Vec3f{plane + lift, y, u} : Vec3f{u, y, plane + lift};
            position = position + offset(*part(static_cast<PartId>(id)), explode);
            slot[static_cast<std::size_t>(j * columns + i)] = static_cast<int>(lattice.sites.size());
            owner[static_cast<std::size_t>(j * columns + i)] = id;
            lattice.sites.push_back({position, kind});
        }
    }
    // Bonds to the right and downward neighbours (only within one part when exploded).
    for (int j = 0; j < rows; ++j) {
        for (int i = 0; i < columns; ++i) {
            const std::size_t here = static_cast<std::size_t>(j * columns + i);
            if (slot[here] < 0)
                continue;
            for (std::size_t there : {here + 1, here + static_cast<std::size_t>(columns)}) {
                if ((there == here + 1 && i + 1 >= columns) || there >= slot.size() || slot[there] < 0)
                    continue;
                if (explode > 0.0f && owner[there] != owner[here])
                    continue;
                lattice.bonds.emplace_back(static_cast<uint32_t>(slot[here]), static_cast<uint32_t>(slot[there]));
            }
        }
    }
    return lattice;
}

} // namespace mosfet
