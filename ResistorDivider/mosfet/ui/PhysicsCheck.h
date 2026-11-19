#pragma once

#include "config/Parameters.h"
#include "physics/DeviceModel.h"

#include <QString>

class QWidget;

// Physics validation mode: the model's equations with the present values
// substituted, its assumptions, and the self-consistency checks.
namespace PhysicsCheck {

QString report(const DeviceModel &model, const MosfetConfig &config, const DeviceState &state);
void show(QWidget *parent, const DeviceModel &model, const MosfetConfig &config, const DeviceState &state);

} // namespace PhysicsCheck
