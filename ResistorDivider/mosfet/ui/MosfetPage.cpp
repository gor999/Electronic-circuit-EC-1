#include "MosfetPage.h"

#include "Explainer.h"
#include "Format.h"
#include "MosfetView.h"
#include "ParamControl.h"
#include "PhysicsCheck.h"
#include "PlotWidget.h"
#include "Presets.h"
#include "Ui.h"
#include "physics/Electrostatics.h"
#include "render/Materials.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QSignalBlocker>
#include <QSlider>
#include <QSplitter>
#include <QTabWidget>
#include <QTableWidget>
#include <QTextBrowser>
#include <QTimer>
#include <QVBoxLayout>

#include <cmath>

using mosfet::PartId;

namespace {

const int kSliderSteps = 1000;
const char *const kSettingsGroup = "MOSFET3D/configs";

QIcon swatch(const QColor &color)
{
    QPixmap pixmap(14, 14);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(color.darker(160), 1.0));
    painter.setBrush(color);
    painter.drawRoundedRect(QRectF(1, 1, 12, 12), 3, 3);
    return QIcon(pixmap);
}

QPushButton *button(const QString &text, const QString &tip = QString())
{
    auto *b = new QPushButton(text);
    b->setObjectName(QStringLiteral("clearButton"));
    b->setCursor(Qt::PointingHandCursor);
    b->setToolTip(tip);
    return b;
}

QScrollArea *scrolled(QWidget *content)
{
    auto *scroll = new QScrollArea;
    scroll->setWidget(content);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    return scroll;
}

QWidget *host(QLayout *layout)
{
    auto *w = new QWidget;
    w->setObjectName(QStringLiteral("transparentHost"));
    w->setLayout(layout);
    return w;
}

QString volts(double v) { return Format::number(Format::tidy(v, 1.0e-9), 4) + QStringLiteral(" V"); }
QString amps(double v) { return Format::si(Format::tidy(v, 1.0e-18), QStringLiteral("A"), 4); }
QString um(double m) { return Format::number(m * 1.0e6, 4) + QStringLiteral(" µm"); }
QString ohms(double r) { return r > 0.0 ? Format::si(r, QStringLiteral("Ω"), 4) : QStringLiteral("∞"); }

QString formatParameter(const ParamSpec &spec, double si)
{
    return ParamControl::formatDisplay(si * spec.unitScale) + (*spec.unit ? QStringLiteral(" ") + QString::fromUtf8(spec.unit) : QString());
}

QColor regionQColor(OperatingRegion r) { return QColor(Explainer::regionColor(r)); }

QTableWidget *makeTable()
{
    auto *table = new QTableWidget(0, 2);
    table->verticalHeader()->setVisible(false);
    table->horizontalHeader()->setVisible(true);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionMode(QAbstractItemView::NoSelection);
    table->setFocusPolicy(Qt::NoFocus);
    table->setShowGrid(false);
    table->setWordWrap(false);
    table->verticalHeader()->setDefaultSectionSize(22);
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table->setStyleSheet(QStringLiteral("QTableWidget { font-size: 9pt; } QTableWidget::item { padding: 0 6px; }"
                                        "QHeaderView::section { padding: 4px 6px; font-size: 8.5pt; }"));
    return table;
}

} // namespace

// ===========================================================================
// Construction
// ===========================================================================

MosfetPage::MosfetPage(QWidget *parent)
    : QWidget(parent)
{
    for (Slot &slot : m_slots) {
        validateConfig(slot.config);
        slot.view = new MosfetView;
    }

    auto *views = new QSplitter(Qt::Horizontal);
    views->setChildrenCollapsible(false);
    views->addWidget(m_slots[0].view);
    views->addWidget(m_slots[1].view);
    m_viewB = m_slots[1].view;
    m_viewB->hide();

    m_graphs[0] = new GraphPanel(GraphType::IdVds);
    m_graphs[1] = new GraphPanel(GraphType::Charge);
    auto *graphRow = new QHBoxLayout;
    graphRow->setContentsMargins(0, 0, 0, 0);
    graphRow->setSpacing(12);
    graphRow->addWidget(m_graphs[0], 1);
    graphRow->addWidget(m_graphs[1], 1);
    QFrame *graphFrame = Ui::createPanel(QStringLiteral("panel"));
    auto *graphLayout = new QVBoxLayout(graphFrame);
    graphLayout->setContentsMargins(12, 10, 12, 10);
    graphLayout->addLayout(graphRow);

    auto *centre = new QSplitter(Qt::Vertical);
    centre->setChildrenCollapsible(false);
    centre->addWidget(views);
    centre->addWidget(graphFrame);
    centre->setStretchFactor(0, 3);
    centre->setStretchFactor(1, 2);
    centre->setSizes({480, 300});

    auto *middle = new QVBoxLayout;
    middle->setContentsMargins(0, 0, 0, 0);
    middle->setSpacing(8);
    middle->addWidget(buildToolbar());
    middle->addWidget(centre, 1);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(12);
    layout->addWidget(buildControls());
    layout->addLayout(middle, 1);
    layout->addWidget(buildResults());

    for (GraphPanel *graph : m_graphs)
        connect(graph, &GraphPanel::typeChanged, this, &MosfetPage::refreshPanels);
    for (int i = 0; i < 2; ++i) {
        MosfetView *self = m_slots[std::size_t(i)].view;
        MosfetView *other = m_slots[std::size_t(1 - i)].view;
        connect(self, &MosfetView::cameraChanged, this, [this, self, other] {
            if (!m_compare || !m_syncCameras || m_syncing)
                return;
            m_syncing = true;
            other->setCamera(self->cameraRig().camera());
            m_syncing = false;
        });
    }

    m_timer = new QTimer(this);
    m_timer->setInterval(16);
    connect(m_timer, &QTimer::timeout, this, &MosfetPage::onFrame);
    m_timer->start();
    m_clock.start();

    recompute(0);
    recompute(1);
    refreshPanels();
    setExplanation(Explainer::describeState(m_slots[0].state, m_slots[0].config.device)
                   + tr("<p style='color:#8a94a3'>Change any control on the left: the 3D device, the carriers, the "
                        "graphs and the results all follow, and this box explains what happened.</p>"));
}

MosfetPage::~MosfetPage() = default;

QWidget *MosfetPage::buildGroup(ParamGroup group, QWidget *extra)
{
    auto *layout = new QVBoxLayout;
    layout->setContentsMargins(4, 8, 8, 8);
    layout->setSpacing(6);
    for (const ParamSpec &spec : parameterSpecs()) {
        if (spec.group != group)
            continue;
        auto *control = new ParamControl(spec);
        const QString key = QString::fromLatin1(spec.key);
        m_controls.insert(key, control);
        connect(control, &ParamControl::edited, this, [this, key](double v) { setParameter(key, v); });
        connect(control, &ParamControl::focused, this, [this](const ParamSpec *s) {
            m_hint->setText(QStringLiteral("<b>%1</b> — %2").arg(QString::fromUtf8(s->name).toHtmlEscaped(),
                                                                  QString::fromUtf8(s->effects).toHtmlEscaped()));
        });
        layout->addWidget(control);
    }
    if (extra)
        layout->addWidget(extra);
    layout->addStretch(1);
    return scrolled(host(layout));
}

