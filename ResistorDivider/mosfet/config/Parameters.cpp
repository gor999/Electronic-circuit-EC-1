#include "Parameters.h"

#include "physics/Electrostatics.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace {

#define REF(path) [](MosfetConfig &c) -> double & { return c.path; }
#define FLAG(path) [](MosfetConfig &c) -> bool & { return c.path; }

const std::vector<ParamSpec> kParameters = {
    // --- Bias -------------------------------------------------------------
    {"vgs", "Gate-source voltage", "VGS", "V", 1.0, -1.0, 6.0, false, ParamGroup::Bias,
     "Voltage between gate and source. It sets the vertical field through the oxide.",
     "Below VTH there is no channel and ID = 0. Above VTH the overdrive VOV = VGS - VTH sets the inversion "
     "charge Cox VOV: more VGS gives more electrons, a thicker channel, lower channel resistance and a larger "
     "ID (quadratic in VOV in saturation). It also raises the pinch-off voltage VDS,sat = VOV.",
     REF(bias.vgs)},
    {"vds", "Drain-source voltage", "VDS", "V", 1.0, 0.0, 6.0, false, ParamGroup::Bias,
     "Voltage between drain and source. It drives electrons along the channel.",
     "The channel potential rises from 0 at the source to VDS at the drain, so the inversion charge "
     "Cox (VOV - V(x)) falls towards the drain. While VDS < VOV the device is linear and ID grows with VDS. "
     "At VDS = VOV the charge at the drain end reaches zero: pinch-off. Beyond it ID saturates; the excess "
     "voltage drops across the pinch-off region, which grows and moves the pinch-off point towards the source "
     "(channel-length modulation). The drain-body depletion widens with VDS.",
     REF(bias.vds)},
    {"vbs", "Body-source voltage", "VBS", "V", 1.0, -4.0, 0.3, false, ParamGroup::Bias,
     "Voltage of the body (substrate) relative to the source. Normally 0 or negative (reverse bias).",
     "Reverse body bias (VBS < 0) widens the depletion region under the gate and around both junctions. "
     "More depletion charge must be balanced before inversion, so VTH rises (body effect): "
     "VTH = VTH0 + gamma (sqrt(2phiF - VBS) - sqrt(2phiF)). ID falls at the same VGS.",
     REF(bias.vbs)},
    {"vs", "Source voltage", "VS", "V", 1.0, -5.0, 5.0, false, ParamGroup::Bias,
     "Potential of the source relative to ground. VG = VS + VGS, VD = VS + VDS, VB = VS + VBS.",
     "Only the differences VGS, VDS and VBS enter the physics, so shifting VS moves all terminal voltages "
     "together and changes nothing inside the device.",
     REF(bias.vs)},

    // --- Threshold ----------------------------------------------------------
    {"vth0", "Threshold voltage at VBS = 0", "VTH0", "V", 1.0, 0.1, 3.0, false, ParamGroup::Threshold,
     "Gate-source voltage at which the inversion channel forms, used here as a model parameter.",
     "In a real MOSFET VTH is not free: VTH0 = VFB + 2phiF + gamma sqrt(2phiF) depends on the gate material, "
     "the substrate doping, the oxide thickness and fixed oxide charge, and the body bias adds to it. "
     "Turn on 'Calculate VTH from physics' to see that dependence. A higher VTH lowers VOV and therefore ID.",
     REF(device.thresholdVoltage)},

    // --- Geometry -------------------------------------------------------------
    {"L", "Channel length", "L", "µm", 1.0e6, 0.1e-6, 50.0e-6, true, ParamGroup::Geometry,
     "Distance between the source and drain junctions at the surface.",
     "ID is proportional to W/L, so a longer channel gives less current and a higher channel resistance "
     "(R ~ L / (W un Cox VOV)). The lateral field VDS/L is smaller, electrons drift more slowly and the transit "
     "time grows roughly as L^2. The pinch-off length dL is set by the doping, so it is a smaller fraction of "
     "a long channel: channel-length modulation is weaker. Below about 1 um short-channel effects matter and "
     "this long-channel model becomes optimistic.",
     REF(device.channelLength)},
    {"W", "Channel width", "W", "µm", 1.0e6, 0.2e-6, 100.0e-6, true, ParamGroup::Geometry,
     "Extent of the channel perpendicular to the current flow.",
     "Wider means more channel area in parallel: ID and the total inversion charge grow in proportion to W, "
     "and the channel resistance falls as 1/W. The charge per unit area, the potential along the channel and "
     "the pinch-off position do not depend on W in this model.",
     REF(device.channelWidth)},
    {"tox", "Gate oxide thickness", "tox", "nm", 1.0e9, 1.0e-9, 200.0e-9, true, ParamGroup::Geometry,
     "Thickness of the SiO2 insulator between gate and channel.",
     "Cox = eps_ox / tox: a thinner oxide gives more capacitance per area, so each volt of overdrive induces "
     "more channel charge and ID grows as 1/tox. The gate's electrostatic control is stronger. If VTH is "
     "calculated from physics, a thinner oxide also lowers VTH (smaller body-charge term gamma sqrt(2phiF)) "
     "and weakens the body effect (gamma ~ tox).",
     REF(device.oxideThickness)},
    {"xj", "Source/drain junction depth", "xj", "µm", 1.0e6, 0.05e-6, 1.0e-6, false, ParamGroup::Geometry,
     "Depth of the n+ source and drain diffusions. The lateral diffusion is taken as 0.8 xj.",
     "Changes the shape of the source, drain and their depletion regions. In this long-channel model it does not "
     "change ID (in short channels deeper junctions worsen short-channel effects).",
     REF(device.junctionDepth)},
    {"Ls", "Source region length", "Ls", "µm", 1.0e6, 0.4e-6, 5.0e-6, false, ParamGroup::Geometry,
     "Length of the n+ source along the channel direction.",
     "Geometry only in this model (source series resistance is not modelled).",
     REF(device.sourceLength)},
    {"Ld", "Drain region length", "Ld", "µm", 1.0e6, 0.4e-6, 5.0e-6, false, ParamGroup::Geometry,
     "Length of the n+ drain along the channel direction.",
     "Geometry only in this model (drain series resistance is not modelled).",
     REF(device.drainLength)},
    {"tg", "Gate electrode thickness", "tgate", "µm", 1.0e6, 0.05e-6, 1.0e-6, false, ParamGroup::Geometry,
     "Thickness of the polysilicon gate.",
     "Geometry only in this model (gate resistance and polysilicon depletion are not modelled).",
     REF(device.gateThickness)},
    {"tsub", "Substrate thickness", "tsub", "mm", 1.0e3, 50.0e-6, 1.0e-3, true, ParamGroup::Geometry,
     "Thickness of the silicon wafer (body).",
     "Only the top few micrometres take part in MOSFET action, so this changes the drawing only; "
     "the drawn substrate is truncated.",
     REF(device.substrateThickness)},

    // --- Material -------------------------------------------------------------
    {"NA", "Substrate doping", "NA", "cm⁻³", 1.0e-6, 1.0e20, 1.0e25, true, ParamGroup::Material,
     "Acceptor (boron) concentration of the p-type body.",
     "More doping raises phiF and the depletion charge: the depletion regions get thinner (W ~ 1/sqrt(NA)), "
     "the body factor gamma grows, and a calculated VTH rises. Fewer free electrons exist in the bulk "
     "(n0 = ni^2/NA). The pinch-off region is shorter, so channel-length modulation weakens. "
     "The junction built-in potential grows slowly (logarithmically).",
     REF(device.substrateDoping)},
    {"NDs", "Source doping", "ND,S", "cm⁻³", 1.0e-6, 1.0e23, 1.0e27, true, ParamGroup::Material,
     "Donor concentration of the n+ source.",
     "Sets the built-in potential of the source-body junction, Vbi = (kT/q) ln(NA ND / ni^2), and therefore its "
     "depletion width. Being much larger than NA, it barely changes ID in this model.",
     REF(device.sourceDoping)},
    {"NDd", "Drain doping", "ND,D", "cm⁻³", 1.0e-6, 1.0e23, 1.0e27, true, ParamGroup::Material,
     "Donor concentration of the n+ drain.",
     "Sets the built-in potential of the drain-body junction and therefore its depletion width, which also grows "
     "with VDS.",
     REF(device.drainDoping)},
    {"un", "Electron mobility", "µn", "cm²/(V·s)", 1.0e4, 0.005, 0.14, false, ParamGroup::Material,
     "Mobility of electrons in the inversion channel at 300 K (surface mobility, lower than in bulk silicon).",
     "Drift velocity v = un E and ID are proportional to un; the channel resistance falls as 1/un and the transit "
     "time shortens. Particles move faster.",
     REF(device.electronMobility)},
    {"up", "Hole mobility", "µp", "cm²/(V·s)", 1.0e4, 0.002, 0.05, false, ParamGroup::Material,
     "Mobility of holes in the p-type substrate at 300 K.",
     "Sets the substrate resistivity 1/(q NA up). Holes do not carry the drain current of an n-channel MOSFET, so "
     "ID does not depend on it.",
     REF(device.holeMobility)},
    {"epsSi", "Semiconductor permittivity", "εr,Si", "", 1.0, 4.0, 20.0, false, ParamGroup::Material,
     "Relative dielectric permittivity of the semiconductor (silicon 11.7).",
     "Higher permittivity widens all depletion regions (W ~ sqrt(eps)), raises gamma and a calculated VTH, and "
     "lengthens the pinch-off region.",
     REF(device.siliconPermittivity)},
    {"epsOx", "Oxide permittivity", "εr,ox", "", 1.0, 2.0, 30.0, false, ParamGroup::Material,
     "Relative permittivity of the gate insulator (SiO2 3.9; high-k materials such as HfO2 about 20-25).",
     "Cox = eps_ox / tox grows in proportion, with the same effects as a thinner oxide: more inversion charge per "
     "volt, more ID, stronger gate control.",
     REF(device.oxidePermittivity)},
    {"T", "Temperature", "T", "K", 1.0, 200.0, 450.0, false, ParamGroup::Material,
     "Lattice temperature of the device.",
     "Raises kT/q. With temperature dependence on, ni grows steeply (about doubling every 11 K near 300 K), "
     "so phiF and a calculated VTH fall; mobility falls as T^-1.5, so at fixed VOV the current drops.",
     REF(device.temperature)},
    {"ni", "Intrinsic carrier density at 300 K", "ni", "cm⁻³", 1.0e-6, 1.0e12, 1.0e19, true, ParamGroup::Material,
     "Intrinsic carrier concentration of the semiconductor at 300 K (silicon 1e10 cm⁻³).",
     "Enters phiF = (kT/q) ln(NA/ni), the built-in potentials and the bulk electron density n0 = ni^2/NA. "
     "A larger ni lowers phiF and therefore a calculated VTH and the depletion depths.",
     REF(device.intrinsicDensity300)},

    // --- Model ----------------------------------------------------------------
    {"theta", "Mobility degradation coefficient", "θ", "1/V", 1.0, 0.0, 1.0, false, ParamGroup::Model,
     "Strength of the vertical-field mobility reduction, ueff = un / (1 + theta VOV).",
     "Strong gate fields press electrons against the oxide interface where they scatter more, so high VGS gives "
     "less current than the square law predicts.",
     REF(device.mobilityTheta)},

    // --- Display --------------------------------------------------------------
    {"drawScale", "Drawn channel thickness", "", "×", 1.0, 0.25, 4.0, false, ParamGroup::Display,
     "How thick the inversion channel is drawn per unit of calculated inversion charge (illustrative).",
     "Drawing only: the real layer is a few nanometres thick.",
     REF(render.channelDrawScale)},
    {"particles", "Particle density", "", "×", 1.0, 0.25, 3.0, false, ParamGroup::Display,
     "Number of dots per unit of charge.",
     "Drawing only: each dot stands for many electrons; the label in the view says how many.",
     REF(render.particleDensity)},
    {"timescale", "Animation time scale", "", "ps per s", 1.0, 1.0, 2000.0, true, ParamGroup::Display,
     "Real time shown per second of animation. Electrons move at their calculated drift velocity.",
     "Slow motion only: the physics does not depend on it.",
     REF(render.picosecondsPerSecond)},
};

