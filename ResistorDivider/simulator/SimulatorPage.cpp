#include "SimulatorPage.h"

#include "CircuitView.h"
#include "ElementDialog.h"
#include "ExampleCircuits.h"
#include "Format.h"
#include "ScopePanel.h"

#include <QActionGroup>
#include <QElapsedTimer>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QSlider>
#include <QSplitter>
#include <QTimer>
#include <QToolBar>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <cmath>

namespace {

const int kFrameIntervalMs = 16;
const int kFrameBudgetMs = 12;      // simulation time allowed per frame
const int kCodeRole = Qt::UserRole;

// Palette tree whose items can be dragged onto the canvas.
class PaletteTree : public QTreeWidget
{
public:
    using QTreeWidget::QTreeWidget;

protected:
    QStringList mimeTypes() const override { return {CircuitView::toolMimeType()}; }

    QMimeData *mimeData(const QList<QTreeWidgetItem *> &items) const override
    {
        if (items.isEmpty())
            return nullptr;
        const QString code = items.first()->data(0, kCodeRole).toString();
        if (code.isEmpty())
            return nullptr;
        auto *data = new QMimeData;
        data->setData(CircuitView::toolMimeType(), code.toUtf8());
        return data;
    }
};

QLabel *toolbarLabel(const QString &text)
{
    auto *label = new QLabel(text);
    label->setObjectName(QStringLiteral("toolbarLabel"));
    return label;
}

} // namespace

SimulatorPage::SimulatorPage(QWidget *parent)
    : QWidget(parent)
{
    m_view = new CircuitView(&m_circuit);
    m_scopes = new ScopePanel(&m_circuit);
    m_scopes->hide();

    createActions();
    m_palette = createPalette();

    auto *splitter = new QSplitter(Qt::Vertical);
    splitter->setChildrenCollapsible(false);
    splitter->setHandleWidth(8);
    splitter->addWidget(m_view);
    splitter->addWidget(m_scopes);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 0);

    auto *body = new QHBoxLayout;
    body->setContentsMargins(8, 8, 8, 8);
    body->setSpacing(8);
    body->addWidget(m_palette);
    body->addWidget(splitter, 1);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(createMenuBar());
    layout->addWidget(createToolBar());
    layout->addLayout(body, 1);

    m_timer = new QTimer(this);
    m_timer->setInterval(kFrameIntervalMs);
    connect(m_timer, &QTimer::timeout, this, &SimulatorPage::onFrame);

    connect(m_view, &CircuitView::editRequested, this, &SimulatorPage::editElement);
    connect(m_view, &CircuitView::scopeRequested, m_scopes, &ScopePanel::addScope);
    connect(m_view, &CircuitView::selectionChanged, this, &SimulatorPage::updateActions);
    connect(m_view, &CircuitView::circuitChanged, this, &SimulatorPage::updateActions);
    connect(m_view, &CircuitView::toolChanged, this, &SimulatorPage::onToolChanged);
    connect(m_scopes, &ScopePanel::countChanged, this, [this](int count) {
        m_scopes->setVisible(count > 0);
        updateActions();
    });

    loadText(exampleCircuits().value(1).text);     // RC low-pass filter
}

// ---------------------------------------------------------------------------
// Actions, menus, toolbar, palette
// ---------------------------------------------------------------------------

