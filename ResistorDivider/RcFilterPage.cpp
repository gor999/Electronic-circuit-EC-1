#include "RcFilterPage.h"

#include "Format.h"
#include "FrequencyResponseWidget.h"
#include "RcCircuitWidget.h"
#include "Ui.h"

#include <QButtonGroup>
#include <QComboBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

#include <exception>

namespace {

const QString kNoValue = QStringLiteral("—");
const QString kOhm = QStringLiteral("Ω");

struct UnitOption
{
    QString label;
    double factor;   // multiply the entered number by this to get SI units
};

QComboBox *createUnitCombo(const QList<UnitOption> &options, int defaultIndex)
{
    auto *combo = new QComboBox;
    for (const UnitOption &option : options)
        combo->addItem(option.label, option.factor);
    combo->setCurrentIndex(defaultIndex);
    combo->setFixedWidth(84);
    combo->setProperty("defaultIndex", defaultIndex);
    return combo;
}

double unitFactor(const QComboBox *combo)
{
    return combo->currentData().toDouble();
}

} // namespace

RcFilterPage::RcFilterPage(QWidget *parent)
    : QWidget(parent)
{
    auto *topRow = new QHBoxLayout;
    topRow->setSpacing(14);
    topRow->addWidget(buildCircuitPanel(), 3);
    topRow->addWidget(buildResultsPanel(), 7);

    auto *right = new QVBoxLayout;
    right->setSpacing(14);
    right->addLayout(topRow);
    right->addWidget(buildResponsePanel(), 1);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(14);
    layout->addWidget(buildInputPanel());
    layout->addLayout(right, 1);

    clearResults();
    m_circuit->setFilterType(selectedType());
}

// ---------------------------------------------------------------------------
// UI construction
// ---------------------------------------------------------------------------

QWidget *RcFilterPage::buildInputPanel()
{
    QFrame *panel = Ui::createPanel(QStringLiteral("inputPanel"));
    panel->setFixedWidth(310);

    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(8);

    layout->addWidget(Ui::createLabel(tr("INPUT"), QStringLiteral("sectionTitle")));
    layout->addSpacing(6);

    // Filter type: two-button segmented control.
    layout->addWidget(Ui::createLabel(tr("Filter type"), QStringLiteral("fieldLabel")));
    auto *lowPassButton = new QPushButton(tr("Low-pass"));
    lowPassButton->setObjectName(QStringLiteral("segmentLeft"));
    auto *highPassButton = new QPushButton(tr("High-pass"));
    highPassButton->setObjectName(QStringLiteral("segmentRight"));
    m_typeGroup = new QButtonGroup(this);
    m_typeGroup->setExclusive(true);
    m_typeGroup->addButton(lowPassButton, static_cast<int>(RcFilterType::LowPass));
    m_typeGroup->addButton(highPassButton, static_cast<int>(RcFilterType::HighPass));
    for (QAbstractButton *button : m_typeGroup->buttons()) {
        button->setCheckable(true);
        button->setCursor(Qt::PointingHandCursor);
    }
    lowPassButton->setChecked(true);

    auto *typeRow = new QHBoxLayout;
    typeRow->setSpacing(0);
    typeRow->addWidget(lowPassButton);
    typeRow->addWidget(highPassButton);
    layout->addLayout(typeRow);
    layout->addSpacing(8);

    // One row per quantity: number field plus unit selector.
    auto addField = [this, layout](const QString &title, const QString &placeholder,
                                   const QString &objectName, QLineEdit *&edit, QWidget *unit) {
        layout->addWidget(Ui::createLabel(title, QStringLiteral("fieldLabel")));
        edit = Ui::createNumberEdit(placeholder);
        edit->setObjectName(objectName);
        auto *row = new QHBoxLayout;
        row->setSpacing(8);
        row->addWidget(edit, 1);
        row->addWidget(unit);
        layout->addLayout(row);
        layout->addSpacing(8);
        connect(edit, &QLineEdit::returnPressed, this, &RcFilterPage::onCalculate);
    };

    m_resistanceUnit = createUnitCombo({{kOhm, 1.0}, {QStringLiteral("k") + kOhm, 1.0e3},
                                        {QStringLiteral("M") + kOhm, 1.0e6}}, 1);
    m_resistanceUnit->setObjectName(QStringLiteral("resistanceUnit"));
    m_capacitanceUnit = createUnitCombo({{QStringLiteral("pF"), 1.0e-12}, {QStringLiteral("nF"), 1.0e-9},
                                         {QStringLiteral("µF"), 1.0e-6}, {QStringLiteral("mF"), 1.0e-3}}, 1);
    m_capacitanceUnit->setObjectName(QStringLiteral("capacitanceUnit"));
    m_frequencyUnit = createUnitCombo({{QStringLiteral("Hz"), 1.0}, {QStringLiteral("kHz"), 1.0e3},
                                       {QStringLiteral("MHz"), 1.0e6}}, 0);
    m_frequencyUnit->setObjectName(QStringLiteral("frequencyUnit"));
    QLabel *voltUnit = Ui::createLabel(QStringLiteral("V"), QStringLiteral("unitLabel"));
    voltUnit->setFixedWidth(84);

    addField(tr("Resistance (R)"), tr("e.g. 1"), QStringLiteral("resistanceEdit"),
             m_resistanceEdit, m_resistanceUnit);
    addField(tr("Capacitance (C)"), tr("e.g. 100"), QStringLiteral("capacitanceEdit"),
             m_capacitanceEdit, m_capacitanceUnit);
    addField(tr("Input signal frequency"), tr("e.g. 1000"), QStringLiteral("frequencyEdit"),
             m_frequencyEdit, m_frequencyUnit);
    addField(tr("Input voltage amplitude"), tr("e.g. 5"), QStringLiteral("amplitudeEdit"),
             m_amplitudeEdit, voltUnit);

    layout->addStretch(1);

    auto *calculateButton = new QPushButton(tr("Calculate"));
    calculateButton->setObjectName(QStringLiteral("calculateButton"));
    calculateButton->setCursor(Qt::PointingHandCursor);
    auto *clearButton = new QPushButton(tr("Clear"));
    clearButton->setObjectName(QStringLiteral("clearButton"));
    clearButton->setCursor(Qt::PointingHandCursor);

    auto *buttonRow = new QHBoxLayout;
    buttonRow->setSpacing(10);
    buttonRow->addWidget(clearButton, 1);
    buttonRow->addWidget(calculateButton, 2);
    layout->addLayout(buttonRow);

    connect(m_typeGroup, &QButtonGroup::idClicked, this, &RcFilterPage::onFilterTypeChanged);
    connect(calculateButton, &QPushButton::clicked, this, &RcFilterPage::onCalculate);
    connect(clearButton, &QPushButton::clicked, this, &RcFilterPage::onClear);

    return panel;
}

