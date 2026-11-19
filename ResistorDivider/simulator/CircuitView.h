#pragma once

#include "ElementPainter.h"

#include <QPointF>
#include <QRectF>
#include <QStringList>
#include <QWidget>

#include <memory>

class Circuit;
class Element;

// The schematic canvas: draws the circuit and implements all editing
// interactions (placing, selecting, moving, rotating, clipboard, undo/redo,
// zoom and pan).
//
// Mouse:
//   tool active    drag to place an element (a plain click places a default-sized one)
//   select mode    click to select, Ctrl/Shift-click to add, drag to move,
//                  drag empty space for a selection rectangle,
//                  drag an end handle of the selected element to reshape it
//   double-click   edit properties         right-click   context menu
//   wheel          zoom                    middle-drag / Alt-drag   pan
class CircuitView : public QWidget
{
    Q_OBJECT

public:
    explicit CircuitView(Circuit *circuit, QWidget *parent = nullptr);
    ~CircuitView() override;

    static QString toolMimeType() { return QStringLiteral("application/x-circuit-tool"); }

    PaintOptions options;
    bool showGrid = true;
    QString statusText;         // simulation time, drawn in the top-right corner

    QString tool() const { return m_tool; }
    QList<Element *> selection() const;
    bool canUndo() const { return !m_undo.isEmpty(); }
    bool canRedo() const { return !m_redo.isEmpty(); }

    void pushUndo();            // call before modifying the circuit from outside
    void clearHistory();

public slots:
    void setTool(const QString &code);      // empty string = select mode
    void undo();
    void redo();
    void cut();
    void copy();
    void paste();
    void duplicate();
    void deleteSelection();
    void selectAll();
    void rotateSelection();
    void flipSelection();
    void zoomIn();
    void zoomOut();
    void zoomToFit();

signals:
    void circuitChanged();              // elements were added, removed or modified
    void selectionChanged();
    void toolChanged(const QString &code);
    void editRequested(int elementId);
    void scopeRequested(int elementId);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    enum class Drag { None, Pan, Place, Move, MovePost, RubberBand };

    QPointF toWorld(const QPointF &screen) const;
    Element *elementAt(const QPointF &world) const;
    int handleAt(const QPointF &world) const;       // 0 or 1 = end of the single selection, -1 = none
    void clearSelection();
    void zoomAt(const QPointF &screen, double factor);
    void changed();                                 // marks dirty, repaints, emits circuitChanged
    void restore(const QString &snapshot);
    QString selectionText() const;
    void insertText(const QString &text, const QPoint &offset);
    void paintOverlay(QPainter &painter);

    Circuit *m_circuit;
    QString m_tool;

    double m_scale = 1.0;
    QPointF m_offset = QPointF(40, 20);

    Drag m_drag = Drag::None;
    QPointF m_pressScreen;
    QPoint m_lastGrid;
    bool m_dragMoved = false;
    bool m_undoPushedForDrag = false;
    int m_dragPost = -1;
    Element *m_pressedElement = nullptr;
    std::unique_ptr<Element> m_placing;
    QRectF m_rubberBand;

    int m_hoverId = 0;
    int m_pasteCount = 0;
    QStringList m_undo;
    QStringList m_redo;
};
