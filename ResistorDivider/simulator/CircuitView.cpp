#include "CircuitView.h"

#include "Circuit.h"
#include "Element.h"

#include <QClipboard>
#include <QContextMenuEvent>
#include <QDragEnterEvent>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <map>
#include <vector>

namespace {

const QColor kBackground(0x0d, 0x11, 0x17);
const QColor kGridDot(0x23, 0x2b, 0x36);
const QColor kPost(0xe6, 0xea, 0xf0);
const QColor kSelect(0x4c, 0xc2, 0xff);
const QColor kInfoText(0xc8, 0xd0, 0xdc);
const QColor kError(0xff, 0x6b, 0x6b);

const double kMinScale = 0.25;
const double kMaxScale = 5.0;
const int kHistoryLimit = 100;

double distanceToSegment(const QPointF &p, const QPointF &a, const QPointF &b)
{
    const QPointF ab = b - a;
    const double lengthSquared = ab.x() * ab.x() + ab.y() * ab.y();
    double t = lengthSquared > 0.0 ? QPointF::dotProduct(p - a, ab) / lengthSquared : 0.0;
    t = qBound(0.0, t, 1.0);
    const QPointF delta = p - (a + ab * t);
    return std::hypot(delta.x(), delta.y());
}

} // namespace

CircuitView::CircuitView(Circuit *circuit, QWidget *parent)
    : QWidget(parent)
    , m_circuit(circuit)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setAcceptDrops(true);
    setMinimumSize(320, 220);
}

CircuitView::~CircuitView() = default;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

QPointF CircuitView::toWorld(const QPointF &screen) const
{
    return (screen - m_offset) / m_scale;
}

QList<Element *> CircuitView::selection() const
{
    QList<Element *> result;
    for (const auto &element : m_circuit->elements) {
        if (element->selected)
            result << element.get();
    }
    return result;
}

void CircuitView::clearSelection()
{
    for (const auto &element : m_circuit->elements)
        element->selected = false;
}

Element *CircuitView::elementAt(const QPointF &world) const
{
    const double tolerance = 9.0;
    Element *best = nullptr;
    double bestDistance = tolerance;
    // Later elements are drawn on top, so prefer them on ties.
    for (auto it = m_circuit->elements.rbegin(); it != m_circuit->elements.rend(); ++it) {
        Element *element = it->get();
        double distance = distanceToSegment(world, element->p1, element->p2);
        if (element->postCount() == 3
            && ElementPainter::bounds(*element).adjusted(-4, -4, 4, 4).contains(world)) {
            distance = qMin(distance, 4.0);
        }
        if (distance < bestDistance) {
            bestDistance = distance;
            best = element;
        }
    }
    return best;
}

int CircuitView::handleAt(const QPointF &world) const
{
    const QList<Element *> selected = selection();
    if (selected.size() != 1)
        return -1;
    const double reach = 7.0 / qMin(1.0, m_scale);
    const Element *element = selected.first();
    for (int i = 0; i < 2; ++i) {
        const QPointF delta = world - QPointF(i == 0 ? element->p1 : element->p2);
        if (std::hypot(delta.x(), delta.y()) <= reach)
            return i;
    }
    return -1;
}

void CircuitView::changed()
{
    m_circuit->markDirty();
    update();
    emit circuitChanged();
}

// ---------------------------------------------------------------------------
// Undo / redo (whole-circuit snapshots)
// ---------------------------------------------------------------------------

void CircuitView::pushUndo()
{
    m_undo << m_circuit->serialize();
    if (m_undo.size() > kHistoryLimit)
        m_undo.removeFirst();
    m_redo.clear();
}

void CircuitView::clearHistory()
{
    m_undo.clear();
    m_redo.clear();
    m_pasteCount = 0;
}

void CircuitView::restore(const QString &snapshot)
{
    m_drag = Drag::None;
    m_pressedElement = nullptr;
    m_circuit->deserialize(snapshot);
    emit selectionChanged();
    changed();
}

void CircuitView::undo()
{
    if (m_undo.isEmpty())
        return;
    m_redo << m_circuit->serialize();
    restore(m_undo.takeLast());
}

void CircuitView::redo()
{
    if (m_redo.isEmpty())
        return;
    m_undo << m_circuit->serialize();
    restore(m_redo.takeLast());
}

// ---------------------------------------------------------------------------
// Editing commands
// ---------------------------------------------------------------------------