void SimulatorPage::createActions()
{
    // Shortcuts are limited to this tab so they do not fire in the other calculators.
    auto make = [this](const QString &text, const QKeySequence &shortcut = QKeySequence()) {
        auto *action = new QAction(text, this);
        if (!shortcut.isEmpty()) {
            action->setShortcut(shortcut);
            action->setShortcutContext(Qt::WidgetWithChildrenShortcut);
        }
        addAction(action);
        return action;
    };

    m_runAction = make(tr("Pause"), Qt::Key_Space);
    m_runAction->setCheckable(true);
    m_runAction->setChecked(true);
    m_runAction->setToolTip(tr("Run / pause the simulation (Space)"));
    m_resetAction = make(tr("Reset"), QKeySequence(Qt::CTRL | Qt::Key_R));
    m_resetAction->setToolTip(tr("Restart the simulation from t = 0"));
    m_stepAction = make(tr("Step"), Qt::Key_Period);
    m_stepAction->setToolTip(tr("Advance one time step while paused (.)"));

    m_newAction = make(tr("New"), QKeySequence::New);
    m_openAction = make(tr("Open…"), QKeySequence::Open);
    m_saveAction = make(tr("Save As…"), QKeySequence::Save);

    m_undoAction = make(tr("Undo"), QKeySequence::Undo);
    m_redoAction = make(tr("Redo"), QKeySequence::Redo);
    m_cutAction = make(tr("Cut"), QKeySequence::Cut);
    m_copyAction = make(tr("Copy"), QKeySequence::Copy);
    m_pasteAction = make(tr("Paste"), QKeySequence::Paste);
    m_duplicateAction = make(tr("Duplicate"), QKeySequence(Qt::CTRL | Qt::Key_D));
    m_deleteAction = make(tr("Delete"), QKeySequence::Delete);
    m_selectAllAction = make(tr("Select All"), QKeySequence::SelectAll);
    m_rotateAction = make(tr("Rotate"), QKeySequence(Qt::CTRL | Qt::Key_T));
    m_rotateAction->setToolTip(tr("Rotate the selection by 90°"));
    m_flipAction = make(tr("Flip"), QKeySequence(Qt::CTRL | Qt::Key_F));
    m_flipAction->setToolTip(tr("Mirror the selected transistor or op-amp"));
    m_editAction = make(tr("Edit Properties…"), QKeySequence(Qt::CTRL | Qt::Key_E));

    m_scopeAction = make(tr("View Selection in Scope"), QKeySequence(Qt::CTRL | Qt::Key_G));
    m_scopeFasterAction = make(tr("Shorter Time Window"));
    m_scopeSlowerAction = make(tr("Longer Time Window"));
    m_scopeClearAction = make(tr("Remove All Scopes"));

    m_zoomInAction = make(tr("Zoom In"), QKeySequence(Qt::CTRL | Qt::Key_Equal));
    m_zoomOutAction = make(tr("Zoom Out"), QKeySequence(Qt::CTRL | Qt::Key_Minus));
    m_zoomFitAction = make(tr("Zoom to Fit"), QKeySequence(Qt::CTRL | Qt::Key_0));
    // Shorter captions for the toolbar buttons.
    m_zoomInAction->setIconText(tr("Zoom +"));
    m_zoomOutAction->setIconText(tr("Zoom −"));
    m_zoomFitAction->setIconText(tr("Fit"));

    auto makeCheck = [&make](const QString &text, bool checked) {
        QAction *action = make(text);
        action->setCheckable(true);
        action->setChecked(checked);
        return action;
    };
    m_showCurrentAction = makeCheck(tr("Show Current"), true);
    m_showVoltageAction = makeCheck(tr("Show Voltage Colours"), true);
    m_showValuesAction = makeCheck(tr("Show Values"), true);
    m_showGridAction = makeCheck(tr("Show Grid"), true);
    m_timeStepAction = make(tr("Time Step…"));

    connect(m_runAction, &QAction::toggled, this, &SimulatorPage::setRunning);
    connect(m_resetAction, &QAction::triggered, this, &SimulatorPage::resetSimulation);
    connect(m_stepAction, &QAction::triggered, this, &SimulatorPage::singleStep);
    connect(m_newAction, &QAction::triggered, this, &SimulatorPage::newCircuit);
    connect(m_openAction, &QAction::triggered, this, &SimulatorPage::openFile);
    connect(m_saveAction, &QAction::triggered, this, &SimulatorPage::saveFile);

    connect(m_undoAction, &QAction::triggered, m_view, &CircuitView::undo);
    connect(m_redoAction, &QAction::triggered, m_view, &CircuitView::redo);
    connect(m_cutAction, &QAction::triggered, m_view, &CircuitView::cut);
    connect(m_copyAction, &QAction::triggered, m_view, &CircuitView::copy);
    connect(m_pasteAction, &QAction::triggered, m_view, &CircuitView::paste);
    connect(m_duplicateAction, &QAction::triggered, m_view, &CircuitView::duplicate);
    connect(m_deleteAction, &QAction::triggered, m_view, &CircuitView::deleteSelection);
    connect(m_selectAllAction, &QAction::triggered, m_view, &CircuitView::selectAll);
    connect(m_rotateAction, &QAction::triggered, m_view, &CircuitView::rotateSelection);
    connect(m_flipAction, &QAction::triggered, m_view, &CircuitView::flipSelection);
    connect(m_editAction, &QAction::triggered, this, &SimulatorPage::editSelected);

    connect(m_scopeAction, &QAction::triggered, this, &SimulatorPage::scopeSelected);
    connect(m_scopeFasterAction, &QAction::triggered, m_scopes, &ScopePanel::faster);
    connect(m_scopeSlowerAction, &QAction::triggered, m_scopes, &ScopePanel::slower);
    connect(m_scopeClearAction, &QAction::triggered, m_scopes, &ScopePanel::removeAll);

    connect(m_zoomInAction, &QAction::triggered, m_view, &CircuitView::zoomIn);
    connect(m_zoomOutAction, &QAction::triggered, m_view, &CircuitView::zoomOut);
    connect(m_zoomFitAction, &QAction::triggered, m_view, &CircuitView::zoomToFit);

    connect(m_showCurrentAction, &QAction::toggled, this, [this](bool on) {
        m_view->options.showCurrent = on;
        m_view->update();
    });
    connect(m_showVoltageAction, &QAction::toggled, this, [this](bool on) {
        m_view->options.showVoltage = on;
        m_view->update();
    });
    connect(m_showValuesAction, &QAction::toggled, this, [this](bool on) {
        m_view->options.showValues = on;
        m_view->update();
    });
    connect(m_showGridAction, &QAction::toggled, this, [this](bool on) {
        m_view->showGrid = on;
        m_view->update();
    });
    connect(m_timeStepAction, &QAction::triggered, this, &SimulatorPage::changeTimeStep);

    // One checkable action per placement tool, plus the select tool.
    m_toolGroup = new QActionGroup(this);
    m_selectToolAction = make(tr("Select / Move"), Qt::Key_Escape);
    m_selectToolAction->setCheckable(true);
    m_selectToolAction->setChecked(true);
    m_toolGroup->addAction(m_selectToolAction);
    connect(m_selectToolAction, &QAction::triggered, this, [this] { m_view->setTool(QString()); });

    for (const ToolInfo &tool : Element::tools()) {
        QAction *action = make(tool.label, tool.shortcut.isEmpty() ? QKeySequence()
                                                                   : QKeySequence(tool.shortcut));
        action->setCheckable(true);
        m_toolGroup->addAction(action);
        m_toolActions.insert(tool.code, action);
        const QString code = tool.code;
        connect(action, &QAction::triggered, this, [this, code] { m_view->setTool(code); });
    }
}

