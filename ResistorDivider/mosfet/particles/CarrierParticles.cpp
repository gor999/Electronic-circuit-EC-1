#include "CarrierParticles.h"

#include "physics/Electrostatics.h"

#include <algorithm>
#include <cmath>

namespace mosfet {

namespace {

// Charge per unit width carried by one channel dot at density 1: about 300
// dots for the default device (|Qinv| ~ 1.7e-3 C/m^2 over 2 um).
const double kSheetChargePerDot = 9.0e-12;   // C/m
const int kBins = 60;
const int kMaxChannelDots = 2500;
const int kBulkPerRegion = 70;
const int kHoles = 220;

} // namespace

double CarrierParticles::electronsPerDotPerMicron() const
{
    return kSheetChargePerDot / m_density * 1.0e-6 / semi::kElementaryCharge;
}

void CarrierParticles::sampleRegion(std::vector<Carrier> &carriers, int count, PartId id, float maxDepth)
{
    const Part *part = m_geometry ? m_geometry->part(id) : nullptr;
    if (!part) {
        carriers.clear();
        return;
    }
    const Region &r = part->region;
    std::uniform_real_distribution<float> ux(r.xMin(), r.xMax()), uz(r.z0, r.z1);
    const float yLow = std::max(part->boundsMin.y, -maxDepth);
    std::uniform_real_distribution<float> uy(yLow, part->boundsMax.y);
    // Keep carriers that are still inside the (possibly changed) region.
    carriers.erase(std::remove_if(carriers.begin(), carriers.end(),
                                  [&](const Carrier &c) { return !r.contains({float(c.x), c.y, r.z1 - 1.0e-4f}); }),
                   carriers.end());
    if (int(carriers.size()) > count)
        carriers.resize(std::size_t(count));
    int attempts = 0;
    while (int(carriers.size()) < count && attempts++ < count * 40) {
        Carrier c;
        c.x = ux(m_rng);
        c.y = uy(m_rng);
        c.z = uz(m_rng);
        if (r.contains({float(c.x), c.y, r.z1 - 1.0e-4f}))
            carriers.push_back(c);
    }
}

void CarrierParticles::update(const MosfetGeometry &geometry, const DeviceState &state, double density)
{
    m_geometry = &geometry;
    m_state = state;
    m_density = std::max(0.05, density);
    m_length = state.x.empty() ? 0.0 : std::min(state.x.back(), geometry.toChannelX(geometry.drainEdge));
    m_channelEnd = state.pinchedOff ? state.pinchOffPosition : m_length;

    // Dots wanted in each bin: (integral of |Qinv| dx over the bin) / charge per dot.
    m_targets.assign(kBins, 0.0);
    if (state.channelExists && m_channelEnd > 0.0) {
        const double perDot = kSheetChargePerDot / m_density;
        double total = 0.0;
        for (int b = 0; b < kBins; ++b) {
            const double x0 = m_channelEnd * b / kBins, x1 = m_channelEnd * (b + 1) / kBins;
            double sum = 0.0;
            for (int k = 0; k < 8; ++k)
                sum += state.at(state.inversionCharge, x0 + (x1 - x0) * (k + 0.5) / 8.0);
            m_targets[std::size_t(b)] = sum / 8.0 * (x1 - x0) / perDot;
            total += m_targets[std::size_t(b)];
        }
        if (total > kMaxChannelDots) {
            for (double &t : m_targets)
                t *= kMaxChannelDots / total;
        }
    }
    if (!state.channelExists)
        m_channel.clear();

    const float holeDepth = 1.4f;
    const int bulk = int(kBulkPerRegion * std::min(m_density, 2.0));
    sampleRegion(m_source, bulk, PartId::Source, 10.0f);
    sampleRegion(m_drain, bulk, PartId::Drain, 10.0f);
    sampleRegion(m_holes, int(kHoles * std::min(m_density, 2.0)), PartId::Substrate, holeDepth);
    correctDensity();
}

// Births and deaths per bin keep the dot density equal to the calculated one.
void CarrierParticles::correctDensity()
{
    if (m_targets.empty() || m_channelEnd <= 0.0)
        return;
    std::vector<std::vector<std::size_t>> members(kBins);
    for (std::size_t i = 0; i < m_channel.size(); ++i) {
        if (m_channel[i].x < m_channelEnd) {
            const int b = std::clamp(int(m_channel[i].x / m_channelEnd * kBins), 0, kBins - 1);
            members[std::size_t(b)].push_back(i);
        }
    }
    std::vector<bool> remove(m_channel.size(), false);
    std::uniform_real_distribution<double> unit(0.0, 1.0);
    std::uniform_real_distribution<float> uz(m_geometry->zBack, m_geometry->zFront);
    for (int b = 0; b < kBins; ++b) {
        const double target = m_targets[std::size_t(b)];
        const int want = int(target) + (unit(m_rng) < target - std::floor(target) ? 1 : 0);
        auto &list = members[std::size_t(b)];
        const int have = int(list.size());
        if (have > want + 1) {
            std::shuffle(list.begin(), list.end(), m_rng);
            for (int k = 0; k < have - want; ++k)
                remove[list[std::size_t(k)]] = true;
        } else if (have < want) {
            for (int k = 0; k < want - have; ++k) {
                Carrier c;
                c.x = m_channelEnd * (b + unit(m_rng)) / kBins;
                c.y = float(unit(m_rng));
                c.z = uz(m_rng);
                m_channel.push_back(c);
            }
        }
    }
    std::size_t out = 0;
    for (std::size_t i = 0; i < remove.size(); ++i) {
        if (!remove[i])
            m_channel[out++] = m_channel[i];
    }
    for (std::size_t i = remove.size(); i < m_channel.size(); ++i)
        m_channel[out++] = m_channel[i];
    m_channel.resize(out);
}

void CarrierParticles::advance(double screenSeconds, double picosecondsPerSecond)
{
    if (!m_geometry)
        return;
    const double realSeconds = screenSeconds * picosecondsPerSecond * 1.0e-12;
    const double maxStep = 0.03 * std::max(m_length, 1.0e-9);
    std::normal_distribution<float> wobble(0.0f, 1.0f);

    // Drift: x += v(x) dt (electrons move from source to drain).
    for (Carrier &c : m_channel) {
        double v = c.x < m_channelEnd ? m_state.at(m_state.velocity, c.x) : semi::kSaturationVelocity;
        v = std::min(v, semi::kSaturationVelocity);
        c.x += std::min(v * realSeconds, maxStep);
        c.y = std::clamp(c.y + 0.03f * wobble(m_rng), 0.0f, 1.0f);
        c.jitter = 0.85f * c.jitter + 0.004f * wobble(m_rng);
    }
    // Electrons reaching the drain junction leave the channel.
    m_channel.erase(std::remove_if(m_channel.begin(), m_channel.end(), [&](const Carrier &c) { return c.x >= m_length; }),
                    m_channel.end());
    correctDensity();

    // Illustrative thermal motion of the other carriers, kept inside their regions.
    auto jiggle = [&](std::vector<Carrier> &list, PartId id) {
        const Part *part = m_geometry->part(id);
        if (!part)
            return;
        for (Carrier &c : list) {
            const Carrier before = c;
            c.x += 0.006 * wobble(m_rng);
            c.y += 0.006f * wobble(m_rng);
            if (!part->region.contains({float(c.x), c.y, part->region.z1 - 1.0e-4f}))
                c = before;
        }
    };
    jiggle(m_source, PartId::Source);
    jiggle(m_drain, PartId::Drain);
    jiggle(m_holes, PartId::Substrate);
}

float CarrierParticles::channelY(const Carrier &c) const
{
    if (c.x < m_channelEnd)
        return -std::max(0.008f, c.y * m_geometry->channelThicknessAt(c.x));
    // Past the pinch-off point electrons travel just below the surface through the depletion region.
    return -(0.015f + 0.06f * c.y);
}

std::vector<CarrierParticles::Dot> CarrierParticles::dots(const ClipPlane &clip, float explode) const
{
    std::vector<Dot> out;
    if (!m_geometry)
        return out;
    const bool acrossChannel = clip.enabled && std::fabs(clip.normal.x) > 0.5f;
    const float plane = clip.enabled ? clip.offset : m_geometry->zFront;
    const float lift = 0.022f;
    const float slab = 0.12f;
    auto offsetOf = [&](PartId id) {
        const Part *p = m_geometry->part(id);
        return p ? m_geometry->offset(*p, explode) : Vec3f{};
    };
    auto emit = [&](float x, float y, float z, Kind kind, PartId id) {
        Vec3f p;
        if (acrossChannel) {
            if (std::fabs(x - plane) > slab)
                return;
            p = {plane + lift, y, z};
        } else {
            p = {x, y, plane + lift};
        }
        out.push_back({p + offsetOf(id), kind});
    };
    for (const Carrier &c : m_channel) {
        const float x = m_geometry->toDisplayX(c.x) + c.jitter;
        emit(x, channelY(c), c.z, ChannelElectron, c.x < m_channelEnd ? PartId::Channel : PartId::Depletion);
    }
    for (const Carrier &c : m_source)
        emit(float(c.x), c.y, c.z, BulkElectron, PartId::Source);
    for (const Carrier &c : m_drain)
        emit(float(c.x), c.y, c.z, BulkElectron, PartId::Drain);
    for (const Carrier &c : m_holes)
        emit(float(c.x), c.y, c.z, Hole, PartId::Substrate);
    return out;
}

} // namespace mosfet