QWidget *MosfetPage::buildControls()
{
    auto *tabs = new QTabWidget;
    tabs->setObjectName(QStringLiteral("controlTabs"));
    tabs->setDocumentMode(true);
    tabs->setStyleSheet(QStringLiteral("QTabBar::tab { padding: 7px 6px 6px 6px; margin-left: 0px; font-size: 8.5pt; }"));
    tabs->setUsesScrollButtons(false);

    // Bias + threshold.
    {
        auto *layout = new QVBoxLayout;
        layout->setContentsMargins(4, 8, 8, 8);
        layout->setSpacing(6);
        layout->addWidget(Ui::createLabel(tr("TERMINAL VOLTAGES"), QStringLiteral("sectionTitle")));
        for (const char *key : {"vgs", "vds", "vbs", "vs"}) {
            const ParamSpec *spec = findParameter(key);
            auto *control = new ParamControl(*spec);
            m_controls.insert(QString::fromLatin1(key), control);
            const QString k = QString::fromLatin1(key);
            connect(control, &ParamControl::edited, this, [this, k](double v) { setParameter(k, v); });
            connect(control, &ParamControl::focused, this, [this](const ParamSpec *s) {
                m_hint->setText(QStringLiteral("<b>%1</b> — %2").arg(QString::fromUtf8(s->name).toHtmlEscaped(),
                                                                      QString::fromUtf8(s->effects).toHtmlEscaped()));
            });
            layout->addWidget(control);
        }
        m_terminals = Ui::createLabel(QString(), QStringLiteral("cardDetail"));
        m_terminals->setWordWrap(true);
        layout->addWidget(m_terminals);
        layout->addSpacing(8);
        layout->addWidget(Ui::createLabel(tr("THRESHOLD"), QStringLiteral("sectionTitle")));
        auto *physics = new QCheckBox(QString::fromUtf8(findToggle("vthPhysics")->name));
        physics->setToolTip(QString::fromUtf8(findToggle("vthPhysics")->description));
        m_toggles.insert(QStringLiteral("vthPhysics"), physics);
        connect(physics, &QCheckBox::toggled, this, [this](bool on) { setToggle(QStringLiteral("vthPhysics"), on); });
        layout->addWidget(physics);
        auto *vth = new ParamControl(*findParameter("vth0"));
        m_controls.insert(QStringLiteral("vth0"), vth);
        connect(vth, &ParamControl::edited, this, [this](double v) { setParameter(QStringLiteral("vth0"), v); });
        layout->addWidget(vth);
        m_physicsThreshold = Ui::createLabel(QString(), QStringLiteral("cardDetail"));
        m_physicsThreshold->setWordWrap(true);
        m_physicsThreshold->setTextFormat(Qt::RichText);
        layout->addWidget(m_physicsThreshold);
        layout->addStretch(1);
        tabs->addTab(scrolled(host(layout)), tr("Bias"));
    }

    m_geometryDerived = Ui::createLabel(QString(), QStringLiteral("cardDetail"));
    m_geometryDerived->setWordWrap(true);
    m_geometryDerived->setTextFormat(Qt::RichText);
    tabs->addTab(buildGroup(ParamGroup::Geometry, m_geometryDerived), tr("Geometry"));

    m_materialDerived = Ui::createLabel(QString(), QStringLiteral("cardDetail"));
    m_materialDerived->setWordWrap(true);
    m_materialDerived->setTextFormat(Qt::RichText);
    tabs->addTab(buildGroup(ParamGroup::Material, m_materialDerived), tr("Material"));
    tabs->addTab(buildModelTab(), tr("Model"));
    tabs->addTab(buildExperimentTab(), tr("Sweep"));
    tabs->addTab(buildViewTab(), tr("View"));

    QFrame *panel = Ui::createPanel(QStringLiteral("inputPanel"));
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(10, 10, 6, 10);
    layout->setSpacing(6);
    layout->addWidget(tabs, 1);
    m_hint = Ui::createLabel(tr("Hover over a control to see what it changes."), QStringLiteral("cardDetail"));
    m_hint->setWordWrap(true);
    m_hint->setTextFormat(Qt::RichText);
    m_hint->setMinimumHeight(70);
    m_hint->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    layout->addWidget(m_hint);
    panel->setFixedWidth(340);
    return panel;
}

QWidget *MosfetPage::buildModelTab()
{
    auto *layout = new QVBoxLayout;
    layout->setContentsMargins(4, 8, 8, 8);
    layout->setSpacing(8);
    layout->addWidget(Ui::createLabel(tr("MODEL OPTIONS"), QStringLiteral("sectionTitle")));
    for (const ToggleSpec &spec : toggleSpecs()) {
        if (spec.group != ParamGroup::Model)
            continue;
        auto *box = new QCheckBox(QString::fromUtf8(spec.name));
        box->setToolTip(QString::fromUtf8(spec.description));
        const QString key = QString::fromLatin1(spec.key);
        m_toggles.insert(key, box);
        connect(box, &QCheckBox::toggled, this, [this, key](bool on) { setToggle(key, on); });
        layout->addWidget(box);
        auto *note = Ui::createLabel(QString::fromUtf8(spec.description), QStringLiteral("cardDetail"));
        note->setWordWrap(true);
        layout->addWidget(note);
        if (key == QLatin1String("mobDeg")) {
            auto *theta = new ParamControl(*findParameter("theta"));
            m_controls.insert(QStringLiteral("theta"), theta);
            connect(theta, &ParamControl::edited, this, [this](double v) { setParameter(QStringLiteral("theta"), v); });
            layout->addWidget(theta);
        }
    }
    layout->addSpacing(6);
    layout->addWidget(Ui::createLabel(tr("NOT YET MODELLED"), QStringLiteral("sectionTitle")));
    auto *missing = Ui::createLabel(tr("Subthreshold conduction · velocity saturation in the channel · short-channel "
                                       "effects (DIBL, VTH roll-off) · punch-through current · polysilicon depletion · "
                                       "series resistance · gate leakage · quantum confinement. The physics check "
                                       "lists every assumption."),
                                    QStringLiteral("cardDetail"));
    missing->setWordWrap(true);
    layout->addWidget(missing);
    layout->addStretch(1);
    return scrolled(host(layout));
}

QWidget *MosfetPage::buildExperimentTab()
{
    auto *layout = new QVBoxLayout;
    layout->setContentsMargins(4, 8, 8, 8);
    layout->setSpacing(8);
    layout->addWidget(Ui::createLabel(tr("PINCH-OFF EXPERIMENT"), QStringLiteral("sectionTitle")));
    auto *guided = button(tr("Guided pinch-off experiment"),
                          tr("VGS = VTH + 1 V, then VDS ramps from 0 to 3 V over 15 s"));
    guided->setObjectName(QStringLiteral("calculateButton"));
    connect(guided, &QPushButton::clicked, this, &MosfetPage::runGuidedPinchOff);
    layout->addWidget(guided);

    auto *grid = new QGridLayout;
    grid->setHorizontalSpacing(8);
    grid->setVerticalSpacing(6);
    m_sweepVariable = new QComboBox;
    m_sweepVariable->addItems({tr("VDS"), tr("VGS")});
    auto spin = [](double lo, double hi, double value, double step, const QString &suffix) {
        auto *s = new QDoubleSpinBox;
        s->setRange(lo, hi);
        s->setDecimals(2);
        s->setSingleStep(step);
        s->setValue(value);
        s->setSuffix(suffix);
        return s;
    };
    m_sweepFrom = spin(-1.0, 6.0, 0.0, 0.1, QStringLiteral(" V"));
    m_sweepTo = spin(-1.0, 6.0, 4.0, 0.1, QStringLiteral(" V"));
    m_sweepDuration = spin(1.0, 120.0, 12.0, 1.0, QStringLiteral(" s"));
    m_sweepStep = spin(0.01, 2.0, 0.1, 0.05, QStringLiteral(" V"));
    int row = 0;
    for (auto [label, widget] : std::initializer_list<std::pair<QString, QWidget *>>{
             {tr("Sweep"), m_sweepVariable}, {tr("From"), m_sweepFrom}, {tr("To"), m_sweepTo},
             {tr("Ramp time"), m_sweepDuration}, {tr("Step size"), m_sweepStep}}) {
        grid->addWidget(Ui::createLabel(label, QStringLiteral("fieldLabel")), row, 0);
        grid->addWidget(widget, row, 1);
        ++row;
    }
    layout->addLayout(grid);
    layout->addWidget(Ui::createLabel(tr("Animation speed"), QStringLiteral("fieldLabel")));
    m_sweepSpeed = new QSlider(Qt::Horizontal);
    m_sweepSpeed->setRange(-60, 60);   // speed = 10^(v/100): x0.25 .. x4
    m_sweepSpeed->setToolTip(tr("×0.25 to ×4"));
    connect(m_sweepSpeed, &QSlider::valueChanged, this, [this](int v) { m_sweep.speed = std::pow(10.0, v / 100.0); });
    layout->addWidget(m_sweepSpeed);

    auto *buttons = new QHBoxLayout;
    buttons->setSpacing(6);
    m_playButton = button(tr("Play"));
    auto *step = button(tr("Step"), tr("Increase the swept voltage by one step (pauses the sweep)"));
    auto *reset = button(tr("Reset"), tr("Back to the start voltage; clears the recorded traces"));
    buttons->addWidget(m_playButton);
    buttons->addWidget(step);
    buttons->addWidget(reset);
    layout->addLayout(buttons);
    connect(m_playButton, &QPushButton::clicked, this, [this] { m_sweep.running ? pauseSweep() : startSweep(); });
    connect(step, &QPushButton::clicked, this, &MosfetPage::stepSweep);
    connect(reset, &QPushButton::clicked, this, &MosfetPage::resetSweep);

    m_sweepReadout = Ui::createLabel(QString(), QString());
    m_sweepReadout->setTextFormat(Qt::RichText);
    m_sweepReadout->setWordWrap(true);
    layout->addWidget(m_sweepReadout);
    auto *note = Ui::createLabel(tr("Pinch-off is set by the operating point, not by time. The sweep changes the "
                                    "voltage slowly (quasi-static): every instant is a steady state, and the time axis "
                                    "only shows the ramp's progress. Watch V(x) rise, Qinv(x) fall towards the drain, "
                                    "pinch-off appear at VDS = VOV and move towards the source, and ID saturate."),
                                 QStringLiteral("cardDetail"));
    note->setWordWrap(true);
    layout->addWidget(note);
    layout->addStretch(1);
    return scrolled(host(layout));
}

