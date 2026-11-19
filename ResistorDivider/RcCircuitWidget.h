#pragma once

#include "RcFilterCalculator.h"

#include <QWidget>

// Schematic of the RC filter. Low-pass: series R, shunt C. High-pass: series C,
// shunt R. The output is taken across the shunt element.
class RcCircuitWidget : public QWidget
{
    Q_OBJECT

public:
    explicit RcCircuitWidget(QWidget *parent = nullptr);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

public slots:
    // Shows the topology only (no component values).
    void setFilterType(RcFilterType type);
    void setResult(const RcFilterResult &result);
    void clearResult();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    RcFilterType m_type = RcFilterType::LowPass;
    bool m_hasResult = false;
    RcFilterResult m_result;
};
