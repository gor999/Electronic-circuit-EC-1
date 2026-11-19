#pragma once

#include "config/Parameters.h"

#include <vector>

// Ready-made experiments. Each starts from the default device, changes a few
// parameters (registry keys, SI values) and says what to look for.
struct Experiment
{
    const char *name;
    const char *observe;
    std::vector<std::pair<const char *, double>> values;
    std::vector<std::pair<const char *, bool>> toggles;
};

const std::vector<Experiment> &experiments();
MosfetConfig applyExperiment(const Experiment &experiment);