void CircuitView::setTool(const QString &code)
{
    if (m_drag == Drag::Place) {
        // Switching tools cancels an element that is being dragged out.
        m_placing.reset();
        m_drag = Drag::None;
    }
    if (m_tool == code) {
        update();
        return;
    }
    m_tool = code;
    setCursor(code.isEmpty() ? Qt::ArrowCursor : Qt::CrossCursor);
    emit toolChanged(code);
    update();
}

void CircuitView::deleteSelection()
{
    if (selection().isEmpty())
        return;
    pushUndo();
    auto &elements = m_circuit->elements;
    elements.erase(std::remove_if(elements.begin(), elements.end(),
                                  [](const std::unique_ptr<Element> &e) { return e->selected; }),
                   elements.end());
    m_pressedElement = nullptr;
    emit selectionChanged();
    changed();
}

void CircuitView::selectAll()
{
    for (const auto &element : m_circuit->elements)
        element->selected = true;
    emit selectionChanged();
    update();
}

QString CircuitView::selectionText() const
{
    QString text;
    for (const Element *element : selection())
        text += element->toLine() + QLatin1Char('\n');
    return text;
}

void CircuitView::insertText(const QString &text, const QPoint &offset)
{
    std::vector<std::unique_ptr<Element>> created;
    const QStringList lines = text.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        std::unique_ptr<Element> element = Element::fromLine(line);
        if (element)
            created.push_back(std::move(element));
    }
    if (created.empty())
        return;

    pushUndo();
    clearSelection();
    for (auto &element : created) {
        element->id = 0;    // a fresh id is assigned by the circuit
        element->p1 += offset;
        element->p2 += offset;
        element->selected = true;
        m_circuit->add(std::move(element));
    }
    emit selectionChanged();
    changed();
}

void CircuitView::copy()
{
    const QString text = selectionText();
    if (text.isEmpty())
        return;
    QGuiApplication::clipboard()->setText(text);
    m_pasteCount = 0;
}

void CircuitView::cut()
{
    copy();
    deleteSelection();
}

void CircuitView::paste()
{
    ++m_pasteCount;
    insertText(QGuiApplication::clipboard()->text(), QPoint(2, 2) * kGridSize * m_pasteCount);
}

void CircuitView::duplicate()
{
    insertText(selectionText(), QPoint(2, 2) * kGridSize);
}

void CircuitView::rotateSelection()
{
    const QList<Element *> selected = selection();
    if (selected.isEmpty())
        return;

    QRectF box;
    for (const Element *element : selected)
        box = box.isNull() ? ElementPainter::bounds(*element) : box.united(ElementPainter::bounds(*element));
    QPoint centre = Element::snapToGrid(box.center());
    if (selected.size() == 1)
        centre = selected.first()->p1;      // a single element pivots on its first terminal

    pushUndo();
    auto rotate = [&centre](const QPoint &p) {
        const QPoint d = p - centre;
        return centre + QPoint(-d.y(), d.x());     // 90 degrees clockwise on screen
    };
    for (Element *element : selected) {
        element->p1 = rotate(element->p1);
        element->p2 = rotate(element->p2);
    }
    changed();
}

void CircuitView::flipSelection()
{
    bool any = false;
    for (const Element *element : selection())
        any = any || element->canFlip();
    if (!any)
        return;
    pushUndo();
    for (Element *element : selection()) {
        if (element->canFlip())
            element->flip = !element->flip;
    }
    changed();
}

// ---------------------------------------------------------------------------
// Zoom
// ---------------------------------------------------------------------------

void CircuitView::zoomAt(const QPointF &screen, double factor)
{
    const double newScale = qBound(kMinScale, m_scale * factor, kMaxScale);
    const QPointF world = toWorld(screen);
    m_scale = newScale;
    m_offset = screen - world * m_scale;
    update();
}

void CircuitView::zoomIn() { zoomAt(QPointF(width() / 2.0, height() / 2.0), 1.25); }
void CircuitView::zoomOut() { zoomAt(QPointF(width() / 2.0, height() / 2.0), 0.8); }

void CircuitView::zoomToFit()
{
    if (m_circuit->elements.empty()) {
        m_scale = 1.0;
        m_offset = QPointF(40, 20);
        update();
        return;
    }
    QRectF box;
    for (const auto &element : m_circuit->elements)
        box = box.isNull() ? ElementPainter::bounds(*element) : box.united(ElementPainter::bounds(*element));
    box.adjust(-70, -50, 70, 50);
    m_scale = qBound(kMinScale, qMin(width() / box.width(), height() / box.height()), 2.5);
    m_offset = QPointF(width() / 2.0, height() / 2.0) - box.center() * m_scale;
    update();
}

