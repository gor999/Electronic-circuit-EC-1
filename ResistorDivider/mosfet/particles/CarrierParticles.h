#pragma once

#include "geometry/MosfetGeometry.h"
#include "physics/DeviceState.h"

#include <random>
#include <vector>

namespace mosfet {

// Electrons and holes drawn as dots on the visible section face.
//
// What is calculated and what is not:
//   - Channel electrons: the number of dots along the channel follows the
//     calculated sheet density ns(x) (each dot is a fixed amount of charge per
//     unit width), and they move at the calculated drift velocity v(x) =
//     ueff dV/dx, capped at the saturation velocity, in slow motion. Beyond the
//     pinch-off point they cross at the saturation velocity. Individual dot
//     positions are random samples of that density.
//   - Electrons in the n+ regions and holes in the neutral substrate:
//     illustrative (fixed counts, random thermal jitter). Holes are absent
//     from depletion regions because those regions are calculated.
class CarrierParticles
{
public:
    enum Kind { ChannelElectron = 0, BulkElectron = 1, Hole = 2 };
    struct Dot { Vec3f position; Kind kind; };

    // Call whenever the device state or geometry changes.
    void update(const MosfetGeometry &geometry, const DeviceState &state, double density);
    // Advance the animation by `screenSeconds`; `picosecondsPerSecond` is the slow-motion factor.
    void advance(double screenSeconds, double picosecondsPerSecond);
    std::vector<Dot> dots(const ClipPlane &clip, float explode) const;

    int channelDots() const { return int(m_channel.size()); }
    // Electrons represented by one channel dot, per micrometre of channel width.
    double electronsPerDotPerMicron() const;

private:
    struct Carrier
    {
        double x = 0.0;      // channel electrons: m from the source; others: display x
        float y = 0.0f;      // channel electrons: depth fraction 0..1; others: display y
        float z = 0.0f;      // display z
        float jitter = 0.0f; // display-only thermal wobble along x
    };

    void sampleRegion(std::vector<Carrier> &carriers, int count, PartId part, float maxDepth);
    void correctDensity();
    float channelY(const Carrier &c) const;

    const MosfetGeometry *m_geometry = nullptr;
    DeviceState m_state;
    double m_density = 1.0;
    double m_length = 0.0;            // m
    double m_channelEnd = 0.0;        // m: L or the pinch-off point
    std::vector<double> m_targets;    // dots wanted per bin along [0, channelEnd]
    std::vector<Carrier> m_channel, m_source, m_drain, m_holes;
    std::mt19937 m_rng{12345};
};

} // namespace mosfet