QMenuBar *SimulatorPage::createMenuBar()
{
    auto *bar = new QMenuBar;
    bar->setNativeMenuBar(false);   // the menu belongs to this tab, not to the window

    QMenu *file = bar->addMenu(tr("File"));
    file->addActions({m_newAction, m_openAction, m_saveAction});

    QMenu *edit = bar->addMenu(tr("Edit"));
    edit->addActions({m_undoAction, m_redoAction});
    edit->addSeparator();
    edit->addActions({m_cutAction, m_copyAction, m_pasteAction, m_duplicateAction, m_deleteAction,
                      m_selectAllAction});
    edit->addSeparator();
    edit->addActions({m_rotateAction, m_flipAction, m_editAction});

    QMenu *draw = bar->addMenu(tr("Draw"));
    draw->addAction(m_selectToolAction);
    draw->addSeparator();
    QMenu *category = nullptr;
    QString categoryName;
    for (const ToolInfo &tool : Element::tools()) {
        if (tool.category != categoryName) {
            categoryName = tool.category;
            category = draw->addMenu(categoryName);
        }
        category->addAction(m_toolActions.value(tool.code));
    }

    QMenu *scopes = bar->addMenu(tr("Scopes"));
    scopes->addAction(m_scopeAction);
    scopes->addSeparator();
    scopes->addActions({m_scopeFasterAction, m_scopeSlowerAction});
    scopes->addSeparator();
    scopes->addAction(m_scopeClearAction);

    QMenu *options = bar->addMenu(tr("Options"));
    options->addActions({m_showCurrentAction, m_showVoltageAction, m_showValuesAction, m_showGridAction});
    options->addSeparator();
    options->addActions({m_zoomInAction, m_zoomOutAction, m_zoomFitAction});
    options->addSeparator();
    options->addAction(m_timeStepAction);

    QMenu *circuits = bar->addMenu(tr("Circuits"));
    const QList<ExampleCircuit> &examples = exampleCircuits();
    for (const ExampleCircuit &example : examples) {
        const QString text = example.text;
        circuits->addAction(example.title, this, [this, text] { loadText(text); });
    }
    return bar;
}