// ---------------------------------------------------------------------------
// Painting
// ---------------------------------------------------------------------------

void CircuitView::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.fillRect(rect(), kBackground);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    painter.save();
    painter.translate(m_offset);
    painter.scale(m_scale, m_scale);

    // Grid dots (skipped when zoomed far out, where they would merge).
    if (showGrid && m_scale >= 0.6) {
        const QPointF topLeft = toWorld(QPointF(0, 0));
        const QPointF bottomRight = toWorld(QPointF(width(), height()));
        painter.setPen(QPen(kGridDot, 1.5 / m_scale));
        const int x0 = static_cast<int>(std::floor(topLeft.x() / kGridSize)) * kGridSize;
        const int y0 = static_cast<int>(std::floor(topLeft.y() / kGridSize)) * kGridSize;
        for (int x = x0; x <= bottomRight.x(); x += kGridSize) {
            for (int y = y0; y <= bottomRight.y(); y += kGridSize)
                painter.drawPoint(x, y);
        }
    }

    QFont font = painter.font();
    font.setPointSizeF(8.5);
    painter.setFont(font);

    for (const auto &element : m_circuit->elements)
        ElementPainter::paint(painter, *element, options, element->id == m_hoverId);
    if (m_placing)
        ElementPainter::paint(painter, *m_placing, options, true);

    // Terminals: a dot on every post, a larger one where three or more meet.
    std::map<std::pair<int, int>, int> postUse;
    for (const auto &element : m_circuit->elements) {
        for (int i = 0; i < element->postCount(); ++i) {
            const QPoint p = element->post(i);
            ++postUse[{p.x(), p.y()}];
        }
    }
    painter.setPen(Qt::NoPen);
    painter.setBrush(kPost);
    for (const auto &entry : postUse) {
        const double radius = entry.second >= 3 ? 3.6 : 2.2;
        painter.drawEllipse(QPointF(entry.first.first, entry.first.second), radius, radius);
    }

    // Reshape handles on the single selected element.
    const QList<Element *> selected = selection();
    if (selected.size() == 1 && m_drag != Drag::Place) {
        painter.setPen(QPen(kSelect, 1.5));
        painter.setBrush(kBackground);
        for (const QPoint &p : {selected.first()->p1, selected.first()->p2})
            painter.drawRect(QRectF(p.x() - 4, p.y() - 4, 8, 8));
    }

    if (m_drag == Drag::RubberBand) {
        painter.setPen(QPen(kSelect, 1.0 / m_scale, Qt::DashLine));
        painter.setBrush(QColor(0x4c, 0xc2, 0xff, 30));
        painter.drawRect(m_rubberBand.normalized());
    }
    painter.restore();

    paintOverlay(painter);
}

void CircuitView::paintOverlay(QPainter &painter)
{
    QFont font = painter.font();
    font.setPointSizeF(9.5);
    painter.setFont(font);
    const QFontMetrics metrics(font);
    const int lineHeight = metrics.height() + 2;

    // Bottom-right: details of the hovered (or the single selected) element.
    const Element *subject = m_circuit->find(m_hoverId);
    if (!subject) {
        const QList<Element *> selected = selection();
        if (selected.size() == 1)
            subject = selected.first();
    }
    if (subject) {
        const QStringList lines = subject->info();
        int y = height() - 12 - lineHeight * (lines.size() - 1);
        for (int i = 0; i < lines.size(); ++i) {
            painter.setPen(i == 0 ? kSelect : kInfoText);
            painter.drawText(QRect(0, y - metrics.ascent(), width() - 14, lineHeight),
                             Qt::AlignRight | Qt::AlignTop, lines[i]);
            y += lineHeight;
        }
    }

    // Bottom-left: simulation problems, or a hint when the canvas is empty.
    if (!m_circuit->error.isEmpty()) {
        painter.setPen(kError);
        painter.drawText(14, height() - 12, m_circuit->error);
    } else if (m_circuit->elements.empty() && !m_placing) {
        painter.setPen(kInfoText);
        painter.drawText(rect(), Qt::AlignCenter,
                         tr("Pick a component on the left and drag on the canvas to place it,\n"
                            "or open a sample from the Circuits menu."));
    }

    painter.setPen(kInfoText);
    painter.drawText(QRect(0, 12, width() - 14, lineHeight), Qt::AlignRight | Qt::AlignTop, statusText);

    if (!m_tool.isEmpty()) {
        QString label;
        for (const ToolInfo &info : Element::tools()) {
            if (info.code == m_tool)
                label = info.label;
        }
        painter.setPen(kSelect);
        painter.drawText(14, 12 + metrics.ascent(), tr("Placing: %1   (Esc to stop)").arg(label));
    }
}

