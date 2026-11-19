#include "PhysicsCheck.h"

#include "Explainer.h"
#include "Format.h"
#include "PartInfo.h"
#include "physics/Electrostatics.h"
#include "physics/Validation.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QTextBrowser>
#include <QVBoxLayout>

#include <cmath>

namespace PhysicsCheck {

namespace {

QString n(double v, int digits = 4) { return Format::number(v, digits); }
QString si(double v, const char *unit, int digits = 4) { return Format::si(v, QString::fromUtf8(unit), digits); }

QString row(const QString &equation, const QString &value, const QString &status = QStringLiteral("Calculated"))
{
    const QString color = status == QLatin1String("Calculated") ? QStringLiteral("#3ddc84")
                          : status == QLatin1String("Approximation") ? QStringLiteral("#ffb454") : QStringLiteral("#8a94a3");
    return QStringLiteral("<tr><td style='padding:3px 10px 3px 0'><code>%1</code></td><td style='padding:3px 10px'>%2</td>"
                          "<td style='color:%3'>%4</td></tr>")
        .arg(equation.toHtmlEscaped(), value, color, status);
}

} // namespace

QString report(const DeviceModel &model, const MosfetConfig &config, const DeviceState &s)
{
    const MosfetParameters &p = config.device;
    const Bias &b = config.bias;
    QString html = QStringLiteral("<style>h3{color:#4cc2ff;margin:14px 0 6px 0} td{vertical-align:top} "
                                  "code{color:#e6eaf0}</style>");
    html += QStringLiteral("<p><b>%1</b><br><span style='color:#8a94a3'>N-channel enhancement MOSFET, n+ polysilicon gate, "
                           "uniformly doped p-type body.</span></p>")
                .arg(QString::fromStdString(s.modelName).toHtmlEscaped());

    html += QStringLiteral("<h3>Equations with the present values</h3><table>");
    html += row(QStringLiteral("Cox = εr,ox ε0 / tox"), QStringLiteral("%1 × 8.854e-12 / %2 nm = %3 F/m² (%4 fF/µm²)")
                                                       .arg(n(p.oxidePermittivity, 3), n(p.oxideThickness * 1e9, 4),
                                                            n(s.oxideCapacitance), n(s.oxideCapacitance * 1e3, 4)));
    html += row(QStringLiteral("kT/q"), QStringLiteral("T = %1 K → %2 mV").arg(n(p.temperature), n(s.thermalVoltage * 1e3)));
    html += row(QStringLiteral("ni(T)"), QStringLiteral("%1%2").arg(dopingText(s.intrinsicDensity),
                                                                   p.temperatureDependence ? QStringLiteral(" (from ni(300 K) and T)") : QString()));
    html += row(QStringLiteral("φF = (kT/q) ln(NA/ni)"), QStringLiteral("%1 × ln(%2 / %3) = %4 V")
                                                       .arg(n(s.thermalVoltage), dopingText(p.substrateDoping),
                                                            dopingText(s.intrinsicDensity), n(s.fermiPotential)));
    html += row(QStringLiteral("γ = √(2 q εs NA) / Cox"), QStringLiteral("%1 V½").arg(n(s.bodyFactor)));
    html += row(QStringLiteral("VTH0 = VFB + 2φF + γ√(2φF)  (n+ poly, no oxide charge)"),
                QStringLiteral("%1 V (physics estimate)%2").arg(n(s.physicalThreshold),
                    p.thresholdFromPhysics ? QStringLiteral(" — in use") : QStringLiteral("; model parameter in use: %1 V").arg(n(p.thresholdVoltage))));
    html += row(QStringLiteral("VTH = VTH0 + γ(√(2φF − VBS) − √(2φF))"),
                QStringLiteral("VBS = %1 V → VTH = %2 V%3").arg(n(b.vbs), n(s.threshold), p.bodyEffect ? QString() : QStringLiteral(" (body effect off)")));
    html += row(QStringLiteral("VOV = VGS − VTH"), QStringLiteral("%1 − %2 = %3 V").arg(n(b.vgs), n(s.threshold), n(s.overdrive)));
    QString condition;
    switch (s.region) {
    case OperatingRegion::Cutoff: condition = QStringLiteral("VOV ≤ 0 → cutoff"); break;
    case OperatingRegion::Linear: condition = QStringLiteral("VDS = %1 V < VOV → linear").arg(n(b.vds)); break;
    case OperatingRegion::Saturation: condition = QStringLiteral("VDS = %1 V ≥ VOV → saturation").arg(n(b.vds)); break;
    }
    html += row(QStringLiteral("Region"), QStringLiteral("<span style='color:%1'>%2</span>").arg(Explainer::regionColor(s.region), condition));
    if (s.region != OperatingRegion::Cutoff) {
        html += row(QStringLiteral("µeff = µn / (1 + θ VOV)"), QStringLiteral("%1 cm²/(V·s)%2").arg(n(s.effectiveMobility * 1e4),
                                                                                                  p.mobilityDegradation ? QString() : QStringLiteral(" (degradation off: µeff = µn(T))")));
        html += row(QStringLiteral("β = µeff Cox W / L"), si(s.beta, "A/V²"));
        if (s.region == OperatingRegion::Linear)
            html += row(QStringLiteral("ID = β [VOV VDS − VDS²/2]"), QStringLiteral("%1 × [%2 × %3 − %3²/2] = %4")
                                                                   .arg(si(s.beta, "A/V²"), n(s.overdrive), n(b.vds), si(s.drainCurrent, "A")));
        else {
            html += row(QStringLiteral("ID,sat = ½ β VOV²"), si(s.drainCurrentIdeal, "A"));
            html += row(QStringLiteral("ΔL = √(2 εs (VDS − VOV) / (q NA))"),
                        p.channelLengthModulation ? si(s.pinchOffLength, "m") : QStringLiteral("0 (CLM off)"), QStringLiteral("Approximation"));
            html += row(QStringLiteral("ID = ID,sat × L / (L − ΔL)"), si(s.drainCurrent, "A"));
            html += row(QStringLiteral("Pinch-off point x = L − ΔL"), QStringLiteral("%1 µm from the source (%2 % of L)")
                                                                  .arg(n(s.pinchOffPosition * 1e6), n(100.0 * s.pinchOffPosition / p.channelLength, 3)));
        }
        html += row(QStringLiteral("Qinv(x) = −Cox [VGS − VTH − V(x)]"),
                    QStringLiteral("|Qinv(0)| = %1 C/m² = %2 electrons/cm²").arg(n(s.inversionCharge.front()), dopingText(s.electronSheet.front() * 1e2).replace(QStringLiteral("cm⁻³"), QString())));
        html += row(QStringLiteral("ID = W µeff |Qinv(x)| dV/dx"), QStringLiteral("constant along the channel (checked below)"));
        html += row(QStringLiteral("R(VDS→0) = 1 / (β VOV)"), si(s.onResistance, "Ω"));
        html += row(QStringLiteral("Transit time τ = ∫ dx / v(x)"), s.transitTime > 0 ? si(s.transitTime, "s") : QStringLiteral("— (no drift at VDS = 0)"));
        html += row(QStringLiteral("n at the surface ≈ ns / (kT / q Es)"), dopingText(s.electronDensity.front()), QStringLiteral("Approximation"));
    }
    html += row(QStringLiteral("Wd(x) = √(2 εs (2φF − VBS + V(x)) / (q NA))"),
                QStringLiteral("%1 µm at the source end, %2 µm at the drain end").arg(n(s.gateDepletion.front() * 1e6, 3), n(s.gateDepletion.back() * 1e6, 3)));
    html += row(QStringLiteral("Junction depletion (p side), abrupt junction"),
                QStringLiteral("source %1 µm, drain %2 µm").arg(n(s.sourceJunctionDepletion * 1e6, 3), n(s.drainJunctionDepletion * 1e6, 3)));
    html += QStringLiteral("</table>");

    html += QStringLiteral("<h3>Self-consistency checks</h3><table>");
    int passed = 0, total = 0;
    for (const ValidationCheck &check : validateModel(model, p, b)) {
        if (!check.applicable)
            continue;
        ++total;
        const bool ok = check.pass();
        passed += ok;
        html += QStringLiteral("<tr><td style='color:%1;padding-right:8px'>%2</td><td style='padding-right:10px'>%3<br>"
                               "<span style='color:#8a94a3'>%4</span></td><td>expected %5<br>got %6</td></tr>")
                    .arg(ok ? QStringLiteral("#3ddc84") : QStringLiteral("#ff6b6b"), ok ? QStringLiteral("✓") : QStringLiteral("✗"),
                         QString::fromStdString(check.name).toHtmlEscaped(), QString::fromStdString(check.detail).toHtmlEscaped(),
                         QString::number(check.expected, 'g', 6), QString::number(check.actual, 'g', 6));
    }
    html += QStringLiteral("</table><p>%1 of %2 checks passed.</p>").arg(passed).arg(total);

    html += QStringLiteral("<h3>Assumptions</h3><ul>"
                           "<li>Long channel, gradual-channel (charge-sheet) approximation: the vertical field dominates, "
                           "the inversion layer has no thickness in the equations.</li>"
                           "<li>Constant mobility along the channel (optionally reduced by θ·VOV); no velocity saturation in "
                           "the channel. Past the pinch-off point electrons are taken to move at the saturation velocity "
                           "(approximation).</li>"
                           "<li>No subthreshold conduction: ID = 0 for VGS ≤ VTH.</li>"
                           "<li>No short-channel effects (DIBL, VTH roll-off, punch-through), no gate leakage, no "
                           "polysilicon depletion, no series resistance, no quantum effects.</li>"
                           "<li>Depletion approximation, uniform doping, abrupt junctions; ellipses for junction corners.</li>"
                           "<li>Physics VTH: n+ polysilicon gate (φms = −(Eg/2 + φF)), no fixed oxide charge.</li>"
                           "<li>Quasi-static: every operating point is a steady state; sweeps change it slowly.</li>"
                           "<li>Visualisation: drawn thickness of the channel is an illustrative mapping of the calculated "
                           "charge; dot positions are random samples of the calculated density; n+ electrons and "
                           "holes are illustrative.</li></ul>");
    if (!s.warnings.empty()) {
        html += QStringLiteral("<h3>Warnings for this operating point</h3><ul>");
        for (const std::string &w : s.warnings)
            html += QStringLiteral("<li style='color:#ffb454'>%1</li>").arg(QString::fromStdString(w).toHtmlEscaped());
        html += QStringLiteral("</ul>");
    }
    return html;
}

void show(QWidget *parent, const DeviceModel &model, const MosfetConfig &config, const DeviceState &state)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(QObject::tr("Physics check"));
    dialog.resize(900, 720);
    auto *browser = new QTextBrowser;
    browser->setObjectName(QStringLiteral("physicsReport"));
    browser->setHtml(report(model, config, state));
    browser->setStyleSheet(QStringLiteral("QTextBrowser { background:#161b23; border:1px solid #2a323e; border-radius:8px; padding:8px; }"));
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(16, 16, 16, 12);
    layout->addWidget(browser, 1);
    layout->addWidget(buttons);
    dialog.exec();
}

} // namespace PhysicsCheck
