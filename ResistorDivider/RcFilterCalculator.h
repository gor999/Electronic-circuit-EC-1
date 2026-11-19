#pragma once

enum class RcFilterType { LowPass, HighPass };

// Result of analysing a first-order passive RC filter at one frequency.
// All values are in base SI units (Ohm, F, Hz, V, s, rad).
struct RcFilterResult
{
    RcFilterType type = RcFilterType::LowPass;

    // Inputs
    double resistance = 0.0;
    double capacitance = 0.0;
    double frequency = 0.0;
    double inputAmplitude = 0.0;

    // Properties of the filter itself
    double timeConstant = 0.0;      // tau = R * C
    double angularCutoff = 0.0;     // wc = 1 / (R * C)
    double cutoffFrequency = 0.0;   // fc = 1 / (2 * pi * R * C)

    // Behaviour at the selected frequency
    double angularFrequency = 0.0;      // w = 2 * pi * f
    double capacitiveReactance = 0.0;   // Xc = 1 / (w * C)
    double magnitude = 0.0;             // |H(jw)|
    double gainDb = 0.0;                // 20 * log10(|H|)
    double phaseRadians = 0.0;
    double phaseDegrees = 0.0;
    double outputAmplitude = 0.0;       // Vin * |H|
};

// Pure calculation logic for RC low-pass and high-pass filters (no Qt).
//
//   Low-pass:   H(jw) = 1 / (1 + jwRC)
//   High-pass:  H(jw) = jwRC / (1 + jwRC)
class RcFilterCalculator
{
public:
    // Throws std::invalid_argument for non-positive or non-finite input.
    static RcFilterResult calculate(RcFilterType type, double resistance, double capacitance,
                                    double frequency, double inputAmplitude);

    // Magnitude and phase as a function of x = w * R * C = f / fc.
    static double magnitude(RcFilterType type, double normalizedFrequency);
    static double phaseRadians(RcFilterType type, double normalizedFrequency);
};
