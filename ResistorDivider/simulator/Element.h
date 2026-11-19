#pragma once

#include <QList>
#include <QPoint>
#include <QPointF>
#include <QString>
#include <QStringList>

#include <memory>

class Circuit;

// All element end points sit on this grid (in world pixels).
constexpr int kGridSize = 16;

// One editable property of an element, used by the property dialog and by
// the text serialization (values are written in this order).
struct ParamInfo
{
    enum Kind { Number, Choice, Toggle };

    QString name;
    QString unit;
    Kind kind = Number;
    double value = 0.0;
    bool mustBePositive = false;
    QStringList choices;
};

// Entry of the component palette / Draw menu.
struct ToolInfo
{
    QString code;       // passed to Element::create()
    QString label;
    QString category;
    QString shortcut;   // single key, may be empty
};

// Base class of every circuit element.
//
// An element is placed by two grid points (p1 -> p2), like in Falstad's
// simulator; its terminals ("posts") are derived from them. The electrical
// model is expressed through the stamp/doStep hooks that write into the
// Circuit's MNA matrix. This class has no dependency on QtWidgets; drawing is
// done separately by ElementPainter.
class Element
{
public:
    enum class Kind {
        Wire, Resistor, Capacitor, Inductor, Switch, Ground, Voltage, Rail, LogicInput,
        Current, Diode, Led, Bjt, Mosfet, OpAmp, Gate, Voltmeter, Ammeter
    };

    virtual ~Element() = default;

    virtual Kind kind() const = 0;
    virtual QString code() const = 0;   // tag used in saved circuits
    virtual QString name() const = 0;   // human readable

    // ---- Geometry ---------------------------------------------------------
    QPoint p1;
    QPoint p2;
    bool flip = false;          // mirrors three-terminal symbols
    int id = 0;                 // stable across undo/redo
    bool selected = false;      // editor state

    virtual int postCount() const { return 2; }
    virtual QPoint post(int index) const { return index == 0 ? p1 : p2; }
    virtual QPoint defaultSpan() const { return QPoint(64, 0); }
    virtual bool canFlip() const { return false; }

    static QPoint snapToGrid(const QPointF &point);
    // Grid point reached from `base` by moving `side` pixels perpendicular to
    // the p1 -> p2 direction (sign follows `flip`).
    QPoint sidePoint(const QPoint &base, double side) const;

    // ---- Electrical model -------------------------------------------------
    int nodes[3] = {0, 0, 0};
    double volts[3] = {0.0, 0.0, 0.0};
    int voltSource = -1;        // index of this element's first voltage source
    double current = 0.0;       // main current, flowing p1 -> p2 inside the element
    double dotPhase[3] = {0.0, 0.0, 0.0};   // animation state of the current dots

    virtual int voltageSourceCount() const { return 0; }
    virtual bool nonLinear() const { return false; }
    virtual bool isWire() const { return false; }       // joins its two posts into one node
    virtual bool isGround() const { return false; }
    virtual bool usesGroundNode() const { return false; }   // references node 0 without a ground symbol
    // Wires and grounds carry whatever current the rest of the circuit pushes
    // through them; it is recovered afterwards with Kirchhoff's current law.
    bool currentFromKcl() const { return isWire() || isGround(); }

    virtual void reset();
    virtual void stamp(Circuit &) {}            // constant matrix / right-side entries
    virtual void startIteration(Circuit &) {}   // once per time step, before solving
    virtual void doStep(Circuit &) {}           // every Newton iteration
    virtual void stepFinished(Circuit &) {}     // once per time step, after solving
    virtual void setSourceCurrent(int /*localIndex*/, double amps) { current = amps; }

    // Current flowing out of the element into the junction at post `index`.
    virtual double postCurrent(int index) const { return index == 0 ? -current : current; }
    void setPostCurrent(int index, double amps) { current = (index == 0) ? -amps : amps; }

    virtual double voltageDiff() const { return volts[0] - volts[1]; }
    virtual double power() const { return voltageDiff() * current; }
    virtual int dotChannels() const { return 1; }
    virtual double dotCurrent(int /*channel*/) const { return current; }

    // ---- Properties -------------------------------------------------------
    virtual QList<ParamInfo> params() const { return {}; }
    virtual void setParam(int /*index*/, double /*value*/) {}
    virtual QString valueText() const { return QString(); }   // shown on the schematic
    virtual QStringList info() const;                         // shown when hovering
    virtual bool toggle() { return false; }                   // click action (switches)

    // ---- Factory / serialization -----------------------------------------
    static std::unique_ptr<Element> create(const QString &code);
    static std::unique_ptr<Element> fromLine(const QString &line);
    QString toLine() const;
    static const QList<ToolInfo> &tools();

protected:
    static QString volt(double v);
    static QString amp(double a);
    static QString watt(double w);
};