QToolBar *SimulatorPage::createToolBar()
{
    auto *bar = new QToolBar;
    bar->setMovable(false);
    bar->setToolButtonStyle(Qt::ToolButtonTextOnly);

    bar->addActions({m_runAction, m_resetAction, m_stepAction});
    bar->addSeparator();
    bar->addActions({m_undoAction, m_redoAction});
    bar->addSeparator();
    bar->addActions({m_rotateAction, m_flipAction, m_deleteAction});
    bar->addSeparator();
    bar->addActions({m_zoomOutAction, m_zoomInAction, m_zoomFitAction});
    bar->addSeparator();

    m_speedSlider = new QSlider(Qt::Horizontal);
    m_speedSlider->setRange(0, 100);
    m_speedSlider->setValue(55);
    m_speedSlider->setFixedWidth(120);
    m_speedSlider->setToolTip(tr("Simulation speed: time steps computed per frame"));
    bar->addWidget(toolbarLabel(tr("Speed")));
    bar->addWidget(m_speedSlider);

    m_currentSlider = new QSlider(Qt::Horizontal);
    m_currentSlider->setRange(0, 100);
    m_currentSlider->setValue(50);
    m_currentSlider->setFixedWidth(120);
    m_currentSlider->setToolTip(tr("How fast the current dots move for a given current"));
    bar->addWidget(toolbarLabel(tr("Current")));
    bar->addWidget(m_currentSlider);

    return bar;
}

QTreeWidget *SimulatorPage::createPalette()
{
    auto *tree = new PaletteTree;
    tree->setObjectName(QStringLiteral("palette"));
    tree->setHeaderHidden(true);
    tree->setFixedWidth(196);
    tree->setIndentation(14);
    tree->setFocusPolicy(Qt::NoFocus);      // keep keyboard shortcuts on the canvas
    tree->setDragEnabled(true);
    tree->setDragDropMode(QAbstractItemView::DragOnly);
    tree->setSelectionMode(QAbstractItemView::SingleSelection);

    auto *select = new QTreeWidgetItem(tree, {tr("Select / Move      Esc")});
    select->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);

    QTreeWidgetItem *category = nullptr;
    for (const ToolInfo &tool : Element::tools()) {
        if (!category || category->text(0) != tool.category) {
            category = new QTreeWidgetItem(tree, {tool.category});
            category->setFlags(Qt::ItemIsEnabled);
            QFont font = category->font(0);
            font.setBold(true);
            category->setFont(0, font);
        }
        QString text = tool.label;
        if (!tool.shortcut.isEmpty())
            text += QStringLiteral("  (%1)").arg(tool.shortcut);
        auto *item = new QTreeWidgetItem(category, {text});
        item->setData(0, kCodeRole, tool.code);
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled);
        item->setToolTip(0, tr("Click, then drag on the canvas - or drag this entry onto the canvas"));
    }
    tree->expandAll();
    tree->setCurrentItem(select);

    connect(tree, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem *item) {
        if (item->childCount() == 0)
            m_view->setTool(item->data(0, kCodeRole).toString());
    });
    return tree;
}

// ---------------------------------------------------------------------------
// Simulation loop
// ---------------------------------------------------------------------------

void SimulatorPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    m_timer->start();
    m_view->setFocus();
    if (!m_shownOnce) {
        // The canvas only has its real size now.
        m_shownOnce = true;
        QTimer::singleShot(0, m_view, &CircuitView::zoomToFit);
    }
}

void SimulatorPage::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    m_timer->stop();    // nothing is simulated while another tab is in front
}