const std::vector<ToggleSpec> kToggles = {
    {"vthPhysics", "Calculate VTH from physics", ParamGroup::Threshold,
     "VTH0 = VFB + 2phiF + gamma sqrt(2phiF) with an n+ polysilicon gate and no oxide charge, so it follows NA, tox, "
     "permittivities, T and ni. When off, VTH0 is the value set above.",
     FLAG(device.thresholdFromPhysics)},
    {"bodyEffect", "Body effect", ParamGroup::Model,
     "VTH rises with reverse body bias: VTH = VTH0 + gamma (sqrt(2phiF - VBS) - sqrt(2phiF)).",
     FLAG(device.bodyEffect)},
    {"clm", "Channel-length modulation", ParamGroup::Model,
     "In saturation the pinch-off point moves towards the source by dL = sqrt(2 eps_si (VDS - VDS,sat) / (q NA)) and "
     "ID grows as L / (L - dL). When off: ideal saturation, flat ID, pinch-off fixed at the drain.",
     FLAG(device.channelLengthModulation)},
    {"mobDeg", "Mobility degradation", ParamGroup::Model,
     "ueff = un / (1 + theta VOV): vertical-field scattering at the oxide interface.",
     FLAG(device.mobilityDegradation)},
    {"tempDep", "Temperature dependence of ni and mobility", ParamGroup::Model,
     "ni(T) = ni(300 K) (T/300)^1.5 exp(-Eg/2k (1/T - 1/300)) and u(T) = u(300 K) (T/300)^-1.5. "
     "When off, T changes only kT/q.",
     FLAG(device.temperatureDependence)},
};

