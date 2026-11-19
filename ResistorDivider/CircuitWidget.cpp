#include "CircuitWidget.h"

#include "Format.h"

#include <QPainter>
#include <QPainterPath>

namespace {

const QColor kWire(0x9a, 0xa5, 0xb5);
const QColor kText(0xe6, 0xea, 0xf0);
const QColor kDim(0x8a, 0x94, 0xa3);
const QColor kResistor(0xff, 0xb4, 0x54);
const QColor kHot(0x4c, 0xc2, 0xff);
const QColor kCold(0x5b, 0x66, 0x77);

const int kSegmentMinHeight = 64;
const int kVerticalPadding = 150;

QColor mix(const QColor &a, const QColor &b, double t)
{
    t = qBound(0.0, t, 1.0);
    return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t,
                            a.greenF() + (b.greenF() - a.greenF()) * t,
                            a.blueF() + (b.blueF() - a.blueF()) * t);
}

} // namespace

CircuitWidget::CircuitWidget(QWidget *parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    updateMinimumHeight();
}

QSize CircuitWidget::sizeHint() const
{
    return QSize(460, kVerticalPadding + m_count * 84);
}

QSize CircuitWidget::minimumSizeHint() const
{
    return QSize(340, kVerticalPadding + m_count * kSegmentMinHeight);
}

void CircuitWidget::setResistorCount(int count)
{
    m_count = qMax(1, count);
    m_hasResult = false;
    updateMinimumHeight();
    update();
}

void CircuitWidget::setResult(const DividerResult &result)
{
    m_result = result;
    m_count = qMax(1, static_cast<int>(result.resistances.size()));
    m_hasResult = true;
    updateMinimumHeight();
    update();
}

void CircuitWidget::clearResult()
{
    m_hasResult = false;
    update();
}

void CircuitWidget::updateMinimumHeight()
{
    // The widget lives in a scroll area: long chains scroll instead of
    // squeezing the resistors together.
    setMinimumHeight(kVerticalPadding + m_count * kSegmentMinHeight);
    updateGeometry();
}

void CircuitWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    const QRectF area = QRectF(rect()).adjusted(20, 20, -20, -20);
    const int n = m_count;

    // --- Geometry -----------------------------------------------------------
    const double sourceRadius = 24.0;
    const double xSource = area.left() + qMax(84.0, area.width() * 0.22);
    // Keep room on the right for the node voltage and voltage drop labels.
    const double xChain = qMin(area.left() + area.width() * 0.60, area.right() - 122.0);
    const double yTop = area.top() + 36.0;
    const double yBottom = area.bottom() - 44.0;
    const double segment = (yBottom - yTop) / n;
    const double yMid = (yTop + yBottom) / 2.0;

    QFont baseFont = font();
    QFont boldFont = baseFont;
    boldFont.setBold(true);
    QFont smallFont = baseFont;
    smallFont.setPointSizeF(baseFont.pointSizeF() * 0.88);

    const QPen wirePen(kWire, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);

    // --- Wires: source -> top rail -> chain, chain -> bottom rail -> source --
    p.setPen(wirePen);
    p.setBrush(Qt::NoBrush);
    QPainterPath rails;
    rails.moveTo(xSource, yMid - sourceRadius);
    rails.lineTo(xSource, yTop);
    rails.lineTo(xChain, yTop);
    rails.moveTo(xSource, yMid + sourceRadius);
    rails.lineTo(xSource, yBottom);
    rails.lineTo(xChain, yBottom);
    p.drawPath(rails);

    // --- Voltage source -----------------------------------------------------
    p.setPen(QPen(kHot, 2.0));
    p.setBrush(QColor(0x4c, 0xc2, 0xff, 28));
    p.drawEllipse(QPointF(xSource, yMid), sourceRadius, sourceRadius);
    p.setPen(QPen(kText, 2.0, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(QPointF(xSource - 6, yMid - 10), QPointF(xSource + 6, yMid - 10)); // +
    p.drawLine(QPointF(xSource, yMid - 16), QPointF(xSource, yMid - 4));
    p.drawLine(QPointF(xSource - 6, yMid + 11), QPointF(xSource + 6, yMid + 11)); // -

    const QRectF sourceLabel(area.left(), yMid - 22, xSource - sourceRadius - 10 - area.left(), 44);
    p.setFont(smallFont);
    p.setPen(kDim);
    p.drawText(sourceLabel, Qt::AlignRight | Qt::AlignTop, tr("SOURCE"));
    p.setFont(boldFont);
    p.setPen(kText);
    p.drawText(sourceLabel, Qt::AlignRight | Qt::AlignBottom,
               m_hasResult ? Format::si(m_result.sourceVoltage, QStringLiteral("V"))
                           : QStringLiteral("Vs"));

    // --- Current arrow on the top rail --------------------------------------
    {
        const double xArrow = (xSource + xChain) / 2.0;
        QPainterPath arrow;
        arrow.moveTo(xArrow + 7, yTop);
        arrow.lineTo(xArrow - 5, yTop - 6);
        arrow.lineTo(xArrow - 5, yTop + 6);
        arrow.closeSubpath();
        p.setPen(Qt::NoPen);
        p.setBrush(kWire);
        p.drawPath(arrow);

        p.setFont(smallFont);
        p.setPen(kDim);
        const QString currentText = m_hasResult
            ? QStringLiteral("I = ") + Format::si(m_result.current, QStringLiteral("A"))
            : QStringLiteral("I");
        p.drawText(QRectF(xSource, yTop - 30, xChain - xSource, 20), Qt::AlignCenter, currentText);
    }

    // --- Resistor chain -----------------------------------------------------
    const double bodyLength = qBound(26.0, segment * 0.5, 64.0);
    const double zigzagWidth = 9.0;
    const int zigzagPeaks = 6;
    const double leftLabelRight = xChain - 20.0;
    const double leftLabelLeft = xSource + sourceRadius + 8.0;
    const double rightLabelLeft = xChain + 18.0;
    const double rightLabelWidth = qMax(40.0, area.right() - rightLabelLeft);

    for (int i = 0; i < n; ++i) {
        const double y0 = yTop + i * segment;
        const double y1 = y0 + segment;
        const double yCentre = (y0 + y1) / 2.0;
        const double bodyTop = yCentre - bodyLength / 2.0;
        const double bodyBottom = yCentre + bodyLength / 2.0;

        p.setPen(wirePen);
        p.drawLine(QPointF(xChain, y0), QPointF(xChain, bodyTop));
        p.drawLine(QPointF(xChain, bodyBottom), QPointF(xChain, y1));

        QPainterPath zigzag;
        zigzag.moveTo(xChain, bodyTop);
        for (int k = 0; k < zigzagPeaks; ++k) {
            const double x = xChain + ((k % 2 == 0) ? zigzagWidth : -zigzagWidth);
            zigzag.lineTo(x, bodyTop + bodyLength * (k + 0.5) / zigzagPeaks);
        }
        zigzag.lineTo(xChain, bodyBottom);
        p.setPen(QPen(kResistor, 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(Qt::NoBrush);
        p.drawPath(zigzag);

        // Name and value, left of the resistor.
        const QRectF nameRect(leftLabelLeft, yCentre - 19, leftLabelRight - leftLabelLeft, 38);
        p.setFont(boldFont);
        p.setPen(kText);
        if (m_hasResult) {
            p.drawText(nameRect, Qt::AlignRight | Qt::AlignTop, QStringLiteral("R%1").arg(i + 1));
            p.setFont(smallFont);
            p.setPen(kResistor);
            p.drawText(nameRect, Qt::AlignRight | Qt::AlignBottom,
                       Format::si(m_result.resistances[i], QStringLiteral("Ω")));
        } else {
            p.drawText(nameRect, Qt::AlignRight | Qt::AlignVCenter, QStringLiteral("R%1").arg(i + 1));
        }

        // Voltage drop, right of the resistor.
        if (m_hasResult) {
            p.setFont(smallFont);
            p.setPen(kDim);
            p.drawText(QRectF(rightLabelLeft, yCentre - 10, rightLabelWidth, 20),
                       Qt::AlignLeft | Qt::AlignVCenter,
                       QStringLiteral("ΔV = ")
                           + Format::si(m_result.voltageDrops[i], QStringLiteral("V")));
        }
    }

    // --- Ground symbol below the last node ----------------------------------
    p.setPen(wirePen);
    p.drawLine(QPointF(xChain, yBottom), QPointF(xChain, yBottom + 14));
    p.drawLine(QPointF(xChain - 12, yBottom + 14), QPointF(xChain + 12, yBottom + 14));
    p.drawLine(QPointF(xChain - 8, yBottom + 19), QPointF(xChain + 8, yBottom + 19));
    p.drawLine(QPointF(xChain - 4, yBottom + 24), QPointF(xChain + 4, yBottom + 24));
    p.setFont(smallFont);
    p.setPen(kDim);
    p.drawText(QRectF(xChain - 40, yBottom + 26, 80, 18), Qt::AlignCenter, tr("GND"));

    // --- Connection points and their voltages -------------------------------
    for (int i = 0; i <= n; ++i) {
        const double y = yTop + i * segment;
        double level = 1.0 - static_cast<double>(i) / n;
        if (m_hasResult && m_result.sourceVoltage > 0.0)
            level = m_result.nodeVoltages[i] / m_result.sourceVoltage;
        const QColor nodeColor = mix(kCold, kHot, level);

        p.setPen(QPen(QColor(0x12, 0x16, 0x1c), 2.0));
        p.setBrush(nodeColor);
        p.drawEllipse(QPointF(xChain, y), 5.5, 5.5);

        if (m_hasResult) {
            p.setFont(boldFont);
            p.setPen(mix(kDim, kHot, level));
            p.drawText(QRectF(rightLabelLeft, y - 10, rightLabelWidth, 20),
                       Qt::AlignLeft | Qt::AlignVCenter,
                       Format::si(m_result.nodeVoltages[i], QStringLiteral("V")));
        }
    }
}