QWidget *MosfetPage::buildViewTab()
{
    auto *layout = new QVBoxLayout;
    layout->setContentsMargins(4, 8, 8, 8);
    layout->setSpacing(6);
    auto forViews = [this](auto fn) {
        for (Slot &slot : m_slots)
            fn(slot.view);
    };

    layout->addWidget(Ui::createLabel(tr("CAMERA"), QStringLiteral("sectionTitle")));
    auto *presets = new QGridLayout;
    presets->setSpacing(6);
    struct PresetButton { QString text; MosfetView::Preset preset; QString tip; };
    const PresetButton list[] = {
        {tr("Full 3D"), MosfetView::Preset::Perspective, tr("Perspective view of the whole device")},
        {tr("2D cross-section"), MosfetView::Preset::CrossSection, tr("Straight at the section along the channel, nearly orthographic")},
        {tr("Top"), MosfetView::Preset::Top, tr("From above, as on a layout drawing")},
        {tr("Side"), MosfetView::Preset::DrainSide, tr("From the drain end, across the channel width")}};
    int i = 0;
    for (const PresetButton &p : list) {
        QPushButton *b = button(p.text, p.tip);
        const MosfetView::Preset preset = p.preset;
        connect(b, &QPushButton::clicked, this, [forViews, preset] { forViews([&](MosfetView *v) { v->setPreset(preset); }); });
        presets->addWidget(b, i / 2, i % 2);
        ++i;
    }
    layout->addLayout(presets);

    layout->addWidget(Ui::createLabel(tr("SHOW"), QStringLiteral("sectionTitle")));
    struct Show { QString text; bool on; std::function<void(MosfetView *, bool)> apply; };
    const Show shows[] = {
        {tr("Labels"), true, [](MosfetView *v, bool on) { v->setLabelsVisible(on); }},
        {tr("Electrons and holes"), true, [](MosfetView *v, bool on) { v->setParticlesVisible(on); }},
        {tr("Electron flow and current arrows"), true, [](MosfetView *v, bool on) { v->setFlow(on, v->property("current").toDouble()); }},
        {tr("Pinch-off marker"), true, [](MosfetView *v, bool on) { v->setPinchMarkerVisible(on); }},
        {tr("Probe marker"), true, [](MosfetView *v, bool on) { v->setProbeMarkerVisible(on); }},
    };
    for (const Show &s : shows) {
        auto *box = new QCheckBox(s.text);
        box->setChecked(s.on);
        auto apply = s.apply;
        connect(box, &QCheckBox::toggled, this, [forViews, apply](bool on) { forViews([&](MosfetView *v) { apply(v, on); }); });
        layout->addWidget(box);
        if (s.text == tr("Electron flow and current arrows"))
            m_toggles.insert(QStringLiteral("_arrows"), box);
        if (s.text == tr("Labels"))
            m_toggles.insert(QStringLiteral("_labels"), box);
    }
    auto *pause = new QCheckBox(tr("Pause the animation"));
    connect(pause, &QCheckBox::toggled, this, [this](bool on) { m_paused = on; });
    layout->addWidget(pause);

    layout->addWidget(Ui::createLabel(tr("COMPONENTS"), QStringLiteral("sectionTitle")));
    struct Toggle { QString text; std::vector<PartId> parts; };
    const Toggle toggles[] = {
        {tr("Gate electrode"), {PartId::Gate}}, {tr("Gate oxide (SiO₂)"), {PartId::GateOxide}},
        {tr("Inversion channel"), {PartId::Channel}}, {tr("Depletion region"), {PartId::Depletion}},
        {tr("N+ source"), {PartId::Source}}, {tr("N+ drain"), {PartId::Drain}},
        {tr("P-type substrate"), {PartId::Substrate}},
        {tr("Metal contacts"), {PartId::GateContact, PartId::SourceContact, PartId::DrainContact, PartId::BodyContact}}};
    for (const Toggle &t : toggles) {
        auto *box = new QCheckBox(t.text);
        box->setChecked(true);
        box->setIcon(swatch(partMaterial(t.parts.front()).color));
        const std::vector<PartId> parts = t.parts;
        connect(box, &QCheckBox::toggled, this, [forViews, parts](bool on) {
            forViews([&](MosfetView *v) {
                for (PartId id : parts)
                    v->setPartVisible(id, on);
            });
        });
        layout->addWidget(box);
    }
    auto *lattice = new QCheckBox(tr("Silicon lattice (illustrative)"));
    lattice->setIcon(swatch(latticeColor(mosfet::Lattice::Silicon)));
    connect(lattice, &QCheckBox::toggled, this, [forViews](bool on) { forViews([&](MosfetView *v) { v->setLatticeVisible(on); }); });
    layout->addWidget(lattice);

    layout->addWidget(Ui::createLabel(tr("INSPECT"), QStringLiteral("sectionTitle")));
    layout->addWidget(Ui::createLabel(tr("Exploded view"), QStringLiteral("fieldLabel")));
    m_explode = new QSlider(Qt::Horizontal);
    m_explode->setRange(0, 100);
    connect(m_explode, &QSlider::valueChanged, this, [forViews](int v) { forViews([&](MosfetView *view) { view->setExplode(v / 100.0f); }); });
    layout->addWidget(m_explode);
    layout->addWidget(Ui::createLabel(tr("Section plane"), QStringLiteral("fieldLabel")));
    m_sectionMode = new QComboBox;
    m_sectionMode->addItems({tr("None (full device)"), tr("Along the width (cut in z)"), tr("Across the channel (cut in x)")});
    m_sectionSlider = new QSlider(Qt::Horizontal);
    m_sectionSlider->setRange(0, kSliderSteps);
    m_sectionSlider->setValue(500);
    m_sectionSlider->setEnabled(false);
    layout->addWidget(m_sectionMode);
    layout->addWidget(m_sectionSlider);
    auto applySection = [this] {
        const int mode = m_sectionMode->currentIndex();
        const float t = m_sectionSlider->value() / float(kSliderSteps);
        m_sectionSlider->setEnabled(mode != 0);
        for (Slot &slot : m_slots) {
            const mosfet::MosfetGeometry &g = slot.geometry;
            if (mode == 1)
                slot.view->setSection(MosfetView::Section::Width, g.zBack + (g.zFront - g.zBack) * (0.05f + 0.95f * t));
            else if (mode == 2)   // across the channel: the slider spans source edge -> drain edge
                slot.view->setSection(MosfetView::Section::Channel, g.sourceEdge + (g.drainEdge - g.sourceEdge) * t);
            else
                slot.view->setSection(MosfetView::Section::None, 0.0f);
        }
    };
    connect(m_sectionMode, qOverload<int>(&QComboBox::currentIndexChanged), this, applySection);
    connect(m_sectionSlider, &QSlider::valueChanged, this, applySection);

    layout->addWidget(Ui::createLabel(tr("DRAWING"), QStringLiteral("sectionTitle")));
    for (const ParamSpec &spec : parameterSpecs()) {
        if (spec.group != ParamGroup::Display)
            continue;
        auto *control = new ParamControl(spec);
        const QString key = QString::fromLatin1(spec.key);
        m_controls.insert(key, control);
        connect(control, &ParamControl::edited, this, [this, key](double v) { setParameter(key, v); });
        layout->addWidget(control);
    }
    auto *dimensions = button(tr("Dimensions and scale…"));
    connect(dimensions, &QPushButton::clicked, this, &MosfetPage::showDimensions);
    layout->addWidget(dimensions);
    layout->addStretch(1);
    return scrolled(host(layout));
}

