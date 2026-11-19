#pragma once

#include "ResistorCalculator.h"

#include <QWidget>
#include <QVector>

class CircuitWidget;
class QLabel;
class QLineEdit;
class QSpinBox;
class QTableWidget;
class QVBoxLayout;

// Resistor divider tab: collects and validates input, asks ResistorCalculator
// for the numbers, and hands the result to the table and the CircuitWidget.
class DividerPage : public QWidget
{
    Q_OBJECT

public:
    explicit DividerPage(QWidget *parent = nullptr);

private slots:
    void onResistorCountChanged(int count);
    void onCalculate();
    void onClear();

private:
    struct ResistorRow
    {
        QWidget *container = nullptr;
        QLineEdit *edit = nullptr;
    };

    QWidget *buildInputPanel();
    QWidget *buildCircuitPanel();
    QWidget *buildResultsPanel();

    void setResistorFieldCount(int count);
    void showResult(const DividerResult &result);
    void clearResults();

    QSpinBox *m_countSpin = nullptr;
    QLineEdit *m_voltageEdit = nullptr;
    QVBoxLayout *m_resistorLayout = nullptr;
    QVector<ResistorRow> m_resistorRows;

    CircuitWidget *m_circuit = nullptr;

    QLabel *m_totalResistanceValue = nullptr;
    QLabel *m_totalResistanceDetail = nullptr;
    QLabel *m_currentValue = nullptr;
    QLabel *m_currentDetail = nullptr;
    QLabel *m_powerValue = nullptr;
    QLabel *m_powerDetail = nullptr;
    QTableWidget *m_table = nullptr;
};