QWidget *RcFilterPage::buildCircuitPanel()
{
    QFrame *panel = Ui::createPanel(QStringLiteral("panel"));
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(18, 18, 18, 12);
    layout->setSpacing(4);
    layout->addWidget(Ui::createLabel(tr("CIRCUIT"), QStringLiteral("sectionTitle")));

    m_circuit = new RcCircuitWidget;
    layout->addWidget(m_circuit, 1);

    m_transferFunction = Ui::createLabel(QString(), QStringLiteral("cardDetail"));
    m_transferFunction->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_transferFunction);
    return panel;
}

QWidget *RcFilterPage::buildResultsPanel()
{
    QFrame *panel = Ui::createPanel(QStringLiteral("panel"));
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(12);
    layout->addWidget(Ui::createLabel(tr("RESULTS"), QStringLiteral("sectionTitle")));

    auto *grid = new QGridLayout;
    grid->setSpacing(10);
    struct Entry { QString title; Card *card; };
    const Entry entries[] = {
        {tr("Cutoff fc"), &m_cutoff},
        {tr("Time constant τ"), &m_timeConstant},
        {tr("Angular ωc"), &m_angularCutoff},
        {tr("Reactance Xc"), &m_reactance},
        {tr("Output Vout"), &m_output},
        {tr("Gain"), &m_gain},
        {tr("Phase shift"), &m_phase},
        {tr("Magnitude |H|"), &m_magnitude},
    };
    int index = 0;
    for (const Entry &entry : entries) {
        QWidget *card = Ui::createSummaryCard(entry.title, entry.card->value, entry.card->detail);
        entry.card->value->setObjectName(QStringLiteral("cardValueCompact"));
        grid->addWidget(card, index / 4, index % 4);
        ++index;
    }
    for (int column = 0; column < 4; ++column)
        grid->setColumnStretch(column, 1);
    layout->addLayout(grid, 1);

    panel->setMinimumWidth(560);
    return panel;
}

QWidget *RcFilterPage::buildResponsePanel()
{
    QFrame *panel = Ui::createPanel(QStringLiteral("panel"));
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(18, 18, 18, 12);
    layout->setSpacing(8);
    layout->addWidget(Ui::createLabel(tr("FREQUENCY RESPONSE"), QStringLiteral("sectionTitle")));

    m_response = new FrequencyResponseWidget;
    layout->addWidget(m_response, 1);
    return panel;
}

// ---------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------

RcFilterType RcFilterPage::selectedType() const
{
    return static_cast<RcFilterType>(m_typeGroup->checkedId());
}

