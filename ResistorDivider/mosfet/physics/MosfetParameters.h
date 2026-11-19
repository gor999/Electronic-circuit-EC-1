#pragma once

// Physical description of the device and the model options, in SI units
// (m, m^-3, m^2/(V s), K). Nothing in this folder depends on Qt or rendering.
//
// The values are edited through the parameter registry (Parameters.h), which
// holds each one's name, symbol, unit, range, description and effects.
struct MosfetParameters
{
    // --- Geometry -----------------------------------------------------------
    double channelLength = 2.0e-6;        // L: between the junction edges at the surface
    double channelWidth = 10.0e-6;        // W
    double oxideThickness = 20.0e-9;      // tox
    double junctionDepth = 0.3e-6;        // xj of source and drain
    double sourceLength = 1.5e-6;         // n+ source along the channel direction
    double drainLength = 1.5e-6;          // n+ drain along the channel direction
    double gateThickness = 0.3e-6;        // polysilicon gate
    double substrateThickness = 500.0e-6; // wafer

    // --- Doping (m^-3) ------------------------------------------------------
    double substrateDoping = 1.0e23;      // NA (1e17 cm^-3)
    double sourceDoping = 1.0e26;         // ND of the source (1e20 cm^-3)
    double drainDoping = 1.0e26;          // ND of the drain

    // --- Material -----------------------------------------------------------
    double electronMobility = 0.05;       // un in the channel at 300 K (500 cm^2/Vs)
    double holeMobility = 0.03;           // up in the substrate at 300 K (300 cm^2/Vs)
    double siliconPermittivity = 11.7;    // relative
    double oxidePermittivity = 3.9;       // relative
    double temperature = 300.0;           // K
    double intrinsicDensity300 = 1.0e16;  // ni at 300 K (1e10 cm^-3)

    // --- Threshold ----------------------------------------------------------
    double thresholdVoltage = 0.8;        // VTH0 at VBS = 0 when used as a model parameter
    bool thresholdFromPhysics = false;    // compute VTH0 from doping, tox and the gate material

    // --- Model options ------------------------------------------------------
    bool bodyEffect = true;               // VTH rises with reverse body bias
    bool channelLengthModulation = true;  // pinch-off point moves; ID grows in saturation
    bool mobilityDegradation = false;     // ueff = un / (1 + theta VOV)
    double mobilityTheta = 0.1;           // 1/V
    bool temperatureDependence = true;    // ni(T) and un(T), up(T)

    // Derived process geometry (not separate inputs).
    double lateralDiffusion() const { return 0.8 * junctionDepth; }   // typical for diffused junctions
    double gateOverlap() const { return 0.6 * lateralDiffusion(); }
};
