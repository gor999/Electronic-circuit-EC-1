#include "PlotWidget.h"

#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

const QColor kBackground(0x16, 0x1b, 0x23);
const QColor kFrame(0x2f, 0x39, 0x47);
const QColor kGrid(0x23, 0x2b, 0x36);
const QColor kText(0xe6, 0xea, 0xf0);
const QColor kDim(0x8a, 0x94, 0xa3);

double niceStep(double span, int target)
{
    const double raw = span / std::max(1, target);
    const double decade = std::pow(10.0, std::floor(std::log10(raw)));
    for (double m : {1.0, 2.0, 5.0, 10.0}) {
        if (m * decade >= raw)
            return m * decade;
    }
    return 10.0 * decade;
}

struct Axis
{
    double lo = 0.0, hi = 1.0;
    bool log = false;
    double map(double v, double a, double b) const   // to pixel range [a, b]
    {
        const double t = log ? (std::log10(v) - std::log10(lo)) / (std::log10(hi) - std::log10(lo)) : (v - lo) / (hi - lo);
        return a + (b - a) * t;
    }
    std::vector<double> ticks(int target) const
    {
        std::vector<double> out;
        if (log) {
            const int first = int(std::ceil(std::log10(lo) - 1e-9)), last = int(std::floor(std::log10(hi) + 1e-9));
            const int stride = std::max(1, (last - first + 1) / target);
            for (int e = first; e <= last; e += stride)
                out.push_back(std::pow(10.0, e));
        } else {
            const double step = niceStep(hi - lo, target);
            for (double v = std::ceil(lo / step - 1e-9) * step; v <= hi + step * 1e-6; v += step)
                out.push_back(std::fabs(v) < step * 1e-9 ? 0.0 : v);
        }
        return out;
    }
};

Axis fit(double lo, double hi, bool log, bool includeZero)
{
    Axis a;
    a.log = log;
    if (!std::isfinite(lo) || !std::isfinite(hi)) {
        lo = log ? 1.0 : 0.0;
        hi = log ? 10.0 : 1.0;
    }
    if (log) {
        lo = std::max(lo, 1e-300);
        if (hi <= lo * 1.0001)
            hi = lo * 10.0;
        a.lo = std::pow(10.0, std::floor(std::log10(lo)));
        a.hi = std::pow(10.0, std::ceil(std::log10(hi)));
        if (a.hi <= a.lo)
            a.hi = a.lo * 10.0;
        return a;
    }
    if (includeZero) {
        lo = std::min(lo, 0.0);
        hi = std::max(hi, 0.0);
    }
    if (hi - lo < 1e-300) {
        hi = lo + (std::fabs(lo) > 0 ? std::fabs(lo) * 0.1 : 1.0);
    }
    const double pad = (hi - lo) * 0.06;
    a.lo = (includeZero && lo == 0.0) ? 0.0 : lo - pad;
    a.hi = hi + pad;
    return a;
}

} // namespace

PlotWidget::PlotWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(260, 170);
    m_formatX = m_formatY = [](double v) { return QString::number(v, 'g', 3); };
}

void PlotWidget::setAxes(const QString &xTitle, Formatter x, bool logX, const QString &yTitle, Formatter y, bool logY)
{
    m_xTitle = xTitle;
    m_yTitle = yTitle;
    m_formatX = std::move(x);
    m_formatY = std::move(y);
    m_logX = logX;
    m_logY = logY;
    update();
}

void PlotWidget::setContent(const QVector<PlotSeries> &series, const QVector<PlotMarker> &markers,
                            const QVector<PlotPoint> &points, const QString &message)
{
    m_series = series;
    m_markers = markers;
    m_points = points;
    m_message = message;
    update();
}

void PlotWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    QFont small = font();
    small.setPointSizeF(8.5);
    p.setFont(small);
    const QFontMetricsF fm(small);

    QRectF plot = QRectF(rect()).adjusted(62, 10, -12, -38);

    // Data range.
    double xlo = std::numeric_limits<double>::max(), xhi = -xlo, ylo = xlo, yhi = -xlo;
    auto take = [&](const QPointF &pt) {
        if (!std::isfinite(pt.x()) || !std::isfinite(pt.y()) || (m_logX && pt.x() <= 0) || (m_logY && pt.y() <= 0))
            return;
        xlo = std::min(xlo, pt.x());
        xhi = std::max(xhi, pt.x());
        ylo = std::min(ylo, pt.y());
        yhi = std::max(yhi, pt.y());
    };
    for (const PlotSeries &s : m_series)
        for (const QPointF &pt : s.points)
            take(pt);
    for (const PlotPoint &pt : m_points)
        take(pt.position);
    const bool empty = xlo > xhi;
    Axis ax, ay;
    if (!empty) {
        ax = fit(xlo, xhi, m_logX, false);
        ay = fit(ylo, yhi, m_logY, !m_logY && ylo >= 0.0);
        // Room on the left for the widest tick label plus the axis title.
        qreal widest = 0.0;
        for (double t : ay.ticks(4))
            widest = std::max(widest, fm.horizontalAdvance(m_formatY(t)));
        plot.setLeft(widest + 30.0);
    }
    p.setPen(kFrame);
    p.setBrush(kBackground);
    p.drawRoundedRect(plot, 4, 4);
    if (empty || !m_message.isEmpty()) {
        p.setPen(kDim);
        p.drawText(plot.adjusted(12, 12, -12, -12), Qt::AlignCenter | Qt::TextWordWrap,
                   m_message.isEmpty() ? QStringLiteral("No data") : m_message);
        if (empty)
            return;
    }
    auto toScreen = [&](const QPointF &pt) {
        return QPointF(ax.map(pt.x(), plot.left(), plot.right()), ay.map(pt.y(), plot.bottom(), plot.top()));
    };

    // Grid and ticks.
    for (double t : ax.ticks(5)) {
        const double x = ax.map(t, plot.left(), plot.right());
        if (x < plot.left() - 0.5 || x > plot.right() + 0.5)
            continue;
        p.setPen(kGrid);
        p.drawLine(QPointF(x, plot.top() + 1), QPointF(x, plot.bottom() - 1));
        p.setPen(kDim);
        p.drawText(QRectF(x - 40, plot.bottom() + 3, 80, 14), Qt::AlignCenter, m_formatX(t));
    }
    for (double t : ay.ticks(4)) {
        const double y = ay.map(t, plot.bottom(), plot.top());
        if (y < plot.top() - 0.5 || y > plot.bottom() + 0.5)
            continue;
        p.setPen(kGrid);
        p.drawLine(QPointF(plot.left() + 1, y), QPointF(plot.right() - 1, y));
        p.setPen(kDim);
        p.drawText(QRectF(18, y - 7, plot.left() - 23, 14), Qt::AlignRight | Qt::AlignVCenter, m_formatY(t));
    }
    p.setPen(kDim);
    p.drawText(QRectF(plot.left(), plot.bottom() + 18, plot.width(), 16), Qt::AlignCenter, m_xTitle);
    p.save();
    p.translate(9, plot.center().y());
    p.rotate(-90);
    p.drawText(QRectF(-plot.height() / 2, -8, plot.height(), 16), Qt::AlignCenter, m_yTitle);
    p.restore();

    p.save();
    p.setClipRect(plot.adjusted(1, 1, -1, -1));
    for (const PlotMarker &m : m_markers) {
        if (!std::isfinite(m.x) || (m_logX && m.x <= 0) || m.x < ax.lo || m.x > ax.hi)
            continue;
        const double x = ax.map(m.x, plot.left(), plot.right());
        p.setPen(QPen(m.color, 1.2, Qt::DashLine));
        p.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
        p.setPen(m.color);
        p.drawText(QRectF(x + 4, plot.top() + 3, 120, 14), Qt::AlignLeft | Qt::AlignVCenter, m.label);
    }
    for (const PlotSeries &s : m_series) {
        if (s.dots) {
            p.setPen(Qt::NoPen);
            p.setBrush(s.color);
            for (const QPointF &pt : s.points) {
                if (std::isfinite(pt.y()) && !(m_logY && pt.y() <= 0) && !(m_logX && pt.x() <= 0))
                    p.drawEllipse(toScreen(pt), 2.4, 2.4);
            }
            continue;
        }
        QPainterPath path;
        bool open = false;
        for (const QPointF &pt : s.points) {
            if (!std::isfinite(pt.x()) || !std::isfinite(pt.y()) || (m_logY && pt.y() <= 0) || (m_logX && pt.x() <= 0)) {
                open = false;
                continue;
            }
            const QPointF q = toScreen(pt);
            if (open)
                path.lineTo(q);
            else
                path.moveTo(q);
            open = true;
        }
        p.setPen(QPen(s.color, s.width, s.style, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(Qt::NoBrush);
        p.drawPath(path);
    }
    for (const PlotPoint &pt : m_points) {
        if (!std::isfinite(pt.position.y()) || (m_logY && pt.position.y() <= 0))
            continue;
        p.setPen(QPen(kBackground, 1.5));
        p.setBrush(pt.color);
        p.drawEllipse(toScreen(pt.position), 4.5, 4.5);
    }
    p.restore();

    // Legend.
    qreal y = plot.top() + 6;
    for (const PlotSeries &s : m_series) {
        if (s.name.isEmpty())
            continue;
        const qreal w = fm.horizontalAdvance(s.name);
        const QRectF box(plot.right() - w - 30, y, w + 24, 15);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(16, 20, 26, 210));
        p.drawRect(box);
        p.setPen(QPen(s.color, 2.0, s.style));
        p.drawLine(QPointF(box.left() + 3, box.center().y()), QPointF(box.left() + 15, box.center().y()));
        p.setPen(kText);
        p.drawText(box.adjusted(19, 0, 0, 0), Qt::AlignLeft | Qt::AlignVCenter, s.name);
        y += 16;
    }
}
