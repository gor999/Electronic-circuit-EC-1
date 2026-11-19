#pragma once

#include "DeviceModel.h"

#include <string>
#include <vector>

// Self-consistency checks of a model at one operating point. Shown in the
// physics check window; also used by the automated tests.
struct ValidationCheck
{
    std::string name;
    std::string detail;     // what is compared
    double expected = 0.0;
    double actual = 0.0;
    double tolerance = 0.0; // relative, or absolute when `absolute`
    bool applicable = true;
    bool absolute = false;
    bool pass() const;
};

std::vector<ValidationCheck> validateModel(const DeviceModel &model, const MosfetParameters &parameters,
                                           const Bias &bias);
