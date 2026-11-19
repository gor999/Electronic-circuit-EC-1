#pragma once

#include "config/Parameters.h"
#include "physics/DeviceModel.h"

#include <QWidget>

#include <array>
#include <vector>

class PlotWidget;
class QCheckBox;
class QComboBox;

enum class GraphType {
    IdVds, IdVgs, Potential, Charge, Field, Density, Velocity, PinchVds, IdTime, ResistanceVgs, IdLength, IdWidth
};

struct SweepSample
{
    double time = 0.0;   // s
    double vgs = 0.0, vds = 0.0, id = 0.0;
    double pinchOff = -1.0;   // m from the source, -1 when not pinched off
};

// What a graph needs: the model, one or two configurations (A, B) and their
// states, the recorded sweep and the probe position.
struct GraphContext
{
    const DeviceModel *model = nullptr;
    std::array<const MosfetConfig *, 2> configs{};
    std::array<const DeviceState *, 2> states{};
    bool compare = false;
    const std::vector<SweepSample> *sweep = nullptr;
    double probeFraction = 0.5;   // probe position as a fraction of L
};

// One graph with a selector. All curves are computed from the model each time
// the configuration changes, so every graph follows every parameter.
class GraphPanel : public QWidget
{
    Q_OBJECT

public:
    explicit GraphPanel(GraphType initial, QWidget *parent = nullptr);

    GraphType type() const;
    void setType(GraphType type);
    void refresh(const GraphContext &context);
    PlotWidget *plot() const { return m_plot; }

    static QString title(GraphType type);

signals:
    void typeChanged();

private:
    QComboBox *m_selector = nullptr;
    QCheckBox *m_log = nullptr;
    QCheckBox *m_family = nullptr;
    PlotWidget *m_plot = nullptr;
};
