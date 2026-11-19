#include "MosfetRenderer.h"

#include "Materials.h"

#include <QOpenGLShaderProgram>

#include <algorithm>
#include <cmath>
#include <map>
#include <vector>

using mosfet::Part;
using mosfet::PartId;

namespace {

// --- Shaders -----------------------------------------------------------------

const char *const kLighting = R"(
vec3 toLinear(vec3 c) { return pow(c, vec3(2.2)); }
vec3 toSrgb(vec3 c) { return pow(max(c, vec3(0.0)), vec3(1.0 / 2.2)); }

// Key light from the upper front left, cool fill from the right, faint back
// light, sky/ground ambient and a soft rim. Inputs are linear colours.
vec3 lighting(vec3 base, vec3 n, vec3 v, float specular, float shininess)
{
    vec3 key = normalize(vec3(-0.45, 0.80, 0.55));
    vec3 fill = normalize(vec3(0.75, 0.25, 0.35));
    vec3 back = normalize(vec3(0.10, 0.35, -1.0));
    float hemi = 0.5 + 0.5 * n.y;
    vec3 ambient = base * mix(vec3(0.07, 0.08, 0.10), vec3(0.24, 0.27, 0.32), hemi);
    vec3 diffuse = base * (max(dot(n, key), 0.0) * vec3(1.00, 0.97, 0.92) * 0.85
                         + max(dot(n, fill), 0.0) * vec3(0.55, 0.66, 0.82) * 0.32
                         + max(dot(n, back), 0.0) * vec3(0.5, 0.55, 0.6) * 0.18);
    vec3 h = normalize(key + v);
    float spec = pow(max(dot(n, h), 0.0), shininess) * specular;
    float rim = pow(1.0 - max(dot(n, v), 0.0), 3.0) * 0.12;
    return ambient + diffuse + vec3(spec) + rim * vec3(0.55, 0.70, 0.95);
}
)";

const char *const kMeshVertex = R"(#version 330 core
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
uniform mat4 uViewProj;
uniform vec3 uOffset;
uniform vec4 uClip;
out vec3 vWorld;
out vec3 vNormal;
void main()
{
    vec3 p = aPosition + uOffset;
    vWorld = p;
    vNormal = aNormal;
    gl_ClipDistance[0] = dot(uClip.xyz, p) + uClip.w;
    gl_Position = uViewProj * vec4(p, 1.0);
}
)";

const char *const kMeshFragment = R"(
in vec3 vWorld;
in vec3 vNormal;
uniform vec3 uEye;
uniform vec3 uColor;
uniform float uAlpha;
uniform float uSpecular;
uniform float uShininess;
uniform float uEmissive;
uniform float uHatch;
uniform float uFresnel;
uniform float uSelected;
uniform float uHovered;
uniform float uCap;
out vec4 fragColor;
void main()
{
    vec3 n = normalize(vNormal);
    if (!gl_FrontFacing)
        n = -n;
    vec3 v = normalize(uEye - vWorld);
    vec3 base = toLinear(uColor);
    if (uHatch > 0.0) {
        float t = (vWorld.x + vWorld.y + vWorld.z) * 11.0;
        float w = fwidth(t);
        float stripe = smoothstep(0.25 - w, 0.25 + w, abs(fract(t) - 0.5));
        base *= mix(1.0 - 0.20 * uHatch, 1.0, stripe);
    }
    if (uCap > 0.0)
        base *= 0.88;   // cut faces read as solid material, slightly darker
    vec3 c = lighting(base, n, v, uSpecular, uShininess) + base * uEmissive;

    vec3 hl = toLinear(vec3(0.30, 0.76, 1.0));
    float facing = 1.0 - max(dot(n, v), 0.0);
    c = mix(c, c * 0.65 + hl * 0.45, uSelected * 0.35);
    c += hl * (uSelected * 0.30 + uHovered * 0.18) * facing * facing;
    c *= 1.0 + uHovered * 0.10;

    float alpha = uAlpha + (1.0 - uAlpha) * uFresnel * pow(facing, 3.0) * 0.6;
    fragColor = vec4(toSrgb(c), alpha);
}
)";