void RcFilterPage::onFilterTypeChanged()
{
    if (m_hasResult) {
        // Keep the results in step with the selected filter type.
        onCalculate();
        return;
    }
    clearResults();
    m_circuit->setFilterType(selectedType());
}

void RcFilterPage::onCalculate()
{
    double resistance = 0.0;
    double capacitance = 0.0;
    double frequency = 0.0;
    double amplitude = 0.0;
    if (!Ui::readPositiveValue(this, m_resistanceEdit, tr("Resistance"), resistance)
        || !Ui::readPositiveValue(this, m_capacitanceEdit, tr("Capacitance"), capacitance)
        || !Ui::readPositiveValue(this, m_frequencyEdit, tr("Frequency"), frequency)
        || !Ui::readPositiveValue(this, m_amplitudeEdit, tr("Input voltage amplitude"), amplitude)) {
        return;
    }

    // Convert to SI units before any calculation.
    resistance *= unitFactor(m_resistanceUnit);
    capacitance *= unitFactor(m_capacitanceUnit);
    frequency *= unitFactor(m_frequencyUnit);

    try {
        showResult(RcFilterCalculator::calculate(selectedType(), resistance, capacitance,
                                                 frequency, amplitude));
    } catch (const std::exception &error) {
        QMessageBox::warning(this, tr("Calculation error"), QString::fromUtf8(error.what()));
    }
}

void RcFilterPage::onClear()
{
    for (QLineEdit *edit : {m_resistanceEdit, m_capacitanceEdit, m_frequencyEdit, m_amplitudeEdit})
        edit->clear();
    for (QComboBox *combo : {m_resistanceUnit, m_capacitanceUnit, m_frequencyUnit})
        combo->setCurrentIndex(combo->property("defaultIndex").toInt());
    m_typeGroup->button(static_cast<int>(RcFilterType::LowPass))->setChecked(true);

    clearResults();
    m_circuit->setFilterType(selectedType());
    m_resistanceEdit->setFocus();
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void RcFilterPage::showResult(const RcFilterResult &r)
{
    m_cutoff.value->setText(Format::si(r.cutoffFrequency, QStringLiteral("Hz")));
    m_cutoff.detail->setText(tr("−3 dB frequency"));

    m_timeConstant.value->setText(Format::si(r.timeConstant, QStringLiteral("s")));
    m_timeConstant.detail->setText(QStringLiteral("R × C"));

    m_angularCutoff.value->setText(Format::si(r.angularCutoff, QStringLiteral("rad/s")));
    m_angularCutoff.detail->setText(QStringLiteral("cutoff, 1 / RC"));

    m_reactance.value->setText(Format::si(r.capacitiveReactance, kOhm));
    m_reactance.detail->setText(tr("at %1").arg(Format::si(r.frequency, QStringLiteral("Hz"), 3)));

    m_output.value->setText(Format::si(r.outputAmplitude, QStringLiteral("V")));
    m_output.detail->setText(tr("amplitude, %1 in").arg(Format::si(r.inputAmplitude, QStringLiteral("V"), 3)));

    m_gain.value->setText(Format::number(Format::tidy(r.gainDb), 4) + QStringLiteral(" dB"));
    m_gain.detail->setText(QStringLiteral("20·log₁₀|H|"));

    m_phase.value->setText(Format::number(Format::tidy(r.phaseDegrees), 4) + QStringLiteral("°"));
    m_phase.detail->setText(Format::number(Format::tidy(r.phaseRadians), 4) + QStringLiteral(" rad"));

    m_magnitude.value->setText(Format::number(r.magnitude, 4));
    m_magnitude.detail->setText(QStringLiteral("Vout / Vin"));

    m_transferFunction->setText(r.type == RcFilterType::LowPass
                                    ? QStringLiteral("H(jω) = 1 / (1 + jωRC)")
                                    : QStringLiteral("H(jω) = jωRC / (1 + jωRC)"));
    m_circuit->setResult(r);
    m_response->setResult(r);
    m_hasResult = true;
}

void RcFilterPage::clearResults()
{
    for (Card *card : {&m_cutoff, &m_timeConstant, &m_angularCutoff, &m_reactance,
                       &m_output, &m_gain, &m_phase, &m_magnitude}) {
        card->value->setText(kNoValue);
        card->detail->setText(QStringLiteral(" "));
    }
    m_transferFunction->setText(selectedType() == RcFilterType::LowPass
                                    ? QStringLiteral("H(jω) = 1 / (1 + jωRC)")
                                    : QStringLiteral("H(jω) = jωRC / (1 + jωRC)"));
    m_circuit->clearResult();
    m_response->clearResult();
    m_hasResult = false;
}
