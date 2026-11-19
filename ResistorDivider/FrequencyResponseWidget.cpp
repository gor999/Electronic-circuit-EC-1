#include "FrequencyResponseWidget.h"

#include "Format.h"

#include <QPainter>
#include <QPainterPath>

#include <cmath>

namespace {

const QColor kFrame(0x2f, 0x39, 0x47);
const QColor kGrid(0x26, 0x2e, 0x3a);
const QColor kPlotBackground(0x16, 0x1b, 0x23);
const QColor kText(0xe6, 0xea, 0xf0);
const QColor kDim(0x8a, 0x94, 0xa3);
const QColor kCurve(0x4c, 0xc2, 0xff);
const QColor kCutoff(0xff, 0xb4, 0x54);

const double kRadToDeg = 180.0 / 3.14159265358979323846;
const int kSamples = 240;

} // namespace

FrequencyResponseWidget::FrequencyResponseWidget(QWidget *parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

QSize FrequencyResponseWidget::sizeHint() const
{
    return QSize(640, 300);
}

QSize FrequencyResponseWidget::minimumSizeHint() const
{
    return QSize(420, 210);
}

void FrequencyResponseWidget::setResult(const RcFilterResult &result)
{
    m_result = result;
    m_hasResult = true;
    update();
}

void FrequencyResponseWidget::clearResult()
{
    m_hasResult = false;
    update();
}

void FrequencyResponseWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    QFont smallFont = font();
    smallFont.setPointSizeF(font().pointSizeF() * 0.85);
    QFont smallBold = smallFont;
    smallBold.setBold(true);
    p.setFont(smallFont);

    // Two stacked plots sharing the frequency axis.
    const QRectF area = QRectF(rect()).adjusted(52, 8, -14, -24);
    const double gap = 14.0;
    const double gainHeight = (area.height() - gap) * 0.58;
    const QRectF gainRect(area.left(), area.top(), area.width(), gainHeight);
    const QRectF phaseRect(area.left(), gainRect.bottom() + gap, area.width(),
                           area.height() - gap - gainHeight);

    for (const QRectF &r : {gainRect, phaseRect}) {
        p.setPen(QPen(kFrame, 1.0));
        p.setBrush(kPlotBackground);
        p.drawRoundedRect(r, 6, 6);
    }

    if (!m_hasResult) {
        p.setPen(kDim);
        p.drawText(gainRect, Qt::AlignCenter, tr("Enter the filter values and press Calculate"));
        return;
    }

    const RcFilterType type = m_result.type;
    const double fc = m_result.cutoffFrequency;
    const double f = m_result.frequency;

    // --- Axis ranges --------------------------------------------------------
    // Three decades either side of fc, widened if the selected frequency lies
    // outside that window, then snapped to whole decades.
    double logLow = std::log10(fc) - 3.0;
    double logHigh = std::log10(fc) + 3.0;
    logLow = std::floor(qMin(logLow, std::log10(f) - 0.5));
    logHigh = std::ceil(qMax(logHigh, std::log10(f) + 0.5));
    const int decades = static_cast<int>(logHigh - logLow);

    const double lowestMagnitude = qMin(RcFilterCalculator::magnitude(type, std::pow(10.0, logLow) / fc),
                                        RcFilterCalculator::magnitude(type, std::pow(10.0, logHigh) / fc));
    const double dbTop = 10.0;
    const double dbBottom = qMin(-60.0, std::floor(20.0 * std::log10(lowestMagnitude) / 20.0) * 20.0);
    const double phaseTop = (type == RcFilterType::LowPass) ? 10.0 : 100.0;
    const double phaseBottom = phaseTop - 110.0;

    auto xOf = [&](double frequency) {
        return area.left() + (std::log10(frequency) - logLow) / (logHigh - logLow) * area.width();
    };
    auto yGain = [&](double db) {
        return gainRect.top() + (dbTop - db) / (dbTop - dbBottom) * gainRect.height();
    };
    auto yPhase = [&](double degrees) {
        return phaseRect.top() + (phaseTop - degrees) / (phaseTop - phaseBottom) * phaseRect.height();
    };

    // --- Grid and tick labels ----------------------------------------------
    const int labelStep = (decades * 62.0 > area.width()) ? 2 : 1;
    for (int d = 0; d <= decades; ++d) {
        const double x = area.left() + area.width() * d / decades;
        if (d > 0 && d < decades) {
            p.setPen(QPen(kGrid, 1.0));
            p.drawLine(QPointF(x, gainRect.top() + 1), QPointF(x, gainRect.bottom() - 1));
            p.drawLine(QPointF(x, phaseRect.top() + 1), QPointF(x, phaseRect.bottom() - 1));
        }
        if (d % labelStep == 0) {
            p.setPen(kDim);
            // Keep the last label inside the widget.
            const double labelLeft = qMin(x - 40.0, width() - 82.0);
            p.drawText(QRectF(labelLeft, phaseRect.bottom() + 4, 80, 16), Qt::AlignCenter,
                       Format::si(std::pow(10.0, logLow + d), QStringLiteral("Hz"), 3));
        }
    }

    const int dbStep = (dbTop - dbBottom > 110.0) ? 40 : 20;
    for (int db = 0; db >= static_cast<int>(dbBottom); db -= dbStep) {
        const double y = yGain(db);
        if (db > dbBottom) {
            p.setPen(QPen(kGrid, 1.0));
            p.drawLine(QPointF(gainRect.left() + 1, y), QPointF(gainRect.right() - 1, y));
        }
        p.setPen(kDim);
        p.drawText(QRectF(0, y - 8, gainRect.left() - 6, 16), Qt::AlignRight | Qt::AlignVCenter,
                   QString::number(db));
    }
    for (int k = 0; k <= 2; ++k) {
        const double degrees = (phaseTop - 10.0) - 45.0 * k;
        const double y = yPhase(degrees);
        p.setPen(QPen(kGrid, 1.0));
        p.drawLine(QPointF(phaseRect.left() + 1, y), QPointF(phaseRect.right() - 1, y));
        p.setPen(kDim);
        p.drawText(QRectF(0, y - 8, phaseRect.left() - 6, 16), Qt::AlignRight | Qt::AlignVCenter,
                   QString::number(degrees) + QStringLiteral("°"));
    }

    // --- Curves -------------------------------------------------------------
    QPainterPath gainPath;
    QPainterPath phasePath;
    for (int i = 0; i <= kSamples; ++i) {
        const double frequency = std::pow(10.0, logLow + (logHigh - logLow) * i / kSamples);
        const double x = xOf(frequency);
        const double normalized = frequency / fc;
        const QPointF gainPoint(x, yGain(20.0 * std::log10(RcFilterCalculator::magnitude(type, normalized))));
        const QPointF phasePoint(x, yPhase(RcFilterCalculator::phaseRadians(type, normalized) * kRadToDeg));
        if (i == 0) {
            gainPath.moveTo(gainPoint);
            phasePath.moveTo(phasePoint);
        } else {
            gainPath.lineTo(gainPoint);
            phasePath.lineTo(phasePoint);
        }
    }

    // --- Cutoff marker ------------------------------------------------------
    const double xCutoff = xOf(fc);
    p.setPen(QPen(kCutoff, 1.2, Qt::DashLine));
    p.drawLine(QPointF(xCutoff, gainRect.top() + 1), QPointF(xCutoff, gainRect.bottom() - 1));
    p.drawLine(QPointF(xCutoff, phaseRect.top() + 1), QPointF(xCutoff, phaseRect.bottom() - 1));

    p.setPen(QPen(kCurve, 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    p.save();
    p.setClipRect(gainRect.adjusted(1, 1, -1, -1));
    p.drawPath(gainPath);
    p.setClipRect(phaseRect.adjusted(1, 1, -1, -1));
    p.drawPath(phasePath);
    p.restore();

    // --- Selected frequency marker -----------------------------------------
    const double xSelected = xOf(f);
    const QPointF gainMarker(xSelected, yGain(m_result.gainDb));
    const QPointF phaseMarker(xSelected, yPhase(m_result.phaseDegrees));
    p.setPen(QPen(kPlotBackground, 2.0));
    p.setBrush(kText);
    p.drawEllipse(gainMarker, 5.5, 5.5);
    p.drawEllipse(phaseMarker, 5.5, 5.5);

    // --- Labels -------------------------------------------------------------
    // Plot titles sit in the corner the curve stays away from.
    const bool lowPass = (type == RcFilterType::LowPass);
    p.setFont(smallBold);
    p.setPen(kDim);
    p.drawText(gainRect.adjusted(10, 0, -10, -6),
               (lowPass ? Qt::AlignLeft : Qt::AlignRight) | Qt::AlignBottom, tr("Gain (dB)"));
    p.drawText(phaseRect.adjusted(10, 0, -10, -6), Qt::AlignLeft | Qt::AlignBottom, tr("Phase"));

    // Cutoff label next to its line, on the side where the gain curve is low.
    p.setPen(kCutoff);
    const QString cutoffText = QStringLiteral("fc = ") + Format::si(fc, QStringLiteral("Hz"));
    if (lowPass) {
        p.drawText(QRectF(xCutoff + 8, gainRect.top() + 6, 160, 16),
                   Qt::AlignLeft | Qt::AlignVCenter, cutoffText);
    } else {
        p.drawText(QRectF(xCutoff - 168, gainRect.top() + 6, 160, 16),
                   Qt::AlignRight | Qt::AlignVCenter, cutoffText);
    }

    // Value label beside the selected-frequency marker, kept inside the plot.
    const QString markerText = Format::number(Format::tidy(m_result.gainDb), 3) + QStringLiteral(" dB @ ")
                               + Format::si(f, QStringLiteral("Hz"), 3);
    const double labelWidth = p.fontMetrics().horizontalAdvance(markerText) + 4.0;
    // Below the marker, on the side where the curve rises away from it
    // (left for low-pass, right for high-pass), unless it would not fit.
    bool placeLeft = lowPass;
    if (placeLeft && gainMarker.x() - 10 - labelWidth < gainRect.left() + 4)
        placeLeft = false;
    else if (!placeLeft && gainMarker.x() + 10 + labelWidth > gainRect.right() - 4)
        placeLeft = true;
    const double labelX = placeLeft ? gainMarker.x() - 10 - labelWidth : gainMarker.x() + 10;
    const double labelY = qBound(gainRect.top() + 24.0, gainMarker.y() + 10.0, gainRect.bottom() - 20.0);
    // Opaque backing so grid and cutoff lines do not run through the text.
    const QRectF labelRect(labelX, labelY, labelWidth, 16);
    p.fillRect(labelRect.adjusted(-3, 0, 3, 0), kPlotBackground);
    p.setPen(kText);
    p.drawText(labelRect, (placeLeft ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter, markerText);
}
