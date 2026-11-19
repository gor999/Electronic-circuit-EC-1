#include "GraphPanel.h"

#include "Format.h"
#include "PlotWidget.h"
#include "physics/Electrostatics.h"

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace {

const QColor kColorA(0x4c, 0xc2, 0xff);
const QColor kColorB(0xff, 0xb4, 0x54);
const QColor kPinch(0xff, 0x7a, 0x6b);
const QColor kProbe(0xc3, 0x8b, 0xff);
const QColor kSweep(0xe6, 0xea, 0xf0);

const GraphType kTypes[] = {GraphType::IdVds, GraphType::IdVgs, GraphType::Potential, GraphType::Charge,
                            GraphType::Field, GraphType::Density, GraphType::Velocity, GraphType::PinchVds,
                            GraphType::IdTime, GraphType::ResistanceVgs, GraphType::IdLength, GraphType::IdWidth};

QString amps(double v) { return Format::si(v, QStringLiteral("A"), 3); }
QString volts(double v) { return Format::number(v, 3) + QStringLiteral(" V"); }
QString metres(double v) { return Format::si(v, QStringLiteral("m"), 3); }
QString ohms(double v) { return Format::si(v, QStringLiteral("Ω"), 3); }
QString seconds(double v) { return Format::number(v, 3) + QStringLiteral(" s"); }
QString chargePerCm2(double v) { return Format::si(v * 1.0e-4, QStringLiteral("C/cm²"), 3); }
QString fieldKvPerCm(double v) { return Format::number(v * 1.0e-5, 3) + QStringLiteral(" kV/cm"); }
QString perCm3(double v)
{
    if (v <= 0.0)
        return QStringLiteral("0");
    return QStringLiteral("1e%1").arg(int(std::lround(std::log10(v * 1.0e-6))));
}
QString speed(double v) { return Format::si(v, QStringLiteral("m/s"), 3); }

QColor shade(const QColor &base, double t)   // darker for t -> 0
{
    return QColor::fromRgbF(base.redF() * (0.35 + 0.65 * t), base.greenF() * (0.35 + 0.65 * t), base.blueF() * (0.35 + 0.65 * t));
}

} // namespace

QString GraphPanel::title(GraphType type)
{
    switch (type) {
    case GraphType::IdVds: return tr("ID vs VDS (output characteristics)");
    case GraphType::IdVgs: return tr("ID vs VGS (transfer characteristic)");
    case GraphType::Potential: return tr("Channel potential V(x)");
    case GraphType::Charge: return tr("Inversion charge |Qinv(x)|");
    case GraphType::Field: return tr("Lateral electric field |Ex(x)|");
    case GraphType::Density: return tr("Electron concentration n(x) at the surface");
    case GraphType::Velocity: return tr("Electron drift velocity v(x)");
    case GraphType::PinchVds: return tr("Pinch-off position vs VDS");
    case GraphType::IdTime: return tr("ID vs time during the sweep");
    case GraphType::ResistanceVgs: return tr("Channel resistance vs VGS");
    case GraphType::IdLength: return tr("ID vs channel length L");
    case GraphType::IdWidth: return tr("ID vs channel width W");
    }
    return QString();
}

GraphPanel::GraphPanel(GraphType initial, QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("transparentHost"));
    m_selector = new QComboBox;
    for (GraphType type : kTypes)
        m_selector->addItem(title(type), int(type));
    m_log = new QCheckBox(tr("log"));
    m_log->setToolTip(tr("Logarithmic vertical axis"));
    m_family = new QCheckBox(tr("family"));
    m_family->setChecked(true);
    m_family->setToolTip(tr("Show curves for several gate voltages"));
    m_plot = new PlotWidget;

    auto *header = new QHBoxLayout;
    header->setSpacing(8);
    header->addWidget(m_selector, 1);
    header->addWidget(m_family);
    header->addWidget(m_log);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    layout->addLayout(header);
    layout->addWidget(m_plot, 1);

    setType(initial);
    connect(m_selector, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] {
        const GraphType t = type();
        m_log->setChecked(t == GraphType::Density || t == GraphType::ResistanceVgs);
        m_family->setVisible(t == GraphType::IdVds);
        emit typeChanged();
    });
    connect(m_log, &QCheckBox::toggled, this, &GraphPanel::typeChanged);
    connect(m_family, &QCheckBox::toggled, this, &GraphPanel::typeChanged);
}

GraphType GraphPanel::type() const { return GraphType(m_selector->currentData().toInt()); }

