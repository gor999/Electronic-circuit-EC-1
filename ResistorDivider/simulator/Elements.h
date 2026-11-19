#pragma once

#include "Element.h"

// Concrete circuit elements. Implementations are split by family:
//   ElementsPassive.cpp  - wire, R, C, L, switch, ground, meters
//   ElementsSources.cpp  - voltage / current sources, rail, logic input
//   ElementsActive.cpp   - diode, LED, BJT, MOSFET, op-amp, logic gates

// ---------------------------------------------------------------------------
// Passive
// ---------------------------------------------------------------------------

class WireElm : public Element
{
public:
    Kind kind() const override { return Kind::Wire; }
    QString code() const override { return QStringLiteral("w"); }
    QString name() const override { return QStringLiteral("Wire"); }
    bool isWire() const override { return true; }
    QStringList info() const override;
};

class ResistorElm : public Element
{
public:
    double resistance = 1000.0;

    Kind kind() const override { return Kind::Resistor; }
    QString code() const override { return QStringLiteral("r"); }
    QString name() const override { return QStringLiteral("Resistor"); }
    void stamp(Circuit &c) override;
    void stepFinished(Circuit &c) override;
    QList<ParamInfo> params() const override;
    void setParam(int index, double value) override;
    QString valueText() const override;
};

class CapacitorElm : public Element
{
public:
    double capacitance = 1.0e-6;

    Kind kind() const override { return Kind::Capacitor; }
    QString code() const override { return QStringLiteral("c"); }
    QString name() const override { return QStringLiteral("Capacitor"); }
    void reset() override;
    void stamp(Circuit &c) override;
    void startIteration(Circuit &c) override;
    void doStep(Circuit &c) override;
    void stepFinished(Circuit &c) override;
    QList<ParamInfo> params() const override;
    void setParam(int index, double value) override;
    QString valueText() const override;

private:
    double m_companionResistance = 1.0;
    double m_sourceValue = 0.0;
    double m_voltage = 0.0;
};

class InductorElm : public Element
{
public:
    double inductance = 0.1;

    Kind kind() const override { return Kind::Inductor; }
    QString code() const override { return QStringLiteral("l"); }
    QString name() const override { return QStringLiteral("Inductor"); }
    void reset() override;
    void stamp(Circuit &c) override;
    void startIteration(Circuit &c) override;
    void doStep(Circuit &c) override;
    void stepFinished(Circuit &c) override;
    QList<ParamInfo> params() const override;
    void setParam(int index, double value) override;
    QString valueText() const override;

private:
    double m_companionResistance = 1.0;
    double m_sourceValue = 0.0;
    double m_voltage = 0.0;
};

class SwitchElm : public Element
{
public:
    bool closed = false;

    Kind kind() const override { return Kind::Switch; }
    QString code() const override { return QStringLiteral("s"); }
    QString name() const override { return QStringLiteral("Switch"); }
    int voltageSourceCount() const override { return closed ? 1 : 0; }
    void stamp(Circuit &c) override;
    void stepFinished(Circuit &c) override;
    QList<ParamInfo> params() const override;
    void setParam(int index, double value) override;
    QStringList info() const override;
    bool toggle() override;
};

class GroundElm : public Element
{
public:
    Kind kind() const override { return Kind::Ground; }
    QString code() const override { return QStringLiteral("g"); }
    QString name() const override { return QStringLiteral("Ground"); }
    int postCount() const override { return 1; }
    QPoint defaultSpan() const override { return QPoint(0, 32); }
    bool isGround() const override { return true; }
    double voltageDiff() const override { return volts[0]; }
    QStringList info() const override;
};

class VoltmeterElm : public Element
{
public:
    Kind kind() const override { return Kind::Voltmeter; }
    QString code() const override { return QStringLiteral("vm"); }
    QString name() const override { return QStringLiteral("Voltmeter"); }
    QString valueText() const override;
    QStringList info() const override;
};

class AmmeterElm : public Element
{
public:
    Kind kind() const override { return Kind::Ammeter; }
    QString code() const override { return QStringLiteral("am"); }
    QString name() const override { return QStringLiteral("Ammeter"); }
    int voltageSourceCount() const override { return 1; }
    void stamp(Circuit &c) override;
    QString valueText() const override;
    QStringList info() const override;
};

// ---------------------------------------------------------------------------
// Sources
// ---------------------------------------------------------------------------

struct Waveform
{
    enum Shape { Dc, Sine, Square, Triangle };

    int shape = Dc;
    double amplitude = 5.0;     // DC value, or peak value of the AC part
    double frequency = 50.0;
    double offset = 0.0;        // DC offset added to AC shapes

    double value(double time) const;
    QList<ParamInfo> params() const;
    void setParam(int index, double value);
    QString text() const;
};

