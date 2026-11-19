#pragma once

#include "config/Parameters.h"
#include "physics/DeviceState.h"

#include <QString>

// Turns a parameter change into a short explanation of what happened
// physically: which results changed and by how much, region transitions, and
// the parameter's documented effects. Output is rich text.
namespace Explainer {

QString regionName(OperatingRegion region);
QString regionColor(OperatingRegion region);

QString describeChange(const QString &what, const QString &from, const QString &to, const DeviceState &before,
                       const DeviceState &after, const char *effects);

// Explanation of the present operating point (used after presets and on demand).
QString describeState(const DeviceState &state, const MosfetParameters &parameters);

} // namespace Explainer
