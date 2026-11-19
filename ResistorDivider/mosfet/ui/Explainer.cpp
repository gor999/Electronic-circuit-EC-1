#include "Explainer.h"

#include "Format.h"

#include <QCoreApplication>
#include <QStringList>

#include <cmath>

namespace Explainer {

namespace {

QString tr(const char *text) { return QCoreApplication::translate("Explainer", text); }

QString percent(double before, double after)
{
    if (before == 0.0)
        return QString();
    const double change = (after - before) / std::fabs(before) * 100.0;
    return QStringLiteral(" (%1%2 %)").arg(change >= 0 ? QStringLiteral("+") : QString()).arg(QString::number(change, 'f', std::fabs(change) < 10 ? 1 : 0));
}

bool changed(double a, double b)
{
    return std::fabs(a - b) > 1.0e-3 * std::max(std::fabs(a), std::fabs(b)) + 1.0e-30;
}

QString ohms(double r) { return r > 0.0 ? Format::si(r, QStringLiteral("Ω"), 3) : tr("∞"); }

QString transition(const DeviceState &a, const DeviceState &b)
{
    using R = OperatingRegion;
    if (a.region == b.region)
        return QString();
    if (a.region == R::Cutoff)
        return tr("VGS exceeded VTH: the gate field attracts electrons to the oxide–silicon interface. They form an "
                  "inversion layer, a conductive channel connecting source and drain, so drain current can flow.");
    if (b.region == R::Cutoff)
        return tr("VGS fell below VTH: the inversion layer disappears and source and drain are again separated by two "
                  "back-to-back p–n junctions. In this model ID drops to zero (real devices keep a small "
                  "subthreshold current).");
    if (b.region == R::Saturation)
        return tr("VDS reached VOV = VGS − VTH: the inversion charge at the drain end, Cox (VOV − VDS), fell to "
                  "zero, so the channel is pinched off. ID does not drop to zero: electrons that reach the pinch-off "
                  "point are swept to the drain by the strong field of the depletion region. ID stops growing with "
                  "VDS (saturation).");
    return tr("VDS fell below VOV: the channel again reaches all the way to the drain (linear region), and ID depends "
              "strongly on VDS.");
}

} // namespace

QString regionName(OperatingRegion region)
{
    switch (region) {
    case OperatingRegion::Cutoff: return tr("Cutoff");
    case OperatingRegion::Linear: return tr("Linear (ohmic)");
    case OperatingRegion::Saturation: return tr("Saturation");
    }
    return QString();
}

QString regionColor(OperatingRegion region)
{
    switch (region) {
    case OperatingRegion::Cutoff: return QStringLiteral("#8a94a3");
    case OperatingRegion::Linear: return QStringLiteral("#3ddc84");
    case OperatingRegion::Saturation: return QStringLiteral("#ffb454");
    }
    return QStringLiteral("#e6eaf0");
}

QString describeChange(const QString &what, const QString &from, const QString &to, const DeviceState &a,
                       const DeviceState &b, const char *effects)
{
    QStringList lines;
    auto add = [&](const QString &name, double before, double after, const QString &formattedBefore,
                   const QString &formattedAfter, bool withPercent = true) {
        if (!changed(before, after))
            return;
        lines << QStringLiteral("%1: %2 → %3%4").arg(name, formattedBefore, formattedAfter,
                                                     withPercent ? percent(before, after) : QString());
    };
    auto volts = [](double v) { return Format::number(Format::tidy(v, 1.0e-6), 4) + QStringLiteral(" V"); };
    auto amps = [](double i) { return Format::si(Format::tidy(i, 1.0e-18), QStringLiteral("A"), 3); };
    auto um = [](double m) { return Format::number(m * 1.0e6, 3) + QStringLiteral(" µm"); };

    if (a.region != b.region)
        lines << QStringLiteral("Region: <span style='color:%1'>%2</span> → <span style='color:%3'>%4</span>")
                     .arg(regionColor(a.region), regionName(a.region), regionColor(b.region), regionName(b.region));
    add(tr("VTH"), a.threshold, b.threshold, volts(a.threshold), volts(b.threshold), false);
    add(tr("ID"), a.drainCurrent, b.drainCurrent, amps(a.drainCurrent), amps(b.drainCurrent));
    add(tr("Channel resistance (VDS → 0)"), a.onResistance, b.onResistance, ohms(a.onResistance), ohms(b.onResistance));
    add(tr("Cox"), a.oxideCapacitance, b.oxideCapacitance, Format::number(a.oxideCapacitance * 1e3, 3) + QStringLiteral(" fF/µm²"),
        Format::number(b.oxideCapacitance * 1e3, 3) + QStringLiteral(" fF/µm²"));
    const double qa = a.inversionCharge.empty() ? 0.0 : a.inversionCharge.front();
    const double qb = b.inversionCharge.empty() ? 0.0 : b.inversionCharge.front();
    add(tr("Inversion charge at the source"), qa, qb, Format::si(qa * 1e-4, QStringLiteral("C/cm²"), 3),
        Format::si(qb * 1e-4, QStringLiteral("C/cm²"), 3));
    add(tr("Total channel electrons"), a.totalInversionCharge, b.totalInversionCharge,
        Format::number(a.totalInversionCharge / 1.602176634e-19, 3), Format::number(b.totalInversionCharge / 1.602176634e-19, 3));
    if (a.pinchedOff || b.pinchedOff) {
        const QString pa = a.pinchedOff ? um(a.pinchOffPosition) : tr("none");
        const QString pb = b.pinchedOff ? um(b.pinchOffPosition) : tr("none");
        if (pa != pb)
            lines << tr("Pinch-off point (from the source): %1 → %2").arg(pa, pb);
    }
    add(tr("Pinch-off voltage VDS,sat"), a.saturationVoltage, b.saturationVoltage, volts(a.saturationVoltage), volts(b.saturationVoltage), false);
    add(tr("Transit time"), a.transitTime, b.transitTime, Format::si(a.transitTime, QStringLiteral("s"), 3),
        Format::si(b.transitTime, QStringLiteral("s"), 3));
    const double da = a.gateDepletion.empty() ? 0.0 : a.gateDepletion.back();
    const double db = b.gateDepletion.empty() ? 0.0 : b.gateDepletion.back();
    add(tr("Depletion depth at the drain end"), da, db, um(da), um(db));
    add(tr("Drain junction depletion"), a.drainJunctionDepletion, b.drainJunctionDepletion, um(a.drainJunctionDepletion),
        um(b.drainJunctionDepletion));

    QString out = QStringLiteral("<b>%1: %2 → %3</b>").arg(what.toHtmlEscaped(), from, to);
    if (lines.isEmpty())
        out += QStringLiteral("<p>") + tr("No calculated result changed: in this model the parameter only changes the "
                                          "drawing.") + QStringLiteral("</p>");
    else
        out += QStringLiteral("<ul style='margin-left:-20px'><li>") + lines.join(QStringLiteral("</li><li>")) + QStringLiteral("</li></ul>");
    const QString moved = transition(a, b);
    if (!moved.isEmpty())
        out += QStringLiteral("<p>") + moved + QStringLiteral("</p>");
    if (effects && *effects)
        out += QStringLiteral("<p style='color:#8a94a3'><span style='color:#4cc2ff'>Why:</span> %1</p>")
                   .arg(QString::fromUtf8(effects).toHtmlEscaped());
    return out;
}

QString describeState(const DeviceState &s, const MosfetParameters &p)
{
    QString text = QStringLiteral("<b style='color:%1'>%2</b> — ").arg(regionColor(s.region), regionName(s.region));
    switch (s.region) {
    case OperatingRegion::Cutoff:
        text += tr("VGS = %1 V is below VTH = %2 V. There are not enough electrons at the surface to form a channel; "
                   "the silicon under the gate is depleted of holes but not inverted. ID = 0 in this model.")
                    .arg(Format::number(s.bias.vgs, 3), Format::number(s.threshold, 3));
        break;
    case OperatingRegion::Linear:
        text += tr("The channel reaches from source to drain. VDS = %1 V is below VOV = %2 V, so the inversion charge "
                   "only thins towards the drain (to %3 % of its source value). ID rises with VDS; for small VDS the "
                   "channel behaves like a resistor of %4.")
                    .arg(Format::number(s.bias.vds, 3), Format::number(s.overdrive, 3),
                         QString::number(100.0 * std::max(0.0, s.overdrive - s.bias.vds) / s.overdrive, 'f', 0),
                         ohms(s.onResistance));
        break;
    case OperatingRegion::Saturation:
        text += tr("VDS = %1 V exceeds VOV = %2 V, so the channel is pinched off %3 µm from the source "
                   "(%4 % of L). The inversion charge falls to zero at that point; beyond it electrons cross the "
                   "depleted region at high speed. ID ≈ ½ µCox (W/L) VOV² = %5%6.")
                    .arg(Format::number(s.bias.vds, 3), Format::number(s.overdrive, 3),
                         Format::number(s.pinchOffPosition * 1e6, 3),
                         QString::number(100.0 * s.pinchOffPosition / p.channelLength, 'f', 1),
                         Format::si(s.drainCurrent, QStringLiteral("A"), 3),
                         p.channelLengthModulation ? tr(", raised slightly by channel-length modulation") : QString());
        break;
    }
    return text;
}

} // namespace Explainer
