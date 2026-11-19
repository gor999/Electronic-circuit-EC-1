#include "Elements.h"

#include "Circuit.h"
#include "Format.h"

#include <algorithm>
#include <cmath>

namespace {

const double kThermalVoltage = 0.025865;

ParamInfo numberParam(const QString &name, const QString &unit, double value, bool positive = false)
{
    ParamInfo info;
    info.name = name;
    info.unit = unit;
    info.value = value;
    info.mustBePositive = positive;
    return info;
}

// Extra junction conductance that is switched in when Newton's method
// struggles, to help it settle.
double convergenceAid(const Circuit &c)
{
    if (c.subIteration <= 100)
        return 0.0;
    return std::min(0.1, std::exp(-9.0 * std::log(10.0) * (1.0 - c.subIteration / 3000.0)));
}

} // namespace

// ---- Junction model -----------------------------------------------------------

void JunctionModel::setup(double saturation, double emission)
{
    saturationCurrent = saturation;
    thermalVoltage = emission * kThermalVoltage;
    criticalVoltage = thermalVoltage * std::log(thermalVoltage / (std::sqrt(2.0) * saturation));
}

// Limits how far the junction voltage may move in one iteration so that the
// exponential cannot overflow.
double JunctionModel::limit(double newVoltage, double oldVoltage, Circuit &c) const
{
    if (newVoltage > criticalVoltage && std::fabs(newVoltage - oldVoltage) > 2.0 * thermalVoltage) {
        if (oldVoltage > 0.0) {
            const double argument = 1.0 + (newVoltage - oldVoltage) / thermalVoltage;
            newVoltage = argument > 0.0 ? oldVoltage + thermalVoltage * std::log(argument)
                                        : criticalVoltage;
        } else {
            newVoltage = thermalVoltage * std::log(newVoltage / thermalVoltage);
        }
        c.converged = false;
    }
    return newVoltage;
}

void JunctionModel::doStep(Circuit &c, int n0, int n1, double voltage)
{
    if (std::fabs(voltage - lastVoltage) > 0.01)
        c.converged = false;
    voltage = limit(voltage, lastVoltage, c);
    lastVoltage = voltage;

    // Linearise i = Is * (exp(v / nVt) - 1) around the operating point.
    const double coefficient = 1.0 / thermalVoltage;
    const double exponential = std::exp(voltage * coefficient);
    const double conductance = coefficient * saturationCurrent * exponential + convergenceAid(c);
    const double offsetCurrent = (exponential - 1.0) * saturationCurrent - conductance * voltage;
    c.stampConductance(n0, n1, conductance);
    c.stampCurrentSource(n0, n1, offsetCurrent);
}

double JunctionModel::current(double voltage) const
{
    return saturationCurrent * (std::exp(voltage / thermalVoltage) - 1.0);
}

// ---- Diode / LED --------------------------------------------------------------

DiodeElm::DiodeElm(bool led)
    : isLed(led)
{
    if (led)
        m_junction.setup(7.8e-18, 2.0);     // about 1.8 V at 10 mA
    else
        m_junction.setup(1.0e-14, 1.0);     // about 0.65 V at 1 mA
}

QStringList DiodeElm::ledColorNames()
{
    return {QStringLiteral("Red"), QStringLiteral("Green"), QStringLiteral("Blue"),
            QStringLiteral("Yellow")};
}

double DiodeElm::brightness() const
{
    return qBound(0.0, current / 0.02, 1.0);
}

void DiodeElm::reset()
{
    Element::reset();
    m_junction.lastVoltage = 0.0;
}

void DiodeElm::doStep(Circuit &c)
{
    m_junction.doStep(c, nodes[0], nodes[1], volts[0] - volts[1]);
}

void DiodeElm::stepFinished(Circuit &)
{
    current = m_junction.current(volts[0] - volts[1]);
}

QList<ParamInfo> DiodeElm::params() const
{
    if (!isLed)
        return {};
    ParamInfo info;
    info.name = QStringLiteral("Colour");
    info.kind = ParamInfo::Choice;
    info.choices = ledColorNames();
    info.value = color;
    return {info};
}

void DiodeElm::setParam(int index, double value)
{
    if (isLed && index == 0)
        color = qBound(0, static_cast<int>(value), static_cast<int>(ledColorNames().size()) - 1);
}

// ---- Bipolar transistor (Ebers-Moll) ------------------------------------------

BjtElm::BjtElm(bool pnp)
    : isPnp(pnp)
{
}

