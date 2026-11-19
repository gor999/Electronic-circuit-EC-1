#pragma once

#include "physics/DeviceState.h"
#include "physics/MosfetParameters.h"

#include <string>
#include <vector>

// Central parameter model. Every value the user can change is described once
// here; the panels, presets, saved configurations and validation all read the
// same table, so no physical value is hard-coded twice.
//
// Four kinds of values are kept apart:
//   - user inputs: device + bias (physics) and render settings (drawing only),
//   - derived quantities: DeviceState, recomputed by the model on every change,
//   - simulation/animation settings: the sweep and particle time scale,
//   - render settings: how calculated quantities are drawn.

struct RenderSettings
{
    double channelDrawScale = 1.0;       // drawn channel thickness per unit of inversion charge
    double particleDensity = 1.0;        // dots per unit of charge
    double picosecondsPerSecond = 40.0;  // animation: real time shown per second on screen
};

struct MosfetConfig
{
    MosfetParameters device;
    Bias bias;
    RenderSettings render;
};

enum class ParamGroup { Bias, Threshold, Geometry, Material, Model, Display };

struct ParamSpec
{
    const char *key;          // stable identifier (saved configurations, presets)
    const char *name;
    const char *symbol;
    const char *unit;         // display unit
    double unitScale;         // display value = SI value * unitScale
    double minimum, maximum;  // SI
    bool logarithmic;         // slider in decades
    ParamGroup group;
    const char *description;  // what it is
    const char *effects;      // what changes when it changes (physical dependencies)
    double &(*ref)(MosfetConfig &);

    double get(const MosfetConfig &config) const { return ref(const_cast<MosfetConfig &>(config)); }
    void set(MosfetConfig &config, double value) const;   // clamps to [minimum, maximum]
    double defaultValue() const;
    bool affectsPhysics() const { return group != ParamGroup::Display; }
};

struct ToggleSpec
{
    const char *key;
    const char *name;
    ParamGroup group;
    const char *description;
    bool &(*ref)(MosfetConfig &);

    bool get(const MosfetConfig &config) const { return ref(const_cast<MosfetConfig &>(config)); }
    bool defaultValue() const;
};

const std::vector<ParamSpec> &parameterSpecs();
const std::vector<ToggleSpec> &toggleSpecs();
const ParamSpec *findParameter(const std::string &key);
const ToggleSpec *findToggle(const std::string &key);

// Clamps every value to its range and enforces rules between parameters.
// Returns a note for each adjustment made.
std::vector<std::string> validateConfig(MosfetConfig &config);

// "key=value" lines (SI values; toggles as 0/1). Unknown keys are ignored.
std::string serializeConfig(const MosfetConfig &config);
MosfetConfig deserializeConfig(const std::string &text);