QWidget *MosfetPage::buildToolbar()
{
    auto *bar = new QHBoxLayout;
    bar->setContentsMargins(0, 0, 0, 0);
    bar->setSpacing(6);
    m_experiments = new QComboBox;
    m_experiments->setMinimumWidth(160);
    m_experiments->setToolTip(tr("Ready-made experiments and your saved configurations"));
    populateExperiments();
    connect(m_experiments, qOverload<int>(&QComboBox::activated), this, [this](int index) {
        const QVariant data = m_experiments->itemData(index);
        if (data.typeId() == QMetaType::Int) {
            applyExperiment(data.toInt());
        } else if (data.typeId() == QMetaType::QString) {
            QSettings settings;
            const QString name = data.toString();
            MosfetConfig config = deserializeConfig(settings.value(QLatin1String(kSettingsGroup) + QLatin1Char('/') + name).toString().toStdString());
            applyConfigToTargets(config, tr("Saved configuration: %1").arg(name), QString());
        }
        m_experiments->setCurrentIndex(0);
    });
    auto *save = button(tr("Save…"), tr("Save the current configuration under a name"));
    auto *defaults = button(tr("Defaults"), tr("Restore every parameter to its default value"));
    connect(save, &QPushButton::clicked, this, &MosfetPage::saveConfiguration);
    connect(defaults, &QPushButton::clicked, this, &MosfetPage::restoreDefaults);

    auto *compare = new QCheckBox(tr("Compare A / B"));
    compare->setToolTip(tr("Two configurations side by side"));
    connect(compare, &QCheckBox::toggled, this, &MosfetPage::setCompare);
    m_toggles.insert(QStringLiteral("_compare"), compare);
    const QString names[] = {tr("Edit A"), tr("Edit B"), tr("Edit both")};
    for (int i = 0; i < 3; ++i) {
        auto *b = new QPushButton(names[i]);
        b->setObjectName(i == 0 ? QStringLiteral("segmentLeft") : i == 2 ? QStringLiteral("segmentRight") : QStringLiteral("segmentLeft"));
        if (i == 1)
            b->setStyleSheet(QStringLiteral("border-radius: 0px; border-left: none;"));
        b->setCheckable(true);
        b->setEnabled(false);
        connect(b, &QPushButton::clicked, this, [this, i] { setEditTarget(i); });
        m_targetButtons[std::size_t(i)] = b;
    }
    m_targetButtons[0]->setChecked(true);
    m_copyButton = button(tr("A → B"), tr("Copy configuration A into B"));
    m_copyButton->setEnabled(false);
    connect(m_copyButton, &QPushButton::clicked, this, &MosfetPage::copyAtoB);
    m_syncBox = new QCheckBox(tr("Sync cameras"));
    m_syncBox->setChecked(true);
    m_syncBox->setEnabled(false);
    connect(m_syncBox, &QCheckBox::toggled, this, [this](bool on) { m_syncCameras = on; });
    auto *check = button(tr("Physics check…"), tr("Equations with values, assumptions and self-consistency checks"));
    connect(check, &QPushButton::clicked, this, &MosfetPage::showPhysicsCheck);

    bar->addWidget(m_experiments, 1);
    bar->addWidget(save);
    bar->addWidget(defaults);
    bar->addWidget(check);
    auto *second = new QHBoxLayout;
    second->setContentsMargins(0, 0, 0, 0);
    second->setSpacing(6);
    second->addWidget(compare);
    auto *segment = new QHBoxLayout;
    segment->setSpacing(0);
    for (QPushButton *b : m_targetButtons)
        segment->addWidget(b);
    second->addLayout(segment);
    second->addWidget(m_copyButton);
    second->addWidget(m_syncBox);
    second->addStretch(1);
    auto *rows = new QVBoxLayout;
    rows->setContentsMargins(0, 0, 0, 0);
    rows->setSpacing(6);
    rows->addLayout(bar);
    rows->addLayout(second);
    return host(rows);
}

QWidget *MosfetPage::buildResults()
{
    QFrame *panel = Ui::createPanel(QStringLiteral("panel"));
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(6);
    layout->addWidget(Ui::createLabel(tr("RESULTS"), QStringLiteral("sectionTitle")));
    m_results = makeTable();
    layout->addWidget(m_results, 3);

    layout->addWidget(Ui::createLabel(tr("PROBE ALONG THE CHANNEL"), QStringLiteral("sectionTitle")));
    auto *probeButtons = new QHBoxLayout;
    probeButtons->setSpacing(4);
    const QString names[] = {tr("Source"), tr("Centre"), tr("Drain"), tr("Pinch-off")};
    for (int i = 0; i < 4; ++i) {
        QPushButton *b = button(names[i]);
        b->setStyleSheet(QStringLiteral("padding: 5px 6px; font-size: 9pt;"));
        connect(b, &QPushButton::clicked, this, [this, i] {
            m_probeMode = i + 1;
            refreshPanels();
        });
        probeButtons->addWidget(b);
    }
    layout->addLayout(probeButtons);
    m_probeSlider = new QSlider(Qt::Horizontal);
    m_probeSlider->setRange(0, kSliderSteps);
    m_probeSlider->setValue(500);
    connect(m_probeSlider, &QSlider::valueChanged, this, [this](int v) {
        if (m_probeSlider->signalsBlocked())
            return;
        m_probeMode = 0;
        m_probeFraction = v / double(kSliderSteps);
        refreshPanels();
    });
    layout->addWidget(m_probeSlider);
    m_probe = makeTable();
    layout->addWidget(m_probe, 2);

    layout->addWidget(Ui::createLabel(tr("WHAT HAPPENED"), QStringLiteral("sectionTitle")));
    m_explanationBox = new QTextBrowser;
    m_explanationBox->setObjectName(QStringLiteral("explanation"));
    m_explanationBox->setStyleSheet(QStringLiteral("QTextBrowser { background:#161b23; border:1px solid #2a323e; "
                                                   "border-radius:8px; padding:4px; font-size:9pt; }"));
    layout->addWidget(m_explanationBox, 3);
    m_warnings = Ui::createLabel(QString(), QStringLiteral("cardDetail"));
    m_warnings->setWordWrap(true);
    m_warnings->setTextFormat(Qt::RichText);
    layout->addWidget(m_warnings);
    panel->setFixedWidth(360);
    return panel;
}

void MosfetPage::populateExperiments()
{
    m_experiments->clear();
    m_experiments->addItem(tr("Experiments and saved configurations…"));
    const auto &list = experiments();
    for (int i = 0; i < int(list.size()); ++i)
        m_experiments->addItem(QString::fromUtf8(list[std::size_t(i)].name), i);
    QSettings settings;
    settings.beginGroup(QLatin1String(kSettingsGroup));
    const QStringList saved = settings.childKeys();
    settings.endGroup();
    for (const QString &name : saved)
        m_experiments->addItem(tr("Saved: %1").arg(name), name);
}

// ===========================================================================
// Model updates
// ===========================================================================

std::vector<int> MosfetPage::targets() const
{
    if (!m_compare)
        return {0};
    if (m_target == 2)
        return {0, 1};
    return {m_target};
}

void MosfetPage::recompute(int i)
{
    Slot &slot = m_slots[std::size_t(i)];
    slot.notes = validateConfig(slot.config);
    slot.state = m_model.solve(slot.config.device, slot.config.bias);
    mosfet::DisplayScale scale;
    scale.inversionMax = 0.16f * float(slot.config.render.channelDrawScale);
    slot.geometry = mosfet::MosfetGeometry::build(slot.config.device, slot.state, scale);
    slot.view->setTexts(describeParts(slot.config.device, slot.state));
    slot.view->setDeviceGeometry(slot.geometry);
    slot.particles.update(slot.geometry, slot.state, slot.config.render.particleDensity);

    const DeviceState &s = slot.state;
    const MosfetParameters &p = slot.config.device;
    const QString prefix = m_compare ? (i == 0 ? tr("A  ·  ") : tr("B  ·  ")) : QString();
    slot.view->setCaption(prefix + tr("N-channel MOSFET  ·  L = %1  ·  W = %2  ·  tox = %3 nm  ·  not to scale")
                                       .arg(um(p.channelLength), um(p.channelWidth), Format::number(p.oxideThickness * 1e9, 3)),
                          tr("VGS %1  ·  VDS %2  ·  VBS %3  ·  %4  ·  ID = %5")
                              .arg(volts(slot.config.bias.vgs), volts(slot.config.bias.vds), volts(slot.config.bias.vbs),
                                   Explainer::regionName(s.region), amps(s.drainCurrent)),
                          regionQColor(s.region));
    slot.view->setProperty("current", s.drainCurrent);
    const QCheckBox *arrows = m_toggles.value(QStringLiteral("_arrows"));
    slot.view->setFlow(!arrows || arrows->isChecked(), s.drainCurrent);
    // Keep an active section plane where it was relative to the new geometry.
    if (m_sectionMode && m_sectionMode->currentIndex() != 0)
        emit m_sectionSlider->valueChanged(m_sectionSlider->value());
}