QPoint BjtElm::post(int index) const
{
    // The emitter of a PNP is drawn on the upper side, as is conventional.
    const double collectorSide = isPnp ? 16.0 : -16.0;
    switch (index) {
    case 0: return p1;
    case 1: return sidePoint(p2, collectorSide);
    default: return sidePoint(p2, -collectorSide);
    }
}

void BjtElm::reset()
{
    Element::reset();
    ib = ic = ie = 0.0;
    m_lastVbc = m_lastVbe = 0.0;
}

void BjtElm::doStep(Circuit &c)
{
    const double saturation = 1.0e-13;
    const double coefficient = 1.0 / kThermalVoltage;
    const double forwardGain = beta / (beta + 1.0);
    const double reverseGain = 0.5;
    const double sign = isPnp ? -1.0 : 1.0;

    JunctionModel limiter;
    limiter.setup(saturation, 1.0);

    double vbc = volts[0] - volts[1];
    double vbe = volts[0] - volts[2];
    if (std::fabs(vbc - m_lastVbc) > 0.01 || std::fabs(vbe - m_lastVbe) > 0.01)
        c.converged = false;
    vbc = sign * limiter.limit(sign * vbc, sign * m_lastVbc, c);
    vbe = sign * limiter.limit(sign * vbe, sign * m_lastVbe, c);
    m_lastVbc = vbc;
    m_lastVbe = vbe;

    const double expBc = std::exp(vbc * coefficient * sign);
    const double expBe = std::exp(vbe * coefficient * sign);
    ie = sign * saturation * (-(expBe - 1.0) / forwardGain + (expBc - 1.0));
    ic = sign * saturation * ((expBe - 1.0) - (expBc - 1.0) / reverseGain);
    ib = -(ie + ic);

    // Partial derivatives of the terminal currents.
    const double aid = convergenceAid(c);
    const double gee = -saturation * coefficient * expBe / forwardGain - aid;   // d ie / d vbe
    const double gcc = -saturation * coefficient * expBc / reverseGain - aid;   // d ic / d vbc
    const double gec = saturation * coefficient * expBc;                        // d ie / d vbc
    const double gce = saturation * coefficient * expBe;                        // d ic / d vbe

    const int b = nodes[0], col = nodes[1], e = nodes[2];
    c.stampMatrix(b, b, -gee - gec - gce - gcc);
    c.stampMatrix(b, col, gec + gcc);
    c.stampMatrix(b, e, gee + gce);
    c.stampMatrix(col, b, gce + gcc);
    c.stampMatrix(col, col, -gcc);
    c.stampMatrix(col, e, -gce);
    c.stampMatrix(e, b, gee + gec);
    c.stampMatrix(e, col, -gec);
    c.stampMatrix(e, e, -gee);

    c.stampRightSide(b, -ib - (gec + gcc) * vbc - (gee + gce) * vbe);
    c.stampRightSide(col, -ic + gce * vbe + gcc * vbc);
    c.stampRightSide(e, -ie + gee * vbe + gec * vbc);
}

void BjtElm::stepFinished(Circuit &)
{
    current = ic;
}

double BjtElm::postCurrent(int index) const
{
    return index == 0 ? -ib : index == 1 ? -ic : -ie;
}

double BjtElm::power() const
{
    return (volts[0] - volts[2]) * ib + (volts[1] - volts[2]) * ic;
}

double BjtElm::dotCurrent(int channel) const
{
    // Channel 0: base lead towards the body, 1: collector lead towards the
    // body, 2: emitter lead away from the body.
    return channel == 0 ? ib : channel == 1 ? ic : -ie;
}

QList<ParamInfo> BjtElm::params() const
{
    return {numberParam(QStringLiteral("Current gain (β)"), QString(), beta, true)};
}

void BjtElm::setParam(int index, double value)
{
    if (index == 0 && value > 0.0)
        beta = value;
}

QStringList BjtElm::info() const
{
    return {name() + QStringLiteral(" (β = %1)").arg(Format::number(beta, 4)),
            QStringLiteral("Vbe = ") + volt(volts[0] - volts[2]),
            QStringLiteral("Vce = ") + volt(volts[1] - volts[2]),
            QStringLiteral("Ic = ") + amp(ic),
            QStringLiteral("Ib = ") + amp(ib),
            QStringLiteral("P = ") + watt(power())};
}

// ---- MOSFET (square-law model) --------------------------------------------------

MosfetElm::MosfetElm(bool pChannel)
    : isP(pChannel)
{
}

