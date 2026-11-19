#include "PartInfo.h"

#include "Format.h"
#include "physics/DeviceState.h"
#include "physics/Electrostatics.h"
#include "physics/MosfetParameters.h"

#include <QCoreApplication>

#include <cmath>

using mosfet::PartId;

namespace {

QString tr(const char *text) { return QCoreApplication::translate("PartInfo", text); }

QString superscript(int value)
{
    static const QString digits = QStringLiteral("⁰¹²³⁴⁵⁶⁷⁸⁹");
    QString out;
    if (value < 0)
        out += QStringLiteral("⁻");
    for (QChar c : QString::number(std::abs(value)))
        out += digits.at(c.digitValue());
    return out;
}

QString micrometres(double metres, int digits = 3) { return Format::number(metres * 1.0e6, digits) + QStringLiteral(" µm"); }
QString nanometres(double metres) { return Format::number(metres * 1.0e9, 3) + QStringLiteral(" nm"); }


} // namespace

QString dopingText(double perCubicMetre)
{
    const double perCm3 = perCubicMetre * 1.0e-6;
    const int exponent = static_cast<int>(std::floor(std::log10(perCm3) + 1.0e-9));
    const double mantissa = perCm3 / std::pow(10.0, exponent);
    QString text = QStringLiteral("10") + superscript(exponent) + QStringLiteral(" cm⁻³");
    if (std::fabs(mantissa - 1.0) > 0.005)
        text = Format::number(mantissa, 3) + QStringLiteral("×") + text;
    return text;
}

QString statusName(PartText::Status status)
{
    switch (status) {
    case PartText::Calculated: return tr("Calculated");
    case PartText::Illustrative: return tr("Illustrative");
    default: return tr("Geometry");
    }
}

QString statusColor(PartText::Status status)
{
    switch (status) {
    case PartText::Calculated: return QStringLiteral("#3ddc84");
    case PartText::Illustrative: return QStringLiteral("#ffb454");
    default: return QStringLiteral("#8a94a3");
    }
}

QString tooltipHtml(const PartText &text)
{
    return QStringLiteral("<b>%1</b><br><span style='color:#8a94a3'>%2</span><p style='margin-top:4px'>%3</p>"
                          "<span style='color:%4'>%5:</span> <span style='color:#8a94a3'>%6</span>")
        .arg(text.title.toHtmlEscaped(), text.subtitle.toHtmlEscaped(), text.tooltip.toHtmlEscaped(),
             statusColor(text.status), statusName(text.status), text.statusText.toHtmlEscaped());
}