void SimulatorPage::runSteps(int steps)
{
    QElapsedTimer clock;
    clock.start();
    for (int i = 0; i < steps; ++i) {
        if (!m_circuit.step())
            break;
        m_scopes->sample();
        if ((i & 15) == 15 && clock.elapsed() > kFrameBudgetMs)
            break;
    }
}

void SimulatorPage::onFrame()
{
    const bool running = m_runAction->isChecked();
    if (running) {
        // Slider 0..100 maps logarithmically to 1..3000 steps per frame.
        const int steps = static_cast<int>(std::lround(std::exp(m_speedSlider->value() / 100.0 * std::log(3000.0))));
        runSteps(steps);
    } else {
        // Paused: still pick up edits so connection problems are reported.
        m_circuit.ensureAnalyzed();
    }

    m_circuit.updateWireCurrents();
    // Dot travel in pixels per frame = current x multiplier. Currents too
    // small to move a dot visibly are drawn without dots.
    const double multiplier = 2.0 * std::pow(10.0, m_currentSlider->value() / 25.0);
    m_view->options.minDotCurrent = 0.004 / multiplier;
    if (running && m_circuit.error.isEmpty()) {
        for (const auto &element : m_circuit.elements) {
            for (int k = 0; k < element->dotChannels(); ++k) {
                const double travel = qBound(-8.0, element->dotCurrent(k) * multiplier, 8.0);
                element->dotPhase[k] = std::fmod(element->dotPhase[k] + travel, 16.0);
            }
        }
    }

    refreshStatus();
    m_view->update();
    if (m_scopes->isVisible())
        m_scopes->update();
}

void SimulatorPage::refreshStatus()
{
    // Shown in the top-right corner of the canvas.
    m_view->statusText = tr("t = %1    time step %2")
                             .arg(Format::si(m_circuit.time, QStringLiteral("s")),
                                  Format::si(m_circuit.timeStep, QStringLiteral("s"), 3));
}

void SimulatorPage::setRunning(bool running)
{
    m_runAction->setText(running ? tr("Pause") : tr("Run"));
    updateActions();
}

void SimulatorPage::resetSimulation()
{
    m_circuit.resetState();
    m_scopes->clearData();
    m_view->update();
}

void SimulatorPage::singleStep()
{
    if (m_runAction->isChecked())
        m_runAction->setChecked(false);
    runSteps(1);
}

// ---------------------------------------------------------------------------
// Files
// ---------------------------------------------------------------------------

QString SimulatorPage::saveText() const
{
    QString text = m_circuit.serialize();
    for (int id : m_scopes->elementIds()) {
        const int index = m_circuit.indexOf(m_circuit.find(id));
        if (index >= 0)
            text += QStringLiteral("o %1\n").arg(index);
    }
    return text;
}

void SimulatorPage::loadText(const QString &text)
{
    m_scopes->removeAll();
    m_circuit.deserialize(text);
    m_circuit.resetState();
    m_view->clearHistory();

    const QStringList lines = text.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        if (!line.startsWith(QLatin1String("o ")))
            continue;
        bool ok = false;
        const int index = line.mid(2).trimmed().toInt(&ok);
        if (ok && index >= 0 && index < static_cast<int>(m_circuit.elements.size()))
            m_scopes->addScope(m_circuit.elements[index]->id);
    }

    m_view->setTool(QString());
    // Fit once the layout has settled (the scope strip may just have appeared).
    QTimer::singleShot(0, m_view, &CircuitView::zoomToFit);
    updateActions();
}

void SimulatorPage::newCircuit()
{
    m_scopes->removeAll();
    m_view->pushUndo();
    m_circuit.clear();
    m_circuit.resetState();
    m_view->zoomToFit();
    updateActions();
}

void SimulatorPage::openFile()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("Open Circuit"), QString(),
                                                      tr("Circuit files (*.circuit *.txt);;All files (*)"));
    if (path.isEmpty())
        return;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Open Circuit"), tr("Cannot read %1:\n%2").arg(path, file.errorString()));
        return;
    }
    const QString text = QString::fromUtf8(file.readAll());
    loadText(text);
    if (m_circuit.elements.empty() && !text.trimmed().isEmpty())
        QMessageBox::warning(this, tr("Open Circuit"), tr("No circuit elements were found in %1.").arg(path));
}