const char *const kLineVertex = R"(#version 330 core
layout(location = 0) in vec3 aPosition;
uniform mat4 uViewProj;
uniform vec3 uOffset;
uniform vec4 uClip;
void main()
{
    vec3 p = aPosition + uOffset;
    gl_ClipDistance[0] = dot(uClip.xyz, p) + uClip.w;
    gl_Position = uViewProj * vec4(p, 1.0);
}
)";

const char *const kLineFragment = R"(#version 330 core
uniform vec4 uColor;
out vec4 fragColor;
void main() { fragColor = uColor; }
)";

const char *const kSphereVertex = R"(#version 330 core
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 iCenter;
layout(location = 2) in vec3 iColor;
layout(location = 3) in float iRadius;
uniform mat4 uViewProj;
out vec3 vWorld;
out vec3 vNormal;
out vec3 vColor;
void main()
{
    vec3 p = iCenter + aPosition * iRadius;
    vWorld = p;
    vNormal = aPosition;
    vColor = iColor;
    gl_Position = uViewProj * vec4(p, 1.0);
}
)";

const char *const kSphereFragment = R"(
in vec3 vWorld;
in vec3 vNormal;
in vec3 vColor;
uniform vec3 uEye;
out vec4 fragColor;
void main()
{
    vec3 n = normalize(vNormal);
    vec3 v = normalize(uEye - vWorld);
    vec3 c = lighting(toLinear(vColor), n, v, 0.6, 48.0);
    fragColor = vec4(toSrgb(c), 1.0);
}
)";

const char *const kBackgroundVertex = R"(#version 330 core
out vec2 vUv;
void main()
{
    vec2 corners[3] = vec2[3](vec2(-1.0, -1.0), vec2(3.0, -1.0), vec2(-1.0, 3.0));
    vec2 p = corners[gl_VertexID];
    vUv = p * 0.5 + 0.5;
    gl_Position = vec4(p, 0.0, 1.0);
}
)";

const char *const kBackgroundFragment = R"(#version 330 core
in vec2 vUv;
out vec4 fragColor;
void main()
{
    vec3 top = vec3(0.098, 0.122, 0.157);
    vec3 bottom = vec3(0.039, 0.047, 0.063);
    vec3 c = mix(bottom, top, smoothstep(0.0, 1.0, vUv.y));
    float d = length((vUv - vec2(0.5, 0.58)) * vec2(1.2, 1.0));
    c *= mix(1.0, 0.72, smoothstep(0.25, 0.95, d));
    fragColor = vec4(c, 1.0);
}
)";

QByteArray fragmentSource(const char *body)
{
    return QByteArray("#version 330 core\n") + kLighting + body;
}

bool buildProgram(std::unique_ptr<QOpenGLShaderProgram> &program, const QByteArray &vertex,
                  const QByteArray &fragment, QString *error)
{
    program = std::make_unique<QOpenGLShaderProgram>();
    if (!program->addShaderFromSourceCode(QOpenGLShader::Vertex, vertex)
        || !program->addShaderFromSourceCode(QOpenGLShader::Fragment, fragment) || !program->link()) {
        if (error)
            *error = program->log();
        return false;
    }
    return true;
}

