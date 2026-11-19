#include "DividerPage.h"

#include "CircuitWidget.h"
#include "Format.h"
#include "Ui.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QSplitter>
#include <QTableWidget>
#include <QToolButton>
#include <QVBoxLayout>

#include <exception>

namespace {

const int kMinResistors = 1;
const int kMaxResistors = 20;
const int kDefaultResistors = 3;
const QString kNoValue = QStringLiteral("—");
const QString kOhm = QStringLiteral("Ω");

} // namespace

DividerPage::DividerPage(QWidget *parent)
    : QWidget(parent)
{
    auto *splitter = new QSplitter(Qt::Horizontal);
    splitter->setChildrenCollapsible(false);
    splitter->setHandleWidth(14);
    splitter->addWidget(buildCircuitPanel());
    splitter->addWidget(buildResultsPanel());
    splitter->setStretchFactor(0, 4);
    splitter->setStretchFactor(1, 5);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(14);
    layout->addWidget(buildInputPanel());
    layout->addWidget(splitter, 1);

    setResistorFieldCount(kDefaultResistors);
    m_circuit->setResistorCount(kDefaultResistors);
    clearResults();
}

// ---------------------------------------------------------------------------
// UI construction
// ---------------------------------------------------------------------------

QWidget *DividerPage::buildInputPanel()
{
    QFrame *panel = Ui::createPanel(QStringLiteral("inputPanel"));
    panel->setFixedWidth(310);

    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(8);

    layout->addWidget(Ui::createLabel(tr("INPUT"), QStringLiteral("sectionTitle")));
    layout->addSpacing(6);

    // Number of resistors: spin box with explicit -/+ buttons.
    layout->addWidget(Ui::createLabel(tr("Number of resistors"), QStringLiteral("fieldLabel")));
    m_countSpin = new QSpinBox;
    m_countSpin->setObjectName(QStringLiteral("countSpin"));
    m_countSpin->setRange(kMinResistors, kMaxResistors);
    m_countSpin->setValue(kDefaultResistors);
    m_countSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_countSpin->setAlignment(Qt::AlignCenter);
    m_countSpin->setToolTip(tr("%1 to %2 resistors").arg(kMinResistors).arg(kMaxResistors));

    auto *minusButton = new QToolButton;
    minusButton->setText(QStringLiteral("−"));
    minusButton->setObjectName(QStringLiteral("stepButton"));
    minusButton->setAutoRepeat(true);
    auto *plusButton = new QToolButton;
    plusButton->setText(QStringLiteral("+"));
    plusButton->setObjectName(QStringLiteral("stepButton"));
    plusButton->setAutoRepeat(true);

    auto *countRow = new QHBoxLayout;
    countRow->setSpacing(8);
    countRow->addWidget(minusButton);
    countRow->addWidget(m_countSpin, 1);
    countRow->addWidget(plusButton);
    layout->addLayout(countRow);
    layout->addSpacing(8);

    // Source voltage.
    layout->addWidget(Ui::createLabel(tr("Source voltage"), QStringLiteral("fieldLabel")));
    m_voltageEdit = Ui::createNumberEdit(tr("e.g. 12"));
    m_voltageEdit->setObjectName(QStringLiteral("voltageEdit"));
    auto *voltageRow = new QHBoxLayout;
    voltageRow->setSpacing(8);
    voltageRow->addWidget(m_voltageEdit, 1);
    QLabel *voltUnit = Ui::createLabel(QStringLiteral("V"), QStringLiteral("unitLabel"));
    voltUnit->setFixedWidth(18);
    voltageRow->addWidget(voltUnit);
    layout->addLayout(voltageRow);
    layout->addSpacing(8);

    // Dynamically generated resistor fields, scrollable for long chains.
    layout->addWidget(Ui::createLabel(tr("Resistor values"), QStringLiteral("fieldLabel")));
    auto *resistorHost = new QWidget;
    resistorHost->setObjectName(QStringLiteral("transparentHost"));
    m_resistorLayout = new QVBoxLayout(resistorHost);
    m_resistorLayout->setContentsMargins(0, 0, 6, 0);
    m_resistorLayout->setSpacing(8);
    m_resistorLayout->addStretch(1);

    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setWidget(resistorHost);
    layout->addWidget(scroll, 1);
    layout->addSpacing(8);

    auto *calculateButton = new QPushButton(tr("Calculate"));
    calculateButton->setObjectName(QStringLiteral("calculateButton"));
    calculateButton->setCursor(Qt::PointingHandCursor);
    calculateButton->setDefault(true);
    auto *clearButton = new QPushButton(tr("Clear"));
    clearButton->setObjectName(QStringLiteral("clearButton"));
    clearButton->setCursor(Qt::PointingHandCursor);

    auto *buttonRow = new QHBoxLayout;
    buttonRow->setSpacing(10);
    buttonRow->addWidget(clearButton, 1);
    buttonRow->addWidget(calculateButton, 2);
    layout->addLayout(buttonRow);

    connect(minusButton, &QToolButton::clicked, m_countSpin, &QSpinBox::stepDown);
    connect(plusButton, &QToolButton::clicked, m_countSpin, &QSpinBox::stepUp);
    connect(m_countSpin, &QSpinBox::valueChanged, this, &DividerPage::onResistorCountChanged);
    connect(m_voltageEdit, &QLineEdit::returnPressed, this, &DividerPage::onCalculate);
    connect(calculateButton, &QPushButton::clicked, this, &DividerPage::onCalculate);
    connect(clearButton, &QPushButton::clicked, this, &DividerPage::onClear);

    return panel;
}

