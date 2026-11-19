#include "RcFilterCalculator.h"

#include <cmath>
#include <stdexcept>
#include <string>

namespace {

const double kPi = 3.14159265358979323846;

void requirePositive(double value, const char *name)
{
    if (!std::isfinite(value) || value <= 0.0)
        throw std::invalid_argument(std::string(name) + " must be a positive number.");
}

} // namespace

double RcFilterCalculator::magnitude(RcFilterType type, double x)
{
    // std::hypot(1, x) = sqrt(1 + x^2) without overflow for large x.
    const double denominator = std::hypot(1.0, x);
    return type == RcFilterType::LowPass ? 1.0 / denominator : x / denominator;
}

double RcFilterCalculator::phaseRadians(RcFilterType type, double x)
{
    // Low-pass:  -arctan(x).  High-pass: 90 deg - arctan(x) = arctan(1 / x).
    return type == RcFilterType::LowPass ? -std::atan(x) : std::atan2(1.0, x);
}

RcFilterResult RcFilterCalculator::calculate(RcFilterType type, double resistance,
                                             double capacitance, double frequency,
                                             double inputAmplitude)
{
    requirePositive(resistance, "Resistance");
    requirePositive(capacitance, "Capacitance");
    requirePositive(frequency, "Frequency");
    requirePositive(inputAmplitude, "Input voltage amplitude");

    const double tau = resistance * capacitance;
    if (!std::isfinite(tau) || tau <= 0.0)
        throw std::invalid_argument("R x C is out of range; the cutoff frequency cannot be calculated.");

    RcFilterResult result;
    result.type = type;
    result.resistance = resistance;
    result.capacitance = capacitance;
    result.frequency = frequency;
    result.inputAmplitude = inputAmplitude;

    result.timeConstant = tau;
    result.angularCutoff = 1.0 / tau;
    result.cutoffFrequency = 1.0 / (2.0 * kPi * tau);

    result.angularFrequency = 2.0 * kPi * frequency;
    result.capacitiveReactance = 1.0 / (result.angularFrequency * capacitance);

    const double x = result.angularFrequency * tau;   // wRC
    result.magnitude = magnitude(type, x);
    result.gainDb = 20.0 * std::log10(result.magnitude);
    result.phaseRadians = phaseRadians(type, x);
    result.phaseDegrees = result.phaseRadians * 180.0 / kPi;
    result.outputAmplitude = inputAmplitude * result.magnitude;

    return result;
}