// Unit icosphere (positions double as normals).
void buildSphere(std::vector<float> &vertices, std::vector<uint32_t> &indices, int subdivisions)
{
    const float t = (1.0f + std::sqrt(5.0f)) / 2.0f;
    std::vector<QVector3D> points = {
        {-1, t, 0}, {1, t, 0}, {-1, -t, 0}, {1, -t, 0}, {0, -1, t}, {0, 1, t},
        {0, -1, -t}, {0, 1, -t}, {t, 0, -1}, {t, 0, 1}, {-t, 0, -1}, {-t, 0, 1}};
    for (QVector3D &p : points)
        p.normalize();
    indices = {0, 11, 5, 0, 5, 1, 0, 1, 7, 0, 7, 10, 0, 10, 11, 1, 5, 9, 5, 11, 4, 11, 10, 2, 10, 7, 6,
               7, 1, 8, 3, 9, 4, 3, 4, 2, 3, 2, 6, 3, 6, 8, 3, 8, 9, 4, 9, 5, 2, 4, 11, 6, 2, 10, 8, 6, 7, 9, 8, 1};
    for (int s = 0; s < subdivisions; ++s) {
        std::map<std::pair<uint32_t, uint32_t>, uint32_t> middle;
        auto midpoint = [&](uint32_t a, uint32_t b) {
            const auto key = std::make_pair(std::min(a, b), std::max(a, b));
            const auto found = middle.find(key);
            if (found != middle.end())
                return found->second;
            points.push_back(((points[a] + points[b]) * 0.5f).normalized());
            const uint32_t id = static_cast<uint32_t>(points.size() - 1);
            middle[key] = id;
            return id;
        };
        std::vector<uint32_t> next;
        for (std::size_t i = 0; i < indices.size(); i += 3) {
            const uint32_t a = indices[i], b = indices[i + 1], c = indices[i + 2];
            const uint32_t ab = midpoint(a, b), bc = midpoint(b, c), ca = midpoint(c, a);
            next.insert(next.end(), {a, ab, ca, b, bc, ab, c, ca, bc, ab, bc, ca});
        }
        indices.swap(next);
    }
    for (const QVector3D &p : points)
        vertices.insert(vertices.end(), {p.x(), p.y(), p.z()});
}

QVector3D toVector(const QColor &c) { return QVector3D(float(c.redF()), float(c.greenF()), float(c.blueF())); }
QVector3D toVector(mosfet::Vec3f v) { return QVector3D(v.x, v.y, v.z); }
QVector4D clipVector(const mosfet::ClipPlane &clip)
{
    if (!clip.enabled)
        return QVector4D(0, 0, 0, 1);
    return QVector4D(clip.normal.x, clip.normal.y, clip.normal.z, clip.offset);
}

} // namespace

MosfetRenderer::MosfetRenderer() = default;
MosfetRenderer::~MosfetRenderer() = default;

bool MosfetRenderer::initialize(QString *error)
{
    if (m_initialized)
        release();
    if (!initializeOpenGLFunctions()) {
        if (error)
            *error = QStringLiteral("OpenGL 3.3 core functions are not available.");
        return false;
    }
    if (!buildProgram(m_meshProgram, kMeshVertex, fragmentSource(kMeshFragment), error)
        || !buildProgram(m_lineProgram, kLineVertex, kLineFragment, error)
        || !buildProgram(m_sphereProgram, kSphereVertex, fragmentSource(kSphereFragment), error)
        || !buildProgram(m_backgroundProgram, kBackgroundVertex, kBackgroundFragment, error))
        return false;

    glGenVertexArrays(1, &m_backgroundVao);

    // Section cap quad: 4 vertices rewritten per draw, fixed indices.
    glGenVertexArrays(1, &m_capVao);
    glGenBuffers(1, &m_capVbo);
    glGenBuffers(1, &m_capIbo);
    glBindVertexArray(m_capVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_capVbo);
    glBufferData(GL_ARRAY_BUFFER, 4 * 6 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), reinterpret_cast<void *>(3 * sizeof(float)));
    const uint32_t capIndices[6] = {0, 1, 2, 0, 2, 3};
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_capIbo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(capIndices), capIndices, GL_STATIC_DRAW);

    // Instanced spheres for lattice atoms (and, from Phase 2, carriers).
    std::vector<float> sphereVertices;
    std::vector<uint32_t> sphereIndices;
    buildSphere(sphereVertices, sphereIndices, 2);
    m_sphereIndices = static_cast<GLsizei>(sphereIndices.size());
    glGenVertexArrays(1, &m_sphereVao);
    glGenBuffers(1, &m_sphereVbo);
    glGenBuffers(1, &m_sphereIbo);
    glGenBuffers(1, &m_instanceVbo);
    glBindVertexArray(m_sphereVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_sphereVbo);
    glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(sphereVertices.size() * sizeof(float)), sphereVertices.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_sphereIbo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, GLsizeiptr(sphereIndices.size() * sizeof(uint32_t)), sphereIndices.data(),
                 GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, m_instanceVbo);
    const GLsizei stride = 7 * sizeof(float);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
    glVertexAttribDivisor(1, 1);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void *>(3 * sizeof(float)));
    glVertexAttribDivisor(2, 1);
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void *>(6 * sizeof(float)));
    glVertexAttribDivisor(3, 1);

    // Second instanced-sphere VAO for carriers, sharing the sphere mesh.
    glGenVertexArrays(1, &m_particleVao);
    glGenBuffers(1, &m_particleVbo);
    glBindVertexArray(m_particleVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_sphereVbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_sphereIbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_particleVbo);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
    glVertexAttribDivisor(1, 1);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void *>(3 * sizeof(float)));
    glVertexAttribDivisor(2, 1);
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void *>(6 * sizeof(float)));
    glVertexAttribDivisor(3, 1);

    glGenVertexArrays(1, &m_bondVao);
    glGenBuffers(1, &m_bondVbo);
    glBindVertexArray(m_bondVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_bondVbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    m_initialized = true;
    if (m_geometry)
        setGeometry(*m_geometry);
    return true;
}