void MosfetPage::setParameter(const QString &key, double value)
{
    const ParamSpec *spec = findParameter(key.toStdString());
    if (!spec)
        return;
    const int shown = shownSlot();
    const DeviceState before = m_slots[std::size_t(shown)].state;
    const double oldValue = spec->get(m_slots[std::size_t(shown)].config);
    for (int t : targets()) {
        spec->set(m_slots[std::size_t(t)].config, value);
        recompute(t);
    }
    const Slot &slot = m_slots[std::size_t(shown)];
    if (spec->affectsPhysics() && !m_sweep.running) {
        QString what = QString::fromUtf8(spec->name);
        if (*spec->symbol)
            what += QStringLiteral(" ") + QString::fromUtf8(spec->symbol);
        QString html = Explainer::describeChange(what, formatParameter(*spec, oldValue),
                                                 formatParameter(*spec, spec->get(slot.config)), before, slot.state, spec->effects);
        for (const std::string &note : slot.notes)
            html += QStringLiteral("<p style='color:#ffb454'>%1</p>").arg(QString::fromStdString(note).toHtmlEscaped());
        setExplanation(html);
    }
    refreshPanels();
}

void MosfetPage::setToggle(const QString &key, bool on)
{
    const ToggleSpec *spec = findToggle(key.toStdString());
    if (!spec)
        return;
    const int shown = shownSlot();
    const DeviceState before = m_slots[std::size_t(shown)].state;
    for (int t : targets()) {
        spec->ref(m_slots[std::size_t(t)].config) = on;
        recompute(t);
    }
    setExplanation(Explainer::describeChange(QString::fromUtf8(spec->name), on ? tr("off") : tr("on"), on ? tr("on") : tr("off"),
                                             before, m_slots[std::size_t(shown)].state, spec->description));
    refreshPanels();
}

void MosfetPage::applyConfigToTargets(const MosfetConfig &config, const QString &heading, const QString &observe)
{
    for (int t : targets()) {
        const RenderSettings render = m_slots[std::size_t(t)].config.render;
        m_slots[std::size_t(t)].config = config;
        m_slots[std::size_t(t)].config.render = render;
        recompute(t);
    }
    const Slot &slot = m_slots[std::size_t(shownSlot())];
    QString html = QStringLiteral("<b>%1</b>").arg(heading.toHtmlEscaped());
    if (!observe.isEmpty())
        html += QStringLiteral("<p><span style='color:#4cc2ff'>What to observe:</span> %1</p>").arg(observe.toHtmlEscaped());
    html += QStringLiteral("<p>") + Explainer::describeState(slot.state, slot.config.device) + QStringLiteral("</p>");
    setExplanation(html);
    refreshPanels();
}

void MosfetPage::applyExperiment(int index)
{
    const auto &list = experiments();
    if (index < 0 || index >= int(list.size()))
        return;
    const Experiment &e = list[std::size_t(index)];
    applyConfigToTargets(::applyExperiment(e), tr("Experiment: %1").arg(QString::fromUtf8(e.name)), QString::fromUtf8(e.observe));
}

void MosfetPage::restoreDefaults()
{
    applyConfigToTargets(MosfetConfig(), tr("Default values restored"), QString());
}

void MosfetPage::saveConfiguration()
{
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("Save configuration"), tr("Name:"), QLineEdit::Normal,
                                               tr("My MOSFET"), &ok).trimmed();
    if (!ok || name.isEmpty())
        return;
    QSettings settings;
    settings.setValue(QLatin1String(kSettingsGroup) + QLatin1Char('/') + name,
                      QString::fromStdString(serializeConfig(m_slots[std::size_t(shownSlot())].config)));
    populateExperiments();
    setExplanation(tr("<b>Saved as “%1”.</b> Pick it from the experiments list to load it again.").arg(name.toHtmlEscaped()));
}

void MosfetPage::setCompare(bool on)
{
    if (on == m_compare)
        return;
    m_compare = on;
    m_viewB->setVisible(on);
    for (QPushButton *b : m_targetButtons)
        b->setEnabled(on);
    m_copyButton->setEnabled(on);
    m_syncBox->setEnabled(on);
    // Two narrow views: labels off by default (the View tab turns them back on).
    if (QCheckBox *labels = m_toggles.value(QStringLiteral("_labels")))
        labels->setChecked(!on);
    if (on) {
        // B starts as A with twice the channel length, so a difference shows at once.
        m_slots[1].config = m_slots[0].config;
        findParameter("L")->set(m_slots[1].config, 2.0 * m_slots[0].config.device.channelLength);
        m_slots[1].view->setCamera(m_slots[0].view->cameraRig().camera());
        setExplanation(tr("<b>Comparison mode.</b> B started as a copy of A with twice the channel length. Choose "
                          "Edit A, Edit B or Edit both above; the results table shows A, B and their ratio, and "
                          "graphs draw B in orange."));
    } else {
        setEditTarget(0);
    }
    recompute(0);
    recompute(1);
    refreshPanels();
    // Re-frame once the splitter has resized the views.
    QTimer::singleShot(0, this, [this] {
        for (Slot &slot : m_slots)
            slot.view->setPreset(MosfetView::Preset::Perspective, false);
        if (m_compare)
            m_slots[1].view->setCamera(m_slots[0].view->cameraRig().camera());
    });
}

void MosfetPage::setEditTarget(int target)
{
    m_target = std::clamp(target, 0, 2);
    for (int i = 0; i < 3; ++i)
        m_targetButtons[std::size_t(i)]->setChecked(i == m_target);
    refreshControls();
}

void MosfetPage::copyAtoB()
{
    m_slots[1].config = m_slots[0].config;
    recompute(1);
    refreshPanels();
}

// ===========================================================================
// Panels
// ===========================================================================

void MosfetPage::setExplanation(const QString &html)
{
    m_explanation = html;
    m_explanationBox->setHtml(html);
}

QString MosfetPage::explanationHtml() const { return m_explanation; }

void MosfetPage::refreshControls()
{
    const MosfetConfig &c = m_slots[std::size_t(shownSlot())].config;
    for (const ParamSpec &spec : parameterSpecs()) {
        if (ParamControl *control = m_controls.value(QString::fromLatin1(spec.key)))
            control->setValue(spec.get(c));
    }
    for (const ToggleSpec &spec : toggleSpecs()) {
        if (QCheckBox *box = m_toggles.value(QString::fromLatin1(spec.key))) {
            const QSignalBlocker block(box);
            box->setChecked(spec.get(c));
        }
    }
    if (ParamControl *vth = m_controls.value(QStringLiteral("vth0")))
        vth->setEnabled(!c.device.thresholdFromPhysics);
    if (ParamControl *theta = m_controls.value(QStringLiteral("theta")))
        theta->setEnabled(c.device.mobilityDegradation);
}

