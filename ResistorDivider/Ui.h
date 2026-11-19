#pragma once

#include <QLocale>
#include <QString>

class QFrame;
class QLabel;
class QLineEdit;
class QWidget;

// Small widget factories and input helpers shared by both calculator pages,
// so the two tabs look and validate the same way.
namespace Ui {

// Locale used for all numeric input: "." decimal separator, no grouping.
QLocale numberLocale();

QFrame *createPanel(const QString &objectName);
QLabel *createLabel(const QString &text, const QString &objectName);

// Line edit that only accepts non-negative decimal numbers.
QLineEdit *createNumberEdit(const QString &placeholder);

// Card with a title, a large value and a small detail line.
QWidget *createSummaryCard(const QString &title, QLabel *&valueLabel, QLabel *&detailLabel);

// Reads a strictly positive number from `edit`. On failure shows a message
// box owned by `parent`, focuses the field and returns false.
bool readPositiveValue(QWidget *parent, QLineEdit *edit, const QString &fieldName, double &value);

} // namespace Ui
