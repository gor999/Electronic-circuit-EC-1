#pragma once

#include "Circuit.h"

#include <QHash>
#include <QString>
#include <QWidget>

class CircuitView;
class QAction;
class QActionGroup;
class QLabel;
class QMenu;
class QMenuBar;
class QSlider;
class QTimer;
class QToolBar;
class QTreeWidget;
class ScopePanel;

// Circuit simulator tab: owns the Circuit, runs the simulation loop and wires
// the menu bar, toolbar, component palette, canvas and scopes together.
class SimulatorPage : public QWidget
{
    Q_OBJECT

public:
    explicit SimulatorPage(QWidget *parent = nullptr);

    // Replaces the circuit with one in the saved-file text format.
    void loadText(const QString &text);
    QString saveText() const;

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private slots:
    void onFrame();
    void setRunning(bool running);
    void resetSimulation();
    void singleStep();
    void newCircuit();
    void openFile();
    void saveFile();
    void editElement(int elementId);
    void editSelected();
    void scopeSelected();
    void changeTimeStep();
    void updateActions();
    void onToolChanged(const QString &code);

private:
    void createActions();
    QMenuBar *createMenuBar();
    QToolBar *createToolBar();
    QTreeWidget *createPalette();
    void runSteps(int steps);
    void refreshStatus();

    Circuit m_circuit;
    CircuitView *m_view = nullptr;
    ScopePanel *m_scopes = nullptr;
    QTreeWidget *m_palette = nullptr;
    QTimer *m_timer = nullptr;

    QSlider *m_speedSlider = nullptr;
    QSlider *m_currentSlider = nullptr;

    bool m_shownOnce = false;

    QAction *m_runAction = nullptr;
    QAction *m_resetAction = nullptr;
    QAction *m_stepAction = nullptr;
    QAction *m_newAction = nullptr;
    QAction *m_openAction = nullptr;
    QAction *m_saveAction = nullptr;
    QAction *m_undoAction = nullptr;
    QAction *m_redoAction = nullptr;
    QAction *m_cutAction = nullptr;
    QAction *m_copyAction = nullptr;
    QAction *m_pasteAction = nullptr;
    QAction *m_duplicateAction = nullptr;
    QAction *m_deleteAction = nullptr;
    QAction *m_selectAllAction = nullptr;
    QAction *m_rotateAction = nullptr;
    QAction *m_flipAction = nullptr;
    QAction *m_editAction = nullptr;
    QAction *m_scopeAction = nullptr;
    QAction *m_scopeFasterAction = nullptr;
    QAction *m_scopeSlowerAction = nullptr;
    QAction *m_scopeClearAction = nullptr;
    QAction *m_zoomInAction = nullptr;
    QAction *m_zoomOutAction = nullptr;
    QAction *m_zoomFitAction = nullptr;
    QAction *m_showCurrentAction = nullptr;
    QAction *m_showVoltageAction = nullptr;
    QAction *m_showValuesAction = nullptr;
    QAction *m_showGridAction = nullptr;
    QAction *m_timeStepAction = nullptr;
    QAction *m_selectToolAction = nullptr;
    QActionGroup *m_toolGroup = nullptr;
    QHash<QString, QAction *> m_toolActions;
};