void SimulatorPage::saveFile()
{
    QString path = QFileDialog::getSaveFileName(this, tr("Save Circuit"), QStringLiteral("circuit.circuit"),
                                                tr("Circuit files (*.circuit);;All files (*)"));
    if (path.isEmpty())
        return;
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)
        || file.write(saveText().toUtf8()) < 0) {
        QMessageBox::warning(this, tr("Save Circuit"), tr("Cannot write %1:\n%2").arg(path, file.errorString()));
    }
}

// ---------------------------------------------------------------------------
// Editing helpers
// ---------------------------------------------------------------------------

void SimulatorPage::editElement(int elementId)
{
    Element *element = m_circuit.find(elementId);
    if (!element || element->params().isEmpty())
        return;

    // The simulation keeps running behind the dialog, so "Apply" shows the
    // effect of a new value immediately.
    ElementDialog dialog(element, this);
    connect(&dialog, &ElementDialog::aboutToApply, m_view, &CircuitView::pushUndo);
    connect(&dialog, &ElementDialog::applied, this, [this] {
        m_circuit.markDirty();
        m_view->update();
        updateActions();
    });
    dialog.exec();
}

void SimulatorPage::editSelected()
{
    const QList<Element *> selected = m_view->selection();
    if (selected.size() == 1)
        editElement(selected.first()->id);
}

void SimulatorPage::scopeSelected()
{
    for (const Element *element : m_view->selection()) {
        if (!element->isWire() && !element->isGround())
            m_scopes->addScope(element->id);
    }
}

void SimulatorPage::changeTimeStep()
{
    bool ok = false;
    const QString text = QInputDialog::getText(
        this, tr("Time Step"),
        tr("Simulation time step in seconds (for example 5u, 1u, 50n).\n"
           "Smaller steps are more accurate but slower."),
        QLineEdit::Normal, Format::si(m_circuit.timeStep, QString(), 6).trimmed(), &ok);
    if (!ok)
        return;
    double value = 0.0;
    if (!ElementDialog::parseValue(text, value) || value <= 0.0 || value > 1.0) {
        QMessageBox::warning(this, tr("Invalid input"),
                             tr("The time step must be a positive number of at most 1 second."));
        return;
    }
    m_circuit.timeStep = value;
    m_circuit.markDirty();
    m_scopes->clearData();
}

void SimulatorPage::updateActions()
{
    const QList<Element *> selected = m_view->selection();
    const bool any = !selected.isEmpty();
    bool flippable = false;
    for (const Element *element : selected)
        flippable = flippable || element->canFlip();

    m_undoAction->setEnabled(m_view->canUndo());
    m_redoAction->setEnabled(m_view->canRedo());
    m_cutAction->setEnabled(any);
    m_copyAction->setEnabled(any);
    m_duplicateAction->setEnabled(any);
    m_deleteAction->setEnabled(any);
    m_rotateAction->setEnabled(any);
    m_flipAction->setEnabled(flippable);
    m_editAction->setEnabled(selected.size() == 1 && !selected.first()->params().isEmpty());
    m_scopeAction->setEnabled(any);
    m_stepAction->setEnabled(!m_runAction->isChecked());

    const bool scopes = m_scopes->count() > 0;
    m_scopeFasterAction->setEnabled(scopes);
    m_scopeSlowerAction->setEnabled(scopes);
    m_scopeClearAction->setEnabled(scopes);
}

void SimulatorPage::onToolChanged(const QString &code)
{
    QAction *action = code.isEmpty() ? m_selectToolAction : m_toolActions.value(code);
    if (action)
        action->setChecked(true);

    // Mirror the tool in the palette.
    QTreeWidgetItemIterator it(m_palette);
    while (*it) {
        const bool isSelectEntry = (*it)->parent() == nullptr && (*it)->childCount() == 0;
        if ((code.isEmpty() && isSelectEntry) || (!code.isEmpty() && (*it)->data(0, kCodeRole).toString() == code)) {
            m_palette->setCurrentItem(*it);
            break;
        }
        ++it;
    }
}
