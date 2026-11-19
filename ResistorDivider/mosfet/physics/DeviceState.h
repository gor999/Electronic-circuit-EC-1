#pragma once

#include <string>
#include <vector>

// How a displayed quantity was obtained. The UI shows this next to values.
enum class Accuracy {
    Calculated,      // follows directly from the model equations
    Approximation,   // analytical estimate outside the model's strict validity
    Illustrative     // drawn to show the idea, not a calculated value
};

struct Bias
{
    double vgs = 1.8;   // gate-source voltage, V
    double vds = 0.5;   // drain-source voltage, V
    double vbs = 0.0;   // body-source voltage, V
    double vs = 0.0;    // source potential relative to ground, V (terminal voltages only)
};

enum class OperatingRegion { Cutoff, Linear, Saturation };

// Everything the geometry, particles, graphs and panels read about the device.
// A DeviceModel fills it; nothing downstream needs to know which model did.
struct DeviceState
{
    Bias bias;
    std::string modelName;

    // --- Material and derived constants -------------------------------------
    double thermalVoltage = 0.0;      // kT/q, V
    double intrinsicDensity = 0.0;    // ni at T, m^-3
    double mobility = 0.0;            // un at T, m^2/Vs
    double holeMobility = 0.0;        // up at T, m^2/Vs
    double oxideCapacitance = 0.0;    // Cox, F/m^2
    double fermiPotential = 0.0;      // phiF, V
    double bodyFactor = 0.0;          // gamma, V^0.5
    double flatBandVoltage = 0.0;     // VFB, V (consistent with the VTH0 in use)
    double physicalThreshold = 0.0;   // VTH0 calculated from doping, tox and an n+ poly gate, V
    double threshold0 = 0.0;          // VTH0 in use (parameter or calculated), V
    double maxDepletion = 0.0;        // Wd,max at VBS = 0, m
    double minorityElectrons = 0.0;   // n0 = ni^2 / NA in the substrate, m^-3
    double substrateResistivity = 0.0;   // 1 / (q NA up), ohm m
    double builtInSource = 0.0, builtInDrain = 0.0;   // V

    // --- Operating point ----------------------------------------------------
    OperatingRegion region = OperatingRegion::Cutoff;
    double threshold = 0.0;           // VTH including the body effect, V
    double overdrive = 0.0;           // VOV = VGS - VTH, V
    double saturationVoltage = 0.0;   // VDS,sat = VOV (pinch-off voltage), V
    double effectiveMobility = 0.0;   // m^2/Vs
    double beta = 0.0;                // ueff Cox W / L, A/V^2
    double drainCurrent = 0.0;        // ID, A
    double drainCurrentIdeal = 0.0;   // ID without channel-length modulation, A
    double onResistance = 0.0;        // 1 / (beta VOV): channel resistance for small VDS, ohm (0 = infinite)
    double dcResistance = 0.0;        // VDS / ID, ohm (0 = infinite or undefined)
    bool pinchedOff = false;
    double pinchOffPosition = 0.0;    // x of the pinch-off point from the source, m
    double pinchOffLength = 0.0;      // dL = L - x_pinch, m
    double transitTime = 0.0;         // source-to-drain time of an electron, s (0 = no transport)
    double totalInversionCharge = 0.0;   // |Q| of all channel electrons, C
    double sourceJunctionDepletion = 0.0;   // p side, m
    double drainJunctionDepletion = 0.0;    // p side, m
    bool channelExists = false;

    // --- Profiles along the channel ----------------------------------------
    // x from the source junction edge (0) to the drain junction edge (L), m.
    std::vector<double> x;
    std::vector<double> potential;         // channel potential V(x) relative to the source, V
    std::vector<double> inversionCharge;   // |Qinv(x)|, C/m^2
    std::vector<double> electronSheet;     // ns(x) = |Qinv| / q, m^-2
    std::vector<double> electronDensity;   // n at the surface (approximation), m^-3
    std::vector<double> lateralField;      // Ex = -dV/dx, V/m (negative: points from drain to source)
    std::vector<double> velocity;          // electron drift velocity towards the drain, m/s
    std::vector<double> currentDensity;    // |Jn| = q n v at the surface, A/m^2
    std::vector<double> surfacePotential;  // psi_s(x) relative to the body, V
    std::vector<double> gateDepletion;     // depletion depth below the surface, m

    Accuracy potentialAccuracy = Accuracy::Calculated;
    Accuracy densityAccuracy = Accuracy::Approximation;
    Accuracy pinchOffRegionAccuracy = Accuracy::Approximation;
    std::vector<std::string> warnings;     // model limits that apply to this operating point

    // Value at position xm by linear interpolation.
    double at(const std::vector<double> &profile, double xm) const;
};