void GraphPanel::setType(GraphType type)
{
    m_selector->setCurrentIndex(m_selector->findData(int(type)));
    m_log->setChecked(type == GraphType::Density || type == GraphType::ResistanceVgs);
    m_family->setVisible(type == GraphType::IdVds);
}

void GraphPanel::refresh(const GraphContext &c)
{
    const GraphType t = type();
    const bool log = m_log->isChecked();
    QVector<PlotSeries> series;
    QVector<PlotMarker> markers;
    QVector<PlotPoint> points;
    QString message;
    const int count = c.compare ? 2 : 1;
    auto colorOf = [](int slot) { return slot == 0 ? kColorA : kColorB; };
    auto nameOf = [&](int slot, const QString &text) {
        return c.compare ? (slot == 0 ? QStringLiteral("A ") : QStringLiteral("B ")) + text : text;
    };
    const DeviceModel &model = *c.model;

    switch (t) {
    case GraphType::IdVds: {
        m_plot->setAxes(tr("VDS"), volts, false, tr("ID"), amps, log);
        double vmax = 0.0;
        for (int s = 0; s < count; ++s)
            vmax = std::max(vmax, std::max(4.0, 1.25 * c.configs[s]->bias.vds));
        const int n = 120;
        for (int s = 0; s < count; ++s) {
            const MosfetConfig &cfg = *c.configs[s];
            const DeviceState &st = *c.states[s];
            std::vector<double> gates;
            if (m_family->isChecked() && !c.compare)
                for (double ov : {0.5, 1.0, 1.5, 2.0, 2.5})
                    gates.push_back(st.threshold + ov);
            gates.push_back(cfg.bias.vgs);
            for (std::size_t g = 0; g < gates.size(); ++g) {
                PlotSeries curve;
                const bool current = g + 1 == gates.size();
                curve.color = current ? colorOf(s) : shade(colorOf(s), 0.35 + 0.65 * double(g) / gates.size());
                curve.width = current ? 2.4 : 1.3;
                curve.name = current ? nameOf(s, tr("VGS = %1 V").arg(Format::number(gates[g], 3))) : QString();
                Bias b = cfg.bias;
                b.vgs = gates[g];
                for (int i = 0; i <= n; ++i) {
                    b.vds = vmax * i / n;
                    curve.points.append(QPointF(b.vds, model.solve(cfg.device, b).drainCurrent));
                }
                series.append(curve);
            }
            // Locus of the pinch-off points VDS = VOV.
            PlotSeries locus;
            locus.color = QColor(0xff, 0x7a, 0x6b, 200);
            locus.style = Qt::DashLine;
            locus.width = 1.2;
            locus.name = s == 0 ? tr("pinch-off  VDS = VOV") : QString();
            // Only as far as the highest gate voltage drawn, so it does not dwarf the curves.
            const double topGate = *std::max_element(gates.begin(), gates.end());
            Bias b = cfg.bias;
            for (int i = 1; i <= 60; ++i) {
                b.vds = vmax * i / 60.0;
                b.vgs = st.threshold + b.vds;
                if (b.vgs > topGate + 1e-9)
                    break;
                locus.points.append(QPointF(b.vds, model.solve(cfg.device, b).drainCurrent));
            }
            series.append(locus);
            points.append({QPointF(cfg.bias.vds, st.drainCurrent), colorOf(s)});
        }
        if (c.sweep && !c.sweep->empty()) {
            PlotSeries trace;
            trace.color = kSweep;
            trace.dots = true;
            trace.name = tr("sweep");
            for (const SweepSample &sample : *c.sweep)
                trace.points.append(QPointF(sample.vds, sample.id));
            series.append(trace);
        }
        break;
    }
    case GraphType::IdVgs: {
        m_plot->setAxes(tr("VGS"), volts, false, tr("ID"), amps, log);
        for (int s = 0; s < count; ++s) {
            const MosfetConfig &cfg = *c.configs[s];
            PlotSeries curve;
            curve.color = colorOf(s);
            curve.width = 2.2;
            curve.name = nameOf(s, tr("VDS = %1 V").arg(Format::number(cfg.bias.vds, 3)));
            Bias b = cfg.bias;
            for (int i = 0; i <= 160; ++i) {
                b.vgs = -1.0 + 7.0 * i / 160.0;
                curve.points.append(QPointF(b.vgs, model.solve(cfg.device, b).drainCurrent));
            }
            series.append(curve);
            markers.append({c.states[s]->threshold, colorOf(s), nameOf(s, QStringLiteral("VTH"))});
            points.append({QPointF(cfg.bias.vgs, c.states[s]->drainCurrent), colorOf(s)});
        }
        break;
    }
    case GraphType::Potential:
    case GraphType::Charge:
    case GraphType::Field:
    case GraphType::Density:
    case GraphType::Velocity: {
        QString yTitle;
        PlotWidget::Formatter fy;
        switch (t) {
        case GraphType::Potential: yTitle = tr("V(x)"); fy = volts; break;
        case GraphType::Charge: yTitle = tr("|Qinv|"); fy = chargePerCm2; break;
        case GraphType::Field: yTitle = tr("|Ex|"); fy = fieldKvPerCm; break;
        case GraphType::Density: yTitle = tr("n (cm⁻³)"); fy = perCm3; break;
        default: yTitle = tr("v"); fy = speed; break;
        }
        m_plot->setAxes(tr("x from the source"), metres, false, yTitle, fy, log);
        bool any = false;
        for (int s = 0; s < count; ++s) {
            const DeviceState &st = *c.states[s];
            if (!st.channelExists)
                continue;
            any = true;
            const std::vector<double> *profile = &st.potential;
            if (t == GraphType::Charge) profile = &st.inversionCharge;
            if (t == GraphType::Field) profile = &st.lateralField;
            if (t == GraphType::Density) profile = &st.electronDensity;
            if (t == GraphType::Velocity) profile = &st.velocity;
            PlotSeries curve, beyond;
            curve.color = colorOf(s);
            curve.width = 2.2;
            curve.name = c.compare ? nameOf(s, QString()) : QString();
            // Past the pinch-off point there is no inversion layer: the mobile
            // electrons crossing the depletion region are an approximation
            // (saturation velocity, current continuity) and drawn dashed.
            beyond.color = shade(colorOf(s), 0.75);
            beyond.style = Qt::DashLine;
            beyond.width = 1.6;
            const bool split = st.pinchedOff && (t == GraphType::Charge || t == GraphType::Density || t == GraphType::Velocity);
            if (split && s == 0)
                beyond.name = t == GraphType::Velocity ? tr("past pinch-off: ~ saturation velocity")
                                                       : tr("past pinch-off: electrons crossing (approx.)");
            const double L = c.configs[s]->device.channelLength;
            for (std::size_t i = 0; i < st.x.size(); ++i) {
                if (st.x[i] > L * (1.0 + 1e-6))
                    continue;
                const QPointF point(st.x[i], std::fabs((*profile)[i]));
                if (split && st.x[i] > st.pinchOffPosition) {
                    if (beyond.points.isEmpty() && t == GraphType::Charge)
                        curve.points.append(QPointF(st.pinchOffPosition, 0.0));
                    beyond.points.append(point);
                } else {
                    curve.points.append(point);
                }
            }
            series.append(curve);
            if (!beyond.points.isEmpty())
                series.append(beyond);
            if (st.pinchedOff)
                markers.append({st.pinchOffPosition, kPinch, nameOf(s, tr("pinch-off"))});
            const double probe = c.probeFraction * L;
            markers.append({probe, kProbe, s == 0 ? tr("probe") : QString()});
            if (t == GraphType::Velocity && s == 0) {
                PlotSeries vsat;
                vsat.color = QColor(0x8a, 0x94, 0xa3);
                vsat.style = Qt::DashLine;
                vsat.width = 1.0;
                vsat.name = tr("saturation velocity");
                vsat.points = {QPointF(0.0, semi::kSaturationVelocity), QPointF(L, semi::kSaturationVelocity)};
                series.append(vsat);
            }
        }
        if (!any)
            message = tr("No inversion channel: the device is in cutoff (VGS ≤ VTH).");
        break;
    }
    case GraphType::PinchVds: {
        m_plot->setAxes(tr("VDS"), volts, false, tr("pinch-off point from the source"), metres, false);
        for (int s = 0; s < count; ++s) {
            const MosfetConfig &cfg = *c.configs[s];
            const DeviceState &st = *c.states[s];
            PlotSeries curve, length;
            curve.color = colorOf(s);
            curve.width = 2.2;
            curve.name = nameOf(s, tr("x pinch-off"));
            length.color = shade(colorOf(s), 0.5);
            length.style = Qt::DotLine;
            length.width = 1.0;
            length.name = s == 0 ? tr("channel length L") : QString();
            const double vmax = std::max(6.0, cfg.bias.vds);
            Bias b = cfg.bias;
            for (int i = 0; i <= 150; ++i) {
                b.vds = vmax * i / 150.0;
                const DeviceState point = model.solve(cfg.device, b);
                if (point.pinchedOff)
                    curve.points.append(QPointF(b.vds, point.pinchOffPosition));
            }
            length.points = {QPointF(0.0, cfg.device.channelLength), QPointF(vmax, cfg.device.channelLength)};
            series.append(length);
            series.append(curve);
            if (st.channelExists)
                markers.append({st.saturationVoltage, kPinch, nameOf(s, QStringLiteral("VDS,sat"))});
            if (st.pinchedOff)
                points.append({QPointF(cfg.bias.vds, st.pinchOffPosition), colorOf(s)});
            if (!st.channelExists)
                message = tr("No channel in cutoff, so no pinch-off.");
        }
        break;
    }
    case GraphType::IdTime: {
        m_plot->setAxes(tr("sweep time"), seconds, false, tr("ID"), amps, log);
        if (!c.sweep || c.sweep->empty()) {
            message = tr("Run a sweep in the Experiment tab to record ID over time.");
        } else {
            PlotSeries curve;
            curve.color = kSweep;
            curve.width = 2.0;
            curve.name = tr("ID (recorded)");
            for (const SweepSample &sample : *c.sweep)
                curve.points.append(QPointF(sample.time, sample.id));
            series.append(curve);
            points.append({curve.points.back(), kColorA});
        }
        break;
    }
    case GraphType::ResistanceVgs: {
        m_plot->setAxes(tr("VGS"), volts, false, tr("resistance"), ohms, log);
        for (int s = 0; s < count; ++s) {
            const MosfetConfig &cfg = *c.configs[s];
            PlotSeries small, large;
            small.color = colorOf(s);
            small.width = 2.2;
            small.name = nameOf(s, tr("R (VDS → 0) = 1/(µCox W/L VOV)"));
            large.color = shade(colorOf(s), 0.6);
            large.style = Qt::DashLine;
            large.width = 1.4;
            large.name = nameOf(s, tr("VDS/ID at VDS = %1 V").arg(Format::number(cfg.bias.vds, 3)));
            Bias b = cfg.bias;
            for (int i = 0; i <= 160; ++i) {
                b.vgs = c.states[s]->threshold + 0.05 + (6.0 - c.states[s]->threshold) * i / 160.0;
                const DeviceState point = model.solve(cfg.device, b);
                if (point.onResistance > 0.0)
                    small.points.append(QPointF(b.vgs, point.onResistance));
                if (cfg.bias.vds > 0.0 && point.dcResistance > 0.0)
                    large.points.append(QPointF(b.vgs, point.dcResistance));
            }
            series.append(small);
            series.append(large);
            if (c.states[s]->onResistance > 0.0)
                points.append({QPointF(cfg.bias.vgs, c.states[s]->onResistance), colorOf(s)});
            markers.append({c.states[s]->threshold, colorOf(s), nameOf(s, QStringLiteral("VTH"))});
        }
        break;
    }
    case GraphType::IdLength:
    case GraphType::IdWidth: {
        const bool length = t == GraphType::IdLength;
        m_plot->setAxes(length ? tr("channel length L") : tr("channel width W"), metres, true, tr("ID"), amps, log);
        for (int s = 0; s < count; ++s) {
            const MosfetConfig &cfg = *c.configs[s];
            PlotSeries curve;
            curve.color = colorOf(s);
            curve.width = 2.2;
            curve.name = nameOf(s, tr("VGS %1 V, VDS %2 V").arg(Format::number(cfg.bias.vgs, 3), Format::number(cfg.bias.vds, 3)));
            const double lo = length ? 0.1e-6 : 0.2e-6, hi = length ? 50.0e-6 : 100.0e-6;
            MosfetParameters p = cfg.device;
            for (int i = 0; i <= 120; ++i) {
                const double v = std::exp(std::log(lo) + (std::log(hi) - std::log(lo)) * i / 120.0);
                (length ? p.channelLength : p.channelWidth) = v;
                curve.points.append(QPointF(v, model.solve(p, cfg.bias).drainCurrent));
            }
            series.append(curve);
            points.append({QPointF(length ? cfg.device.channelLength : cfg.device.channelWidth, c.states[s]->drainCurrent), colorOf(s)});
        }
        if (c.states[0]->region == OperatingRegion::Cutoff)
            message = tr("ID = 0 at every length and width: the device is in cutoff.");
        break;
    }
    }
    m_plot->setContent(series, markers, points, message);
}
