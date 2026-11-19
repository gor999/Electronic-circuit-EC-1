#include "ElementDialog.h"

#include "Element.h"
#include "Format.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QVBoxLayout>

#include <cmath>

ElementDialog::ElementDialog(Element *element, QWidget *parent)
    : QDialog(parent)
    , m_element(element)
{
    setWindowTitle(tr("Edit %1").arg(element->name()));

    auto *form = new QFormLayout;
    form->setHorizontalSpacing(14);
    form->setVerticalSpacing(10);

    const QList<ParamInfo> params = element->params();
    for (const ParamInfo &param : params) {
        QWidget *editor = nullptr;
        QWidget *rowWidget = nullptr;
        switch (param.kind) {
        case ParamInfo::Number: {
            auto *edit = new QLineEdit(Format::si(param.value, QString(), 6).trimmed());
            edit->setMinimumWidth(150);
            editor = edit;
            auto *row = new QWidget;
            auto *rowLayout = new QHBoxLayout(row);
            rowLayout->setContentsMargins(0, 0, 0, 0);
            rowLayout->setSpacing(8);
            rowLayout->addWidget(edit, 1);
            auto *unit = new QLabel(param.unit);
            unit->setObjectName(QStringLiteral("unitLabel"));
            unit->setMinimumWidth(36);
            rowLayout->addWidget(unit);
            rowWidget = row;
            connect(edit, &QLineEdit::returnPressed, this, &ElementDialog::apply);
            break;
        }
        case ParamInfo::Choice: {
            auto *combo = new QComboBox;
            combo->addItems(param.choices);
            combo->setCurrentIndex(static_cast<int>(param.value));
            editor = rowWidget = combo;
            break;
        }
        case ParamInfo::Toggle: {
            auto *check = new QCheckBox;
            check->setChecked(param.value != 0.0);
            editor = rowWidget = check;
            break;
        }
        }
        m_editors << editor;
        form->addRow(param.name, rowWidget);
    }

    auto *hint = new QLabel(tr("Suffixes: p, n, u, m, k, meg, G  (for example 4.7k or 100n)"));
    hint->setObjectName(QStringLiteral("cardDetail"));

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Apply
                                         | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        if (applyValues())
            accept();
    });
    connect(buttons->button(QDialogButtonBox::Apply), &QPushButton::clicked, this, &ElementDialog::apply);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 18, 18, 14);
    layout->setSpacing(14);
    layout->addLayout(form);
    layout->addWidget(hint);
    layout->addWidget(buttons);
}

bool ElementDialog::parseValue(const QString &text, double &value)
{
    // number, optional SI prefix, optional unit letters
    static const QRegularExpression pattern(
        QStringLiteral("^\\s*([+-]?(?:\\d+\\.?\\d*|\\.\\d+)(?:[eE][+-]?\\d+)?)\\s*"
                       "(meg|Meg|MEG|[pnuµμmkKMG])?\\s*[A-Za-zΩ/²]*\\s*$"));
    const QRegularExpressionMatch match = pattern.match(text);
    if (!match.hasMatch())
        return false;

    bool ok = false;
    value = match.captured(1).toDouble(&ok);
    if (!ok)
        return false;

    const QString prefix = match.captured(2);
    if (prefix.compare(QLatin1String("meg"), Qt::CaseInsensitive) == 0)
        value *= 1.0e6;
    else if (prefix == QLatin1String("p"))
        value *= 1.0e-12;
    else if (prefix == QLatin1String("n"))
        value *= 1.0e-9;
    else if (prefix == QLatin1String("u") || prefix == QStringLiteral("µ") || prefix == QStringLiteral("μ"))
        value *= 1.0e-6;
    else if (prefix == QLatin1String("m"))
        value *= 1.0e-3;
    else if (prefix == QLatin1String("k") || prefix == QLatin1String("K"))
        value *= 1.0e3;
    else if (prefix == QLatin1String("M"))
        value *= 1.0e6;
    else if (prefix == QLatin1String("G"))
        value *= 1.0e9;
    return std::isfinite(value);
}

void ElementDialog::apply()
{
    applyValues();
}

bool ElementDialog::applyValues()
{
    const QList<ParamInfo> params = m_element->params();
    QList<double> values;

    // Validate everything first so that a bad field changes nothing.
    for (int i = 0; i < params.size(); ++i) {
        const ParamInfo &param = params[i];
        double value = param.value;
        if (param.kind == ParamInfo::Number) {
            auto *edit = static_cast<QLineEdit *>(m_editors[i]);
            QString problem;
            if (edit->text().trimmed().isEmpty())
                problem = tr("%1 is empty. Please enter a value.").arg(param.name);
            else if (!parseValue(edit->text(), value))
                problem = tr("%1 is not a valid number: \"%2\".").arg(param.name, edit->text());
            else if (param.mustBePositive && value <= 0.0)
                problem = tr("%1 must be greater than zero.").arg(param.name);
            if (!problem.isEmpty()) {
                QMessageBox::warning(this, tr("Invalid input"), problem);
                edit->setFocus();
                edit->selectAll();
                return false;
            }
        } else if (param.kind == ParamInfo::Choice) {
            value = static_cast<QComboBox *>(m_editors[i])->currentIndex();
        } else {
            value = static_cast<QCheckBox *>(m_editors[i])->isChecked() ? 1.0 : 0.0;
        }
        values << value;
    }

    if (!m_changed) {
        m_changed = true;
        emit aboutToApply();
    }
    for (int i = 0; i < values.size(); ++i)
        m_element->setParam(i, values[i]);
    emit applied();
    return true;
}
