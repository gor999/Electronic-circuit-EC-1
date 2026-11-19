#include "RcCircuitWidget.h"

#include "Format.h"

#include <QPainter>
#include <QPainterPath>

#include <cmath>

namespace {

const QColor kWire(0x9a, 0xa5, 0xb5);
const QColor kText(0xe6, 0xea, 0xf0);
const QColor kDim(0x8a, 0x94, 0xa3);
const QColor kResistor(0xff, 0xb4, 0x54);
const QColor kCapacitor(0x7d, 0xdc, 0xa0);
const QColor kAccent(0x4c, 0xc2, 0xff);
const QColor kBackground(0x1a, 0x20, 0x29);

const double kBodyLength = 44.0;

// Draws a two-terminal component between a and b. The painter is rotated so
// that `drawBody` can draw the body horizontally, centred on the origin.
template <typename DrawBody>
void drawComponent(QPainter &p, const QPointF &a, const QPointF &b, const QColor &color,
                   double bodyLength, DrawBody drawBody)
{
    const QLineF line(a, b);
    const double lead = (line.length() - bodyLength) / 2.0;

    p.save();
    p.translate((a + b) / 2.0);
    p.rotate(-line.angle());
    p.setPen(QPen(kWire, 2.0, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(QPointF(-bodyLength / 2.0 - lead, 0), QPointF(-bodyLength / 2.0, 0));
    p.drawLine(QPointF(bodyLength / 2.0, 0), QPointF(bodyLength / 2.0 + lead, 0));
    p.setPen(QPen(color, 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    drawBody(p);
    p.restore();
}

void drawResistor(QPainter &p, const QPointF &a, const QPointF &b)
{
    drawComponent(p, a, b, kResistor, kBodyLength, [](QPainter &painter) {
        const int peaks = 6;
        QPainterPath zigzag;
        zigzag.moveTo(-kBodyLength / 2.0, 0);
        for (int k = 0; k < peaks; ++k) {
            zigzag.lineTo(-kBodyLength / 2.0 + kBodyLength * (k + 0.5) / peaks,
                          (k % 2 == 0) ? -9.0 : 9.0);
        }
        zigzag.lineTo(kBodyLength / 2.0, 0);
        painter.drawPath(zigzag);
    });
}

void drawCapacitor(QPainter &p, const QPointF &a, const QPointF &b)
{
    const double gap = 10.0;
    drawComponent(p, a, b, kCapacitor, gap, [gap](QPainter &painter) {
        QPen pen = painter.pen();
        pen.setWidthF(3.0);
        painter.setPen(pen);
        painter.drawLine(QPointF(-gap / 2.0, -14), QPointF(-gap / 2.0, 14));
        painter.drawLine(QPointF(gap / 2.0, -14), QPointF(gap / 2.0, 14));
    });
}

} // namespace

RcCircuitWidget::RcCircuitWidget(QWidget *parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

QSize RcCircuitWidget::sizeHint() const
{
    return QSize(420, 250);
}

QSize RcCircuitWidget::minimumSizeHint() const
{
    return QSize(360, 210);
}

void RcCircuitWidget::setFilterType(RcFilterType type)
{
    m_type = type;
    m_hasResult = false;
    update();
}

void RcCircuitWidget::setResult(const RcFilterResult &result)
{
    m_result = result;
    m_type = result.type;
    m_hasResult = true;
    update();
}

void RcCircuitWidget::clearResult()
{
    m_hasResult = false;
    update();
}

void RcCircuitWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    const QRectF area = QRectF(rect()).adjusted(12, 8, -12, -8);
    const bool lowPass = (m_type == RcFilterType::LowPass);

    // --- Geometry -----------------------------------------------------------
    const double sourceRadius = 22.0;
    const double xSource = area.left() + 84.0;
    const double xOut = area.right() - 92.0;                    // output terminals
    const double xNode = xSource + (xOut - xSource) * 0.55;     // series/shunt junction
    const double yTop = area.top() + 52.0;
    const double yBottom = area.bottom() - 40.0;
    const double yMid = (yTop + yBottom) / 2.0;

    QFont baseFont = font();
    QFont boldFont = baseFont;
    boldFont.setBold(true);
    QFont smallFont = baseFont;
    smallFont.setPointSizeF(baseFont.pointSizeF() * 0.88);

    const QPen wirePen(kWire, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);

    // --- Wires --------------------------------------------------------------
    p.setPen(wirePen);
    QPainterPath wires;
    wires.moveTo(xSource, yMid - sourceRadius);
    wires.lineTo(xSource, yTop);
    wires.moveTo(xNode, yTop);
    wires.lineTo(xOut, yTop);
    wires.moveTo(xSource, yMid + sourceRadius);
    wires.lineTo(xSource, yBottom);
    wires.lineTo(xOut, yBottom);
    p.drawPath(wires);

    // --- Series and shunt components ---------------------------------------
    const QPointF seriesA(xSource, yTop), seriesB(xNode, yTop);
    const QPointF shuntA(xNode, yTop), shuntB(xNode, yBottom);
    if (lowPass) {
        drawResistor(p, seriesA, seriesB);
        drawCapacitor(p, shuntA, shuntB);
    } else {
        drawCapacitor(p, seriesA, seriesB);
        drawResistor(p, shuntA, shuntB);
    }

    const QString resistorText = m_hasResult
        ? QStringLiteral("R = ") + Format::si(m_result.resistance, QStringLiteral("Ω"))
        : QStringLiteral("R");
    const QString capacitorText = m_hasResult
        ? QStringLiteral("C = ") + Format::si(m_result.capacitance, QStringLiteral("F"))
        : QStringLiteral("C");

    // Series label above the top rail, shunt label right of the shunt branch.
    p.setFont(boldFont);
    p.setPen(lowPass ? kResistor : kCapacitor);
    p.drawText(QRectF(xSource, yTop - 44, xNode - xSource, 20), Qt::AlignCenter,
               lowPass ? resistorText : capacitorText);
    p.setPen(lowPass ? kCapacitor : kResistor);
    p.drawText(QRectF(xNode + 22, yMid - 10, area.right() - (xNode + 22), 20),
               Qt::AlignLeft | Qt::AlignVCenter, lowPass ? capacitorText : resistorText);

    // --- AC source ----------------------------------------------------------
    p.setPen(QPen(kAccent, 2.0));
    p.setBrush(QColor(0x4c, 0xc2, 0xff, 28));
    p.drawEllipse(QPointF(xSource, yMid), sourceRadius, sourceRadius);
    QPainterPath sine;
    for (int i = 0; i <= 24; ++i) {
        const double t = i / 24.0;
        const QPointF point(xSource - 11 + 22 * t, yMid - 7 * std::sin(t * 2.0 * 3.14159265358979));
        if (i == 0)
            sine.moveTo(point);
        else
            sine.lineTo(point);
    }
    p.setPen(QPen(kText, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    p.drawPath(sine);

    const QRectF sourceLabel(area.left(), yMid - 30, xSource - sourceRadius - 8 - area.left(), 60);
    p.setFont(smallFont);
    p.setPen(kDim);
    p.drawText(sourceLabel, Qt::AlignRight | Qt::AlignTop, QStringLiteral("Vin"));
    if (m_hasResult) {
        p.setFont(boldFont);
        p.setPen(kText);
        p.drawText(sourceLabel, Qt::AlignRight | Qt::AlignVCenter,
                   Format::si(m_result.inputAmplitude, QStringLiteral("V"), 3));
        p.setFont(smallFont);
        p.setPen(kDim);
        p.drawText(sourceLabel, Qt::AlignRight | Qt::AlignBottom,
                   Format::si(m_result.frequency, QStringLiteral("Hz"), 3));
    }

    // --- Ground -------------------------------------------------------------
    p.setPen(wirePen);
    p.drawLine(QPointF(xNode, yBottom), QPointF(xNode, yBottom + 12));
    p.drawLine(QPointF(xNode - 12, yBottom + 12), QPointF(xNode + 12, yBottom + 12));
    p.drawLine(QPointF(xNode - 8, yBottom + 17), QPointF(xNode + 8, yBottom + 17));
    p.drawLine(QPointF(xNode - 4, yBottom + 22), QPointF(xNode + 4, yBottom + 22));

    // --- Junction dots and output terminals --------------------------------
    p.setPen(QPen(kBackground, 2.0));
    p.setBrush(kAccent);
    p.drawEllipse(QPointF(xNode, yTop), 5.0, 5.0);
    p.setBrush(kWire);
    p.drawEllipse(QPointF(xNode, yBottom), 4.5, 4.5);

    p.setPen(QPen(kAccent, 2.0));
    p.setBrush(kBackground);
    p.drawEllipse(QPointF(xOut, yTop), 5.0, 5.0);
    p.setPen(QPen(kWire, 2.0));
    p.drawEllipse(QPointF(xOut, yBottom), 5.0, 5.0);

    const QRectF outLabel(xOut + 12, yTop - 22, area.right() - xOut - 12, 44);
    p.setFont(smallFont);
    p.setPen(kDim);
    p.drawText(outLabel, Qt::AlignLeft | (m_hasResult ? Qt::AlignTop : Qt::AlignVCenter),
               QStringLiteral("Vout"));
    if (m_hasResult) {
        p.setFont(boldFont);
        p.setPen(kAccent);
        p.drawText(outLabel, Qt::AlignLeft | Qt::AlignBottom,
                   Format::si(m_result.outputAmplitude, QStringLiteral("V"), 3));
    }
    p.setFont(smallFont);
    p.setPen(kDim);
    p.drawText(QRectF(xOut + 12, yBottom - 10, area.right() - xOut - 12, 20),
               Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("GND"));
}