QWidget *DividerPage::buildCircuitPanel()
{
    QFrame *panel = Ui::createPanel(QStringLiteral("panel"));
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(18, 18, 18, 12);
    layout->setSpacing(8);
    layout->addWidget(Ui::createLabel(tr("CIRCUIT"), QStringLiteral("sectionTitle")));

    m_circuit = new CircuitWidget;
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setWidget(m_circuit);
    layout->addWidget(scroll, 1);

    panel->setMinimumWidth(400);
    return panel;
}

QWidget *DividerPage::buildResultsPanel()
{
    QFrame *panel = Ui::createPanel(QStringLiteral("panel"));
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(12);
    layout->addWidget(Ui::createLabel(tr("RESULTS"), QStringLiteral("sectionTitle")));

    auto *cards = new QHBoxLayout;
    cards->setSpacing(10);
    cards->addWidget(Ui::createSummaryCard(tr("Total resistance"), m_totalResistanceValue, m_totalResistanceDetail));
    cards->addWidget(Ui::createSummaryCard(tr("Circuit current"), m_currentValue, m_currentDetail));
    cards->addWidget(Ui::createSummaryCard(tr("Total power"), m_powerValue, m_powerDetail));
    layout->addLayout(cards);

    m_table = new QTableWidget(0, 4);
    // Two-line headers keep the columns readable when the panel is narrow.
    m_table->setHorizontalHeaderLabels({tr("Resistor"),
                                        tr("Resistance\n(%1)").arg(kOhm),
                                        tr("Voltage Drop\n(V)"),
                                        tr("Remaining\nVoltage (V)")});
    for (int column = 0; column < 4; ++column) {
        m_table->horizontalHeaderItem(column)->setTextAlignment(
            (column == 0 ? Qt::AlignLeft : Qt::AlignRight) | Qt::AlignVCenter);
    }
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->horizontalHeader()->setHighlightSections(false);
    m_table->horizontalHeader()->setMinimumSectionSize(90);
    m_table->verticalHeader()->setVisible(false);
    m_table->verticalHeader()->setDefaultSectionSize(36);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setAlternatingRowColors(true);
    m_table->setShowGrid(false);
    m_table->setWordWrap(false);
    m_table->setFocusPolicy(Qt::NoFocus);
    layout->addWidget(m_table, 1);

    QLabel *note = Ui::createLabel(tr("Remaining voltage is the node voltage after each resistor, "
                                  "measured against ground."),
                               QStringLiteral("cardDetail"));
    note->setWordWrap(true);
    layout->addWidget(note);

    panel->setMinimumWidth(480);
    return panel;
}

// ---------------------------------------------------------------------------
// Dynamic resistor fields
// ---------------------------------------------------------------------------

