#include "ResistorCalculator.h"

#include <cmath>
#include <stdexcept>
#include <string>

double ResistorCalculator::totalResistance(const std::vector<double> &resistances)
{
    double total = 0.0;
    for (double r : resistances)
        total += r;
    return total;
}

DividerResult ResistorCalculator::calculate(double sourceVoltage,
                                            const std::vector<double> &resistances)
{
    if (resistances.empty())
        throw std::invalid_argument("At least one resistor is required.");

    if (!std::isfinite(sourceVoltage) || sourceVoltage <= 0.0)
        throw std::invalid_argument("Source voltage must be a positive number.");

    for (std::size_t i = 0; i < resistances.size(); ++i) {
        if (!std::isfinite(resistances[i]) || resistances[i] <= 0.0) {
            throw std::invalid_argument("Resistor R" + std::to_string(i + 1)
                                        + " must be a positive number.");
        }
    }

    const double total = totalResistance(resistances);
    if (!std::isfinite(total) || total <= 0.0)
        throw std::invalid_argument("Total resistance must be greater than zero.");

    DividerResult result;
    result.sourceVoltage = sourceVoltage;
    result.resistances = resistances;
    result.totalResistance = total;
    result.current = sourceVoltage / total;          // I = V / R_total
    result.totalPower = sourceVoltage * result.current;

    const std::size_t n = resistances.size();
    result.voltageDrops.resize(n);
    result.nodeVoltages.resize(n + 1);

    for (std::size_t i = 0; i < n; ++i)
        result.voltageDrops[i] = result.current * resistances[i];   // Vn = I * Rn

    // The voltage at a node equals the drop across everything still below it:
    // V_node(i) = I * (R(i+1) + ... + Rn). This is the same as subtracting each
    // drop from the previous node, but building it up from ground avoids
    // accumulated rounding error, so the last node is exactly 0 V.
    result.nodeVoltages[n] = 0.0;
    double resistanceBelow = 0.0;
    for (std::size_t i = n; i-- > 0;) {
        resistanceBelow += resistances[i];
        result.nodeVoltages[i] = result.current * resistanceBelow;
    }
    result.nodeVoltages[0] = sourceVoltage;

    return result;
}
