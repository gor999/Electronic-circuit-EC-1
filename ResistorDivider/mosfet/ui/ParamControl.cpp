#include "ParamControl.h"

#include "Ui.h"

#include <QEvent>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSlider>

#include <algorithm>
#include <cmath>

namespace {

const int kSteps = 1000;

QString html(const char *text) { return QString::fromUtf8(text).toHtmlEscaped(); }

} // namespace

ParamControl::ParamControl(const ParamSpec &spec, QWidget *parent)
    : QWidget(parent), m_spec(spec)
{
    setObjectName(QStringLiteral("transparentHost"));
    QString title = QString::fromUtf8(spec.name);
    if (*spec.symbol)
        title += QStringLiteral("  ") + QString::fromUtf8(spec.symbol);
    auto *name = Ui::createLabel(title, QStringLiteral("fieldLabel"));
    m_slider = new QSlider(Qt::Horizontal);
    m_slider->setRange(0, kSteps);
    m_edit = new QLineEdit;
    m_edit->setFixedWidth(78);
    m_edit->setAlignment(Qt::AlignRight);
    auto *unit = Ui::createLabel(QString::fromUtf8(spec.unit), QStringLiteral("unitLabel"));
    unit->setMinimumWidth(44);

    auto *grid = new QGridLayout(this);
    grid->setContentsMargins(0, 2, 0, 2);
    grid->setHorizontalSpacing(8);
    grid->setVerticalSpacing(2);
    grid->addWidget(name, 0, 0, 1, 3);
    grid->addWidget(m_slider, 1, 0);
    grid->addWidget(m_edit, 1, 1);
    grid->addWidget(unit, 1, 2);
    grid->setColumnStretch(0, 1);

    const QString tip = QStringLiteral("<b>%1</b><p>%2</p><p><span style='color:#4cc2ff'>Effects:</span> %3</p>"
                                       "<p style='color:#8a94a3'>Range %4 to %5 %6</p>")
                            .arg(html(spec.name), html(spec.description), html(spec.effects),
                                 formatDisplay(spec.minimum * spec.unitScale), formatDisplay(spec.maximum * spec.unitScale),
                                 QString::fromUtf8(spec.unit).toHtmlEscaped());
    for (QWidget *w : {static_cast<QWidget *>(name), static_cast<QWidget *>(m_slider), static_cast<QWidget *>(m_edit)}) {
        w->setToolTip(tip);
        w->installEventFilter(this);
    }

    connect(m_slider, &QSlider::valueChanged, this, [this](int position) {
        if (m_updating)
            return;
        m_value = fromSlider(position);
        m_updating = true;
        m_edit->setText(formatDisplay(m_value * m_spec.unitScale));
        m_updating = false;
        emit edited(m_value);
    });
    connect(m_edit, &QLineEdit::editingFinished, this, &ParamControl::commitText);
    setValue(spec.defaultValue());
}

QString ParamControl::formatDisplay(double v)
{
    if (v == 0.0)
        return QStringLiteral("0");
    const double magnitude = std::fabs(v);
    if (magnitude >= 1.0e5 || magnitude < 1.0e-3) {
        const int exponent = int(std::floor(std::log10(magnitude)));
        const double mantissa = v / std::pow(10.0, exponent);
        return QString::number(mantissa, 'f', std::fabs(mantissa - std::round(mantissa)) < 0.005 ? 0 : 2)
               + QStringLiteral("e") + QString::number(exponent);
    }
    return QString::number(v, 'g', 4);
}

int ParamControl::toSlider(double v) const
{
    double t;
    if (m_spec.logarithmic)
        t = (std::log(v) - std::log(m_spec.minimum)) / (std::log(m_spec.maximum) - std::log(m_spec.minimum));
    else
        t = (v - m_spec.minimum) / (m_spec.maximum - m_spec.minimum);
    return int(std::lround(std::clamp(t, 0.0, 1.0) * kSteps));
}

double ParamControl::fromSlider(int position) const
{
    const double t = double(position) / kSteps;
    if (m_spec.logarithmic)
        return std::exp(std::log(m_spec.minimum) + t * (std::log(m_spec.maximum) - std::log(m_spec.minimum)));
    return m_spec.minimum + t * (m_spec.maximum - m_spec.minimum);
}

void ParamControl::setValue(double siValue)
{
    m_value = siValue;
    m_updating = true;
    m_slider->setValue(toSlider(siValue));
    m_edit->setText(formatDisplay(siValue * m_spec.unitScale));
    m_updating = false;
}

void ParamControl::commitText()
{
    if (m_updating)
        return;
    bool ok = false;
    const double display = Ui::numberLocale().toDouble(m_edit->text().trimmed().replace(QLatin1Char(','), QLatin1Char('.')), &ok);
    if (!ok || !std::isfinite(display)) {
        setValue(m_value);   // put the last good value back
        return;
    }
    const double si = std::clamp(display / m_spec.unitScale, m_spec.minimum, m_spec.maximum);
    if (si == m_value) {
        setValue(si);
        return;
    }
    setValue(si);
    emit edited(si);
}

bool ParamControl::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::FocusIn || event->type() == QEvent::Enter)
        emit focused(&m_spec);
    return QWidget::eventFilter(watched, event);
}