const MosfetConfig &defaults()
{
    static const MosfetConfig config;
    return config;
}

} // namespace

void ParamSpec::set(MosfetConfig &config, double value) const
{
    ref(config) = std::clamp(value, minimum, maximum);
}

double ParamSpec::defaultValue() const { return get(defaults()); }
bool ToggleSpec::defaultValue() const { return get(defaults()); }

const std::vector<ParamSpec> &parameterSpecs() { return kParameters; }
const std::vector<ToggleSpec> &toggleSpecs() { return kToggles; }

const ParamSpec *findParameter(const std::string &key)
{
    for (const ParamSpec &spec : kParameters) {
        if (key == spec.key)
            return &spec;
    }
    return nullptr;
}

const ToggleSpec *findToggle(const std::string &key)
{
    for (const ToggleSpec &spec : kToggles) {
        if (key == spec.key)
            return &spec;
    }
    return nullptr;
}

std::vector<std::string> validateConfig(MosfetConfig &c)
{
    std::vector<std::string> notes;
    for (const ParamSpec &spec : kParameters) {
        const double value = spec.get(c);
        if (!std::isfinite(value)) {
            spec.ref(c) = spec.defaultValue();
            notes.push_back(std::string(spec.name) + " was not a number; reset to its default.");
        } else if (value < spec.minimum || value > spec.maximum) {
            spec.set(c, value);
            notes.push_back(std::string(spec.name) + " limited to its allowed range.");
        }
    }
    MosfetParameters &d = c.device;
    // Both elliptical corners of a diffusion must fit inside its length.
    const double minimumRegion = 2.2 * d.lateralDiffusion();
    if (d.sourceLength < minimumRegion) {
        d.sourceLength = minimumRegion;
        notes.push_back("Source length raised to fit the junction corners (2.2 x lateral diffusion).");
    }
    if (d.drainLength < minimumRegion) {
        d.drainLength = minimumRegion;
        notes.push_back("Drain length raised to fit the junction corners (2.2 x lateral diffusion).");
    }
    if (d.junctionDepth > 0.5 * d.substrateThickness) {
        d.junctionDepth = 0.5 * d.substrateThickness;
        notes.push_back("Junction depth limited to half the substrate thickness.");
    }
    if (d.sourceDoping < 10.0 * d.substrateDoping || d.drainDoping < 10.0 * d.substrateDoping)
        notes.push_back("Source/drain doping is not much higher than NA: the one-sided junction picture is a rough "
                        "approximation.");
    // The substrate must stay extrinsic: ni(T) well below NA.
    auto niAt = [&] {
        return d.temperatureDependence ? semi::intrinsicDensity(d.intrinsicDensity300, d.temperature)
                                       : d.intrinsicDensity300;
    };
    if (niAt() > 0.1 * d.substrateDoping) {
        d.intrinsicDensity300 *= 0.1 * d.substrateDoping / niAt();
        notes.push_back("ni lowered so that ni(T) stays below NA/10; otherwise the substrate would be intrinsic "
                        "and there would be no MOSFET action.");
    }
    // Forward body bias beyond about 0.3 V would turn on the source-body diode.
    const double ni = niAt();
    const double twoPhi = 2.0 * semi::fermiPotential(d.substrateDoping, ni, d.temperature);
    if (c.bias.vbs > std::min(0.3, twoPhi - 0.05)) {
        c.bias.vbs = std::min(0.3, twoPhi - 0.05);
        notes.push_back("Forward body bias limited: the source-body junction would conduct.");
    }
    return notes;
}

std::string serializeConfig(const MosfetConfig &config)
{
    std::ostringstream out;
    out.precision(10);
    for (const ParamSpec &spec : kParameters)
        out << spec.key << '=' << spec.get(config) << '\n';
    for (const ToggleSpec &spec : kToggles)
        out << spec.key << '=' << (spec.get(config) ? 1 : 0) << '\n';
    return out.str();
}

MosfetConfig deserializeConfig(const std::string &text)
{
    MosfetConfig config;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        const std::size_t eq = line.find('=');
        if (eq == std::string::npos)
            continue;
        const std::string key = line.substr(0, eq);
        double value = 0.0;
        try {
            value = std::stod(line.substr(eq + 1));
        } catch (...) {
            continue;
        }
        if (const ParamSpec *spec = findParameter(key))
            spec->set(config, value);
        else if (const ToggleSpec *toggle = findToggle(key))
            toggle->ref(config) = value != 0.0;
    }
    validateConfig(config);
    return config;
}
