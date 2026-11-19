#include "ScopePanel.h"

#include "Circuit.h"
#include "Format.h"

#include <QContextMenuEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>

#include <algorithm>
#include <cmath>

namespace {

const QColor kBackground(0x0d, 0x11, 0x17);
const QColor kFrame(0x2a, 0x32, 0x3e);
const QColor kGrid(0x1f, 0x27, 0x32);
const QColor kVoltage(0x2f, 0xe0, 0x6a);
const QColor kCurrent(0xff, 0xd8, 0x4a);
const QColor kText(0xc8, 0xd0, 0xdc);
const QColor kDim(0x8a, 0x94, 0xa3);

const int kCapacity = 2048;     // columns kept per scope
const int kMinSteps = 1;
const int kMaxSteps = 4096;

// Rounds up to the next 1 / 2 / 5 x 10^n value: a tidy full-scale value.
double niceScale(double value)
{
    if (!(value > 1e-12))
        return 1e-3;
    const double power = std::pow(10.0, std::floor(std::log10(value)));
    const double fraction = value / power;
    return (fraction <= 1.0 ? 1.0 : fraction <= 2.0 ? 2.0 : fraction <= 5.0 ? 5.0 : 10.0) * power;
}

} // namespace

ScopePanel::ScopePanel(Circuit *circuit, QWidget *parent)
    : QWidget(parent)
    , m_circuit(circuit)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
}

bool ScopePanel::contains(int elementId) const
{
    return std::any_of(m_scopes.begin(), m_scopes.end(),
                       [elementId](const Scope &s) { return s.elementId == elementId; });
}

QList<int> ScopePanel::elementIds() const
{
    QList<int> ids;
    for (const Scope &scope : m_scopes)
        ids << scope.elementId;
    return ids;
}

void ScopePanel::addScope(int elementId)
{
    const Element *element = m_circuit->find(elementId);
    if (contains(elementId) || !element)
        return;
    Scope scope;
    scope.elementId = elementId;
    scope.showCurrent = element->kind() != Element::Kind::Voltmeter;
    scope.showVoltage = element->kind() != Element::Kind::Ammeter;
    scope.columns.resize(kCapacity);
    m_scopes.push_back(std::move(scope));
    emit countChanged(count());
    update();
}

void ScopePanel::removeScope(int index)
{
    if (index < 0 || index >= count())
        return;
    m_scopes.erase(m_scopes.begin() + index);
    emit countChanged(count());
    update();
}

void ScopePanel::removeAll()
{
    if (m_scopes.empty())
        return;
    m_scopes.clear();
    emit countChanged(0);
    update();
}

void ScopePanel::clearData()
{
    for (Scope &scope : m_scopes) {
        scope.head = scope.filled = scope.pendingSteps = 0;
    }
    update();
}

void ScopePanel::faster()
{
    m_stepsPerColumn = qMax(kMinSteps, m_stepsPerColumn / 2);
    clearData();
}

void ScopePanel::slower()
{
    m_stepsPerColumn = qMin(kMaxSteps, m_stepsPerColumn * 2);
    clearData();
}

void ScopePanel::sample()
{
    for (std::size_t n = 0; n < m_scopes.size();) {
        Scope &scope = m_scopes[n];
        const Element *element = m_circuit->find(scope.elementId);
        if (!element) {
            // The element was deleted: its scope goes with it.
            removeScope(static_cast<int>(n));
            continue;
        }
        const float v = static_cast<float>(element->voltageDiff());
        const float i = static_cast<float>(element->current);
        if (scope.pendingSteps == 0) {
            scope.pending = {v, v, i, i};
        } else {
            scope.pending.vMin = qMin(scope.pending.vMin, v);
            scope.pending.vMax = qMax(scope.pending.vMax, v);
            scope.pending.iMin = qMin(scope.pending.iMin, i);
            scope.pending.iMax = qMax(scope.pending.iMax, i);
        }
        if (++scope.pendingSteps >= m_stepsPerColumn) {
            scope.columns[scope.head] = scope.pending;
            scope.head = (scope.head + 1) % kCapacity;
            scope.filled = qMin(scope.filled + 1, kCapacity);
            scope.pendingSteps = 0;
        }
        ++n;
    }
}

// ---------------------------------------------------------------------------
// Layout and interaction
// ---------------------------------------------------------------------------

QRect ScopePanel::cellRect(int index) const
{
    const int n = qMax(1, count());
    const int gap = 8;
    const int cellWidth = (width() - gap * (n - 1)) / n;
    return QRect(index * (cellWidth + gap), 0, cellWidth, height());
}

QRect ScopePanel::closeRect(int index) const
{
    const QRect cell = cellRect(index);
    return QRect(cell.right() - 21, cell.top() + 4, 18, 18);
}