// Two-terminal source: v(p2) - v(p1) = waveform.
class VoltageElm : public Element
{
public:
    Waveform wave;

    Kind kind() const override { return Kind::Voltage; }
    QString code() const override { return QStringLiteral("v"); }
    QString name() const override;
    int voltageSourceCount() const override { return 1; }
    void stamp(Circuit &c) override;
    void doStep(Circuit &c) override;
    double voltageDiff() const override { return volts[1] - volts[0]; }
    QList<ParamInfo> params() const override { return wave.params(); }
    void setParam(int index, double value) override { wave.setParam(index, value); }
    QString valueText() const override { return wave.text(); }
};

// One-terminal source referenced to ground.
class RailElm : public Element
{
public:
    Waveform wave;

    Kind kind() const override { return Kind::Rail; }
    QString code() const override { return QStringLiteral("rail"); }
    QString name() const override { return QStringLiteral("Voltage rail"); }
    int postCount() const override { return 1; }
    QPoint defaultSpan() const override { return QPoint(0, -32); }
    bool usesGroundNode() const override { return true; }
    int voltageSourceCount() const override { return 1; }
    void stamp(Circuit &c) override;
    void doStep(Circuit &c) override;
    double postCurrent(int) const override { return current; }
    double voltageDiff() const override { return volts[0]; }
    QList<ParamInfo> params() const override { return wave.params(); }
    void setParam(int index, double value) override { wave.setParam(index, value); }
    QString valueText() const override { return wave.text(); }
};

// One-terminal logic level; clicking it toggles between low and high.
class LogicInputElm : public Element
{
public:
    bool high = false;
    double highVoltage = 5.0;

    Kind kind() const override { return Kind::LogicInput; }
    QString code() const override { return QStringLiteral("logic"); }
    QString name() const override { return QStringLiteral("Logic input"); }
    int postCount() const override { return 1; }
    QPoint defaultSpan() const override { return QPoint(-32, 0); }
    bool usesGroundNode() const override { return true; }
    int voltageSourceCount() const override { return 1; }
    void stamp(Circuit &c) override;
    void doStep(Circuit &c) override;
    double postCurrent(int) const override { return current; }
    double voltageDiff() const override { return volts[0]; }
    QList<ParamInfo> params() const override;
    void setParam(int index, double value) override;
    QString valueText() const override;
    bool toggle() override;
};

// Current flows p1 -> p2 through the source.
class CurrentElm : public Element
{
public:
    double amps = 0.01;

    Kind kind() const override { return Kind::Current; }
    QString code() const override { return QStringLiteral("i"); }
    QString name() const override { return QStringLiteral("Current source"); }
    void stamp(Circuit &c) override;
    void stepFinished(Circuit &c) override;
    double voltageDiff() const override { return volts[1] - volts[0]; }
    QList<ParamInfo> params() const override;
    void setParam(int index, double value) override;
    QString valueText() const override;
};

// ---------------------------------------------------------------------------
// Semiconductors and active parts
// ---------------------------------------------------------------------------

// Shockley diode equation with Newton step limiting.
struct JunctionModel
{
    double saturationCurrent = 1.0e-14;
    double thermalVoltage = 0.025865;   // n * Vt
    double criticalVoltage = 0.0;
    double lastVoltage = 0.0;

    void setup(double saturation, double emission);
    double limit(double newVoltage, double oldVoltage, Circuit &c) const;
    void doStep(Circuit &c, int n0, int n1, double voltage);
    double current(double voltage) const;
};

// Anode at p1, cathode at p2. Also used for LEDs.
class DiodeElm : public Element
{
public:
    explicit DiodeElm(bool led);

    bool isLed = false;
    int color = 0;      // LED only: index into ledColorNames()

    static QStringList ledColorNames();
    double brightness() const;      // 0..1, LED only

    Kind kind() const override { return isLed ? Kind::Led : Kind::Diode; }
    QString code() const override { return isLed ? QStringLiteral("led") : QStringLiteral("d"); }
    QString name() const override { return isLed ? QStringLiteral("LED") : QStringLiteral("Diode"); }
    bool nonLinear() const override { return true; }
    void reset() override;
    void doStep(Circuit &c) override;
    void stepFinished(Circuit &c) override;
    QList<ParamInfo> params() const override;
    void setParam(int index, double value) override;

private:
    JunctionModel m_junction;
};

// Posts: 0 = base (p1), 1 = collector, 2 = emitter (either side of p2).
class BjtElm : public Element
{
public:
    explicit BjtElm(bool pnp);

    bool isPnp = false;
    double beta = 100.0;
    double ib = 0.0, ic = 0.0, ie = 0.0;    // into the terminals