void MosfetRenderer::destroyMesh(GpuMesh &mesh)
{
    if (mesh.vao) {
        glDeleteVertexArrays(1, &mesh.vao);
        glDeleteBuffers(1, &mesh.vbo);
        glDeleteBuffers(1, &mesh.ibo);
    }
    if (mesh.edgeVao) {
        glDeleteVertexArrays(1, &mesh.edgeVao);
        glDeleteBuffers(1, &mesh.edgeVbo);
    }
    mesh = GpuMesh();
}

void MosfetRenderer::release()
{
    if (!m_initialized)
        return;
    for (GpuMesh &mesh : m_meshes)
        destroyMesh(mesh);
    const GLuint arrays[] = {m_backgroundVao, m_capVao, m_sphereVao, m_bondVao, m_particleVao};
    glDeleteVertexArrays(5, arrays);
    const GLuint buffers[] = {m_capVbo, m_capIbo, m_sphereVbo, m_sphereIbo, m_instanceVbo, m_bondVbo, m_particleVbo};
    glDeleteBuffers(7, buffers);
    m_backgroundVao = m_capVao = m_sphereVao = m_bondVao = m_particleVao = 0;
    m_capVbo = m_capIbo = m_sphereVbo = m_sphereIbo = m_instanceVbo = m_bondVbo = m_particleVbo = 0;
    m_instanceCount = m_bondVertices = m_particleCount = 0;
    m_meshProgram.reset();
    m_lineProgram.reset();
    m_sphereProgram.reset();
    m_backgroundProgram.reset();
    m_initialized = false;
}