void MosfetPage::refreshPanels()
{
    refreshControls();
    const Slot &shown = m_slots[std::size_t(shownSlot())];
    const DeviceState &s = shown.state;
    const MosfetConfig &c = shown.config;

    m_terminals->setText(tr("Terminal voltages: VG = %1 · VS = %2 · VD = %3 · VB = %4")
                             .arg(volts(c.bias.vs + c.bias.vgs), volts(c.bias.vs), volts(c.bias.vs + c.bias.vds),
                                  volts(c.bias.vs + c.bias.vbs)));
    m_physicsThreshold->setText(tr("Physics estimate VTH0 = <b>%1</b> (from NA, tox, εr and an n+ poly gate)%2. "
                                   "With VBS = %3 V the threshold in use is <b>%4</b>.")
                                    .arg(volts(s.physicalThreshold),
                                         c.device.thresholdFromPhysics ? tr(" — in use") : tr("; the slider value is used instead"),
                                         Format::number(c.bias.vbs, 3), volts(s.threshold)));
    m_geometryDerived->setText(tr("Cox = εox/tox = <b>%1 fF/µm²</b> · W/L = <b>%2</b> · gate area W·L = %3 µm²<br>"
                                  "Lateral diffusion 0.8·xj = %4, gate overlap = %5")
                                   .arg(Format::number(s.oxideCapacitance * 1e3, 4), Format::number(c.device.channelWidth / c.device.channelLength, 4),
                                        Format::number(c.device.channelWidth * c.device.channelLength * 1e12, 4),
                                        um(c.device.lateralDiffusion()), um(c.device.gateOverlap())));
    m_materialDerived->setText(tr("φF = <b>%1</b> · γ = <b>%2 V½</b> · VTH0 (physics) = <b>%3</b><br>"
                                  "Wd,max = %4 · n₀ = nᵢ²/NA = %5 · nᵢ(T) = %6<br>Vbi source %7, drain %8 · "
                                  "µn(T) = %9 cm²/(V·s)")
                                   .arg(volts(s.fermiPotential), Format::number(s.bodyFactor, 4), volts(s.physicalThreshold),
                                        um(s.maxDepletion), dopingText(s.minorityElectrons), dopingText(s.intrinsicDensity),
                                        volts(s.builtInSource), volts(s.builtInDrain), Format::number(s.mobility * 1e4, 4)));
    QString warnings;
    for (const std::string &w : s.warnings)
        warnings += QStringLiteral("<p style='color:#ffb454;margin:2px 0'>⚠ %1</p>").arg(QString::fromStdString(w).toHtmlEscaped());
    m_warnings->setText(warnings);

    // Probe position.
    const double L = c.device.channelLength;
    switch (m_probeMode) {
    case 1: m_probeFraction = 0.02; break;
    case 2: m_probeFraction = 0.5; break;
    case 3: m_probeFraction = 0.98; break;
    case 4: m_probeFraction = s.pinchedOff ? s.pinchOffPosition / L : 0.999; break;
    default: break;
    }
    {
        const QSignalBlocker block(m_probeSlider);
        m_probeSlider->setValue(int(std::lround(m_probeFraction * kSliderSteps)));
    }
    for (int i = 0; i < 2; ++i) {
        const Slot &slot = m_slots[std::size_t(i)];
        slot.view->setMarkers(slot.state.pinchedOff ? slot.state.pinchOffPosition : -1.0,
                              m_probeFraction * slot.config.device.channelLength);
    }

    refreshResults();
    refreshProbe();
    refreshSweepReadout();

    GraphContext context;
    context.model = &m_model;
    context.configs = {&m_slots[0].config, &m_slots[1].config};
    context.states = {&m_slots[0].state, &m_slots[1].state};
    context.compare = m_compare;
    context.sweep = &m_sweep.samples;
    context.probeFraction = m_probeFraction;
    for (GraphPanel *graph : m_graphs)
        graph->refresh(context);
}

void MosfetPage::refreshResults()
{
    struct Row { QString name; std::function<QString(const DeviceState &, const MosfetConfig &)> text; std::function<double(const DeviceState &)> number; };
    auto none = [](const DeviceState &) { return std::nan(""); };
    const std::vector<Row> rows = {
        {tr("Operating region"), [](const DeviceState &s, const MosfetConfig &) { return Explainer::regionName(s.region); }, none},
        {tr("VTH (with body effect)"), [](const DeviceState &s, const MosfetConfig &) { return volts(s.threshold); }, [](const DeviceState &s) { return s.threshold; }},
        {tr("VOV = VGS − VTH"), [](const DeviceState &s, const MosfetConfig &) { return volts(s.overdrive); }, [](const DeviceState &s) { return s.overdrive; }},
        {tr("Pinch-off voltage VDS,sat"), [](const DeviceState &s, const MosfetConfig &) { return s.channelExists ? volts(s.saturationVoltage) : QStringLiteral("—"); }, [](const DeviceState &s) { return s.saturationVoltage; }},
        {tr("Drain current ID"), [](const DeviceState &s, const MosfetConfig &) { return amps(s.drainCurrent); }, [](const DeviceState &s) { return s.drainCurrent; }},
        {tr("Channel resistance (VDS→0)"), [](const DeviceState &s, const MosfetConfig &) { return ohms(s.onResistance); }, [](const DeviceState &s) { return s.onResistance; }},
        {tr("VDS / ID"), [](const DeviceState &s, const MosfetConfig &c) { return c.bias.vds > 0 && s.drainCurrent > 0 ? ohms(s.dcResistance) : QStringLiteral("—"); }, [](const DeviceState &s) { return s.dcResistance; }},
        {tr("Pinch-off point (from source)"), [](const DeviceState &s, const MosfetConfig &c) {
             return s.pinchedOff ? um(s.pinchOffPosition) + QStringLiteral(" (%1 %)").arg(QString::number(100.0 * s.pinchOffPosition / c.device.channelLength, 'f', 1)) : QStringLiteral("none"); },
         [](const DeviceState &s) { return s.pinchedOff ? s.pinchOffPosition : std::nan(""); }},
        {tr("Pinch-off length ΔL"), [](const DeviceState &s, const MosfetConfig &) { return s.pinchedOff ? um(s.pinchOffLength) : QStringLiteral("—"); }, [](const DeviceState &s) { return s.pinchOffLength; }},
        {tr("Cox"), [](const DeviceState &s, const MosfetConfig &) { return Format::number(s.oxideCapacitance * 1e3, 4) + QStringLiteral(" fF/µm²"); }, [](const DeviceState &s) { return s.oxideCapacitance; }},
        {tr("|Qinv| at the source"), [](const DeviceState &s, const MosfetConfig &) { return Format::si(s.inversionCharge.front() * 1e-4, QStringLiteral("C/cm²"), 3); }, [](const DeviceState &s) { return s.inversionCharge.front(); }},
        {tr("Electrons in the channel"), [](const DeviceState &s, const MosfetConfig &) { return Format::number(s.totalInversionCharge / semi::kElementaryCharge, 3); }, [](const DeviceState &s) { return s.totalInversionCharge; }},
        {tr("n at the surface, source end (approx.)"), [](const DeviceState &s, const MosfetConfig &) { return dopingText(std::max(s.electronDensity.front(), 1.0)); }, [](const DeviceState &s) { return s.electronDensity.front(); }},
        {tr("µeff"), [](const DeviceState &s, const MosfetConfig &) { return Format::number(s.effectiveMobility * 1e4, 4) + QStringLiteral(" cm²/(V·s)"); }, [](const DeviceState &s) { return s.effectiveMobility; }},
        {tr("Drift velocity, mid-channel"), [](const DeviceState &s, const MosfetConfig &c) { return Format::si(s.at(s.velocity, 0.5 * c.device.channelLength), QStringLiteral("m/s"), 3); }, none},
        {tr("Transit time"), [](const DeviceState &s, const MosfetConfig &) { return s.transitTime > 0 ? Format::si(s.transitTime, QStringLiteral("s"), 3) : QStringLiteral("—"); }, [](const DeviceState &s) { return s.transitTime; }},
        {tr("Depletion depth, source / drain end"), [](const DeviceState &s, const MosfetConfig &) { return um(s.gateDepletion.front()) + QStringLiteral(" / ") + um(s.gateDepletion.back()); }, none},
        {tr("Junction depletion S / D"), [](const DeviceState &s, const MosfetConfig &) { return um(s.sourceJunctionDepletion) + QStringLiteral(" / ") + um(s.drainJunctionDepletion); }, none},
        {tr("VTH0 physics estimate"), [](const DeviceState &s, const MosfetConfig &) { return volts(s.physicalThreshold); }, [](const DeviceState &s) { return s.physicalThreshold; }},
    };
    const int columns = m_compare ? 4 : 2;
    m_results->setColumnCount(columns);
    m_results->setHorizontalHeaderLabels(m_compare ? QStringList{tr("Quantity"), QStringLiteral("A"), QStringLiteral("B"), QStringLiteral("B/A")}
                                                   : QStringList{tr("Quantity"), tr("Value")});
    m_results->setRowCount(int(rows.size()));
    for (int r = 0; r < int(rows.size()); ++r) {
        auto set = [&](int column, const QString &text, const QColor &color = QColor()) {
            auto *item = new QTableWidgetItem(text);
            if (color.isValid())
                item->setForeground(color);
            if (column > 0)
                item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            m_results->setItem(r, column, item);
        };
        const Row &row = rows[std::size_t(r)];
        set(0, row.name);
        for (int s = 0; s < (m_compare ? 2 : 1); ++s) {
            const Slot &slot = m_slots[std::size_t(s)];
            set(1 + s, row.text(slot.state, slot.config), r == 0 ? regionQColor(slot.state.region) : QColor());
        }
        if (m_compare) {
            const double a = row.number(m_slots[0].state), b = row.number(m_slots[1].state);
            set(3, std::isfinite(a) && std::isfinite(b) && a != 0.0 ? QStringLiteral("×") + Format::number(b / a, 2) : QStringLiteral("—"),
                QColor(0x8a, 0x94, 0xa3));
        }
    }
    if (m_compare) {
        m_results->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Interactive);
        m_results->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
        m_results->setColumnWidth(0, 104);
    } else {
        m_results->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    }
    for (int col = 1; col < columns; ++col)
        m_results->horizontalHeader()->setSectionResizeMode(col, m_compare ? QHeaderView::Stretch : QHeaderView::ResizeToContents);
    m_results->setTextElideMode(Qt::ElideRight);
    m_results->setStyleSheet(m_compare ? QStringLiteral("QTableWidget { font-size: 8pt; } QTableWidget::item { padding: 0 3px; }"
                                                        "QHeaderView::section { padding: 4px 3px; font-size: 8pt; }")
                                       : QStringLiteral("QTableWidget { font-size: 9pt; } QTableWidget::item { padding: 0 6px; }"
                                                        "QHeaderView::section { padding: 4px 6px; font-size: 8.5pt; }"));
}