    Kind kind() const override { return Kind::Bjt; }
    QString code() const override { return isPnp ? QStringLiteral("pnp") : QStringLiteral("npn"); }
    QString name() const override { return isPnp ? QStringLiteral("PNP transistor") : QStringLiteral("NPN transistor"); }
    int postCount() const override { return 3; }
    QPoint post(int index) const override;
    QPoint defaultSpan() const override { return QPoint(32, 0); }
    bool canFlip() const override { return true; }
    bool nonLinear() const override { return true; }
    void reset() override;
    void doStep(Circuit &c) override;
    void stepFinished(Circuit &c) override;
    double postCurrent(int index) const override;
    double voltageDiff() const override { return volts[1] - volts[2]; }     // Vce
    double power() const override;
    int dotChannels() const override { return 3; }
    double dotCurrent(int channel) const override;
    QList<ParamInfo> params() const override;
    void setParam(int index, double value) override;
    QString valueText() const override { return QString(); }
    QStringList info() const override;

private:
    double m_lastVbc = 0.0;
    double m_lastVbe = 0.0;
};

// Posts: 0 = gate (p1), 1 = drain, 2 = source (either side of p2).
class MosfetElm : public Element
{
public:
    explicit MosfetElm(bool pChannel);

    bool isP = false;
    double threshold = 1.5;
    double beta = 0.02;

    Kind kind() const override { return Kind::Mosfet; }
    QString code() const override { return isP ? QStringLiteral("pmos") : QStringLiteral("nmos"); }
    QString name() const override { return isP ? QStringLiteral("PMOS MOSFET") : QStringLiteral("NMOS MOSFET"); }
    int postCount() const override { return 3; }
    QPoint post(int index) const override;
    QPoint defaultSpan() const override { return QPoint(48, 0); }
    bool canFlip() const override { return true; }
    bool nonLinear() const override { return true; }
    void reset() override;
    void doStep(Circuit &c) override;
    double postCurrent(int index) const override;       // `current` flows drain -> source
    double voltageDiff() const override { return volts[1] - volts[2]; }     // Vds
    QList<ParamInfo> params() const override;
    void setParam(int index, double value) override;
    QStringList info() const override;

private:
    double m_last[3] = {0.0, 0.0, 0.0};
};

// Posts: 0 = inverting input, 1 = non-inverting input (either side of p1),
// 2 = output (p2). Finite gain with output limiting.
class OpAmpElm : public Element
{
public:
    double maxOutput = 15.0;
    double minOutput = -15.0;
    double gain = 100000.0;

    Kind kind() const override { return Kind::OpAmp; }
    QString code() const override { return QStringLiteral("opamp"); }
    QString name() const override { return QStringLiteral("Op-amp"); }
    int postCount() const override { return 3; }
    QPoint post(int index) const override;
    bool canFlip() const override { return true; }
    bool nonLinear() const override { return true; }
    bool usesGroundNode() const override { return true; }
    int voltageSourceCount() const override { return 1; }
    void reset() override;
    void stamp(Circuit &c) override;
    void doStep(Circuit &c) override;
    void setSourceCurrent(int, double amps) override { current = -amps; }
    double postCurrent(int index) const override { return index == 2 ? current : 0.0; }
    double voltageDiff() const override { return volts[2]; }
    QList<ParamInfo> params() const override;
    void setParam(int index, double value) override;
    QStringList info() const override;

private:
    double m_lastDifference = 0.0;
};

// Inputs either side of p1 (a single input at p1 for NOT), output at p2.
class GateElm : public Element
{
public:
    enum Function { And, Nand, Or, Nor, Xor, Not };

    explicit GateElm(Function function);

    Function function = And;
    double highVoltage = 5.0;

    int inputCount() const { return function == Not ? 1 : 2; }
    bool inverting() const { return function == Nand || function == Nor || function == Not; }
    bool outputHigh() const { return m_output > highVoltage * 0.5; }

    Kind kind() const override { return Kind::Gate; }
    QString code() const override;
    QString name() const override;
    int postCount() const override { return inputCount() + 1; }
    QPoint post(int index) const override;
    QPoint defaultSpan() const override { return QPoint(80, 0); }
    bool usesGroundNode() const override { return true; }
    int voltageSourceCount() const override { return 1; }
    void reset() override;
    void stamp(Circuit &c) override;
    void startIteration(Circuit &c) override;
    void doStep(Circuit &c) override;
    double postCurrent(int index) const override { return index == inputCount() ? current : 0.0; }
    double voltageDiff() const override { return volts[inputCount()]; }
    QList<ParamInfo> params() const override;
    void setParam(int index, double value) override;
    QStringList info() const override;

private:
    double m_output = 0.0;
};