void MosfetRenderer::setGeometry(const mosfet::MosfetGeometry &geometry)
{
    m_geometry = &geometry;
    if (!m_initialized)
        return;
    for (GpuMesh &mesh : m_meshes)
        destroyMesh(mesh);
    for (const Part &part : geometry.parts()) {
        GpuMesh &mesh = m_meshes[std::size_t(mosfet::index(part.id))];
        glGenVertexArrays(1, &mesh.vao);
        glGenBuffers(1, &mesh.vbo);
        glGenBuffers(1, &mesh.ibo);
        glBindVertexArray(mesh.vao);
        glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
        glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(part.mesh.vertices.size() * sizeof(float)), part.mesh.vertices.data(),
                     GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), nullptr);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), reinterpret_cast<void *>(3 * sizeof(float)));
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ibo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, GLsizeiptr(part.mesh.indices.size() * sizeof(uint32_t)),
                     part.mesh.indices.data(), GL_STATIC_DRAW);
        mesh.indexCount = GLsizei(part.mesh.indices.size());

        glGenVertexArrays(1, &mesh.edgeVao);
        glGenBuffers(1, &mesh.edgeVbo);
        glBindVertexArray(mesh.edgeVao);
        glBindBuffer(GL_ARRAY_BUFFER, mesh.edgeVbo);
        glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(part.mesh.edges.size() * sizeof(float)), part.mesh.edges.data(),
                     GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
        mesh.edgeVertices = GLsizei(part.mesh.edges.size() / 3);
    }
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void MosfetRenderer::setLattice(const mosfet::Lattice &lattice)
{
    if (!m_initialized)
        return;
    std::vector<float> instances;
    instances.reserve(lattice.sites.size() * 7);
    for (const auto &site : lattice.sites) {
        const QColor color = latticeColor(site.kind);
        const float radius = site.kind == mosfet::Lattice::Silicon ? lattice.radius : lattice.radius * 1.25f;
        instances.insert(instances.end(), {site.position.x, site.position.y, site.position.z, float(color.redF()),
                                           float(color.greenF()), float(color.blueF()), radius});
    }
    glBindBuffer(GL_ARRAY_BUFFER, m_instanceVbo);
    glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(instances.size() * sizeof(float)), instances.data(), GL_DYNAMIC_DRAW);
    m_instanceCount = GLsizei(lattice.sites.size());

    std::vector<float> bonds;
    bonds.reserve(lattice.bonds.size() * 6);
    for (const auto &bond : lattice.bonds) {
        const mosfet::Vec3f a = lattice.sites[bond.first].position, b = lattice.sites[bond.second].position;
        bonds.insert(bonds.end(), {a.x, a.y, a.z, b.x, b.y, b.z});
    }
    glBindBuffer(GL_ARRAY_BUFFER, m_bondVbo);
    glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(bonds.size() * sizeof(float)), bonds.data(), GL_DYNAMIC_DRAW);
    m_bondVertices = GLsizei(lattice.bonds.size() * 2);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void MosfetRenderer::setParticles(const std::vector<SphereInstance> &particles)
{
    if (!m_initialized)
        return;
    static_assert(sizeof(SphereInstance) == 7 * sizeof(float), "instance layout");
    glBindBuffer(GL_ARRAY_BUFFER, m_particleVbo);
    glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(particles.size() * sizeof(SphereInstance)), particles.data(), GL_STREAM_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    m_particleCount = GLsizei(particles.size());
}

void MosfetRenderer::drawPart(const Part &part, const RenderState &state, bool cap)
{
    const Material material = partMaterial(part.id);
    const int id = mosfet::index(part.id);
    m_meshProgram->setUniformValue("uColor", toVector(material.color));
    m_meshProgram->setUniformValue("uAlpha", material.alpha);
    m_meshProgram->setUniformValue("uSpecular", material.specular);
    m_meshProgram->setUniformValue("uShininess", material.shininess);
    m_meshProgram->setUniformValue("uEmissive", material.emissive);
    m_meshProgram->setUniformValue("uHatch", material.hatch);
    m_meshProgram->setUniformValue("uFresnel", material.fresnel);
    m_meshProgram->setUniformValue("uSelected", state.selected == id ? 1.0f : 0.0f);
    m_meshProgram->setUniformValue("uHovered", state.hovered == id && state.selected != id ? 1.0f : 0.0f);
    m_meshProgram->setUniformValue("uCap", cap ? 1.0f : 0.0f);
    m_meshProgram->setUniformValue("uOffset", cap ? QVector3D() : toVector(m_geometry->offset(part, state.explode)));
    if (cap) {
        glBindVertexArray(m_capVao);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
    } else {
        const GpuMesh &mesh = m_meshes[std::size_t(id)];
        glBindVertexArray(mesh.vao);
        glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, nullptr);
    }
}

