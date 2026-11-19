#pragma once

#include <QRectF>

class Element;
class QPainter;

struct PaintOptions
{
    bool showVoltage = true;    // colour wires by voltage (green +, red -)
    bool showCurrent = true;    // moving dots
    bool showValues = true;     // component values next to the symbols
    double voltageRange = 5.0;  // voltage that maps to full colour
    double minDotCurrent = 1e-6; // currents below this draw no dots
};

// Draws schematic symbols. Kept apart from the element classes so that the
// simulation model has no painting code.
namespace ElementPainter {

void paint(QPainter &painter, const Element &element, const PaintOptions &options, bool hovered);

// Rectangle covering the element's posts, used for hit testing and selection.
QRectF bounds(const Element &element);

} // namespace ElementPainter
