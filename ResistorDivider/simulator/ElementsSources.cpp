#include "Elements.h"

#include "Circuit.h"
#include "Format.h"

#include <cmath>

namespace {

const double kTwoPi = 6.283185307179586;

ParamInfo numberParam(const QString &name, const QString &unit, double value, bool positive = false)
{
    ParamInfo info;
    info.name = name;
    info.unit = unit;
    info.value = value;
    info.mustBePositive = positive;
    return info;
}

} // namespace

// ---- Waveform ---------------------------------------------------------------

double Waveform::value(double time) const
{
    if (shape == Dc)
        return amplitude;

    const double cycles = time * frequency;
    const double phase = cycles - std::floor(cycles);   // 0..1
    switch (shape) {
    case Sine:
        return offset + amplitude * std::sin(kTwoPi * phase);
    case Square:
        return offset + (phase < 0.5 ? amplitude : -amplitude);
    case Triangle:
        if (phase < 0.25)
            return offset + amplitude * (4.0 * phase);
        if (phase < 0.75)
            return offset + amplitude * (2.0 - 4.0 * phase);
        return offset + amplitude * (4.0 * phase - 4.0);
    default:
        return amplitude;
    }
}

QList<ParamInfo> Waveform::params() const
{
    ParamInfo shapeInfo;
    shapeInfo.name = QStringLiteral("Waveform");
    shapeInfo.kind = ParamInfo::Choice;
    shapeInfo.choices = {QStringLiteral("DC"), QStringLiteral("Sine"), QStringLiteral("Square"),
                         QStringLiteral("Triangle")};
    shapeInfo.value = shape;
    return {shapeInfo,
            numberParam(QStringLiteral("Voltage (DC value or peak)"), QStringLiteral("V"), amplitude),
            numberParam(QStringLiteral("Frequency"), QStringLiteral("Hz"), frequency, true),
            numberParam(QStringLiteral("DC offset (AC shapes)"), QStringLiteral("V"), offset)};
}

void Waveform::setParam(int index, double value)
{
    switch (index) {
    case 0:
        shape = qBound(0, static_cast<int>(value), 3);
        break;
    case 1:
        amplitude = value;
        break;
    case 2:
        if (value > 0.0)
            frequency = value;
        break;
    case 3:
        offset = value;
        break;
    default:
        break;
    }
}

QString Waveform::text() const
{
    QString result = Format::si(amplitude, QStringLiteral("V"));
    if (shape != Dc)
        result += QStringLiteral("  ") + Format::si(frequency, QStringLiteral("Hz"));
    return result;
}

// ---- Two-terminal voltage source ---------------------------------------------

QString VoltageElm::name() const
{
    switch (wave.shape) {
    case Waveform::Sine: return QStringLiteral("AC voltage source");
    case Waveform::Square: return QStringLiteral("Square wave source");
    case Waveform::Triangle: return QStringLiteral("Triangle wave source");
    default: return QStringLiteral("DC voltage source");
    }
}

void VoltageElm::stamp(Circuit &c)
{
    c.stampVoltageSource(nodes[0], nodes[1], voltSource);
}

void VoltageElm::doStep(Circuit &c)
{
    c.setVoltageSourceValue(voltSource, wave.value(c.time));
}

// ---- Voltage rail --------------------------------------------------------------

void RailElm::stamp(Circuit &c)
{
    c.stampVoltageSource(0, nodes[0], voltSource);
}

void RailElm::doStep(Circuit &c)
{
    c.setVoltageSourceValue(voltSource, wave.value(c.time));
}

// ---- Logic input ---------------------------------------------------------------

void LogicInputElm::stamp(Circuit &c)
{
    c.stampVoltageSource(0, nodes[0], voltSource);
}

void LogicInputElm::doStep(Circuit &c)
{
    c.setVoltageSourceValue(voltSource, high ? highVoltage : 0.0);
}

QList<ParamInfo> LogicInputElm::params() const
{
    ParamInfo state;
    state.name = QStringLiteral("High");
    state.kind = ParamInfo::Toggle;
    state.value = high ? 1.0 : 0.0;
    return {state, numberParam(QStringLiteral("High level"), QStringLiteral("V"), highVoltage, true)};
}

void LogicInputElm::setParam(int index, double value)
{
    if (index == 0)
        high = (value != 0.0);
    else if (index == 1 && value > 0.0)
        highVoltage = value;
}

QString LogicInputElm::valueText() const
{
    return high ? QStringLiteral("H") : QStringLiteral("L");
}

bool LogicInputElm::toggle()
{
    high = !high;
    return true;
}

// ---- Current source ------------------------------------------------------------

void CurrentElm::stamp(Circuit &c)
{
    c.stampCurrentSource(nodes[0], nodes[1], amps);
}

void CurrentElm::stepFinished(Circuit &)
{
    current = amps;
}

QList<ParamInfo> CurrentElm::params() const
{
    return {numberParam(QStringLiteral("Current"), QStringLiteral("A"), amps)};
}

void CurrentElm::setParam(int index, double value)
{
    if (index == 0)
        amps = value;
}

QString CurrentElm::valueText() const
{
    return Format::si(amps, QStringLiteral("A"));
}
