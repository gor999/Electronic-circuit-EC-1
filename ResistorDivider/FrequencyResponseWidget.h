#pragma once

#include "RcFilterCalculator.h"

#include <QWidget>

// Bode-style plot of the filter: gain (dB) and phase (degrees) against a
// logarithmic frequency axis, with markers at the cutoff frequency and at the
// frequency the user entered.
class FrequencyResponseWidget : public QWidget
{
    Q_OBJECT

public:
    explicit FrequencyResponseWidget(QWidget *parent = nullptr);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

public slots:
    void setResult(const RcFilterResult &result);
    void clearResult();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    bool m_hasResult = false;
    RcFilterResult m_result;
};