QPoint MosfetElm::post(int index) const
{
    // The source of a PMOS is drawn on the upper side, as is conventional.
    const double drainSide = isP ? 16.0 : -16.0;
    switch (index) {
    case 0: return p1;
    case 1: return sidePoint(p2, drainSide);
    default: return sidePoint(p2, -drainSide);
    }
}

void MosfetElm::reset()
{
    Element::reset();
    m_last[0] = m_last[1] = m_last[2] = 0.0;
}

void MosfetElm::doStep(Circuit &c)
{
    const double sign = isP ? -1.0 : 1.0;
    double v[3] = {volts[0], volts[1], volts[2]};

    for (int i = 0; i < 3; ++i) {
        if (std::fabs(v[i] - m_last[i]) > 0.01)
            c.converged = false;
    }
    // Limit the change of the channel voltages per iteration.
    for (int i = 1; i < 3; ++i)
        v[i] = qBound(m_last[i] - 0.5, v[i], m_last[i] + 0.5);
    for (int i = 0; i < 3; ++i)
        m_last[i] = v[i];

    // The device is symmetric: whichever channel end is at the lower
    // potential (for N-channel) acts as the source.
    int drain = 1, source = 2;
    if (sign * v[source] > sign * v[drain])
        std::swap(drain, source);

    const double realVgs = v[0] - v[source];
    const double realVds = v[drain] - v[source];
    const double vgs = sign * realVgs;
    const double vds = sign * realVds;

    const double leakage = 1.0e-8;
    double ids = 0.0, gm = 0.0, gds = 0.0;
    if (vgs < threshold) {
        // Cut-off.
        gds = leakage;
        ids = vds * gds;
    } else if (vds < vgs - threshold) {
        // Linear (triode) region.
        ids = beta * ((vgs - threshold) * vds - vds * vds * 0.5);
        gm = beta * vds;
        gds = beta * (vgs - vds - threshold);
    } else {
        // Saturation.
        gm = beta * (vgs - threshold);
        gds = leakage;
        ids = 0.5 * beta * (vgs - threshold) * (vgs - threshold) + (vds - (vgs - threshold)) * gds;
    }

    const double rightSide = -sign * ids + gds * realVds + gm * realVgs;
    const int g = nodes[0], d = nodes[drain], s = nodes[source];
    c.stampMatrix(d, d, gds);
    c.stampMatrix(d, s, -gds - gm);
    c.stampMatrix(d, g, gm);
    c.stampMatrix(s, d, -gds);
    c.stampMatrix(s, s, gds + gm);
    c.stampMatrix(s, g, -gm);
    c.stampRightSide(d, rightSide);
    c.stampRightSide(s, -rightSide);

    const double intoActingDrain = sign * ids;
    current = (drain == 1) ? intoActingDrain : -intoActingDrain;
}

double MosfetElm::postCurrent(int index) const
{
    return index == 1 ? -current : index == 2 ? current : 0.0;
}

QList<ParamInfo> MosfetElm::params() const
{
    return {numberParam(QStringLiteral("Threshold voltage"), QStringLiteral("V"), threshold, true),
            numberParam(QStringLiteral("Transconductance parameter (β)"), QStringLiteral("A/V²"), beta, true)};
}

void MosfetElm::setParam(int index, double value)
{
    if (value <= 0.0)
        return;
    if (index == 0)
        threshold = value;
    else if (index == 1)
        beta = value;
}

QStringList MosfetElm::info() const
{
    return {name() + QStringLiteral(" (Vt = %1)").arg(volt(threshold)),
            QStringLiteral("Vgs = ") + volt(volts[0] - volts[2]),
            QStringLiteral("Vds = ") + volt(volts[1] - volts[2]),
            QStringLiteral("Id = ") + amp(current),
            QStringLiteral("P = ") + watt(power())};
}

// ---- Op-amp ---------------------------------------------------------------------

QPoint OpAmpElm::post(int index) const
{
    switch (index) {
    case 0: return sidePoint(p1, -16.0);
    case 1: return sidePoint(p1, 16.0);
    default: return p2;
    }
}

void OpAmpElm::reset()
{
    Element::reset();
    m_lastDifference = 0.0;
}

void OpAmpElm::stamp(Circuit &c)
{
    // The output is a controlled voltage source to ground; its control
    // equation is rewritten every iteration in doStep().
    c.stampMatrix(nodes[2], c.sourceRow(voltSource), 1.0);
}

