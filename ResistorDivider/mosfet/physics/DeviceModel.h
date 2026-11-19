#pragma once

#include "DeviceState.h"
#include "MosfetParameters.h"

// The single seam between physics and everything else. A Poisson /
// drift-diffusion solver can later implement this interface without any change
// to the geometry, particles, renderer, graphs or panels.
class DeviceModel
{
public:
    virtual ~DeviceModel() = default;
    virtual DeviceState solve(const MosfetParameters &parameters, const Bias &bias) const = 0;
};

// Long-channel n-channel MOSFET, gradual-channel (charge-sheet) approximation.
//
//   Cox  = eps_ox / tox                      gamma = sqrt(2 q eps_si NA) / Cox
//   VTH  = VTH0 + gamma (sqrt(2 phiF - VBS) - sqrt(2 phiF))       (body effect)
//   VOV  = VGS - VTH
//   Cutoff      VOV <= 0          ID = 0 (subthreshold conduction not modelled)
//   Linear      VDS <  VOV        ID = ueff Cox (W/L) (VOV VDS - VDS^2/2)
//   Saturation  VDS >= VOV        ID = 1/2 ueff Cox (W/L) VOV^2 * L / (L - dL)
//   dL = sqrt(2 eps_si (VDS - VOV) / (q NA))                       (channel-length modulation)
//   Qinv(x) = Cox (VOV - V(x)),   ID = W ueff Qinv(x) dV/dx  (constant along the channel)
//   Linear:      V(x) = VOV - sqrt(VOV^2 - 2 (x/L) (VOV VDS - VDS^2/2))
//   Saturation:  V(x) = VOV (1 - sqrt(1 - x/L')) for x < L' = L - dL,
//                then rises to VDS across the pinch-off region (depletion approximation).
class AnalyticalModel : public DeviceModel
{
public:
    DeviceState solve(const MosfetParameters &parameters, const Bias &bias) const override;
};
