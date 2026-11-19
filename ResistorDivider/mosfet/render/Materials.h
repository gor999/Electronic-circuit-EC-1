#pragma once

#include "geometry/MosfetGeometry.h"

#include <QColor>

// Surface appearance of each part. Colours follow the usual textbook coding:
// n-type regions blue (electrons), p-type reddish (holes), depletion neutral.
struct Material
{
    QColor color;
    float alpha = 1.0f;
    float specular = 0.25f;
    float shininess = 32.0f;
    float emissive = 0.0f;
    float hatch = 0.0f;     // diagonal hatching (depletion region)
    float fresnel = 0.0f;   // edges more opaque than faces (glass)
};

inline Material partMaterial(mosfet::PartId id)
{
    using mosfet::PartId;
    switch (id) {
    case PartId::Gate: return {QColor(0x80, 0x86, 0x94), 1.0f, 0.45f, 48.0f, 0.0f, 0.0f, 0.0f};
    case PartId::GateOxide: return {QColor(0xc4, 0xe6, 0xff), 0.30f, 0.9f, 96.0f, 0.04f, 0.0f, 1.0f};
    case PartId::Channel: return {QColor(0x38, 0xd8, 0xff), 1.0f, 0.4f, 40.0f, 0.35f, 0.0f, 0.0f};
    case PartId::Depletion: return {QColor(0xd8, 0xc7, 0x9c), 1.0f, 0.10f, 16.0f, 0.0f, 1.0f, 0.0f};
    case PartId::Source:
    case PartId::Drain: return {QColor(0x3f, 0x80, 0xe0), 1.0f, 0.25f, 32.0f, 0.0f, 0.0f, 0.0f};
    case PartId::Substrate: return {QColor(0xa9, 0x6b, 0x7b), 1.0f, 0.18f, 24.0f, 0.0f, 0.0f, 0.0f};
    case PartId::GateContact:
    case PartId::SourceContact:
    case PartId::DrainContact:
    case PartId::BodyContact: return {QColor(0xcf, 0xd5, 0xde), 1.0f, 0.9f, 80.0f, 0.0f, 0.0f, 0.0f};
    }
    return {};
}

inline QColor latticeColor(mosfet::Lattice::Kind kind)
{
    switch (kind) {
    case mosfet::Lattice::Donor: return QColor(0xff, 0xc9, 0x4a);
    case mosfet::Lattice::Acceptor: return QColor(0xc3, 0x8b, 0xff);
    default: return QColor(0xb9, 0xc3, 0xcf);
    }
}

inline QColor highlightColor() { return QColor(0x4c, 0xc2, 0xff); }
