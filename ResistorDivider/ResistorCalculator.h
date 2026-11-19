#pragma once

#include <vector>

// Result of analysing a chain of resistors connected in series across a
// voltage source. All values are in base SI units (V, A, Ohm, W).
struct DividerResult
{
    double sourceVoltage = 0.0;
    double totalResistance = 0.0;
    double current = 0.0;
    double totalPower = 0.0;

    std::vector<double> resistances;   // R1..Rn
    std::vector<double> voltageDrops;  // drop across R1..Rn
    // Voltage at every connection point, measured against ground.
    // nodeVoltages[0] is the node before R1 (= source voltage),
    // nodeVoltages[i] is the node after Ri, nodeVoltages[n] is ground (0 V).
    std::vector<double> nodeVoltages;
};

// Pure calculation logic. It has no dependency on Qt, so it can be reused in
// a console program or covered by unit tests without a GUI.
class ResistorCalculator
{
public:
    // Throws std::invalid_argument when the input cannot describe a valid
    // divider (no resistors, non-positive or non-finite values).
    static DividerResult calculate(double sourceVoltage,
                                   const std::vector<double> &resistances);

    static double totalResistance(const std::vector<double> &resistances);
};