void MosfetPage::refreshProbe()
{
    struct Row { QString name; std::function<QString(const DeviceState &, double)> text; };
    const std::vector<Row> rows = {
        {tr("Position x"), [](const DeviceState &, double x) { return um(x); }},
        {tr("Channel potential V(x)"), [](const DeviceState &s, double x) { return s.channelExists ? volts(s.at(s.potential, x)) : QStringLiteral("—"); }},
        {tr("|Qinv(x)|"), [](const DeviceState &s, double x) { return Format::si(s.at(s.inversionCharge, x) * 1e-4, QStringLiteral("C/cm²"), 3); }},
        {tr("Electron sheet density ns"), [](const DeviceState &s, double x) {
             return dopingText(std::max(s.at(s.electronSheet, x) * 1.0e2, 1.0)).replace(QStringLiteral("cm⁻³"), QStringLiteral("cm⁻²")); }},
        {tr("n at the surface (approx.)"), [](const DeviceState &s, double x) { return dopingText(std::max(s.at(s.electronDensity, x), 1.0)); }},
        {tr("Lateral field |Ex|"), [](const DeviceState &s, double x) { return Format::number(std::fabs(s.at(s.lateralField, x)) * 1e-5, 3) + QStringLiteral(" kV/cm"); }},
        {tr("Drift velocity v"), [](const DeviceState &s, double x) { return Format::si(s.at(s.velocity, x), QStringLiteral("m/s"), 3); }},
        {tr("Current density |Jn| (surface)"), [](const DeviceState &s, double x) { return Format::si(s.at(s.currentDensity, x) * 1e-4, QStringLiteral("A/cm²"), 3); }},
        {tr("Depletion depth"), [](const DeviceState &s, double x) { return um(s.at(s.gateDepletion, x)); }},
        {tr("Surface potential ψs"), [](const DeviceState &s, double x) { return volts(s.at(s.surfacePotential, x)); }},
    };
    m_probe->setColumnCount(m_compare ? 3 : 2);
    m_probe->setHorizontalHeaderLabels(m_compare ? QStringList{tr("At the probe"), QStringLiteral("A"), QStringLiteral("B")}
                                                 : QStringList{tr("At the probe"), tr("Value")});
    m_probe->setRowCount(int(rows.size()));
    for (int r = 0; r < int(rows.size()); ++r) {
        m_probe->setItem(r, 0, new QTableWidgetItem(rows[std::size_t(r)].name));
        for (int s = 0; s < (m_compare ? 2 : 1); ++s) {
            const Slot &slot = m_slots[std::size_t(s)];
            const double x = m_probeFraction * slot.config.device.channelLength;
            auto *item = new QTableWidgetItem(rows[std::size_t(r)].text(slot.state, x));
            item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            m_probe->setItem(r, 1 + s, item);
        }
    }
    for (int col = 1; col < m_probe->columnCount(); ++col)
        m_probe->horizontalHeader()->setSectionResizeMode(col, QHeaderView::ResizeToContents);
}

// ===========================================================================
// Sweep and animation
// ===========================================================================

void MosfetPage::applySweepValue(double value, bool record)
{
    const QString key = m_sweep.variable == 0 ? QStringLiteral("vds") : QStringLiteral("vgs");
    const ParamSpec *spec = findParameter(key.toStdString());
    const OperatingRegion before = m_slots[0].state.region;
    const DeviceState previous = m_slots[0].state;
    // The sweep drives both configurations so a comparison stays synchronised.
    for (int t : (m_compare ? std::vector<int>{0, 1} : std::vector<int>{0})) {
        spec->set(m_slots[std::size_t(t)].config, value);
        recompute(t);
    }
    const DeviceState &s = m_slots[0].state;
    if (record) {
        SweepSample sample;
        sample.time = m_sweep.time;
        sample.vgs = m_slots[0].config.bias.vgs;
        sample.vds = m_slots[0].config.bias.vds;
        sample.id = s.drainCurrent;
        sample.pinchOff = s.pinchedOff ? s.pinchOffPosition : -1.0;
        m_sweep.samples.push_back(sample);
    }
    if (s.region != before)
        setExplanation(Explainer::describeChange(QString::fromUtf8(spec->name),
                                                 formatParameter(*spec, m_sweep.variable == 0 ? previous.bias.vds : previous.bias.vgs),
                                                 formatParameter(*spec, value), previous, s, spec->effects));
    refreshPanels();
}

void MosfetPage::startSweep()
{
    m_sweep.variable = m_sweepVariable->currentIndex();
    m_sweep.from = m_sweepFrom->value();
    m_sweep.to = m_sweepTo->value();
    m_sweep.duration = m_sweepDuration->value();
    m_sweep.step = m_sweepStep->value();
    if (m_sweep.time >= m_sweep.duration || m_sweep.samples.empty()) {
        m_sweep.time = 0.0;
        m_sweep.samples.clear();
        applySweepValue(m_sweep.from, true);
    }
    m_sweep.running = true;
    m_playButton->setText(tr("Pause"));
    refreshSweepReadout();
}

void MosfetPage::pauseSweep()
{
    m_sweep.running = false;
    m_playButton->setText(tr("Play"));
    refreshSweepReadout();
}

void MosfetPage::resetSweep()
{
    pauseSweep();
    m_sweep.time = 0.0;
    m_sweep.samples.clear();
    m_sweep.variable = m_sweepVariable->currentIndex();
    m_sweep.from = m_sweepFrom->value();
    m_sweep.to = m_sweepTo->value();
    m_sweep.duration = m_sweepDuration->value();
    applySweepValue(m_sweep.from, false);
}

void MosfetPage::stepSweep()
{
    pauseSweep();
    m_sweep.variable = m_sweepVariable->currentIndex();
    m_sweep.from = m_sweepFrom->value();
    m_sweep.to = m_sweepTo->value();
    m_sweep.duration = m_sweepDuration->value();
    m_sweep.step = m_sweepStep->value();
    const double current = m_sweep.variable == 0 ? m_slots[0].config.bias.vds : m_slots[0].config.bias.vgs;
    const double direction = m_sweep.to >= m_sweep.from ? 1.0 : -1.0;
    double next = current + direction * m_sweep.step;
    if (direction * (next - m_sweep.to) > 0)
        next = m_sweep.to;
    const double span = m_sweep.to - m_sweep.from;
    m_sweep.time = span != 0.0 ? m_sweep.duration * std::clamp((next - m_sweep.from) / span, 0.0, 1.0) : 0.0;
    applySweepValue(next, true);
}