// ---------------------------------------------------------------------------
// Mouse
// ---------------------------------------------------------------------------

void CircuitView::mousePressEvent(QMouseEvent *event)
{
    setFocus();
    m_pressScreen = event->position();
    const QPointF world = toWorld(event->position());
    m_lastGrid = Element::snapToGrid(world);
    m_dragMoved = false;
    m_undoPushedForDrag = false;

    if (event->button() == Qt::MiddleButton
        || (event->button() == Qt::LeftButton && (event->modifiers() & Qt::AltModifier))) {
        m_drag = Drag::Pan;
        setCursor(Qt::ClosedHandCursor);
        return;
    }
    if (event->button() != Qt::LeftButton)
        return;

    if (!m_tool.isEmpty()) {
        m_placing = Element::create(m_tool);
        if (m_placing) {
            m_placing->p1 = m_placing->p2 = m_lastGrid;
            m_drag = Drag::Place;
        }
        return;
    }

    const int handle = handleAt(world);
    if (handle >= 0) {
        m_drag = Drag::MovePost;
        m_dragPost = handle;
        m_pressedElement = selection().first();
        return;
    }

    Element *hit = elementAt(world);
    const bool additive = event->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier);
    if (hit) {
        if (additive) {
            hit->selected = !hit->selected;
        } else if (!hit->selected) {
            clearSelection();
            hit->selected = true;
        }
        m_pressedElement = hit;
        m_drag = Drag::Move;
        emit selectionChanged();
    } else {
        if (!additive)
            clearSelection();
        m_pressedElement = nullptr;
        m_drag = Drag::RubberBand;
        m_rubberBand = QRectF(world, world);
        emit selectionChanged();
    }
    update();
}

void CircuitView::mouseMoveEvent(QMouseEvent *event)
{
    const QPointF world = toWorld(event->position());
    const QPoint grid = Element::snapToGrid(world);

    switch (m_drag) {
    case Drag::Pan:
        m_offset += event->position() - m_pressScreen;
        m_pressScreen = event->position();
        update();
        break;

    case Drag::Place:
        if (m_placing && m_placing->p2 != grid) {
            m_placing->p2 = grid;
            update();
        }
        break;

    case Drag::Move: {
        const QPoint delta = grid - m_lastGrid;
        if (delta.isNull())
            break;
        if (!m_undoPushedForDrag) {
            pushUndo();
            m_undoPushedForDrag = true;
        }
        for (Element *element : selection()) {
            element->p1 += delta;
            element->p2 += delta;
        }
        m_lastGrid = grid;
        m_dragMoved = true;
        changed();
        break;
    }

    case Drag::MovePost: {
        Element *element = m_pressedElement;
        if (!element)
            break;
        QPoint &moving = (m_dragPost == 0) ? element->p1 : element->p2;
        const QPoint &fixed = (m_dragPost == 0) ? element->p2 : element->p1;
        if (grid == moving || grid == fixed)
            break;
        if (!m_undoPushedForDrag) {
            pushUndo();
            m_undoPushedForDrag = true;
        }
        moving = grid;
        m_dragMoved = true;
        changed();
        break;
    }

    case Drag::RubberBand:
        m_rubberBand.setBottomRight(world);
        update();
        break;

    case Drag::None: {
        const Element *hit = m_tool.isEmpty() ? elementAt(world) : nullptr;
        const int id = hit ? hit->id : 0;
        if (id != m_hoverId) {
            m_hoverId = id;
            update();
        }
        if (m_tool.isEmpty())
            setCursor(handleAt(world) >= 0 ? Qt::SizeAllCursor : Qt::ArrowCursor);
        break;
    }
    }
}

