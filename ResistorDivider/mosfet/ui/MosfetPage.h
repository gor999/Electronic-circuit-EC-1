#pragma once

#include "GraphPanel.h"
#include "PartInfo.h"
#include "config/Parameters.h"
#include "geometry/MosfetGeometry.h"
#include "particles/CarrierParticles.h"
#include "physics/DeviceModel.h"

#include <QElapsedTimer>
#include <QHash>
#include <QWidget>

#include <array>
#include <vector>

class MosfetView;
class ParamControl;
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QSlider;
class QTableWidget;
class QTextBrowser;
class QTimer;

// MOSFET 3D tab: an interactive lab around one (or, in comparison mode, two)
// device configurations.
//
// Data flow, in this order on every change:
//   control -> MosfetConfig (validated) -> AnalyticalModel -> DeviceState
//   -> MosfetGeometry -> 3D view, particles, results, probe, graphs, explanation.
class MosfetPage : public QWidget
{
    Q_OBJECT

public:
    explicit MosfetPage(QWidget *parent = nullptr);
    ~MosfetPage() override;

    // Access for tests.
    MosfetView *view(int slot = 0) const { return m_slots[std::size_t(slot)].view; }
    const MosfetConfig &config(int slot = 0) const { return m_slots[std::size_t(slot)].config; }
    const DeviceState &state(int slot = 0) const { return m_slots[std::size_t(slot)].state; }
    ParamControl *control(const QString &key) const { return m_controls.value(key); }
    QCheckBox *toggle(const QString &key) const { return m_toggles.value(key); }
    GraphPanel *graph(int index) const { return m_graphs[std::size_t(index)]; }
    QString explanationHtml() const;
    const std::vector<SweepSample> &sweepSamples() const { return m_sweep.samples; }
    bool sweepRunning() const { return m_sweep.running; }

public slots:
    void setParameter(const QString &key, double siValue);   // as if typed into its control
    void setToggle(const QString &key, bool on);
    void applyExperiment(int index);
    void setCompare(bool on);
    void setEditTarget(int target);   // 0 = A, 1 = B, 2 = both
    void startSweep();
    void pauseSweep();
    void stepSweep();
    void resetSweep();
    void runGuidedPinchOff();
    void advanceTime(double seconds);   // animation + sweep; called by the frame timer

private slots:
    void onFrame();
    void showDimensions();
    void showPhysicsCheck();
    void saveConfiguration();
    void restoreDefaults();
    void copyAtoB();

private:
    struct Slot
    {
        MosfetConfig config;
        DeviceState state;
        mosfet::MosfetGeometry geometry;
        mosfet::CarrierParticles particles;
        std::vector<std::string> notes;
        MosfetView *view = nullptr;
    };
    struct Sweep
    {
        bool running = false;
        int variable = 0;   // 0 VDS, 1 VGS
        double from = 0.0, to = 4.0, duration = 12.0, step = 0.1, speed = 1.0, time = 0.0;
        std::vector<SweepSample> samples;
    };

    QWidget *buildControls();
    QWidget *buildGroup(ParamGroup group, QWidget *extra = nullptr);
    QWidget *buildModelTab();
    QWidget *buildExperimentTab();
    QWidget *buildViewTab();
    QWidget *buildToolbar();
    QWidget *buildResults();

    std::vector<int> targets() const;
    int shownSlot() const { return m_target == 1 ? 1 : 0; }
    void recompute(int slot);
    void refreshPanels();
    void refreshResults();
    void refreshProbe();
    void refreshControls();
    void refreshSweepReadout();
    void pushParticles(int slot);
    void setExplanation(const QString &html);
    void applyConfigToTargets(const MosfetConfig &config, const QString &heading, const QString &observe);
    void applySweepValue(double value, bool record);
    void populateExperiments();

    std::array<Slot, 2> m_slots;
    AnalyticalModel m_model;
    bool m_compare = false;
    int m_target = 0;
    bool m_syncCameras = true;
    bool m_syncing = false;
    bool m_paused = false;
    double m_probeFraction = 0.5;
    int m_probeMode = 2;   // 0 custom, 1 source, 2 centre, 3 drain, 4 pinch-off
    Sweep m_sweep;
    QString m_explanation;

    QHash<QString, ParamControl *> m_controls;
    QHash<QString, QCheckBox *> m_toggles;
    std::array<GraphPanel *, 2> m_graphs{};
    QWidget *m_viewB = nullptr;
    QTableWidget *m_results = nullptr;
    QTableWidget *m_probe = nullptr;
    QTextBrowser *m_explanationBox = nullptr;
    QLabel *m_hint = nullptr;
    QLabel *m_physicsThreshold = nullptr;
    QLabel *m_terminals = nullptr;
    QLabel *m_geometryDerived = nullptr;
    QLabel *m_materialDerived = nullptr;
    QLabel *m_warnings = nullptr;
    QLabel *m_sweepReadout = nullptr;
    QSlider *m_probeSlider = nullptr;
    QComboBox *m_experiments = nullptr;
    QComboBox *m_sectionMode = nullptr;
    QSlider *m_sectionSlider = nullptr;
    QSlider *m_explode = nullptr;
    QPushButton *m_playButton = nullptr;
    std::array<QPushButton *, 3> m_targetButtons{};
    QPushButton *m_copyButton = nullptr;
    QCheckBox *m_syncBox = nullptr;
    QComboBox *m_sweepVariable = nullptr;
    QDoubleSpinBox *m_sweepFrom = nullptr, *m_sweepTo = nullptr, *m_sweepDuration = nullptr, *m_sweepStep = nullptr;
    QSlider *m_sweepSpeed = nullptr;

    QTimer *m_timer = nullptr;
    QElapsedTimer m_clock;
};
