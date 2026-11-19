#pragma once

#include "geometry/MosfetGeometry.h"

#include <QString>

#include <array>

struct DeviceState;
struct MosfetParameters;

// Names and short physical explanations shown in labels, tooltips and the
// side panel. Numbers come from the parameters and the device state, so the
// text stays correct when they change.
struct PartText
{
    enum Status { Geometry, Calculated, Illustrative };

    QString title;
    QString subtitle;   // one line under the label
    QString tooltip;    // a sentence or two
    QString details;    // longer explanation for the side panel
    QString statusText; // what in the drawing is calculated and what is not
    Status status = Geometry;
};

using PartTexts = std::array<PartText, mosfet::kPartCount>;

PartTexts describeParts(const MosfetParameters &parameters, const DeviceState &state);
PartText describeLattice(const MosfetParameters &parameters);

QString statusName(PartText::Status status);
QString statusColor(PartText::Status status);   // "#rrggbb"
QString tooltipHtml(const PartText &text);

// "10¹⁷ cm⁻³" style doping text from a density in m^-3.
QString dopingText(double perCubicMetre);
