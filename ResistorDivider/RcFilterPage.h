#pragma once

#include "RcFilterCalculator.h"

#include <QWidget>

class FrequencyResponseWidget;
class QButtonGroup;
class QComboBox;
class QLabel;
class QLineEdit;
class RcCircuitWidget;

// RC filter tab: low-pass / high-pass selection, R, C, frequency and amplitude
// inputs with unit prefixes, result cards, schematic and frequency response.
class RcFilterPage : public QWidget
{
    Q_OBJECT

public:
    explicit RcFilterPage(QWidget *parent = nullptr);

private slots:
    void onFilterTypeChanged();
    void onCalculate();
    void onClear();

private:
    struct Card
    {
        QLabel *value = nullptr;
        QLabel *detail = nullptr;
    };

    QWidget *buildInputPanel();
    QWidget *buildCircuitPanel();
    QWidget *buildResultsPanel();
    QWidget *buildResponsePanel();

    RcFilterType selectedType() const;
    void showResult(const RcFilterResult &result);
    void clearResults();

    QButtonGroup *m_typeGroup = nullptr;
    QLineEdit *m_resistanceEdit = nullptr;
    QComboBox *m_resistanceUnit = nullptr;
    QLineEdit *m_capacitanceEdit = nullptr;
    QComboBox *m_capacitanceUnit = nullptr;
    QLineEdit *m_frequencyEdit = nullptr;
    QComboBox *m_frequencyUnit = nullptr;
    QLineEdit *m_amplitudeEdit = nullptr;

    RcCircuitWidget *m_circuit = nullptr;
    FrequencyResponseWidget *m_response = nullptr;
    QLabel *m_transferFunction = nullptr;

    Card m_cutoff;
    Card m_timeConstant;
    Card m_angularCutoff;
    Card m_output;
    Card m_gain;
    Card m_phase;
    Card m_magnitude;
    Card m_reactance;

    bool m_hasResult = false;
};
