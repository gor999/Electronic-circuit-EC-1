#pragma once

#include "ResistorCalculator.h"

#include <QWidget>

// Draws the series divider: voltage source on the left, the resistor chain on
// the right, ground at the bottom and a voltage label at every node.
class CircuitWidget : public QWidget
{
    Q_OBJECT

public:
    explicit CircuitWidget(QWidget *parent = nullptr);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

public slots:
    // Shows an unlabelled chain with `count` resistors (no calculated values).
    void setResistorCount(int count);
    // Shows the chain with resistor values and calculated node voltages.
    void setResult(const DividerResult &result);
    void clearResult();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void updateMinimumHeight();

    int m_count = 3;
    bool m_hasResult = false;
    DividerResult m_result;
};