int ScopePanel::scopeAt(const QPoint &pos) const
{
    for (int i = 0; i < count(); ++i) {
        if (cellRect(i).contains(pos))
            return i;
    }
    return -1;
}

void ScopePanel::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return;
    for (int i = 0; i < count(); ++i) {
        if (closeRect(i).contains(event->pos())) {
            removeScope(i);
            return;
        }
    }
}

void ScopePanel::contextMenuEvent(QContextMenuEvent *event)
{
    const int index = scopeAt(event->pos());
    if (index < 0)
        return;

    QMenu menu(this);
    QAction *voltage = menu.addAction(tr("Show Voltage"));
    voltage->setCheckable(true);
    voltage->setChecked(m_scopes[index].showVoltage);
    QAction *current = menu.addAction(tr("Show Current"));
    current->setCheckable(true);
    current->setChecked(m_scopes[index].showCurrent);
    menu.addSeparator();
    QAction *shorter = menu.addAction(tr("Shorter Time Window"));
    QAction *longer = menu.addAction(tr("Longer Time Window"));
    menu.addSeparator();
    QAction *remove = menu.addAction(tr("Remove Scope"));
    QAction *removeEvery = menu.addAction(tr("Remove All Scopes"));

    const QAction *chosen = menu.exec(event->globalPos());
    if (!chosen || index >= count())
        return;
    if (chosen == voltage)
        m_scopes[index].showVoltage = voltage->isChecked();
    else if (chosen == current)
        m_scopes[index].showCurrent = current->isChecked();
    else if (chosen == shorter)
        faster();
    else if (chosen == longer)
        slower();
    else if (chosen == remove)
        removeScope(index);
    else if (chosen == removeEvery)
        removeAll();
    update();
}

// ---------------------------------------------------------------------------
// Painting
// ---------------------------------------------------------------------------

void ScopePanel::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);
    QFont font = painter.font();
    font.setPointSizeF(8.5);
    painter.setFont(font);
    for (int i = 0; i < count(); ++i)
        paintScope(painter, m_scopes[i], cellRect(i));
}