// Closes the opening the section plane cuts into a solid part. The part is
// drawn into the stencil buffer with every face inverting it: pixels where the
// plane lies inside the solid end up with an odd count, and a plane-sized quad
// is coloured only there. The quad then carries the correct depth for later passes.
void MosfetRenderer::drawCap(const Part &part, const RenderState &state)
{
    const mosfet::ClipPlane &clip = state.clip;
    const mosfet::Vec3f off = m_geometry->offset(part, state.explode);
    float lowest = 1.0e9f, highest = -1.0e9f;
    for (int c = 0; c < 8; ++c) {
        const mosfet::Vec3f corner{(c & 1) ? part.boundsMax.x : part.boundsMin.x,
                                   (c & 2) ? part.boundsMax.y : part.boundsMin.y,
                                   (c & 4) ? part.boundsMax.z : part.boundsMin.z};
        const float d = clip.distance(corner + off);
        lowest = std::min(lowest, d);
        highest = std::max(highest, d);
    }
    if (lowest >= 0.0f || highest <= 0.0f)
        return;   // the plane does not cut this part

    // Quad on the plane covering the part, facing the removed side.
    const mosfet::Vec3f lo = part.boundsMin + off, hi = part.boundsMax + off;
    const float m = 0.05f;
    std::array<mosfet::Vec3f, 4> corners;
    const bool acrossChannel = std::fabs(clip.normal.x) > 0.5f;
    if (acrossChannel) {
        const float x = clip.offset;
        corners = {{{x, lo.y - m, lo.z - m}, {x, lo.y - m, hi.z + m}, {x, hi.y + m, hi.z + m}, {x, hi.y + m, lo.z - m}}};
    } else {
        const float z = clip.offset;
        corners = {{{lo.x - m, lo.y - m, z}, {hi.x + m, lo.y - m, z}, {hi.x + m, hi.y + m, z}, {lo.x - m, hi.y + m, z}}};
    }
    const mosfet::Vec3f n = clip.normal * -1.0f;
    float data[24];
    for (int i = 0; i < 4; ++i) {
        const float v[6] = {corners[i].x, corners[i].y, corners[i].z, n.x, n.y, n.z};
        std::copy(v, v + 6, data + i * 6);
    }
    glBindBuffer(GL_ARRAY_BUFFER, m_capVbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(data), data);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // 1. Parity of the clipped solid's surfaces in the stencil buffer.
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);
    glStencilFunc(GL_ALWAYS, 0, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_INVERT);
    m_meshProgram->setUniformValue("uOffset", QVector3D(off.x, off.y, off.z));
    glBindVertexArray(m_meshes[std::size_t(mosfet::index(part.id))].vao);
    glDrawElements(GL_TRIANGLES, m_meshes[std::size_t(mosfet::index(part.id))].indexCount, GL_UNSIGNED_INT, nullptr);

    // 2. The cap where the count is odd; resets the stencil as it goes.
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glStencilFunc(GL_NOTEQUAL, 0, 0xFF);
    glStencilOp(GL_ZERO, GL_ZERO, GL_ZERO);
    glDisable(GL_CLIP_DISTANCE0);
    drawPart(part, state, true);
    glEnable(GL_CLIP_DISTANCE0);
}

void MosfetRenderer::drawEdges(const Part &part, const RenderState &state)
{
    const int id = mosfet::index(part.id);
    const GpuMesh &mesh = m_meshes[std::size_t(id)];
    QColor color = partMaterial(part.id).color;
    QVector4D rgba(float(color.redF()) * 0.42f, float(color.greenF()) * 0.42f, float(color.blueF()) * 0.42f, 0.85f);
    if (part.translucent)
        rgba = QVector4D(0.75f, 0.88f, 1.0f, 0.55f);
    if (state.selected == id) {
        color = highlightColor();
        rgba = QVector4D(float(color.redF()), float(color.greenF()), float(color.blueF()), 1.0f);
    } else if (state.hovered == id) {
        rgba = QVector4D(0.85f, 0.92f, 1.0f, 0.9f);
    }
    m_lineProgram->setUniformValue("uColor", rgba);
    m_lineProgram->setUniformValue("uOffset", toVector(m_geometry->offset(part, state.explode)));
    glBindVertexArray(mesh.edgeVao);
    glDrawArrays(GL_LINES, 0, mesh.edgeVertices);
}

