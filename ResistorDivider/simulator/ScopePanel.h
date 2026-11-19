#pragma once

#include <QList>
#include <QRect>
#include <QString>
#include <QWidget>

#include <vector>

class Circuit;

// Oscilloscope strip below the canvas. Each scope follows one element and
// plots its voltage and current against time, with amplitude and frequency
// read-outs. Scopes are laid out side by side.
class ScopePanel : public QWidget
{
    Q_OBJECT

public:
    explicit ScopePanel(Circuit *circuit, QWidget *parent = nullptr);

    int count() const { return static_cast<int>(m_scopes.size()); }
    bool contains(int elementId) const;
    QList<int> elementIds() const;

    void sample();              // call once after every simulation step
    QSize sizeHint() const override { return QSize(600, 190); }
    QSize minimumSizeHint() const override { return QSize(200, 120); }

public slots:
    void addScope(int elementId);
    void removeAll();
    void clearData();           // keep the scopes, drop the recorded samples
    void faster();              // shorter time window
    void slower();              // longer time window

signals:
    void countChanged(int count);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    struct Column
    {
        float vMin = 0, vMax = 0, iMin = 0, iMax = 0;
    };

    struct Scope
    {
        int elementId = 0;
        bool showVoltage = true;
        bool showCurrent = true;
        std::vector<Column> columns;    // ring buffer, one entry per pixel column
        int head = 0;                   // next write position
        int filled = 0;
        Column pending;                 // column being accumulated
        int pendingSteps = 0;
    };

    QRect cellRect(int index) const;
    QRect closeRect(int index) const;
    int scopeAt(const QPoint &pos) const;
    void removeScope(int index);
    void paintScope(QPainter &painter, const Scope &scope, const QRect &cell);

    Circuit *m_circuit;
    std::vector<Scope> m_scopes;
    int m_stepsPerColumn = 8;
};
