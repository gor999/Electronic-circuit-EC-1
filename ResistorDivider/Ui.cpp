#include "Ui.h"

#include <QCoreApplication>
#include <QDoubleValidator>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QVBoxLayout>

#include <cmath>

namespace Ui {

QLocale numberLocale()
{
    QLocale locale = QLocale::c();
    locale.setNumberOptions(QLocale::RejectGroupSeparator);
    return locale;
}

QFrame *createPanel(const QString &objectName)
{
    auto *panel = new QFrame;
    panel->setObjectName(objectName);
    panel->setFrameShape(QFrame::NoFrame);
    return panel;
}

QLabel *createLabel(const QString &text, const QString &objectName)
{
    auto *label = new QLabel(text);
    label->setObjectName(objectName);
    return label;
}

QLineEdit *createNumberEdit(const QString &placeholder)
{
    auto *edit = new QLineEdit;
    edit->setPlaceholderText(placeholder);
    edit->setClearButtonEnabled(true);

    // Only digits and one decimal point can be typed; letters, signs and
    // exponents are rejected by the validator.
    auto *validator = new QDoubleValidator(0.0, 1.0e12, 9, edit);
    validator->setNotation(QDoubleValidator::StandardNotation);
    validator->setLocale(numberLocale());
    edit->setValidator(validator);
    return edit;
}

QWidget *createSummaryCard(const QString &title, QLabel *&valueLabel, QLabel *&detailLabel)
{
    QFrame *card = createPanel(QStringLiteral("card"));
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(2);
    layout->addWidget(createLabel(title, QStringLiteral("cardTitle")));
    valueLabel = createLabel(QStringLiteral("—"), QStringLiteral("cardValue"));
    valueLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(valueLabel);
    detailLabel = createLabel(QStringLiteral(" "), QStringLiteral("cardDetail"));
    layout->addWidget(detailLabel);
    return card;
}

bool readPositiveValue(QWidget *parent, QLineEdit *edit, const QString &fieldName, double &value)
{
    const QString text = edit->text().trimmed();
    QString problem;

    if (text.isEmpty()) {
        problem = QCoreApplication::translate("Ui", "%1 is empty. Please enter a value.").arg(fieldName);
    } else {
        bool ok = false;
        value = numberLocale().toDouble(text, &ok);
        if (!ok || !std::isfinite(value)) {
            problem = QCoreApplication::translate("Ui", "%1 is not a valid number: \"%2\".")
                          .arg(fieldName, text);
        } else if (value <= 0.0) {
            problem = QCoreApplication::translate("Ui", "%1 must be greater than zero.").arg(fieldName);
        }
    }

    if (problem.isEmpty())
        return true;

    QMessageBox::warning(parent, QCoreApplication::translate("Ui", "Invalid input"), problem);
    edit->setFocus();
    edit->selectAll();
    return false;
}

} // namespace Ui
