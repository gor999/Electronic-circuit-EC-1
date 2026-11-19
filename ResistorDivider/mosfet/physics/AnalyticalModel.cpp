#include "DeviceModel.h"
#include "Electrostatics.h"

#include <algorithm>
#include <cmath>

namespace {

double trapezoid(const std::vector<double> &x, const std::vector<double> &y)
{
    double sum = 0.0;
    for (std::size_t i = 1; i < x.size(); ++i)
        sum += 0.5 * (y[i] + y[i - 1]) * (x[i] - x[i - 1]);
    return sum;
}

} // namespace

double DeviceState::at(const std::vector<double> &profile, double xm) const
{
    if (x.empty() || profile.size() != x.size())
        return 0.0;
    if (xm <= x.front())
        return profile.front();
    if (xm >= x.back())
        return profile.back();
    const auto upper = std::upper_bound(x.begin(), x.end(), xm);
    const std::size_t i = static_cast<std::size_t>(upper - x.begin());
    const double span = x[i] - x[i - 1];
    const double t = span > 0.0 ? (xm - x[i - 1]) / span : 0.0;
    return profile[i - 1] + (profile[i] - profile[i - 1]) * t;
}

DeviceState AnalyticalModel::solve(const MosfetParameters &p, const Bias &bias) const
{
    using namespace semi;
    DeviceState s;
    s.bias = bias;
    s.modelName = "Long-channel analytical model (gradual-channel approximation)";

    // --- Material constants -------------------------------------------------
    const double q = kElementaryCharge;
    const double T = p.temperature;
    const double vt = thermalVoltage(T);
    const double ni = p.temperatureDependence ? intrinsicDensity(p.intrinsicDensity300, T) : p.intrinsicDensity300;
    const double mu = p.temperatureDependence ? mobilityAt(p.electronMobility, T) : p.electronMobility;
    const double muP = p.temperatureDependence ? mobilityAt(p.holeMobility, T) : p.holeMobility;
    const double epsSi = p.siliconPermittivity * kVacuumPermittivity;
    const double na = p.substrateDoping;
    const double cox = oxideCapacitance(p.oxidePermittivity, p.oxideThickness);
    double phiF = fermiPotential(na, ni, T);
    if (phiF < 0.05) {
        phiF = 0.05;
        s.warnings.push_back("ni is close to NA: the substrate is nearly intrinsic and the MOSFET model does not apply; "
                             "phiF limited to 50 mV.");
    }
    const double twoPhi = 2.0 * phiF;
    const double gamma = bodyFactor(na, p.siliconPermittivity, cox);
    const double L = p.channelLength, W = p.channelWidth;

    // n+ polysilicon gate: phi_ms = -(Eg/2 + phiF); no fixed oxide charge.
    const double vfbPhysical = -(kSiliconBandGap / 2.0 + phiF);
    const double vth0Physical = vfbPhysical + twoPhi + gamma * std::sqrt(twoPhi);
    const double vth0 = p.thresholdFromPhysics ? vth0Physical : p.thresholdVoltage;

    s.thermalVoltage = vt;
    s.intrinsicDensity = ni;
    s.mobility = mu;
    s.holeMobility = muP;
    s.oxideCapacitance = cox;
    s.fermiPotential = phiF;
    s.bodyFactor = gamma;
    s.physicalThreshold = vth0Physical;
    s.threshold0 = vth0;
    // VFB consistent with the VTH0 in use, so the depletion below threshold matches it.
    s.flatBandVoltage = vth0 - twoPhi - gamma * std::sqrt(twoPhi);
    s.maxDepletion = depletionWidth(na, twoPhi, p.siliconPermittivity);
    s.minorityElectrons = ni * ni / na;
    s.substrateResistivity = 1.0 / (q * na * muP);
    s.builtInSource = builtInPotential(na, p.sourceDoping, ni, T);
    s.builtInDrain = builtInPotential(na, p.drainDoping, ni, T);

    // --- Body bias and threshold ----------------------------------------------
    const double vds = std::max(0.0, bias.vds);
    const double vsb = -bias.vbs;
    double psiOnset = twoPhi + vsb;   // surface potential at the onset of strong inversion
    if (psiOnset < 0.05) {
        psiOnset = 0.05;
        s.warnings.push_back("Forward body bias this large turns on the source-body junction; not modelled.");
    }
    s.threshold = vth0 + (p.bodyEffect ? gamma * (std::sqrt(psiOnset) - std::sqrt(twoPhi)) : 0.0);
    s.overdrive = bias.vgs - s.threshold;
    if (!p.bodyEffect && std::fabs(bias.vbs) > 1.0e-9)
        s.warnings.push_back("Body effect is off: VBS changes the depletion widths but not VTH.");

    s.sourceJunctionDepletion =
        junctionDepletion(na, p.sourceDoping, std::max(0.02, s.builtInSource + vsb), p.siliconPermittivity).pSide;
    s.drainJunctionDepletion =
        junctionDepletion(na, p.drainDoping, std::max(0.02, s.builtInDrain + vds + vsb), p.siliconPermittivity).pSide;

    if (s.sourceJunctionDepletion + s.drainJunctionDepletion >= L)
        s.warnings.push_back("Punch-through: the source and drain depletion regions merge under the channel. "
                             "Current then also flows below the surface; that is not modelled.");
    if (L < 1.0e-6)
        s.warnings.push_back("L below 1 um: short-channel effects (velocity saturation, DIBL, VTH roll-off) are "
                             "not modelled, so the long-channel results become increasingly optimistic.");
    if (p.oxideThickness < 3.0e-9)
        s.warnings.push_back("tox below 3 nm: gate tunnelling current and quantum effects are not modelled.");

    // --- Cutoff --------------------------------------------------------------
    if (s.overdrive <= 0.0) {
        s.region = OperatingRegion::Cutoff;
        s.channelExists = false;
        // Surface potential set by the gate alone: VGB - VFB = psi + gamma sqrt(psi).
        const double drive = bias.vgs - bias.vbs - s.flatBandVoltage;
        double psi = 0.0;
        if (drive > 0.0) {
            const double root = -gamma / 2.0 + std::sqrt(gamma * gamma / 4.0 + drive);
            psi = std::min(root * root, psiOnset);
        }
        const int n = 121;
        for (int i = 0; i < n; ++i) {
            s.x.push_back(L * i / (n - 1));
            s.potential.push_back(0.0);
            s.inversionCharge.push_back(0.0);
            s.electronSheet.push_back(0.0);
            s.electronDensity.push_back(0.0);
            s.lateralField.push_back(0.0);
            s.velocity.push_back(0.0);
            s.currentDensity.push_back(0.0);
            s.surfacePotential.push_back(psi);
            s.gateDepletion.push_back(depletionWidth(na, psi, p.siliconPermittivity));
        }
        s.saturationVoltage = 0.0;
        s.effectiveMobility = mu;
        s.beta = mu * cox * W / L;
        return s;
    }

    // --- Inversion: linear or saturation ---------------------------------------
    s.channelExists = true;
    const double vov = s.overdrive;
    const double muEff = p.mobilityDegradation ? mu / (1.0 + p.mobilityTheta * vov) : mu;
    const double beta = muEff * cox * W / L;
    s.effectiveMobility = muEff;
    s.beta = beta;
    s.saturationVoltage = vov;
    s.onResistance = 1.0 / (beta * vov);

    std::vector<double> &x = s.x, &v = s.potential, &dvdx = s.lateralField;
    double channelEnd = L;   // L' : where the inversion layer ends
    if (vds < vov) {
        s.region = OperatingRegion::Linear;
        const double a = vov * vds - 0.5 * vds * vds;
        s.drainCurrent = s.drainCurrentIdeal = beta * a;
        const int n = 241;
        for (int i = 0; i < n; ++i) {
            const double xi = L * i / (n - 1);
            const double vi = vov - std::sqrt(std::max(0.0, vov * vov - 2.0 * (xi / L) * a));
            x.push_back(xi);
            v.push_back(vi);
            dvdx.push_back(a / (L * (vov - vi)));
        }
    } else {
        s.region = OperatingRegion::Saturation;
        s.pinchedOff = true;
        const double excess = vds - vov;
        double dL = 0.0;
        if (p.channelLengthModulation)
            dL = std::min(std::sqrt(2.0 * epsSi * excess / (q * na)), 0.9 * L);
        channelEnd = L - dL;
        s.pinchOffLength = dL;
        s.pinchOffPosition = channelEnd;
        s.drainCurrentIdeal = 0.5 * beta * vov * vov;
        s.drainCurrent = s.drainCurrentIdeal * L / channelEnd;
        // Dense near the pinch-off point, where V(x) is steep.
        const int n1 = 200;
        for (int i = 0; i < n1; ++i) {
            const double u = double(i) / (n1 - 1);
            const double xi = channelEnd * (1.0 - (1.0 - u) * (1.0 - u));
            const double root = std::sqrt(std::max(1.0 - xi / channelEnd, 1.0e-6));
            x.push_back(xi);
            v.push_back(vov * (1.0 - std::sqrt(std::max(0.0, 1.0 - xi / channelEnd))));
            dvdx.push_back(vov / (2.0 * channelEnd * root));
        }
        if (dL > 0.0) {
            const int n2 = 41;
            for (int i = 1; i < n2; ++i) {
                const double xi = channelEnd + dL * i / (n2 - 1);
                const double u = (xi - channelEnd) / dL;
                x.push_back(xi);
                v.push_back(vov + excess * u * u);
                dvdx.push_back(2.0 * excess * u / dL);
            }
        } else if (excess > 0.0) {
            // Ideal saturation: the excess voltage drops in a vanishing region at the drain.
            x.push_back(L * (1.0 + 1.0e-9));
            v.push_back(vds);
            dvdx.push_back(dvdx.back());
            s.warnings.push_back("Channel-length modulation is off: ideal saturation, the pinch-off point stays at the "
                                 "drain end and VDS - VDS,sat drops across a vanishingly short region there.");
        }
        s.warnings.push_back("The gradual-channel approximation breaks down at the pinch-off point: the lateral "
                             "field there is shown capped; real devices need a 2-D solution.");
    }

    // --- Charge, density, field, velocity along the channel -------------------
    const std::size_t n = x.size();
    s.inversionCharge.resize(n);
    s.electronSheet.resize(n);
    s.electronDensity.resize(n);
    s.velocity.resize(n);
    s.currentDensity.resize(n);
    s.surfacePotential.resize(n);
    s.gateDepletion.resize(n);
    std::vector<double> inverseVelocity(n, 0.0);
    for (std::size_t i = 0; i < n; ++i) {
        const bool inChannel = x[i] <= channelEnd * (1.0 + 1.0e-12);
        const double psi = psiOnset + v[i];   // strong inversion: psi_s = 2 phiF + VCB
        s.surfacePotential[i] = psi;
        s.gateDepletion[i] = depletionWidth(na, psi, p.siliconPermittivity);
        double charge, velocity;
        if (inChannel) {
            charge = cox * std::max(0.0, vov - v[i]);
            velocity = muEff * dvdx[i];
        } else {
            // Electrons cross the pinch-off region near the saturation velocity,
            // so their density there follows from current continuity.
            velocity = kSaturationVelocity;
            charge = s.drainCurrent / (W * velocity);
        }
        s.inversionCharge[i] = charge;
        s.electronSheet[i] = charge / q;
        // Classical inversion-layer thickness kT/(q Es), with Es from Gauss's law.
        const double depletionCharge = q * na * s.gateDepletion[i];
        const double layer = inChannel ? vt * epsSi / std::max(depletionCharge + charge, 1.0e-12)
                                       : std::max(0.5 * s.gateDepletion[i], 1.0e-9);
        s.electronDensity[i] = s.electronSheet[i] / layer;
        s.velocity[i] = velocity;
        s.currentDensity[i] = q * s.electronDensity[i] * velocity;
        dvdx[i] = -dvdx[i];   // store the field Ex = -dV/dx
        inverseVelocity[i] = velocity > 0.0 ? 1.0 / velocity : 0.0;
    }

    // The gradual-channel field is singular exactly at the pinch-off point
    // (E ~ 1/sqrt(L' - x)). Cap it only in the last 0.2 % of the channel so
    // graphs stay readable; everywhere else the values are the model's.
    if (s.pinchedOff) {
        const double edge = 0.998 * channelEnd;
        const double cap = vov / (2.0 * channelEnd * std::sqrt(0.002));
        for (std::size_t i = 0; i < n; ++i) {
            if (x[i] > edge && x[i] <= channelEnd && std::fabs(dvdx[i]) > cap) {
                dvdx[i] = -cap;
                s.velocity[i] = muEff * cap;
            }
        }
    }

    std::vector<double> charges(n);
    for (std::size_t i = 0; i < n; ++i)
        charges[i] = x[i] <= channelEnd ? s.inversionCharge[i] : 0.0;
    s.totalInversionCharge = W * trapezoid(x, charges);
    s.dcResistance = vds > 0.0 ? vds / s.drainCurrent : s.onResistance;
    if (vds > 0.0) {
        // tau = integral of dx / v over the path from source to drain.
        double tau = 0.0;
        for (std::size_t i = 1; i < n; ++i)
            tau += 0.5 * (inverseVelocity[i] + inverseVelocity[i - 1]) * (x[i] - x[i - 1]);
        s.transitTime = tau;
    }
    return s;
}
