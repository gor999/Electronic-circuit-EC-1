#include "Validation.h"

#include "Electrostatics.h"

#include <cmath>

bool ValidationCheck::pass() const
{
    if (!applicable)
        return true;
    if (absolute)
        return std::fabs(actual - expected) <= tolerance;
    const double scale = std::max(std::fabs(expected), 1.0e-30);
    return std::fabs(actual - expected) <= tolerance * scale + 1.0e-30;
}

std::vector<ValidationCheck> validateModel(const DeviceModel &model, const MosfetParameters &p, const Bias &bias)
{
    std::vector<ValidationCheck> checks;
    const DeviceState s = model.solve(p, bias);
    auto add = [&](std::string name, std::string detail, double expected, double actual, double tolerance,
                   bool applicable = true) {
        checks.push_back({std::move(name), std::move(detail), expected, actual, tolerance, applicable});
    };

    add("Oxide capacitance", "Cox = eps_ox / tox", p.oxidePermittivity * semi::kVacuumPermittivity / p.oxideThickness,
        s.oxideCapacitance, 1.0e-12);

    const bool on = s.region != OperatingRegion::Cutoff;
    add("Channel starts at the source potential", "V(0) = 0", 0.0, on ? s.potential.front() : 0.0, 1.0e-9, on);
    const bool endsAtDrain = on && (s.region == OperatingRegion::Linear || p.channelLengthModulation);
    add("Channel ends at the drain potential", "V(L) = VDS", bias.vds, endsAtDrain ? s.at(s.potential, p.channelLength) : 0.0,
        1.0e-6, endsAtDrain && bias.vds > 0.0);

    add("Inversion charge at the source", "|Qinv(0)| = Cox VOV", s.oxideCapacitance * std::max(s.overdrive, 0.0),
        on ? s.inversionCharge.front() : 0.0, 1.0e-9, on);

    // Current continuity: W |Qinv(x)| v(x) is the same everywhere in the channel.
    if (on && s.drainCurrent > 0.0) {
        const double end = s.pinchedOff ? s.pinchOffPosition : p.channelLength;
        for (double f : {0.1, 0.4, 0.7, 0.9}) {
            const double xm = f * end;
            const double current = p.channelWidth * s.at(s.inversionCharge, xm) * s.at(s.velocity, xm);
            add("Current continuity at " + std::to_string(int(f * 100)) + " % of the channel",
                "W |Qinv(x)| v(x) = ID", s.drainCurrent, current, 2.0e-3);
        }
    }

    // Continuity of ID at the linear/saturation boundary. With channel-length
    // modulation dL grows as sqrt(VDS - VDS,sat), so the approach is slow but
    // the gap still closes: check the ideal model exactly and that the CLM gap
    // shrinks as the step does.
    if (on) {
        MosfetParameters ideal = p;
        ideal.channelLengthModulation = false;
        Bias below = bias, above = bias;
        below.vds = s.overdrive * (1.0 - 1.0e-7);
        above.vds = s.overdrive * (1.0 + 1.0e-7);
        add("ID continuous at VDS = VDS,sat", "ID(VDS,sat-) = ID(VDS,sat+), ideal model",
            model.solve(ideal, below).drainCurrent, model.solve(ideal, above).drainCurrent, 1.0e-6);
        if (p.channelLengthModulation) {
            auto gap = [&](double step) {
                Bias a = bias, b = bias;
                a.vds = s.overdrive * (1.0 - step);
                b.vds = s.overdrive * (1.0 + step);
                return std::fabs(model.solve(p, b).drainCurrent - model.solve(p, a).drainCurrent)
                       / std::max(model.solve(p, a).drainCurrent, 1.0e-30);
            };
            add("ID continuous at VDS,sat with CLM", "relative jump shrinks as the step shrinks (1e-4 -> 1e-8)",
                1.0, gap(1.0e-8) < gap(1.0e-4) ? 1.0 : 0.0, 0.0);
        }
    }

    if (s.pinchedOff) {
        const double justBefore = s.at(s.inversionCharge, s.pinchOffPosition * (1.0 - 1.0e-4));
        add("Inversion charge vanishes at pinch-off", "|Qinv| / (Cox VOV) just before x_p below 2 %", 0.0,
            justBefore / (s.oxideCapacitance * s.overdrive), 0.02);
        checks.back().absolute = true;
    }

    // Scaling with W and L (long-channel model).
    if (on) {
        MosfetParameters wide = p;
        wide.channelWidth *= 2.0;
        add("ID proportional to W", "ID(2W) = 2 ID(W)", 2.0 * s.drainCurrent, model.solve(wide, bias).drainCurrent, 1.0e-9);
        MosfetParameters ideal = p, longer = p;
        ideal.channelLengthModulation = longer.channelLengthModulation = false;
        longer.channelLength *= 2.0;
        add("ID inversely proportional to L (without CLM)", "ID(2L) = ID(L) / 2", 0.5 * model.solve(ideal, bias).drainCurrent,
            model.solve(longer, bias).drainCurrent, 1.0e-9);
    }

    Bias off = bias;
    off.vgs = s.threshold - 0.1;
    add("No current below threshold", "ID(VGS < VTH) = 0 (subthreshold not modelled)", 0.0,
        model.solve(p, off).drainCurrent, 0.0);
    checks.back().absolute = true;

    if (p.bodyEffect) {
        Bias reverse = bias;
        reverse.vbs = bias.vbs - 1.0;
        const double expected = s.threshold0 + s.bodyFactor
                                    * (std::sqrt(2.0 * s.fermiPotential - reverse.vbs) - std::sqrt(2.0 * s.fermiPotential));
        add("Body effect", "VTH(VBS - 1 V) = VTH0 + gamma (sqrt(2phiF - VBS) - sqrt(2phiF))", expected,
            model.solve(p, reverse).threshold, 1.0e-9);
    }
    return checks;
}
