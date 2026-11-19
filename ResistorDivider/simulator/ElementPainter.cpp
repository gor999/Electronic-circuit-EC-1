#include "ElementPainter.h"

#include "Elements.h"

#include <QFontMetricsF>
#include <QPainter>
#include <QPainterPath>
#include <QTransform>

#include <cmath>

namespace {

const QColor kSelect(0x4c, 0xc2, 0xff);
const QColor kBody(0xc8, 0xd0, 0xdc);
const QColor kHover(0xff, 0xff, 0xff);
const QColor kNeutral(0x8e, 0x98, 0xa6);
const QColor kPositive(0x2f, 0xe0, 0x6a);
const QColor kNegative(0xff, 0x4d, 0x4d);
const QColor kDot(0xff, 0xd8, 0x4a);
const QColor kText(0xb8, 0xc0, 0xcc);
const QColor kCanvas(0x0d, 0x11, 0x17);

const double kDotSpacing = 16.0;

QColor mix(const QColor &a, const QColor &b, double t)
{
    return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t,
                            a.greenF() + (b.greenF() - a.greenF()) * t,
                            a.blueF() + (b.blueF() - a.blueF()) * t);
}

// Painting context for one element: a local frame along p1 -> p2 plus the
// small drawing helpers every symbol uses.
struct Ctx
{
    QPainter &p;
    const Element &e;
    const PaintOptions &o;
    bool hovered;

    QPointF a, b, d, n;     // end points, unit direction, unit normal
    double len = 0.0;

    Ctx(QPainter &painter, const Element &element, const PaintOptions &options, bool isHovered)
        : p(painter), e(element), o(options), hovered(isHovered)
    {
        a = e.p1;
        b = e.p2;
        const QPointF delta = b - a;
        len = std::hypot(delta.x(), delta.y());
        d = len > 1e-9 ? delta / len : QPointF(1, 0);
        n = QPointF(-d.y(), d.x());
        if (e.flip)
            n = -n;
    }

    QPointF at(double along, double side = 0.0) const { return a + d * along + n * side; }
    QTransform local() const { return QTransform(d.x(), d.y(), n.x(), n.y(), a.x(), a.y()); }

    QColor voltColor(double v) const
    {
        if (e.selected)
            return kSelect;
        if (!o.showVoltage)
            return hovered ? kHover : kBody;
        const double t = qBound(-1.0, v / o.voltageRange, 1.0);
        const QColor base = hovered ? kHover : kNeutral;
        return t >= 0.0 ? mix(base, kPositive, t) : mix(base, kNegative, -t);
    }

    QColor bodyColor() const { return e.selected ? kSelect : hovered ? kHover : kBody; }