void ScopePanel::paintScope(QPainter &painter, const Scope &scope, const QRect &cell)
{
    painter.setPen(kFrame);
    painter.setBrush(kBackground);
    painter.drawRect(cell.adjusted(0, 0, -1, -1));

    const QRect plot = cell.adjusted(1, 24, -1, -20);
    const int visible = qMin(scope.filled, qMin(plot.width(), kCapacity));
    const double columnTime = m_stepsPerColumn * m_circuit->timeStep;

    // Statistics over the visible columns.
    double vMax = -1e30, vMin = 1e30, iPeak = 0.0;
    auto columnAt = [&](int k) -> const Column & {    // k = 0 is the oldest visible column
        return scope.columns[(scope.head - visible + k + kCapacity * 2) % kCapacity];
    };
    for (int k = 0; k < visible; ++k) {
        const Column &column = columnAt(k);
        vMax = qMax(vMax, static_cast<double>(column.vMax));
        vMin = qMin(vMin, static_cast<double>(column.vMin));
        iPeak = qMax(iPeak, qMax(std::fabs(static_cast<double>(column.iMax)),
                                 std::fabs(static_cast<double>(column.iMin))));
    }
    if (visible == 0)
        vMax = vMin = 0.0;

    // Frequency: average spacing of rising crossings through the mid level,
    // with hysteresis so that noise near the mid level is not counted.
    double frequency = 0.0;
    const double swing = vMax - vMin;
    if (visible > 8 && swing > 1e-6) {
        const double middle = (vMax + vMin) / 2.0;
        const double high = middle + swing * 0.1;
        const double low = middle - swing * 0.1;
        bool armed = false;
        int first = -1, last = -1, crossings = 0;
        for (int k = 0; k < visible; ++k) {
            const Column &column = columnAt(k);
            const double value = (column.vMin + column.vMax) / 2.0;
            if (value < low) {
                armed = true;
            } else if (value > high && armed) {
                armed = false;
                if (first < 0)
                    first = k;
                last = k;
                ++crossings;
            }
        }
        if (crossings >= 2)
            frequency = (crossings - 1) / ((last - first) * columnTime);
    }

    const double vScale = niceScale(qMax(std::fabs(vMax), std::fabs(vMin)));
    const double iScale = niceScale(iPeak);
    const double centre = plot.center().y();
    const double half = plot.height() / 2.0 - 3.0;

    // Grid: zero line, half-scale lines and a vertical line every 64 px.
    painter.setPen(kGrid);
    for (int x = plot.right() - 64; x > plot.left(); x -= 64)
        painter.drawLine(x, plot.top(), x, plot.bottom());
    painter.drawLine(plot.left(), static_cast<int>(centre - half / 2), plot.right(), static_cast<int>(centre - half / 2));
    painter.drawLine(plot.left(), static_cast<int>(centre + half / 2), plot.right(), static_cast<int>(centre + half / 2));
    painter.setPen(kFrame);
    painter.drawLine(plot.left(), static_cast<int>(centre), plot.right(), static_cast<int>(centre));

    // Traces: one vertical min..max stroke per column, joined to the previous one.
    auto drawTrace = [&](bool voltage, double scale, const QColor &color) {
        painter.setPen(color);
        int previousLow = 0, previousHigh = 0;
        for (int k = 0; k < visible; ++k) {
            const Column &column = columnAt(k);
            const double lowValue = voltage ? column.vMin : column.iMin;
            const double highValue = voltage ? column.vMax : column.iMax;
            int yHigh = static_cast<int>(std::lround(centre - highValue / scale * half));
            int yLow = static_cast<int>(std::lround(centre - lowValue / scale * half));
            if (k > 0) {
                // Bridge the gap to the previous column so the trace is continuous.
                if (yLow < previousHigh)
                    yLow = previousHigh;
                if (yHigh > previousLow)
                    yHigh = previousLow;
            }
            const int x = plot.right() - (visible - 1 - k);
            painter.drawLine(x, yHigh, x, yLow);
            previousHigh = static_cast<int>(std::lround(centre - highValue / scale * half));
            previousLow = static_cast<int>(std::lround(centre - lowValue / scale * half));
        }
    };
    painter.save();
    painter.setClipRect(plot);
    if (scope.showCurrent)
        drawTrace(false, iScale, kCurrent);
    if (scope.showVoltage)
        drawTrace(true, vScale, kVoltage);
    painter.restore();

    // Header: element title on the left; on the right the full-scale values
    // (in their trace colours, which doubles as the legend) and the time base.
    const QFontMetrics metrics(painter.font());
    const QRect header(cell.left() + 8, cell.top() + 4, cell.width() - 36, 18);
    int right = header.right();
    auto drawRight = [&](const QString &text, const QColor &color) {
        const int textWidth = metrics.horizontalAdvance(text);
        painter.setPen(color);
        painter.drawText(QRect(right - textWidth, header.top(), textWidth, header.height()),
                         Qt::AlignLeft | Qt::AlignVCenter, text);
        right -= textWidth + 12;
    };
    drawRight(Format::si(64 * columnTime, QStringLiteral("s"), 3) + tr("/div"), kDim);
    if (scope.showCurrent)
        drawRight(QStringLiteral("I ±") + Format::si(iScale, QStringLiteral("A"), 3), kCurrent);
    if (scope.showVoltage)
        drawRight(QStringLiteral("V ±") + Format::si(vScale, QStringLiteral("V"), 3), kVoltage);

    const Element *element = m_circuit->find(scope.elementId);
    QString title = element ? element->name() : QString();
    if (element && !element->valueText().isEmpty() && element->kind() != Element::Kind::Voltmeter
        && element->kind() != Element::Kind::Ammeter) {
        title += QStringLiteral("  ") + element->valueText();
    }
    const int titleWidth = qMax(0, right - header.left());
    painter.setPen(kText);
    painter.drawText(QRect(header.left(), header.top(), titleWidth, header.height()),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     metrics.elidedText(title, Qt::ElideRight, titleWidth));

    // Footer: measurements.
    QStringList measurements;
    if (scope.showVoltage) {
        measurements << tr("max %1").arg(Format::si(Format::tidy(vMax, 1e-6), QStringLiteral("V"), 3))
                     << tr("min %1").arg(Format::si(Format::tidy(vMin, 1e-6), QStringLiteral("V"), 3))
                     << tr("p-p %1").arg(Format::si(Format::tidy(swing, 1e-6), QStringLiteral("V"), 3));
        if (frequency > 0.0)
            measurements << tr("f %1").arg(Format::si(frequency, QStringLiteral("Hz"), 3));
    }
    if (scope.showCurrent)
        measurements << tr("I peak %1").arg(Format::si(Format::tidy(iPeak, 1e-12), QStringLiteral("A"), 3));
    painter.setPen(kDim);
    painter.drawText(QRect(cell.left() + 8, cell.bottom() - 19, cell.width() - 16, 18),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     metrics.elidedText(measurements.join(QStringLiteral("   ")), Qt::ElideRight,
                                        cell.width() - 16));

    // Close button.
    const QRect close = QRect(cell.right() - 21, cell.top() + 4, 18, 18);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(kDim, 1.6));
    painter.drawLine(close.left() + 5, close.top() + 5, close.right() - 5, close.bottom() - 5);
    painter.drawLine(close.right() - 5, close.top() + 5, close.left() + 5, close.bottom() - 5);
    painter.setRenderHint(QPainter::Antialiasing, false);
}
