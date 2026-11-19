#pragma once

#include "config/Parameters.h"

#include <QWidget>

class QLabel;
class QLineEdit;
class QSlider;

// One parameter from the registry: name and symbol, a slider (linear or in
// decades), a text field that accepts numbers like 2.5, 1e17 or 0.5, and the
// unit. The tooltip carries the description and the physical effects.
// Values passed in and out are SI.
class ParamControl : public QWidget
{
    Q_OBJECT

public:
    explicit ParamControl(const ParamSpec &spec, QWidget *parent = nullptr);

    const ParamSpec &spec() const { return m_spec; }
    void setValue(double siValue);   // does not emit
    double value() const { return m_value; }

    static QString formatDisplay(double displayValue);

signals:
    void edited(double siValue);
    void focused(const ParamSpec *spec);   // the explanation box shows the effects

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    int toSlider(double siValue) const;
    double fromSlider(int position) const;
    void commitText();

    const ParamSpec &m_spec;
    double m_value = 0.0;
    QSlider *m_slider = nullptr;
    QLineEdit *m_edit = nullptr;
    bool m_updating = false;
};