    void setPen(const QColor &color, double width = 2.0)
    {
        p.setPen(QPen(color, width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    }

    void line(const QPointF &from, const QPointF &to, const QColor &color, double width = 2.0)
    {
        setPen(color, width);
        p.drawLine(from, to);
    }

    void lead(const QPointF &from, const QPointF &to, double v) { line(from, to, voltColor(v)); }

    // Moving current dots between two points, for one of the element's
    // current channels. Nothing is drawn where no current flows.
    void dots(const QPointF &from, const QPointF &to, int channel)
    {
        if (!o.showCurrent || std::fabs(e.dotCurrent(channel)) < o.minDotCurrent)
            return;
        const double phase = e.dotPhase[channel];
        const QPointF delta = to - from;
        const double length = std::hypot(delta.x(), delta.y());
        if (length < 1.0)
            return;
        double position = std::fmod(phase, kDotSpacing);
        if (position < 0.0)
            position += kDotSpacing;
        p.setPen(Qt::NoPen);
        p.setBrush(kDot);
        for (; position < length; position += kDotSpacing)
            p.drawEllipse(from + delta * (position / length), 2.0, 2.0);
        p.setBrush(Qt::NoBrush);
    }

    // Value text beside the middle of the element: above a horizontal
    // element, to the right of a vertical one.
    void label(const QString &text, double offset = 14.0)
    {
        if (!o.showValues || text.isEmpty())
            return;
        const QFontMetricsF metrics(p.font());
        const QPointF middle = at(len / 2.0);
        p.setPen(kText);
        if (std::fabs(d.x()) >= std::fabs(d.y())) {
            p.drawText(QPointF(middle.x() - metrics.horizontalAdvance(text) / 2.0, middle.y() - offset), text);
        } else {
            p.drawText(QPointF(middle.x() + offset, middle.y() + metrics.ascent() / 2.0 - 1.0), text);
        }
    }

    void textCentered(const QPointF &centre, const QString &text, const QColor &color, bool bold = false)
    {
        QFont font = p.font();
        const QFont original = font;
        font.setBold(bold);
        p.setFont(font);
        const QFontMetricsF metrics(font);
        p.setPen(color);
        p.drawText(QPointF(centre.x() - metrics.horizontalAdvance(text) / 2.0,
                           centre.y() + metrics.ascent() / 2.0 - 1.0), text);
        p.setFont(original);
    }

    void arrowHead(const QPointF &from, const QPointF &tip, const QColor &color, double size = 7.0)
    {
        const QPointF delta = tip - from;
        const double length = std::hypot(delta.x(), delta.y());
        if (length < 1e-6)
            return;
        const QPointF u = delta / length;
        const QPointF v(-u.y(), u.x());
        QPolygonF head;
        head << tip << tip - u * size + v * size * 0.5 << tip - u * size - v * size * 0.5;
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawPolygon(head);
        p.setBrush(Qt::NoBrush);
    }

    // Start of a body of the given length centred on the element.
    double bodyStart(double bodyLength) const { return qMax(0.0, (len - bodyLength) / 2.0); }
};

// ---------------------------------------------------------------------------
// Symbols
// ---------------------------------------------------------------------------

void paintWire(Ctx &c)
{
    c.lead(c.a, c.b, c.e.volts[0]);
    c.dots(c.a, c.b, 0);
}

void paintResistor(Ctx &c)
{
    const double body = qMin(32.0, c.len);
    const double s = c.bodyStart(body);
    c.lead(c.a, c.at(s), c.e.volts[0]);
    c.lead(c.at(s + body), c.b, c.e.volts[1]);

    // Zigzag, coloured along the voltage gradient.
    const int peaks = 6;
    QPointF previous = c.at(s);
    for (int k = 0; k <= peaks; ++k) {
        const QPointF next = (k == peaks) ? c.at(s + body)
                                          : c.at(s + body * (k + 0.5) / peaks, (k % 2 == 0) ? -6.0 : 6.0);
        const double v = c.e.volts[0] + (c.e.volts[1] - c.e.volts[0]) * (k + 0.5) / (peaks + 1);
        c.line(previous, next, c.voltColor(v));
        previous = next;
    }
    c.dots(c.a, c.b, 0);
    c.label(c.e.valueText());
}

void paintCapacitor(Ctx &c)
{
    const double gap = 8.0;
    const double s = c.bodyStart(gap);
    c.lead(c.a, c.at(s), c.e.volts[0]);
    c.lead(c.at(s + gap), c.b, c.e.volts[1]);
    c.line(c.at(s, -12), c.at(s, 12), c.voltColor(c.e.volts[0]), 3.0);
    c.line(c.at(s + gap, -12), c.at(s + gap, 12), c.voltColor(c.e.volts[1]), 3.0);
    c.dots(c.a, c.at(s), 0);
    c.dots(c.at(s + gap), c.b, 0);
    c.label(c.e.valueText(), 18.0);
}

void paintInductor(Ctx &c)
{
    const double body = qMin(32.0, c.len);
    const double s = c.bodyStart(body);
    c.lead(c.a, c.at(s), c.e.volts[0]);
    c.lead(c.at(s + body), c.b, c.e.volts[1]);

    const int loops = 4;
    const double w = body / loops;
    QPainterPath coil;
    coil.moveTo(s, 0);
    for (int k = 0; k < loops; ++k)
        coil.arcTo(QRectF(s + k * w, -w / 2.0, w, w), 180.0, -180.0);
    c.setPen(c.voltColor((c.e.volts[0] + c.e.volts[1]) / 2.0));
    c.p.drawPath(c.local().map(coil));
    c.dots(c.a, c.b, 0);
    c.label(c.e.valueText());
}

void paintSwitch(Ctx &c)
{
    const auto &sw = static_cast<const SwitchElm &>(c.e);
    const double body = qMin(32.0, c.len);
    const double s = c.bodyStart(body);
    c.lead(c.a, c.at(s), c.e.volts[0]);
    c.lead(c.at(s + body), c.b, c.e.volts[1]);
    c.line(c.at(s), sw.closed ? c.at(s + body) : c.at(s + body - 4, -14), c.bodyColor(), 2.5);
    c.p.setPen(Qt::NoPen);
    c.p.setBrush(c.bodyColor());
    c.p.drawEllipse(c.at(s), 3.0, 3.0);
    c.p.drawEllipse(c.at(s + body), 3.0, 3.0);
    c.p.setBrush(Qt::NoBrush);
    if (sw.closed)
        c.dots(c.a, c.b, 0);
}

void paintGround(Ctx &c)
{
    c.lead(c.a, c.b, c.e.volts[0]);
    const QColor color = c.voltColor(0.0);
    for (int k = 0; k < 3; ++k) {
        const double half = 12.0 - 4.0 * k;
        c.line(c.at(c.len + 4.0 * k, -half), c.at(c.len + 4.0 * k, half), color);
    }
    c.dots(c.a, c.b, 0);
}

// Small picture of the waveform inside a source circle.
void paintWaveGlyph(Ctx &c, const QPointF &centre, int shape, const QColor &color)
{
    QPainterPath glyph;
    const double w = 8.0, h = 5.0;
    switch (shape) {
    case Waveform::Sine:
        glyph.moveTo(centre.x() - w, centre.y());
        for (int i = 1; i <= 16; ++i) {
            const double t = i / 16.0;
            glyph.lineTo(centre.x() - w + 2.0 * w * t, centre.y() - h * std::sin(t * 6.283185307));
        }
        break;
    case Waveform::Square:
        glyph.moveTo(centre.x() - w, centre.y());
        glyph.lineTo(centre.x() - w, centre.y() - h);
        glyph.lineTo(centre.x(), centre.y() - h);
        glyph.lineTo(centre.x(), centre.y() + h);
        glyph.lineTo(centre.x() + w, centre.y() + h);
        glyph.lineTo(centre.x() + w, centre.y());
        break;
    case Waveform::Triangle:
        glyph.moveTo(centre.x() - w, centre.y());
        glyph.lineTo(centre.x() - w / 2.0, centre.y() - h);
        glyph.lineTo(centre.x() + w / 2.0, centre.y() + h);
        glyph.lineTo(centre.x() + w, centre.y());
        break;
    default:
        return;
    }
    c.setPen(color, 1.6);
    c.p.drawPath(glyph);
}

void paintVoltage(Ctx &c)
{
    const auto &source = static_cast<const VoltageElm &>(c.e);
    if (source.wave.shape == Waveform::Dc) {
        // Battery: short thick plate = negative (p1 side), long plate = positive.
        const double gap = 8.0;
        const double s = c.bodyStart(gap);
        c.lead(c.a, c.at(s), c.e.volts[0]);
        c.lead(c.at(s + gap), c.b, c.e.volts[1]);
        c.line(c.at(s, -6), c.at(s, 6), c.voltColor(c.e.volts[0]), 4.0);
        c.line(c.at(s + gap, -13), c.at(s + gap, 13), c.voltColor(c.e.volts[1]), 2.0);
        c.dots(c.a, c.at(s), 0);
        c.dots(c.at(s + gap), c.b, 0);
        c.label(c.e.valueText(), 20.0);
    } else {
        const double radius = 14.0;
        const double s = c.bodyStart(2.0 * radius);
        c.lead(c.a, c.at(s), c.e.volts[0]);
        c.lead(c.at(s + 2.0 * radius), c.b, c.e.volts[1]);
        c.setPen(c.bodyColor());
        c.p.drawEllipse(c.at(c.len / 2.0), radius, radius);
        paintWaveGlyph(c, c.at(c.len / 2.0), source.wave.shape, c.bodyColor());
        c.dots(c.a, c.at(s), 0);
        c.dots(c.at(s + 2.0 * radius), c.b, 0);
        c.label(c.e.valueText(), 22.0);
    }
}

void paintRail(Ctx &c)
{
    const auto &rail = static_cast<const RailElm &>(c.e);
    c.lead(c.a, c.b, c.e.volts[0]);
    c.dots(c.b, c.a, 0);
    if (rail.wave.shape == Waveform::Dc) {
        QString text = rail.wave.text();
        if (rail.wave.amplitude > 0.0)
            text.prepend(QLatin1Char('+'));
        c.textCentered(c.at(c.len + 12.0), text, c.voltColor(c.e.volts[0]), true);
    } else {
        c.setPen(c.bodyColor());
        c.p.drawEllipse(c.at(c.len + 12.0), 12.0, 12.0);
        paintWaveGlyph(c, c.at(c.len + 12.0), rail.wave.shape, c.bodyColor());
        if (c.o.showValues)
            c.textCentered(c.at(c.len + 12.0) + QPointF(0, 24), rail.wave.text(), kText);
    }
}

void paintLogicInput(Ctx &c)
{
    const auto &input = static_cast<const LogicInputElm &>(c.e);
    c.lead(c.a, c.b, c.e.volts[0]);
    const QPointF centre = c.at(c.len + 11.0);
    c.setPen(c.bodyColor(), 1.5);
    c.p.drawRoundedRect(QRectF(centre.x() - 10, centre.y() - 10, 20, 20), 3, 3);
    c.textCentered(centre, input.valueText(), input.high ? kPositive : kNeutral, true);
}

void paintCurrentSource(Ctx &c)
{
    const double radius = 14.0;
    const double s = c.bodyStart(2.0 * radius);
    c.lead(c.a, c.at(s), c.e.volts[0]);
    c.lead(c.at(s + 2.0 * radius), c.b, c.e.volts[1]);
    c.setPen(c.bodyColor());
    c.p.drawEllipse(c.at(c.len / 2.0), radius, radius);
    c.line(c.at(s + 6), c.at(s + 20), c.bodyColor());
    c.arrowHead(c.at(s + 6), c.at(s + 23), c.bodyColor());
    c.dots(c.a, c.b, 0);
    c.label(c.e.valueText(), 22.0);
}

void paintDiode(Ctx &c)
{
    const auto &diode = static_cast<const DiodeElm &>(c.e);
    const double body = 16.0;
    const double s = c.bodyStart(body);
    c.lead(c.a, c.at(s), c.e.volts[0]);
    c.lead(c.at(s + body), c.b, c.e.volts[1]);

    if (diode.isLed) {
        static const QColor colors[] = {QColor(0xff, 0x3b, 0x30), QColor(0x34, 0xe0, 0x5a),
                                        QColor(0x3d, 0x8b, 0xff), QColor(0xff, 0xd6, 0x0a)};
        QColor fill = colors[qBound(0, diode.color, 3)];
        fill.setAlphaF(0.12 + 0.88 * diode.brightness());
        c.setPen(c.bodyColor(), 1.5);
        c.p.setBrush(fill);
        c.p.drawEllipse(c.at(s + body / 2.0), 15.0, 15.0);
        c.p.setBrush(Qt::NoBrush);
    }

    QPolygonF triangle;
    triangle << c.at(s, -8) << c.at(s, 8) << c.at(s + body, 0);
    c.p.setPen(Qt::NoPen);
    c.p.setBrush(c.voltColor(c.e.volts[0]));
    c.p.drawPolygon(triangle);
    c.p.setBrush(Qt::NoBrush);
    c.line(c.at(s + body, -8), c.at(s + body, 8), c.voltColor(c.e.volts[1]), 2.5);
    c.dots(c.a, c.b, 0);
}

void paintBjt(Ctx &c)
{
    const auto &q = static_cast<const BjtElm &>(c.e);
    const double bar = qMax(0.0, c.len - 16.0);
    const double collectorSign = q.isPnp ? 1.0 : -1.0;
    const QPointF collectorPost = c.e.post(1);
    const QPointF emitterPost = c.e.post(2);
    const QPointF collectorAtBar = c.at(bar, collectorSign * 6.0);
    const QPointF emitterAtBar = c.at(bar, -collectorSign * 6.0);

    c.lead(c.a, c.at(bar), c.e.volts[0]);
    c.lead(collectorPost, collectorAtBar, c.e.volts[1]);
    c.lead(emitterAtBar, emitterPost, c.e.volts[2]);
    c.line(c.at(bar, -14), c.at(bar, 14), c.bodyColor(), 3.5);

    // The emitter arrow points out of an NPN and into a PNP.
    if (q.isPnp)
        c.arrowHead(emitterPost, emitterAtBar + (emitterPost - emitterAtBar) * 0.15, c.bodyColor(), 6.0);
    else
        c.arrowHead(emitterAtBar, emitterPost - (emitterPost - emitterAtBar) * 0.2, c.bodyColor(), 6.0);

    c.dots(c.a, c.at(bar), 0);
    c.dots(collectorPost, collectorAtBar, 1);
    c.dots(emitterAtBar, emitterPost, 2);
}

void paintMosfet(Ctx &c)
{
    const auto &m = static_cast<const MosfetElm &>(c.e);
    const double channel = qMax(0.0, c.len - 22.0);
    const double plate = qMax(0.0, c.len - 28.0);
    const double drainSign = m.isP ? 1.0 : -1.0;
    const QPointF drainPost = c.e.post(1);
    const QPointF sourcePost = c.e.post(2);
    const QPointF drainAtChannel = c.at(channel, drainSign * 16.0);
    const QPointF sourceAtChannel = c.at(channel, -drainSign * 16.0);

    // Gate lead; a P-channel device gets the usual inversion bubble.
    if (m.isP) {
        c.lead(c.a, c.at(plate - 6.0), c.e.volts[0]);
        c.setPen(c.bodyColor(), 1.5);
        c.p.drawEllipse(c.at(plate - 3.0), 3.0, 3.0);
    } else {
        c.lead(c.a, c.at(plate), c.e.volts[0]);
    }
    c.line(c.at(plate, -12), c.at(plate, 12), c.bodyColor(), 2.5);
    c.line(drainAtChannel, sourceAtChannel, c.bodyColor(), 3.0);
    c.lead(drainPost, drainAtChannel, c.e.volts[1]);
    c.lead(sourceAtChannel, sourcePost, c.e.volts[2]);

    // Arrow at the source end of the channel.
    const QPointF middle = (sourceAtChannel + sourcePost) / 2.0;
    if (m.isP)
        c.arrowHead(sourcePost, middle, c.bodyColor(), 6.0);
    else
        c.arrowHead(sourceAtChannel, middle + (sourcePost - sourceAtChannel) * 0.15, c.bodyColor(), 6.0);

    c.dots(drainPost, drainAtChannel, 0);
    c.dots(drainAtChannel, sourceAtChannel, 0);
    c.dots(sourceAtChannel, sourcePost, 0);
}

void paintOpAmp(Ctx &c)
{
    const double body = qMin(48.0, c.len);
    const double s = c.bodyStart(body);
    c.lead(c.e.post(0), c.at(s, -16), c.e.volts[0]);
    c.lead(c.e.post(1), c.at(s, 16), c.e.volts[1]);
    c.lead(c.at(s + body), c.b, c.e.volts[2]);

    QPolygonF triangle;
    triangle << c.at(s, -26) << c.at(s, 26) << c.at(s + body, 0);
    c.setPen(c.bodyColor());
    c.p.drawPolygon(triangle);

    // "-" at the inverting input, "+" at the non-inverting input.
    c.line(c.at(s + 6, -16), c.at(s + 12, -16), c.bodyColor(), 1.5);
    c.line(c.at(s + 6, 16), c.at(s + 12, 16), c.bodyColor(), 1.5);
    c.line(c.at(s + 9, 13), c.at(s + 9, 19), c.bodyColor(), 1.5);

    c.dots(c.at(s + body), c.b, 0);
}

void paintGate(Ctx &c)
{
    const auto &gate = static_cast<const GateElm &>(c.e);
    const bool isNot = gate.function == GateElm::Not;
    const double body = isNot ? 28.0 : 40.0;
    const double s = c.bodyStart(body + (gate.inverting() ? 8.0 : 0.0));
    const int out = gate.inputCount();

    QPainterPath shape;
    double inputInset = 0.0;
    switch (gate.function) {
    case GateElm::And:
    case GateElm::Nand:
        shape.moveTo(s, -20);
        shape.lineTo(s + 20, -20);
        shape.arcTo(QRectF(s, -20, 40, 40), 90, -180);
        shape.lineTo(s, 20);
        shape.closeSubpath();
        break;
    case GateElm::Or:
    case GateElm::Nor:
    case GateElm::Xor:
        shape.moveTo(s, -20);
        shape.quadTo(s + 26, -20, s + 40, 0);
        shape.quadTo(s + 26, 20, s, 20);
        shape.quadTo(s + 10, 0, s, -20);
        if (gate.function == GateElm::Xor) {
            shape.moveTo(s - 6, -20);
            shape.quadTo(s + 4, 0, s - 6, 20);
        }
        inputInset = 4.0;
        break;
    case GateElm::Not:
        shape.moveTo(s, -14);
        shape.lineTo(s, 14);
        shape.lineTo(s + body, 0);
        shape.closeSubpath();
        break;
    }

    if (isNot) {
        c.lead(c.a, c.at(s), c.e.volts[0]);
    } else {
        c.lead(c.e.post(0), c.at(s + inputInset, -16), c.e.volts[0]);
        c.lead(c.e.post(1), c.at(s + inputInset, 16), c.e.volts[1]);
    }

    c.setPen(c.bodyColor());
    c.p.drawPath(c.local().map(shape));

    double outputStart = s + body;
    if (gate.inverting()) {
        c.setPen(c.bodyColor(), 1.5);
        c.p.drawEllipse(c.at(s + body + 4.0), 4.0, 4.0);
        outputStart += 8.0;
    }
    c.lead(c.at(outputStart), c.b, c.e.volts[out]);
    c.dots(c.at(outputStart), c.b, 0);
}

void paintVoltmeter(Ctx &c)
{
    QPen dashed(c.e.selected ? kSelect : kNeutral, 1.2, Qt::DashLine);
    c.p.setPen(dashed);
    c.p.drawLine(c.a, c.b);

    const QString text = c.e.valueText();
    const QFontMetricsF metrics(c.p.font());
    const QPointF middle = c.at(c.len / 2.0);
    const QRectF box(middle.x() - metrics.horizontalAdvance(text) / 2.0 - 5.0, middle.y() - 9.0,
                     metrics.horizontalAdvance(text) + 10.0, 18.0);
    c.setPen(c.bodyColor(), 1.2);
    c.p.setBrush(kCanvas);
    c.p.drawRoundedRect(box, 4, 4);
    c.p.setBrush(Qt::NoBrush);
    c.textCentered(middle, text, c.voltColor(c.e.voltageDiff()), true);
}

void paintAmmeter(Ctx &c)
{
    const double radius = 10.0;
    const double s = c.bodyStart(2.0 * radius);
    c.lead(c.a, c.at(s), c.e.volts[0]);
    c.lead(c.at(s + 2.0 * radius), c.b, c.e.volts[1]);
    c.setPen(c.bodyColor());
    c.p.drawEllipse(c.at(c.len / 2.0), radius, radius);
    c.textCentered(c.at(c.len / 2.0), QStringLiteral("A"), c.bodyColor(), true);
    c.dots(c.a, c.at(s), 0);
    c.dots(c.at(s + 2.0 * radius), c.b, 0);
    if (c.o.showValues) {
        const bool horizontal = std::fabs(c.d.x()) >= std::fabs(c.d.y());
        const QPointF middle = c.at(c.len / 2.0);
        if (horizontal) {
            c.textCentered(middle - QPointF(0, 20), c.e.valueText(), kText);
        } else {
            c.p.setPen(kText);
            c.p.drawText(middle + QPointF(16, 4), c.e.valueText());
        }
    }
}

} // namespace

namespace ElementPainter {

void paint(QPainter &painter, const Element &element, const PaintOptions &options, bool hovered)
{
    Ctx c(painter, element, options, hovered);
    painter.setBrush(Qt::NoBrush);
    switch (element.kind()) {
    case Element::Kind::Wire: paintWire(c); break;
    case Element::Kind::Resistor: paintResistor(c); break;
    case Element::Kind::Capacitor: paintCapacitor(c); break;
    case Element::Kind::Inductor: paintInductor(c); break;
    case Element::Kind::Switch: paintSwitch(c); break;
    case Element::Kind::Ground: paintGround(c); break;
    case Element::Kind::Voltage: paintVoltage(c); break;
    case Element::Kind::Rail: paintRail(c); break;
    case Element::Kind::LogicInput: paintLogicInput(c); break;
    case Element::Kind::Current: paintCurrentSource(c); break;
    case Element::Kind::Diode:
    case Element::Kind::Led: paintDiode(c); break;
    case Element::Kind::Bjt: paintBjt(c); break;
    case Element::Kind::Mosfet: paintMosfet(c); break;
    case Element::Kind::OpAmp: paintOpAmp(c); break;
    case Element::Kind::Gate: paintGate(c); break;
    case Element::Kind::Voltmeter: paintVoltmeter(c); break;
    case Element::Kind::Ammeter: paintAmmeter(c); break;
    }
}

QRectF bounds(const Element &element)
{
    QRectF rect(QPointF(element.p1), QPointF(element.p2));
    rect = rect.normalized();
    for (int i = 0; i < element.postCount(); ++i) {
        const QPointF post = element.post(i);
        rect.setLeft(qMin(rect.left(), post.x()));
        rect.setRight(qMax(rect.right(), post.x()));
        rect.setTop(qMin(rect.top(), post.y()));
        rect.setBottom(qMax(rect.bottom(), post.y()));
    }
    return rect;
}

} // namespace ElementPainter
