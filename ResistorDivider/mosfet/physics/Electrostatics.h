#pragma once

#include <algorithm>
#include <cmath>

// Textbook semiconductor electrostatics (SI units). Depletion widths use the
// abrupt-junction depletion approximation. Every function takes its material
// values as arguments so that the parameter panel can change them.
namespace semi {

constexpr double kElementaryCharge = 1.602176634e-19;    // C
constexpr double kBoltzmann = 1.380649e-23;              // J/K
constexpr double kVacuumPermittivity = 8.8541878128e-12; // F/m
constexpr double kSiliconAtomDensity = 5.0e28;           // m^-3
constexpr double kSiliconBandGap = 1.12;                 // eV, taken constant
constexpr double kSaturationVelocity = 1.0e5;            // m/s, electrons in silicon

inline double thermalVoltage(double temperature)
{
    return kBoltzmann * temperature / kElementaryCharge;
}

// ni(T) = ni(300 K) (T/300)^1.5 exp(-Eg/(2k) (1/T - 1/300)).
inline double intrinsicDensity(double ni300, double temperature)
{
    const double eg = kSiliconBandGap * kElementaryCharge / (2.0 * kBoltzmann);
    return ni300 * std::pow(temperature / 300.0, 1.5) * std::exp(-eg * (1.0 / temperature - 1.0 / 300.0));
}

// Lattice-scattering-limited mobility: u(T) = u(300 K) (T/300)^-1.5.
inline double mobilityAt(double mobility300, double temperature)
{
    return mobility300 * std::pow(temperature / 300.0, -1.5);
}

inline double oxideCapacitance(double relativePermittivity, double oxideThickness)
{
    return relativePermittivity * kVacuumPermittivity / oxideThickness;   // F/m^2
}

// phiF = (kT/q) ln(NA / ni)
inline double fermiPotential(double acceptorDensity, double ni, double temperature)
{
    return thermalVoltage(temperature) * std::log(acceptorDensity / ni);
}

// Vbi = (kT/q) ln(NA ND / ni^2)
inline double builtInPotential(double acceptorDensity, double donorDensity, double ni, double temperature)
{
    return thermalVoltage(temperature) * std::log(acceptorDensity * donorDensity / (ni * ni));
}

struct JunctionWidths
{
    double pSide = 0.0;
    double nSide = 0.0;
};

// Abrupt p-n junction with `potential` (Vbi plus reverse bias) across it.
inline JunctionWidths junctionDepletion(double acceptorDensity, double donorDensity, double potential,
                                        double relativePermittivity)
{
    const double eps = relativePermittivity * kVacuumPermittivity;
    const double total = std::sqrt(2.0 * eps * std::max(potential, 0.0) * (acceptorDensity + donorDensity)
                                   / (kElementaryCharge * acceptorDensity * donorDensity));
    return {total * donorDensity / (acceptorDensity + donorDensity),
            total * acceptorDensity / (acceptorDensity + donorDensity)};
}

// One-sided depletion depth for a potential drop psi across p-type silicon.
inline double depletionWidth(double acceptorDensity, double potential, double relativePermittivity)
{
    if (potential <= 0.0)
        return 0.0;
    return std::sqrt(2.0 * relativePermittivity * kVacuumPermittivity * potential
                     / (kElementaryCharge * acceptorDensity));
}

// Body-effect coefficient gamma = sqrt(2 q eps_si NA) / Cox, in V^0.5.
inline double bodyFactor(double acceptorDensity, double relativePermittivity, double oxideCapacitancePerArea)
{
    return std::sqrt(2.0 * kElementaryCharge * relativePermittivity * kVacuumPermittivity * acceptorDensity)
           / oxideCapacitancePerArea;
}

} // namespace semi