PartTexts describeParts(const MosfetParameters &p, const DeviceState &state)
{
    const double cox = state.oxideCapacitance;
    const double phiF = state.fermiPotential;
    const QString na = dopingText(p.substrateDoping), nds = dopingText(p.sourceDoping), ndd = dopingText(p.drainDoping);
    const QString n0 = dopingText(state.minorityElectrons);

    PartTexts t;
    auto &gate = t[mosfet::index(PartId::Gate)];
    gate.title = tr("Gate electrode");
    gate.subtitle = tr("n+ polysilicon · VG = %1 V").arg(Format::number(state.bias.vs + state.bias.vgs, 3));
    gate.tooltip = tr("The control terminal. A gate voltage sets up a vertical electric field through the oxide; "
                      "no current flows into the gate because the oxide insulates it.");
    gate.details = tr("Heavily doped n+ polysilicon, %1 thick. The gate, the oxide and the silicon form a capacitor: "
                      "charge on the gate is mirrored by charge in the silicon just below the oxide, which is how "
                      "the gate controls the channel without touching it. VGS = %2 V against VTH = %3 V.")
                       .arg(micrometres(p.gateThickness), Format::number(state.bias.vgs, 3),
                            Format::number(state.threshold, 3));
    gate.statusText = tr("Shape from the device dimensions (thickness exaggerated, see Dimensions).");

    auto &oxide = t[mosfet::index(PartId::GateOxide)];
    oxide.title = tr("Gate oxide (SiO₂)");
    oxide.subtitle = tr("%1 · Cox = %2").arg(nanometres(p.oxideThickness), Format::number(cox * 1.0e3, 3) + QStringLiteral(" fF/µm²"));
    oxide.tooltip = tr("A thin insulating layer between gate and channel. Its capacitance per area, Cox = εox/tox, "
                       "sets how much channel charge each volt of overdrive induces.");
    oxide.details = tr("tox = %1, relative permittivity %2. Cox = εox/tox = %3 F/m² (%4 fF/µm²). Halving tox doubles "
                       "Cox and so the channel charge and the drain current. Drawn thicker than scale.")
                        .arg(nanometres(p.oxideThickness), Format::number(p.oxidePermittivity, 3),
                             Format::number(cox, 4), Format::number(cox * 1.0e3, 3));
    oxide.statusText = tr("Cox calculated from tox and εox; thickness exaggerated.");
    oxide.status = PartText::Calculated;

    auto &channel = t[mosfet::index(PartId::Channel)];
    channel.title = tr("Inversion channel");
    const double qSource = state.inversionCharge.empty() ? 0.0 : state.inversionCharge.front();
    channel.subtitle = state.channelExists
                           ? tr("|Qinv(0)| = %1 · ns = %2").arg(Format::si(qSource * 1.0e-4, QStringLiteral("C/cm²"), 3),
                                                              dopingText(qSource / semi::kElementaryCharge * 1.0e2).replace(QStringLiteral("cm⁻³"), QStringLiteral("cm⁻²")))
                           : tr("no channel (cutoff)");
    channel.tooltip = tr("A thin layer of electrons at the silicon surface that forms when VGS exceeds VTH. Its charge "
                         "falls from Cox·VOV at the source towards the drain, and vanishes at the pinch-off point "
                         "in saturation.");
    channel.details = tr("In the p-type body electrons are scarce (n₀ = nᵢ²/NA ≈ %1), yet a gate voltage above "
                         "threshold draws so many to the surface that it becomes n-type there. The inversion charge "
                         "follows the gradual-channel approximation, |Qinv(x)| = Cox (VGS − VTH − V(x)); the drawn "
                         "thickness follows that charge. The real layer is only a few nanometres thick.")
                          .arg(n0);
    channel.statusText = tr("Charge calculated (gradual-channel approximation); drawn thickness is an illustrative "
                            "mapping of that charge.");
    channel.status = PartText::Calculated;

    auto &depletion = t[mosfet::index(PartId::Depletion)];
    depletion.title = tr("Depletion region");
    depletion.subtitle = tr("no free carriers · fixed ionised acceptors");
    depletion.tooltip = tr("Silicon emptied of mobile holes, leaving the negative charge of the ionised acceptors. "
                           "It surrounds the junctions and lies under the gate; it widens towards the drain as the "
                           "channel potential rises.");
    depletion.details = tr("Source junction: Vbi = %1 V with VSB = %2 V gives %3 on the p side. Drain junction: "
                           "Vbi + VDB = %4 V gives %5. Under the gate the surface potential is 2φF + VSB + V(x) "
                           "(2φF = %6 V), so the depth grows from %7 at the source to %8 at the drain.")
                            .arg(Format::number(state.builtInSource, 3), Format::number(-state.bias.vbs, 3),
                                 micrometres(state.sourceJunctionDepletion),
                                 Format::number(state.builtInDrain + state.bias.vds - state.bias.vbs, 3),
                                 micrometres(state.drainJunctionDepletion), Format::number(2.0 * phiF, 3),
                                 micrometres(state.gateDepletion.empty() ? 0.0 : state.gateDepletion.front()),
                                 micrometres(state.gateDepletion.empty() ? 0.0 : state.gateDepletion.back()));
    depletion.statusText = tr("Calculated widths (depletion approximation) for the present bias; junction corners "
                              "approximated by ellipses; depths exaggerated (see Dimensions).");
    depletion.status = PartText::Calculated;

    auto &source = t[mosfet::index(PartId::Source)];
    source.title = tr("N+ source");
    source.subtitle = tr("ND = %1 · VS = %2 V").arg(nds, Format::number(state.bias.vs, 3));
    source.tooltip = tr("Heavily donor-doped region full of free electrons. Electrons enter the channel from here.");
    source.details = tr("ND = %1. It is the reference terminal: VGS, VDS and VBS are measured from it. "
                        "Junction depth %2, length %3.")
                         .arg(nds, micrometres(p.junctionDepth), micrometres(p.sourceLength));
    source.statusText = tr("Shape from the device dimensions; junction depth exaggerated.");

    auto &drain = t[mosfet::index(PartId::Drain)];
    drain.title = tr("N+ drain");
    drain.subtitle = tr("ND = %1 · VD = %2 V").arg(ndd, Format::number(state.bias.vs + state.bias.vds, 3));
    drain.tooltip = tr("Collects the electrons that cross the channel. Conventional current ID flows the other way, "
                       "into the drain terminal and from drain to source.");
    drain.details = tr("ND = %1. A positive VDS reverse-biases the drain-body junction, so its depletion region grows "
                       "with VDS (now %2 on the p side). Length %3.")
                        .arg(ndd, micrometres(state.drainJunctionDepletion), micrometres(p.drainLength));
    drain.statusText = source.statusText;

    auto &body = t[mosfet::index(PartId::Substrate)];
    body.title = tr("P-type substrate (body)");
    body.subtitle = tr("NA = %1 · VB = %2 V").arg(na, Format::number(state.bias.vs + state.bias.vbs, 3));
    body.tooltip = tr("Silicon doped with acceptors. Holes are the majority carriers; free electrons are rare.");
    body.details = tr("NA = %1: about that many holes per cm³ and only nᵢ²/NA ≈ %2 electrons (nᵢ = %3 at %4 K). "
                      "φF = %5 V, resistivity %6. The body terminal sets its potential; reverse body bias raises "
                      "VTH (body effect). The wafer is %7 thick; only the top is drawn.")
                       .arg(na, n0, dopingText(state.intrinsicDensity), Format::number(p.temperature, 4),
                            Format::number(phiF, 3), Format::si(state.substrateResistivity * 100.0, QStringLiteral("Ω·cm"), 3),
                            micrometres(p.substrateThickness));
    body.statusText = tr("Shape from the device dimensions; thickness truncated.");

    const QString contactStatus = tr("Geometry: contact sizes are schematic.");
    auto contact = [&](PartId id, const QString &title, const QString &letter, const QString &what) {
        auto &c = t[mosfet::index(id)];
        c.title = title;
        c.subtitle = tr("metal · terminal %1").arg(letter);
        c.tooltip = what;
        c.details = what + QLatin1Char(' ')
                    + tr("Aluminium makes an ohmic (non-rectifying) contact to the heavily doped silicon below it.");
        c.statusText = contactStatus;
    };
    contact(PartId::GateContact, tr("Gate contact"), QStringLiteral("G"), tr("Connects the gate electrode to the circuit."));
    contact(PartId::SourceContact, tr("Source contact"), QStringLiteral("S"), tr("Connects the n+ source to the circuit."));
    contact(PartId::DrainContact, tr("Drain contact"), QStringLiteral("D"), tr("Connects the n+ drain to the circuit."));
    contact(PartId::BodyContact, tr("Body contact"), QStringLiteral("B"),
            tr("Back-side contact to the p-type substrate (the body terminal)."));
    return t;
}

PartText describeLattice(const MosfetParameters &p)
{
    PartText t;
    t.title = tr("Silicon lattice");
    t.subtitle = tr("diamond lattice, a = 0.543 nm · simplified");
    t.tooltip = tr("Each silicon atom bonds to four neighbours. Dopant atoms take silicon's place: donors (gold) in the "
                   "n+ regions, acceptors (violet) in the p-type silicon.");
    const double fraction = p.substrateDoping / semi::kSiliconAtomDensity;
    t.details = tr("Drawn as a flat square grid, several hundred times larger than the rest of the drawing. Real dopants are far rarer than shown: "
                   "NA = %1 is about %2 boron atoms per million silicon atoms.")
                    .arg(dopingText(p.substrateDoping), Format::number(fraction * 1.0e6, 2));
    t.statusText = tr("Illustrative: spacing, geometry and dopant fraction are not to scale.");
    t.status = PartText::Illustrative;
    return t;
}
