#include "Elements.h"

#include "Circuit.h"
#include "Format.h"

namespace {

ParamInfo numberParam(const QString &name, const QString &unit, double value, bool positive)
{
    ParamInfo info;
    info.name = name;
    info.unit = unit;
    info.value = value;
    info.mustBePositive = positive;
    return info;
}

const QString kOhm = QStringLiteral("Ω");

} // namespace

// ---- Wire -------------------------------------------------------------------

QStringList WireElm::info() const
{
    return {name(), QStringLiteral("V = ") + volt(volts[0]), QStringLiteral("I = ") + amp(current)};
}

// ---- Resistor ---------------------------------------------------------------

void ResistorElm::stamp(Circuit &c)
{
    c.stampResistor(nodes[0], nodes[1], resistance);
}

void ResistorElm::stepFinished(Circuit &)
{
    current = (volts[0] - volts[1]) / resistance;
}

QList<ParamInfo> ResistorElm::params() const
{
    return {numberParam(QStringLiteral("Resistance"), kOhm, resistance, true)};
}

void ResistorElm::setParam(int index, double value)
{
    if (index == 0 && value > 0.0)
        resistance = value;
}

QString ResistorElm::valueText() const
{
    return Format::si(resistance, kOhm);
}

// ---- Capacitor (trapezoidal companion model) --------------------------------
//
//   i = (2C / dt) * (v - v_old) - i_old
//
// which is a resistor of dt / 2C in parallel with a current source.

void CapacitorElm::reset()
{
    Element::reset();
    m_sourceValue = 0.0;
    m_voltage = 0.0;
}

void CapacitorElm::stamp(Circuit &c)
{
    m_companionResistance = c.timeStep / (2.0 * capacitance);
    c.stampResistor(nodes[0], nodes[1], m_companionResistance);
}

void CapacitorElm::startIteration(Circuit &)
{
    m_sourceValue = -m_voltage / m_companionResistance - current;
}

void CapacitorElm::doStep(Circuit &c)
{
    c.stampCurrentSource(nodes[0], nodes[1], m_sourceValue);
}

void CapacitorElm::stepFinished(Circuit &)
{
    m_voltage = volts[0] - volts[1];
    current = m_voltage / m_companionResistance + m_sourceValue;
}

QList<ParamInfo> CapacitorElm::params() const
{
    return {numberParam(QStringLiteral("Capacitance"), QStringLiteral("F"), capacitance, true)};
}

void CapacitorElm::setParam(int index, double value)
{
    if (index == 0 && value > 0.0)
        capacitance = value;
}

QString CapacitorElm::valueText() const
{
    return Format::si(capacitance, QStringLiteral("F"));
}

// ---- Inductor (trapezoidal companion model) ---------------------------------
//
//   i = i_old + (dt / 2L) * (v + v_old)

void InductorElm::reset()
{
    Element::reset();
    m_sourceValue = 0.0;
    m_voltage = 0.0;
}

void InductorElm::stamp(Circuit &c)
{
    m_companionResistance = 2.0 * inductance / c.timeStep;
    c.stampResistor(nodes[0], nodes[1], m_companionResistance);
}

void InductorElm::startIteration(Circuit &)
{
    m_sourceValue = m_voltage / m_companionResistance + current;
}

void InductorElm::doStep(Circuit &c)
{
    c.stampCurrentSource(nodes[0], nodes[1], m_sourceValue);
}

void InductorElm::stepFinished(Circuit &)
{
    m_voltage = volts[0] - volts[1];
    current = m_voltage / m_companionResistance + m_sourceValue;
}

QList<ParamInfo> InductorElm::params() const
{
    return {numberParam(QStringLiteral("Inductance"), QStringLiteral("H"), inductance, true)};
}

void InductorElm::setParam(int index, double value)
{
    if (index == 0 && value > 0.0)
        inductance = value;
}

QString InductorElm::valueText() const
{
    return Format::si(inductance, QStringLiteral("H"));
}

// ---- Switch -----------------------------------------------------------------
// Closed: an ideal 0 V source, so the current through it is known.
// Open: no connection at all.

void SwitchElm::stamp(Circuit &c)
{
    if (closed)
        c.stampVoltageSource(nodes[0], nodes[1], voltSource);
}

void SwitchElm::stepFinished(Circuit &)
{
    if (!closed)
        current = 0.0;
}

QList<ParamInfo> SwitchElm::params() const
{
    ParamInfo info;
    info.name = QStringLiteral("Closed");
    info.kind = ParamInfo::Toggle;
    info.value = closed ? 1.0 : 0.0;
    return {info};
}

void SwitchElm::setParam(int index, double value)
{
    if (index == 0)
        closed = (value != 0.0);
}

QStringList SwitchElm::info() const
{
    return {name(), closed ? QStringLiteral("closed") : QStringLiteral("open"),
            QStringLiteral("Vd = ") + volt(voltageDiff()), QStringLiteral("I = ") + amp(current)};
}

bool SwitchElm::toggle()
{
    closed = !closed;
    return true;
}

// ---- Ground -----------------------------------------------------------------

QStringList GroundElm::info() const
{
    return {name(), QStringLiteral("I = ") + amp(current)};
}

// ---- Meters -----------------------------------------------------------------

QString VoltmeterElm::valueText() const
{
    return volt(voltageDiff());
}

QStringList VoltmeterElm::info() const
{
    return {name(), QStringLiteral("V = ") + volt(voltageDiff())};
}

void AmmeterElm::stamp(Circuit &c)
{
    c.stampVoltageSource(nodes[0], nodes[1], voltSource);
}

QString AmmeterElm::valueText() const
{
    return amp(current);
}

QStringList AmmeterElm::info() const
{
    return {name(), QStringLiteral("I = ") + amp(current)};
}