void DividerPage::setResistorFieldCount(int count)
{
    // Remove surplus rows from the end; values in the remaining rows are kept.
    while (m_resistorRows.size() > count) {
        const ResistorRow row = m_resistorRows.takeLast();
        m_resistorLayout->removeWidget(row.container);
        row.container->deleteLater();
    }

    while (m_resistorRows.size() < count) {
        const int index = m_resistorRows.size() + 1;

        ResistorRow row;
        row.container = new QWidget;
        row.container->setObjectName(QStringLiteral("transparentHost"));
        auto *rowLayout = new QHBoxLayout(row.container);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(8);

        QLabel *name = Ui::createLabel(QStringLiteral("R%1").arg(index), QStringLiteral("resistorName"));
        name->setFixedWidth(34);
        row.edit = Ui::createNumberEdit(tr("Resistor R%1").arg(index));
        row.edit->setObjectName(QStringLiteral("resistorEdit%1").arg(index));
        row.edit->setAccessibleName(tr("Resistor R%1 (%2)").arg(index).arg(kOhm));
        QLabel *unit = Ui::createLabel(kOhm, QStringLiteral("unitLabel"));
        unit->setFixedWidth(18);

        rowLayout->addWidget(name);
        rowLayout->addWidget(row.edit, 1);
        rowLayout->addWidget(unit);

        connect(row.edit, &QLineEdit::returnPressed, this, &DividerPage::onCalculate);

        // Insert before the trailing stretch item.
        m_resistorLayout->insertWidget(m_resistorLayout->count() - 1, row.container);
        m_resistorRows.append(row);
    }

    // Keep keyboard navigation in visual order: voltage -> R1 -> ... -> Rn.
    QWidget *previous = m_voltageEdit;
    for (const ResistorRow &row : std::as_const(m_resistorRows)) {
        setTabOrder(previous, row.edit);
        previous = row.edit;
    }
}

// ---------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------

void DividerPage::onResistorCountChanged(int count)
{
    setResistorFieldCount(count);
    // The previous result no longer matches the circuit, so drop it.
    clearResults();
    m_circuit->setResistorCount(count);
}

void DividerPage::onCalculate()
{
    if (m_resistorRows.isEmpty()) {
        QMessageBox::warning(this, tr("Invalid input"),
                             tr("The number of resistors must be greater than zero."));
        return;
    }

    double voltage = 0.0;
    if (!Ui::readPositiveValue(this, m_voltageEdit, tr("Source voltage"), voltage))
        return;

    std::vector<double> resistances;
    resistances.reserve(static_cast<std::size_t>(m_resistorRows.size()));
    for (int i = 0; i < m_resistorRows.size(); ++i) {
        double resistance = 0.0;
        if (!Ui::readPositiveValue(this, m_resistorRows[i].edit, tr("Resistor R%1").arg(i + 1), resistance))
            return;
        resistances.push_back(resistance);
    }

    try {
        showResult(ResistorCalculator::calculate(voltage, resistances));
    } catch (const std::exception &error) {
        QMessageBox::warning(this, tr("Calculation error"), QString::fromUtf8(error.what()));
    }
}

void DividerPage::onClear()
{
    m_voltageEdit->clear();
    for (const ResistorRow &row : std::as_const(m_resistorRows))
        row.edit->clear();

    if (m_countSpin->value() != kDefaultResistors) {
        m_countSpin->setValue(kDefaultResistors);   // triggers onResistorCountChanged
    } else {
        clearResults();
        m_circuit->setResistorCount(kDefaultResistors);
    }
    m_voltageEdit->setFocus();
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void DividerPage::showResult(const DividerResult &result)
{
    // Headline value with an SI prefix, exact base-unit value underneath.
    m_totalResistanceValue->setText(Format::si(result.totalResistance, kOhm));
    m_totalResistanceDetail->setText(Format::number(result.totalResistance) + QLatin1Char(' ') + kOhm);
    m_currentValue->setText(Format::si(result.current, QStringLiteral("A")));
    m_currentDetail->setText(Format::number(result.current) + QStringLiteral(" A"));
    m_powerValue->setText(Format::si(result.totalPower, QStringLiteral("W")));
    m_powerDetail->setText(Format::number(result.totalPower) + QStringLiteral(" W"));

    const int rows = static_cast<int>(result.resistances.size());
    m_table->setRowCount(rows);
    for (int row = 0; row < rows; ++row) {
        const std::size_t i = static_cast<std::size_t>(row);
        const QString cells[] = {
            QStringLiteral("R%1").arg(row + 1),
            Format::number(result.resistances[i]),
            Format::number(result.voltageDrops[i]),
            Format::number(result.nodeVoltages[i + 1]),
        };
        for (int column = 0; column < 4; ++column) {
            auto *item = new QTableWidgetItem(cells[column]);
            item->setTextAlignment((column == 0 ? Qt::AlignLeft : Qt::AlignRight) | Qt::AlignVCenter);
            m_table->setItem(row, column, item);
        }
    }

    m_circuit->setResult(result);
}

void DividerPage::clearResults()
{
    for (QLabel *label : {m_totalResistanceValue, m_currentValue, m_powerValue})
        label->setText(kNoValue);
    for (QLabel *label : {m_totalResistanceDetail, m_currentDetail, m_powerDetail})
        label->setText(QStringLiteral(" "));
    m_table->setRowCount(0);
    m_circuit->clearResult();
}
