#pragma once

#include "geometry/MosfetGeometry.h"

#include <QMatrix4x4>
#include <QOpenGLFunctions_3_3_Core>
#include <QString>
#include <QVector3D>

#include <array>
#include <memory>
#include <vector>

class QOpenGLShaderProgram;

// One instanced sphere (lattice atoms, electrons, holes).
struct SphereInstance
{
    float x, y, z;
    float r, g, b;   // sRGB 0..1
    float radius;
};

// Everything the renderer needs to draw one frame.
struct RenderState
{
    QMatrix4x4 viewProjection;
    QVector3D eye;
    mosfet::ClipPlane clip;
    float explode = 0.0f;
    mosfet::PartFlags visible{};
    int hovered = -1;
    int selected = -1;
    bool lattice = false;
    bool particles = false;
};

// OpenGL 3.3 core renderer for the MOSFET structure. It only draws; it knows
// nothing about physics or the UI. Call every method with the context current.
//
// Passes: background gradient, section-plane caps (stencil), opaque parts,
// outline edges, lattice atoms (instanced spheres), translucent oxide.
class MosfetRenderer : protected QOpenGLFunctions_3_3_Core
{
public:
    MosfetRenderer();
    ~MosfetRenderer();

    bool initialize(QString *error);
    void release();
    bool initialized() const { return m_initialized; }

    void setGeometry(const mosfet::MosfetGeometry &geometry);
    void setLattice(const mosfet::Lattice &lattice);
    void setParticles(const std::vector<SphereInstance> &particles);
    void render(const RenderState &state);

private:
    struct GpuMesh
    {
        GLuint vao = 0, vbo = 0, ibo = 0;
        GLsizei indexCount = 0;
        GLuint edgeVao = 0, edgeVbo = 0;
        GLsizei edgeVertices = 0;
    };

    void destroyMesh(GpuMesh &mesh);
    void drawPart(const mosfet::Part &part, const RenderState &state, bool cap);
    void drawCap(const mosfet::Part &part, const RenderState &state);
    void drawEdges(const mosfet::Part &part, const RenderState &state);

    bool m_initialized = false;
    std::unique_ptr<QOpenGLShaderProgram> m_meshProgram, m_lineProgram, m_sphereProgram, m_backgroundProgram;

    const mosfet::MosfetGeometry *m_geometry = nullptr;
    std::array<GpuMesh, mosfet::kPartCount> m_meshes{};

    GLuint m_backgroundVao = 0;
    GLuint m_capVao = 0, m_capVbo = 0, m_capIbo = 0;
    GLuint m_sphereVao = 0, m_sphereVbo = 0, m_sphereIbo = 0, m_instanceVbo = 0;
    GLsizei m_sphereIndices = 0, m_instanceCount = 0;
    GLuint m_particleVao = 0, m_particleVbo = 0;
    GLsizei m_particleCount = 0;
    GLuint m_bondVao = 0, m_bondVbo = 0;
    GLsizei m_bondVertices = 0;
};