void MosfetRenderer::render(const RenderState &state)
{
    glClearColor(0.05f, 0.06f, 0.08f, 1.0f);
    glClearStencil(0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    if (!m_initialized || !m_geometry)
        return;

    // Background gradient.
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    m_backgroundProgram->bind();
    glBindVertexArray(m_backgroundVao);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);
    glEnable(GL_CLIP_DISTANCE0);
    const QVector4D clip = clipVector(state.clip);
    const auto visible = [&](const Part &part) { return state.visible[std::size_t(mosfet::index(part.id))]; };

    m_meshProgram->bind();
    m_meshProgram->setUniformValue("uViewProj", state.viewProjection);
    m_meshProgram->setUniformValue("uClip", clip);
    m_meshProgram->setUniformValue("uEye", state.eye);

    // Section caps, only seen when looking at the cut from the removed side.
    const mosfet::Vec3f eye{state.eye.x(), state.eye.y(), state.eye.z()};
    if (state.clip.enabled && state.clip.distance(eye) < 0.0f) {
        glEnable(GL_STENCIL_TEST);
        glDisable(GL_CULL_FACE);
        for (const Part &part : m_geometry->parts()) {
            if (visible(part) && !part.translucent)
                drawCap(part, state);
        }
        glDisable(GL_STENCIL_TEST);
    }

    // Opaque parts, pushed back slightly so outline edges win the depth test.
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.0f, 1.0f);
    for (const Part &part : m_geometry->parts()) {
        if (visible(part) && !part.translucent)
            drawPart(part, state, false);
    }
    glDisable(GL_POLYGON_OFFSET_FILL);

    // Lattice atoms and bonds (not clipped: they sit on the section face).
    if ((state.lattice && m_instanceCount > 0) || (state.particles && m_particleCount > 0)) {
        glDisable(GL_CLIP_DISTANCE0);
        m_sphereProgram->bind();
        m_sphereProgram->setUniformValue("uViewProj", state.viewProjection);
        m_sphereProgram->setUniformValue("uEye", state.eye);
        if (state.lattice && m_instanceCount > 0) {
            glBindVertexArray(m_sphereVao);
            glDrawElementsInstanced(GL_TRIANGLES, m_sphereIndices, GL_UNSIGNED_INT, nullptr, m_instanceCount);
        }
        if (state.particles && m_particleCount > 0) {
            glBindVertexArray(m_particleVao);
            glDrawElementsInstanced(GL_TRIANGLES, m_sphereIndices, GL_UNSIGNED_INT, nullptr, m_particleCount);
        }
        glEnable(GL_CLIP_DISTANCE0);
    }

    // Outline edges.
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    m_lineProgram->bind();
    m_lineProgram->setUniformValue("uViewProj", state.viewProjection);
    m_lineProgram->setUniformValue("uClip", clip);
    for (const Part &part : m_geometry->parts()) {
        if (visible(part) && !part.translucent)
            drawEdges(part, state);
    }
    if (state.lattice && m_bondVertices > 0) {
        m_lineProgram->setUniformValue("uColor", QVector4D(0.72f, 0.78f, 0.86f, 0.45f));
        m_lineProgram->setUniformValue("uOffset", QVector3D());
        m_lineProgram->setUniformValue("uClip", QVector4D(0, 0, 0, 1));
        glBindVertexArray(m_bondVao);
        glDrawArrays(GL_LINES, 0, m_bondVertices);
        m_lineProgram->setUniformValue("uClip", clip);
    }

    // Translucent parts last, back to front.
    std::vector<const Part *> glass;
    for (const Part &part : m_geometry->parts()) {
        if (visible(part) && part.translucent)
            glass.push_back(&part);
    }
    auto depth = [&](const Part *part) {
        const mosfet::Vec3f c = (part->boundsMin + part->boundsMax) * 0.5f + m_geometry->offset(*part, state.explode);
        return (QVector3D(c.x, c.y, c.z) - state.eye).lengthSquared();
    };
    std::sort(glass.begin(), glass.end(), [&](const Part *a, const Part *b) { return depth(a) > depth(b); });
    m_meshProgram->bind();
    for (const Part *part : glass)
        drawPart(*part, state, false);
    m_lineProgram->bind();
    for (const Part *part : glass)
        drawEdges(*part, state);

    // Leave the state the way QPainter expects it.
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CLIP_DISTANCE0);
    glBindVertexArray(0);
    m_lineProgram->release();
}