void MosfetPage::runGuidedPinchOff()
{
    MosfetConfig config = m_slots[0].config;
    config.bias.vds = 0.0;
    config.bias.vbs = 0.0;
    const DeviceState probe = m_model.solve(config.device, config.bias);
    config.bias.vgs = std::min(6.0, probe.threshold + 1.0);
    applyConfigToTargets(config, tr("Guided pinch-off experiment"),
                         tr("VGS is set 1 V above VTH, so VOV = 1 V. VDS now ramps from 0 to 3 V. Below 1 V the channel "
                            "thins towards the drain; at VDS = VOV it pinches off at the drain; beyond that the pinch-off "
                            "point moves towards the source while ID levels off."));
    m_sweepVariable->setCurrentIndex(0);
    m_sweepFrom->setValue(0.0);
    m_sweepTo->setValue(3.0);
    m_sweepDuration->setValue(15.0);
    m_graphs[0]->setType(GraphType::IdVds);
    m_graphs[1]->setType(GraphType::Charge);
    m_sweep.samples.clear();
    m_sweep.time = 0.0;
    startSweep();
}

void MosfetPage::refreshSweepReadout()
{
    if (!m_sweepReadout)
        return;
    const DeviceState &s = m_slots[0].state;
    const MosfetConfig &c = m_slots[0].config;
    m_sweepReadout->setText(tr("<table cellspacing='3'>"
                               "<tr><td style='color:#8a94a3'>Time</td><td><b>%1 s</b> of %2 s %3</td></tr>"
                               "<tr><td style='color:#8a94a3'>VGS / VDS</td><td><b>%4</b> / <b>%5</b></td></tr>"
                               "<tr><td style='color:#8a94a3'>Pinch-off voltage</td><td>VDS,sat = VOV = <b>%6</b></td></tr>"
                               "<tr><td style='color:#8a94a3'>Pinch-off point</td><td><b>%7</b></td></tr>"
                               "<tr><td style='color:#8a94a3'>ID</td><td><b>%8</b></td></tr>"
                               "<tr><td style='color:#8a94a3'>Region</td><td style='color:%9'><b>%10</b></td></tr></table>")
                                .arg(QString::number(m_sweep.time, 'f', 1), QString::number(m_sweepDuration->value(), 'f', 0),
                                     m_sweep.running ? tr("(running)") : tr("(paused)"), volts(c.bias.vgs), volts(c.bias.vds),
                                     s.channelExists ? volts(s.saturationVoltage) : tr("— (cutoff)"),
                                     s.pinchedOff ? tr("%1 from the source (%2 % of L)").arg(um(s.pinchOffPosition),
                                                                                             QString::number(100.0 * s.pinchOffPosition / c.device.channelLength, 'f', 1))
                                                  : tr("none: the channel reaches the drain"),
                                     amps(s.drainCurrent), Explainer::regionColor(s.region), Explainer::regionName(s.region)));
}

void MosfetPage::advanceTime(double seconds)
{
    if (m_sweep.running) {
        m_sweep.time = std::min(m_sweep.duration, m_sweep.time + seconds * m_sweep.speed);
        const double value = m_sweep.from + (m_sweep.to - m_sweep.from) * (m_sweep.time / m_sweep.duration);
        applySweepValue(value, true);
        if (m_sweep.time >= m_sweep.duration)
            pauseSweep();
    }
    if (!m_paused) {
        for (int i = 0; i < (m_compare ? 2 : 1); ++i) {
            Slot &slot = m_slots[std::size_t(i)];
            slot.particles.advance(seconds, slot.config.render.picosecondsPerSecond);
            pushParticles(i);
        }
    }
}

void MosfetPage::pushParticles(int i)
{
    Slot &slot = m_slots[std::size_t(i)];
    const auto dots = slot.particles.dots(slot.view->clipPlane(), slot.view->explode());
    std::vector<SphereInstance> spheres;
    spheres.reserve(dots.size());
    for (const auto &d : dots) {
        switch (d.kind) {
        case mosfet::CarrierParticles::ChannelElectron:
            spheres.push_back({d.position.x, d.position.y, d.position.z, 0.43f, 0.80f, 1.0f, 0.034f});
            break;
        case mosfet::CarrierParticles::BulkElectron:
            spheres.push_back({d.position.x, d.position.y, d.position.z, 0.30f, 0.58f, 1.0f, 0.026f});
            break;
        default:
            spheres.push_back({d.position.x, d.position.y, d.position.z, 1.0f, 0.38f, 0.38f, 0.028f});
            break;
        }
    }
    const QString legend = tr("Blue: electrons (channel dots each ≈ %1 electrons per µm of width; n+ dots illustrative) · "
                              "red: holes (illustrative positions). Real time shown: %2 ps per second.")
                               .arg(Format::number(slot.particles.electronsPerDotPerMicron(), 2),
                                    Format::number(slot.config.render.picosecondsPerSecond, 3));
    slot.view->setParticles(spheres, legend);
}

void MosfetPage::onFrame()
{
    const double seconds = std::min(0.1, m_clock.restart() / 1000.0);
    if (isVisible())
        advanceTime(seconds);
}

// ===========================================================================
// Dialogs
// ===========================================================================

void MosfetPage::showPhysicsCheck()
{
    const Slot &slot = m_slots[std::size_t(shownSlot())];
    PhysicsCheck::show(this, m_model, slot.config, slot.state);
}

void MosfetPage::showDimensions()
{
    const Slot &slot = m_slots[std::size_t(shownSlot())];
    const MosfetParameters &p = slot.config.device;
    const DeviceState &st = slot.state;
    const mosfet::DisplayScale &s = slot.geometry.scale();
    auto factor = [&](double drawn, double realMicrons) { return QStringLiteral("×") + Format::number(drawn / realMicrons, 3); };

    struct Row { QString quantity, real, drawn; };
    const std::vector<Row> rows = {
        {tr("Channel length L"), um(p.channelLength), tr("×1 (reference: 1 unit = 1 µm)")},
        {tr("Channel width W"), um(p.channelWidth), factor(slot.geometry.drawnWidth, p.channelWidth * 1e6) + tr(" (shortened; drawn width limited to 0.3–12)")},
        {tr("Gate oxide thickness tox"), Format::number(p.oxideThickness * 1e9, 3) + QStringLiteral(" nm"), QStringLiteral("×") + Format::number(s.oxide, 3) + tr(" (0.05 + 7·tox, exaggerated)")},
        {tr("Polysilicon gate thickness"), um(p.gateThickness), factor(std::clamp(float(p.gateThickness * 1e6) * s.gate, 0.12f, 1.5f), p.gateThickness * 1e6)},
        {tr("Depths in the silicon (xj, depletion)"), um(p.junctionDepth), QStringLiteral("×") + Format::number(s.siliconDepth, 3) + tr(" (reduced when depletion is very deep)")},
        {tr("Source / drain length"), um(p.sourceLength) + QStringLiteral(" / ") + um(p.drainLength), tr("×1")},
        {tr("Junction depletion, p side (S / D)"), um(st.sourceJunctionDepletion) + QStringLiteral(" / ") + um(st.drainJunctionDepletion) + tr(" calculated"),
         tr("×%1 down, ×1 sideways").arg(Format::number(s.siliconDepth, 3))},
        {tr("Inversion layer"), tr("a few nm"), tr("thickness drawn from the calculated charge (illustrative mapping)")},
        {tr("Substrate thickness"), um(p.substrateThickness), tr("truncated: drawn %1 units deep").arg(Format::number(s.substrateDepth, 3))}};

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Dimensions and scale"));
    dialog.resize(760, 420);
    auto *table = new QTableWidget(int(rows.size()), 3);
    table->setHorizontalHeaderLabels({tr("Quantity"), tr("Real device"), tr("In the drawing")});
    table->verticalHeader()->setVisible(false);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionMode(QAbstractItemView::NoSelection);
    table->setShowGrid(false);
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    for (int r = 0; r < int(rows.size()); ++r) {
        table->setItem(r, 0, new QTableWidgetItem(rows[std::size_t(r)].quantity));
        table->setItem(r, 1, new QTableWidgetItem(rows[std::size_t(r)].real));
        table->setItem(r, 2, new QTableWidgetItem(rows[std::size_t(r)].drawn));
    }
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    auto *layout = new QVBoxLayout(&dialog);
    layout->addWidget(table, 1);
    layout->addWidget(buttons);
    dialog.exec();
}