void OpAmpElm::doStep(Circuit &c)
{
    const double difference = volts[1] - volts[0];
    if (std::fabs(m_lastDifference - difference) > 0.1)
        c.converged = false;
    else if (volts[2] > maxOutput + 0.1 || volts[2] < minOutput - 0.1)
        c.converged = false;

    // vout = slope * (v+ - v-) + intercept, with a nearly flat slope once the
    // output reaches a supply limit.
    double slope = gain;
    double intercept = 0.0;
    if (difference >= maxOutput / gain && m_lastDifference >= 0.0) {
        slope = 1.0e-4;
        intercept = maxOutput - slope * maxOutput / gain;
    } else if (difference <= minOutput / gain && m_lastDifference <= 0.0) {
        slope = 1.0e-4;
        intercept = minOutput - slope * minOutput / gain;
    }

    const int row = c.sourceRow(voltSource);
    c.stampMatrix(row, nodes[0], slope);
    c.stampMatrix(row, nodes[1], -slope);
    c.stampMatrix(row, nodes[2], 1.0);
    c.stampRightSide(row, intercept);
    m_lastDifference = difference;
}

QList<ParamInfo> OpAmpElm::params() const
{
    return {numberParam(QStringLiteral("Maximum output"), QStringLiteral("V"), maxOutput),
            numberParam(QStringLiteral("Minimum output"), QStringLiteral("V"), minOutput)};
}

void OpAmpElm::setParam(int index, double value)
{
    if (index == 0)
        maxOutput = value;
    else if (index == 1)
        minOutput = value;
    if (maxOutput < minOutput)
        std::swap(maxOutput, minOutput);
}

QStringList OpAmpElm::info() const
{
    return {name(),
            QStringLiteral("V+ = ") + volt(volts[1]),
            QStringLiteral("V− = ") + volt(volts[0]),
            QStringLiteral("Vout = ") + volt(volts[2]),
            QStringLiteral("Iout = ") + amp(current)};
}

// ---- Logic gates ------------------------------------------------------------------
// The output is a voltage source to ground. It follows the inputs with one
// time step of delay, which keeps feedback circuits (latches) stable.

GateElm::GateElm(Function fn)
    : function(fn)
{
}

QString GateElm::code() const
{
    switch (function) {
    case And: return QStringLiteral("and");
    case Nand: return QStringLiteral("nand");
    case Or: return QStringLiteral("or");
    case Nor: return QStringLiteral("nor");
    case Xor: return QStringLiteral("xor");
    default: return QStringLiteral("not");
    }
}

QString GateElm::name() const
{
    switch (function) {
    case And: return QStringLiteral("AND gate");
    case Nand: return QStringLiteral("NAND gate");
    case Or: return QStringLiteral("OR gate");
    case Nor: return QStringLiteral("NOR gate");
    case Xor: return QStringLiteral("XOR gate");
    default: return QStringLiteral("NOT gate");
    }
}

QPoint GateElm::post(int index) const
{
    if (index == inputCount())
        return p2;
    if (function == Not)
        return p1;
    return sidePoint(p1, index == 0 ? -16.0 : 16.0);
}

void GateElm::reset()
{
    Element::reset();
    m_output = 0.0;
}

void GateElm::stamp(Circuit &c)
{
    c.stampVoltageSource(0, nodes[inputCount()], voltSource);
}

void GateElm::startIteration(Circuit &)
{
    const double half = highVoltage * 0.5;
    const bool a = volts[0] > half;
    const bool b = inputCount() > 1 ? volts[1] > half : false;

    bool result = false;
    switch (function) {
    case And: result = a && b; break;
    case Nand: result = !(a && b); break;
    case Or: result = a || b; break;
    case Nor: result = !(a || b); break;
    case Xor: result = a != b; break;
    case Not: result = !a; break;
    }
    // Move halfway to the target each step: a short, finite edge.
    m_output += ((result ? highVoltage : 0.0) - m_output) * 0.5;
}

void GateElm::doStep(Circuit &c)
{
    c.setVoltageSourceValue(voltSource, m_output);
}

QList<ParamInfo> GateElm::params() const
{
    return {numberParam(QStringLiteral("High level"), QStringLiteral("V"), highVoltage, true)};
}

void GateElm::setParam(int index, double value)
{
    if (index == 0 && value > 0.0)
        highVoltage = value;
}

QStringList GateElm::info() const
{
    return {name(),
            QStringLiteral("Vout = ") + volt(volts[inputCount()]),
            QStringLiteral("Iout = ") + amp(current)};
}