void CircuitView::mouseReleaseEvent(QMouseEvent *event)
{
    const Drag finished = m_drag;
    m_drag = Drag::None;

    switch (finished) {
    case Drag::Pan:
        setCursor(m_tool.isEmpty() ? Qt::ArrowCursor : Qt::CrossCursor);
        break;

    case Drag::Place:
        if (m_placing) {
            // A click without dragging places the element at its default size.
            if (m_placing->p1 == m_placing->p2)
                m_placing->p2 = m_placing->p1 + m_placing->defaultSpan();
            pushUndo();
            clearSelection();
            m_circuit->add(std::move(m_placing));
            emit selectionChanged();
            changed();
        }
        break;

    case Drag::Move:
        // A plain click on a switch or logic input operates it.
        if (!m_dragMoved && m_pressedElement && event->button() == Qt::LeftButton
            && !(event->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier))
            && m_pressedElement->toggle()) {
            changed();
        }
        break;

    case Drag::RubberBand: {
        const QRectF band = m_rubberBand.normalized();
        if (band.width() > 2 || band.height() > 2) {
            for (const auto &element : m_circuit->elements) {
                if (band.intersects(ElementPainter::bounds(*element).adjusted(-1, -1, 1, 1)))
                    element->selected = true;
            }
            emit selectionChanged();
        }
        break;
    }

    default:
        break;
    }
    m_placing.reset();
    update();
}

void CircuitView::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton || !m_tool.isEmpty())
        return;
    if (const Element *hit = elementAt(toWorld(event->position()))) {
        m_drag = Drag::None;
        emit editRequested(hit->id);
    }
}

void CircuitView::wheelEvent(QWheelEvent *event)
{
    zoomAt(event->position(), std::pow(1.0015, event->angleDelta().y()));
    event->accept();
}

void CircuitView::leaveEvent(QEvent *)
{
    if (m_hoverId != 0) {
        m_hoverId = 0;
        update();
    }
}

void CircuitView::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Escape:
        if (m_drag == Drag::Place) {
            m_placing.reset();
            m_drag = Drag::None;
            update();
        } else if (!m_tool.isEmpty()) {
            setTool(QString());
        } else {
            clearSelection();
            emit selectionChanged();
            update();
        }
        break;
    case Qt::Key_Backspace:
        deleteSelection();
        break;
    default:
        QWidget::keyPressEvent(event);
    }
}

void CircuitView::contextMenuEvent(QContextMenuEvent *event)
{
    if (m_drag != Drag::None)
        return;
    Element *hit = elementAt(toWorld(event->pos()));
    if (hit && !hit->selected) {
        clearSelection();
        hit->selected = true;
        emit selectionChanged();
        update();
    }

    QMenu menu(this);
    if (hit) {
        const int id = hit->id;
        QAction *edit = menu.addAction(tr("Edit…"), this, [this, id] { emit editRequested(id); });
        edit->setEnabled(!hit->params().isEmpty());
        menu.addAction(tr("View in Scope"), this, [this, id] { emit scopeRequested(id); });
        menu.addSeparator();
        menu.addAction(tr("Rotate"), this, &CircuitView::rotateSelection);
        if (hit->canFlip())
            menu.addAction(tr("Flip"), this, &CircuitView::flipSelection);
        menu.addSeparator();
        menu.addAction(tr("Cut"), this, &CircuitView::cut);
        menu.addAction(tr("Copy"), this, &CircuitView::copy);
        menu.addAction(tr("Duplicate"), this, &CircuitView::duplicate);
        menu.addAction(tr("Delete"), this, &CircuitView::deleteSelection);
    } else {
        QAction *pasteAction = menu.addAction(tr("Paste"), this, &CircuitView::paste);
        pasteAction->setEnabled(!QGuiApplication::clipboard()->text().isEmpty());
        menu.addAction(tr("Select All"), this, &CircuitView::selectAll);
        menu.addSeparator();
        menu.addAction(tr("Zoom to Fit"), this, &CircuitView::zoomToFit);
    }
    menu.exec(event->globalPos());
}

// ---------------------------------------------------------------------------
// Drag and drop from the component palette
// ---------------------------------------------------------------------------

void CircuitView::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasFormat(toolMimeType()))
        event->acceptProposedAction();
}

void CircuitView::dragMoveEvent(QDragMoveEvent *event)
{
    if (event->mimeData()->hasFormat(toolMimeType()))
        event->acceptProposedAction();
}

void CircuitView::dropEvent(QDropEvent *event)
{
    const QString code = QString::fromUtf8(event->mimeData()->data(toolMimeType()));
    std::unique_ptr<Element> element = Element::create(code);
    if (!element)
        return;
    element->p1 = Element::snapToGrid(toWorld(event->position()));
    element->p2 = element->p1 + element->defaultSpan();
    pushUndo();
    clearSelection();
    element->selected = true;
    m_circuit->add(std::move(element));
    event->acceptProposedAction();
    setFocus();
    emit selectionChanged();
    changed();
}
