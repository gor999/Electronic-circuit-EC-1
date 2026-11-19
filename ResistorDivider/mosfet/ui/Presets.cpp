#include "Presets.h"

const std::vector<Experiment> &experiments()
{
    static const std::vector<Experiment> list = {
        {"MOSFET OFF",
         "VGS is below VTH. There is no inversion channel, only depletion under the gate; ID = 0. Source and drain are "
         "isolated by two back-to-back p-n junctions. Raise VGS past VTH and watch the channel appear.",
         {{"vgs", 0.4}, {"vds", 1.0}}, {}},
        {"MOSFET ON (small VDS)",
         "VGS is well above VTH and VDS is tiny: the channel is uniform from source to drain and the device behaves "
         "like a resistor. ID is proportional to VDS; check the channel resistance.",
         {{"vgs", 2.5}, {"vds", 0.05}}, {}},
        {"Linear region",
         "VDS is below VOV. The channel thins towards the drain because the channel potential rises along it; ID "
         "still grows with VDS, but sub-linearly. Look at V(x) and Qinv(x).",
         {{"vgs", 2.5}, {"vds", 0.9}}, {}},
        {"Saturation region",
         "VDS is well above VOV: the channel is pinched off before the drain and ID barely depends on VDS. The "
         "excess voltage drops across the short pinch-off region, where the field is strongest.",
         {{"vgs", 1.8}, {"vds", 3.0}}, {}},
        {"Pinch-off (onset)",
         "VDS equals VOV = VGS - VTH exactly: the inversion charge just reaches zero at the drain end. Increase VDS "
         "slightly and the pinch-off point starts moving towards the source.",
         {{"vgs", 1.8}, {"vds", 1.0}}, {}},
        {"High VGS",
         "A large overdrive: a thick, charge-rich channel, low resistance and a large current. Turn on mobility "
         "degradation to see the square law flatten at high VGS.",
         {{"vgs", 5.0}, {"vds", 1.0}}, {}},
        {"High VDS",
         "Deep saturation: the drain depletion is wide, the pinch-off point has moved well towards the source and "
         "channel-length modulation raises ID slightly above the ideal saturation value.",
         {{"vgs", 1.8}, {"vds", 6.0}}, {}},
        {"Short-channel MOSFET (L = 0.3 µm)",
         "ID is large because ID ~ 1/L, the transit time is very short and channel-length modulation is strong. "
         "Read the warning: real short-channel devices suffer velocity saturation and DIBL, not modelled here.",
         {{"L", 0.3e-6}, {"vgs", 1.8}, {"vds", 2.0}}, {}},
        {"Long-channel MOSFET (L = 10 µm)",
         "Five times longer than the default: ID is five times smaller, the resistance five times larger, the transit "
         "time about 25 times longer, and channel-length modulation is weak.",
         {{"L", 10.0e-6}, {"vgs", 1.8}, {"vds", 2.0}}, {}},
        {"Narrow channel (W = 1 µm)",
         "ID and the total channel charge are ten times smaller than with W = 10 µm; the charge per area, V(x) and "
         "the pinch-off point do not change.",
         {{"W", 1.0e-6}, {"vgs", 1.8}, {"vds", 1.0}}, {}},
        {"Wide channel (W = 50 µm)",
         "Five times wider: five times the current and total charge, one fifth of the channel resistance.",
         {{"W", 50.0e-6}, {"vgs", 1.8}, {"vds", 1.0}}, {}},
        {"Thin oxide (tox = 5 nm)",
         "Cox is four times larger: more charge per volt, more current, and (with VTH calculated from physics) a lower "
         "threshold voltage and a weaker body effect.",
         {{"tox", 5.0e-9}, {"vgs", 1.8}, {"vds", 1.0}}, {{"vthPhysics", true}}},
        {"Thick oxide (tox = 60 nm)",
         "Cox is three times smaller: weaker gate control, less charge, less current, and a higher calculated VTH.",
         {{"tox", 60.0e-9}, {"vgs", 3.0}, {"vds", 1.0}}, {{"vthPhysics", true}}},
        {"Low substrate doping (NA = 1e15 cm⁻³)",
         "VTH (calculated) is low, the depletion regions are an order of magnitude deeper, and channel-length "
         "modulation is strong because the pinch-off region is long.",
         {{"NA", 1.0e21}, {"vgs", 1.5}, {"vds", 2.0}}, {{"vthPhysics", true}}},
        {"High substrate doping (NA = 1e18 cm⁻³)",
         "VTH (calculated) is high, the depletion regions are thin and the body effect is strong.",
         {{"NA", 1.0e24}, {"vgs", 4.0}, {"vds", 2.0}}, {{"vthPhysics", true}}},
        {"Body effect (VBS = -2 V)",
         "Reverse body bias deepens the depletion everywhere and raises VTH by gamma (sqrt(2phiF - VBS) - sqrt(2phiF)); "
         "the current falls at the same VGS.",
         {{"vbs", -2.0}, {"vgs", 1.8}, {"vds", 1.0}}, {}},
        {"Hot device (T = 400 K)",
         "Mobility falls as T^-1.5, so ID drops; ni rises steeply, lowering phiF and a calculated VTH.",
         {{"T", 400.0}, {"vgs", 1.8}, {"vds", 1.0}}, {{"vthPhysics", true}}},
    };
    return list;
}

MosfetConfig applyExperiment(const Experiment &e)
{
    MosfetConfig config;
    for (const auto &[key, value] : e.toggles) {
        if (const ToggleSpec *spec = findToggle(key))
            spec->ref(config) = value;
    }
    for (const auto &[key, value] : e.values) {
        if (const ParamSpec *spec = findParameter(key))
            spec->set(config, value);
    }
    validateConfig(config);
    return config;
}
