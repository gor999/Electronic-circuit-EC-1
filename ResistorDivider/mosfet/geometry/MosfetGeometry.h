#pragma once

#include "Region.h"

#include <array>
#include <utility>
#include <vector>

struct DeviceState;
struct MosfetParameters;

namespace mosfet {

enum class PartId : int {
    Gate, GateOxide, Channel, Depletion, Source, Drain, Substrate,
    GateContact, SourceContact, DrainContact, BodyContact
};
constexpr int kPartCount = 11;
using PartFlags = std::array<bool, kPartCount>;
inline int index(PartId id) { return static_cast<int>(id); }

// How real dimensions map to the drawing. The display unit along x is 1 um and
// lengths along the channel are to scale. Thin layers are exaggerated so that
// they can be seen; build() adapts the depth scale when depletion regions are
// deep, and reports the scales it used in MosfetGeometry::scale().
struct DisplayScale
{
    float lateral = 1.0f;          // x (along the channel): display units per um
    float width = 0.3f;            // z (channel width): display units per um, drawn width clamped
    float siliconDepth = 2.0f;     // y inside the silicon, per um (lowered for very deep depletion)
    float oxide = 0.0f;            // drawn oxide thickness / real thickness (set by build)
    float gate = 1.5f;             // polysilicon thickness, per um
    float inversionMax = 0.16f;    // drawn channel thickness approached at very large inversion charge
    float substrateDepth = 1.8f;   // drawn substrate depth (set by build; the wafer is truncated)
    float siliconMargin = 0.5f;    // silicon beyond the source and drain, display units
};

// Inversion charge (C/m^2) drawn at half of DisplayScale::inversionMax.
constexpr double kInversionChargeReference = 1.7e-3;

// Section plane: points with dot(normal, p) + offset >= 0 are kept.
struct ClipPlane
{
    bool enabled = false;
    Vec3f normal{0.0f, 0.0f, -1.0f};
    float offset = 0.0f;
    float distance(Vec3f p) const { return enabled ? dot(normal, p) + offset : 1.0f; }
};

struct Part
{
    PartId id = PartId::Gate;
    Region region;
    MeshData mesh;
    std::vector<Vec3f> anchors;   // label anchor candidates, in order of preference
    float explodeWeight = 0.0f;   // exploded-view lift, in display units at full explosion
    bool translucent = false;
    Vec3f boundsMin, boundsMax;
};

// Illustrative crystal lattice drawn on the visible section face.
struct Lattice
{
    enum Kind { Silicon = 0, Donor = 1, Acceptor = 2 };
    struct Site { Vec3f position; Kind kind; };
    std::vector<Site> sites;
    std::vector<std::pair<uint32_t, uint32_t>> bonds;
    float radius = 0.03f;
};

class MosfetGeometry
{
public:
    static MosfetGeometry build(const MosfetParameters &parameters, const DeviceState &state,
                                const DisplayScale &scale = DisplayScale());

    const std::vector<Part> &parts() const { return m_parts; }
    const Part *part(PartId id) const;
    const DisplayScale &scale() const { return m_scale; }

    Vec3f offset(const Part &part, float explode) const { return {0.0f, part.explodeWeight * explode, 0.0f}; }

    // Nearest visible part hit by the ray, honouring the section plane (cut
    // faces count as hits). Returns -1 for no hit.
    int pick(Vec3f origin, Vec3f dir, float explode, const ClipPlane &clip, const PartFlags &visible,
             bool includeTranslucent, float *tHit = nullptr) const;

    // Silicon part containing the point (exploded offsets not applied), or -1.
    int siliconPartAt(Vec3f p) const;

    Lattice lattice(const ClipPlane &clip, float explode, const PartFlags &visible) const;

    // Display coordinates of a few landmarks.
    float zFront = 0.0f, zBack = 0.0f;
    float siliconLeft = 0.0f, siliconRight = 0.0f, siliconBottom = 0.0f;
    float sourceEdge = 0.0f, drainEdge = 0.0f;   // junction edges at the surface (x)
    float gateLeft = 0.0f, gateRight = 0.0f;
    float gateTop = 0.0f;
    float drawnWidth = 0.0f;
    Vec3f boundsMin, boundsMax;

    // Channel position (m from the source junction) <-> display x.
    float toDisplayX(double metres) const { return sourceEdge + float(metres * 1.0e6) * m_scale.lateral; }
    double toChannelX(float displayX) const { return double(displayX - sourceEdge) / m_scale.lateral * 1.0e-6; }
    // Drawn channel thickness at a channel position (0 where there is none).
    float channelThicknessAt(double metres) const;

private:
    void addPart(PartId id, Region region, float explodeWeight, bool translucent = false);

    std::vector<Part> m_parts;
    DisplayScale m_scale;
};

} // namespace mosfet
