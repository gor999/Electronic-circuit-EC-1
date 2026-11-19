#include "MosfetView.h"

#include "Format.h"
#include "render/Materials.h"

#include <QHelpEvent>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QPainter>
#include <QPainterPath>
#include <QToolTip>
#include <QWheelEvent>
#include <QtMath>

#include <algorithm>
#include <cmath>

using mosfet::PartId;
using mosfet::Vec3f;

namespace {

const QColor kText(0xe6, 0xea, 0xf0);
const QColor kDim(0x8a, 0x94, 0xa3);
const QColor kLabelFill(16, 20, 26, 228);
const QColor kLabelBorder(0x2f, 0x39, 0x47);

const PartId kLabelledParts[] = {PartId::Gate, PartId::GateOxide, PartId::Channel, PartId::Depletion,
                                 PartId::Source, PartId::Drain, PartId::Substrate};

QVector3D toQt(Vec3f v) { return QVector3D(v.x, v.y, v.z); }
Vec3f fromQt(const QVector3D &v) { return {v.x(), v.y(), v.z()}; }

} // namespace

MosfetView::MosfetView(QWidget *parent)
    : QOpenGLWidget(parent)
{
    QSurfaceFormat surface = format();
    surface.setVersion(3, 3);
    surface.setProfile(QSurfaceFormat::CoreProfile);
    surface.setDepthBufferSize(24);
    surface.setStencilBufferSize(8);
    surface.setSamples(4);
    setFormat(surface);

    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(420, 320);
    m_visible.fill(true);

    m_timer.setInterval(16);
    connect(&m_timer, &QTimer::timeout, this, [this] {
        const float seconds = std::min(0.25f, m_clock.restart() / 1000.0f);
        if (!m_rig.update(seconds))
            m_timer.stop();
        emit cameraChanged();
        update();
    });
}

MosfetView::~MosfetView()
{
    releaseGl();
}

void MosfetView::releaseGl()
{
    if (!context() || !m_renderer.initialized())
        return;
    makeCurrent();
    m_renderer.release();
    doneCurrent();
}

// ---------------------------------------------------------------------------
// State setters
// ---------------------------------------------------------------------------

void MosfetView::setDeviceGeometry(const mosfet::MosfetGeometry &geometry)
{
    m_geometry = geometry;
    m_hasGeometry = true;
    m_geometryDirty = true;
    m_latticeDirty = true;
    const float extent = geometry.boundsMax.x - geometry.boundsMin.x;
    if (!m_framed && width() > 0 && height() > 0) {
        m_rig.jumpTo(presetCamera(Preset::Perspective));
        m_framed = true;
        m_framedWidth = extent;
    } else if (m_framed && (extent > 1.25f * m_framedWidth || extent < 0.75f * m_framedWidth)) {
        // L, W or the S/D lengths changed the size a lot: keep the same view, re-framed.
        Camera camera = m_rig.goal();
        const Camera framed = presetCamera(Preset::Perspective);
        camera.distance = framed.distance * (camera.fov < 20.0f ? 3.28f : 1.0f);
        camera.target = framed.target;
        m_rig.animateTo(camera);
        startAnimation();
        m_framedWidth = extent;
    }
    update();
}

void MosfetView::setPartVisible(PartId id, bool visible)
{
    m_visible[mosfet::index(id)] = visible;
    m_latticeDirty = true;
    if (!visible && m_selected == mosfet::index(id))
        setSelectedPart(-1);
    if (!visible && m_hovered == mosfet::index(id))
        m_hovered = -1;
    update();
}

void MosfetView::setLatticeVisible(bool visible)
{
    m_lattice = visible;
    m_latticeDirty = true;
    update();
}

void MosfetView::setLabelsVisible(bool visible)
{
    m_labels = visible;
    update();
}

void MosfetView::setExplode(float amount)
{
    m_explode = std::clamp(amount, 0.0f, 1.0f);
    m_latticeDirty = true;
    update();
}

void MosfetView::setSection(Section section, float position)
{
    const bool modeChanged = section != m_section;
    m_section = section;
    m_sectionPosition = position;
    m_latticeDirty = true;
    if (modeChanged && section != Section::None) {
        // Turn to face the cut.
        Camera camera = m_rig.goal();
        camera.yaw = section == Section::Width ? 0.50f : 1.05f;
        camera.pitch = 0.32f;
        if (section == Section::Channel)
            camera.target.setX(0.5f * (m_geometry.siliconLeft + position));
        m_rig.animateTo(camera);
        startAnimation();
    }
    update();
}

void MosfetView::setSelectedPart(int id)
{
    if (id == m_selected)
        return;
    m_selected = id;
    emit partSelected(id);
    update();
}

void MosfetView::setCamera(const Camera &camera)
{
    m_rig.jumpTo(camera);
    update();
}

void MosfetView::setCaption(const QString &title, const QString &status, const QColor &statusColor)
{
    m_title = title;
    m_status = status;
    m_statusColor = statusColor;
    update();
}

void MosfetView::setMarkers(double pinchOffMetres, double probeMetres)
{
    m_pinchOff = pinchOffMetres;
    m_probe = probeMetres;
    update();
}

void MosfetView::setFlow(bool visible, double drainCurrent)
{
    m_flow = visible;
    m_current = drainCurrent;
    update();
}

void MosfetView::setParticles(const std::vector<SphereInstance> &particles, const QString &legend)
{
    m_particleData = particles;
    m_particleLegend = legend;
    m_particlesDirty = true;
    update();
}

void MosfetView::setParticlesVisible(bool visible)
{
    m_particles = visible;
    update();
}

void MosfetView::setPreset(Preset preset, bool animate)
{
    const Camera camera = presetCamera(preset);
    if (animate) {
        m_rig.animateTo(camera);
        startAnimation();
    } else {
        m_rig.jumpTo(camera);
    }
    update();
}

void MosfetView::startAnimation()
{
    m_clock.restart();
    if (!m_timer.isActive())
        m_timer.start();
}

Camera MosfetView::presetCamera(Preset preset) const
{
    Camera camera;
    Vec3f lo = m_geometry.boundsMin, hi = m_geometry.boundsMax;
    float liftUp = 0.0f, liftDown = 0.0f;
    for (const mosfet::Part &part : m_geometry.parts()) {
        liftUp = std::max(liftUp, part.explodeWeight * m_explode);
        liftDown = std::min(liftDown, part.explodeWeight * m_explode);
    }
    hi.y += liftUp;
    lo.y += liftDown;
    camera.target = QVector3D(0.5f * (lo.x + hi.x), 0.5f * (lo.y + hi.y), 0.0f);
    const float halfWidth = 0.5f * (hi.x - lo.x);
    const float halfHeight = 0.5f * (hi.y - lo.y) * 1.2f + 0.35f;
    const float t = std::tan(qDegreesToRadians(camera.fov) / 2.0f);
    const float aspect = height() > 0 ? std::max(0.5f, float(width()) / float(height())) : 1.6f;
    // Keep a column of about 270 px free on each side for the labels.
    const float viewWidth = float(std::max(width(), 1));
    const float margin = m_labels ? 270.0f : 50.0f;
    const float usable = std::max(0.35f, (viewWidth - 2.0f * margin) / viewWidth);
    const float distance = std::max(halfHeight / t, halfWidth / (t * aspect * usable)) + 0.5f * (hi.z - lo.z);

    switch (preset) {
    case Preset::Perspective:
        camera.yaw = 0.62f;
        camera.pitch = 0.40f;
        camera.distance = distance;
        break;
    case Preset::CrossSection:
        // Narrow field of view from far away: almost an orthographic 2-D section.
        camera.fov = 10.0f;
        camera.distance = distance * 0.95f * 3.28f;
        break;
    case Preset::Top:
        camera.pitch = 1.45f;
        camera.distance = distance * 0.95f;
        break;
    case Preset::DrainSide:
        camera.yaw = float(M_PI) / 2.0f;
        camera.pitch = 0.22f;
        camera.distance = distance * 0.7f;
        break;
    }
    return CameraRig::clamped(camera);
}

// ---------------------------------------------------------------------------
// Geometry queries
// ---------------------------------------------------------------------------

mosfet::ClipPlane MosfetView::clipPlane() const
{
    mosfet::ClipPlane clip;
    if (m_section == Section::Width) {
        clip.enabled = true;
        clip.normal = {0.0f, 0.0f, -1.0f};   // keep z <= position
        clip.offset = m_sectionPosition;
    } else if (m_section == Section::Channel) {
        clip.enabled = true;
        clip.normal = {-1.0f, 0.0f, 0.0f};   // keep x <= position: the source side
        clip.offset = m_sectionPosition;
    }
    return clip;
}

QMatrix4x4 MosfetView::viewProjection() const
{
    const float aspect = height() > 0 ? float(width()) / float(height()) : 1.0f;
    return m_rig.camera().projection(aspect) * m_rig.camera().view();
}

void MosfetView::ray(const QPointF &position, Vec3f &origin, Vec3f &direction) const
{
    const QMatrix4x4 inverse = viewProjection().inverted();
    const float x = 2.0f * float(position.x()) / float(std::max(1, width())) - 1.0f;
    const float y = 1.0f - 2.0f * float(position.y()) / float(std::max(1, height()));
    QVector4D nearPoint = inverse * QVector4D(x, y, -1.0f, 1.0f);
    QVector4D farPoint = inverse * QVector4D(x, y, 1.0f, 1.0f);
    nearPoint /= nearPoint.w();
    farPoint /= farPoint.w();
    origin = fromQt(m_rig.camera().eye());
    direction = mosfet::normalized(fromQt(farPoint.toVector3D() - nearPoint.toVector3D()));
}

int MosfetView::pickAt(const QPointF &position) const
{
    for (const LabelBox &label : m_labelBoxes) {
        if (label.box.contains(position))
            return label.part;
    }
    if (!m_hasGeometry)
        return -1;
    Vec3f origin, direction;
    ray(position, origin, direction);
    return m_geometry.pick(origin, direction, m_explode, clipPlane(), m_visible, true);
}

QPointF MosfetView::project(Vec3f world, bool *inFront) const
{
    const QVector4D clip = viewProjection() * QVector4D(toQt(world), 1.0f);
    if (inFront)
        *inFront = clip.w() > 1.0e-5f;
    if (clip.w() <= 1.0e-5f)
        return QPointF(-1.0e6, -1.0e6);
    const float x = clip.x() / clip.w(), y = clip.y() / clip.w();
    return QPointF((x + 1.0f) * 0.5f * width(), (1.0f - y) * 0.5f * height());
}

bool MosfetView::anchorVisible(int part, Vec3f world) const
{
    if (clipPlane().distance(world) < -1.0e-3f)
        return false;
    const Vec3f eye = fromQt(m_rig.camera().eye());
    const Vec3f toAnchor = world - eye;
    const float distance = mosfet::length(toAnchor);
    float t = 0.0f;
    const int hit = m_geometry.pick(eye, mosfet::normalized(toAnchor), m_explode, clipPlane(), m_visible, false, &t);
    return hit < 0 || hit == part || t >= distance - 0.03f;
}

// ---------------------------------------------------------------------------
// OpenGL
// ---------------------------------------------------------------------------

void MosfetView::initializeGL()
{
    connect(context(), &QOpenGLContext::aboutToBeDestroyed, this, &MosfetView::releaseGl, Qt::UniqueConnection);
    const QSurfaceFormat actual = context()->format();
    if (actual.version() < qMakePair(3, 3)) {
        m_glError = tr("This computer provides OpenGL %1.%2; the 3D view needs 3.3 or newer.")
                        .arg(actual.majorVersion())
                        .arg(actual.minorVersion());
        return;
    }
    QString error;
    if (!m_renderer.initialize(&error)) {
        m_glError = tr("The 3D renderer could not start: %1").arg(error);
        return;
    }
    m_geometryDirty = m_hasGeometry;
    m_latticeDirty = true;
}

void MosfetView::paintGL()
{
    if (!m_glError.isEmpty()) {
        QPainter painter(this);
        painter.fillRect(rect(), QColor(0x0f, 0x13, 0x19));
        painter.setPen(kText);
        painter.drawText(rect().adjusted(40, 40, -40, -40), Qt::AlignCenter | Qt::TextWordWrap, m_glError);
        return;
    }
    if (m_renderer.initialized()) {
        if (m_geometryDirty) {
            m_renderer.setGeometry(m_geometry);
            m_geometryDirty = false;
            m_latticeDirty = true;
        }
        if (m_particlesDirty) {
            m_renderer.setParticles(m_particleData);
            m_particlesDirty = false;
        }
        if (m_lattice && m_latticeDirty) {
            m_renderer.setLattice(m_geometry.lattice(clipPlane(), m_explode, m_visible));
            m_latticeDirty = false;
        }
        RenderState state;
        state.viewProjection = viewProjection();
        state.eye = m_rig.camera().eye();
        state.clip = clipPlane();
        state.explode = m_explode;
        state.visible = m_visible;
        state.hovered = m_hovered;
        state.selected = m_selected;
        state.lattice = m_lattice;
        state.particles = m_particles && !m_particleData.empty();
        m_renderer.render(state);
    }
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    paintOverlay(painter);
}

void MosfetView::resizeEvent(QResizeEvent *event)
{
    QOpenGLWidget::resizeEvent(event);
    if (!m_framed && m_hasGeometry && width() > 0 && height() > 0) {
        m_rig.jumpTo(presetCamera(Preset::Perspective));
        m_framed = true;
    }
}

// ---------------------------------------------------------------------------
// Overlay: labels, badges, gizmo, captions
// ---------------------------------------------------------------------------

void MosfetView::paintOverlay(QPainter &painter)
{
    m_labelBoxes.clear();
    if (!m_hasGeometry)
        return;
    paintMarkers(painter);
    paintFlow(painter);
    if (m_labels)
        paintLabels(painter);
    paintGizmo(painter);

    QFont font = painter.font();
    font.setPointSizeF(10.0);
    font.setBold(true);
    painter.setFont(font);
    painter.setPen(kText);
    painter.drawText(QRectF(16, 12, width() - 32, 20), Qt::AlignLeft | Qt::AlignVCenter,
                     m_title.isEmpty() ? tr("N-channel enhancement MOSFET  ·  not to scale") : m_title);
    font.setBold(false);
    font.setPointSizeF(9.0);
    painter.setFont(font);
    painter.setPen(m_statusColor.isValid() ? m_statusColor : kDim);
    painter.drawText(QRectF(16, 32, width() - 32, 18), Qt::AlignLeft | Qt::AlignVCenter, m_status);
    if (m_particles && !m_particleLegend.isEmpty()) {
        painter.setPen(kDim);
        painter.drawText(QRectF(16, height() - 46, width() - 140, 18), Qt::AlignLeft | Qt::AlignVCenter, m_particleLegend);
    }

    QString section;
    const mosfet::DisplayScale &s = m_geometry.scale();
    if (m_section == Section::Channel) {
        const float fromSource = (m_sectionPosition - m_geometry.sourceEdge) / s.lateral;
        const float length = (m_geometry.drainEdge - m_geometry.sourceEdge) / s.lateral;
        section = tr("Section across the channel at x = %1 µm from the source junction (%2 % of L)")
                      .arg(QString::number(fromSource, 'f', 2))
                      .arg(QString::number(100.0 * fromSource / length, 'f', 0));
    } else if (m_section == Section::Width) {
        section = tr("Section along the width at z = %1 µm from the centre")
                      .arg(QString::number(m_sectionPosition / s.width, 'f', 2));
    }
    if (!section.isEmpty()) {
        painter.setPen(highlightColor());
        painter.drawText(QRectF(16, 50, width() - 32, 18), Qt::AlignLeft | Qt::AlignVCenter, section);
    }

    painter.setPen(kDim);
    painter.drawText(QRectF(16, height() - 28, width() - 140, 18), Qt::AlignLeft | Qt::AlignVCenter,
                     tr("Drag to rotate  ·  Right-drag to pan  ·  Wheel to zoom  ·  Click to inspect  ·  "
                        "Double-click to focus"));
}

void MosfetView::paintLabels(QPainter &painter)
{
    struct Item
    {
        int part;
        QPointF anchor;
        bool left;
        QRectF box;
    };

    // Screen-space bounds of the visible device.
    QRectF device;
    for (const mosfet::Part &part : m_geometry.parts()) {
        if (!m_visible[mosfet::index(part.id)])
            continue;
        const Vec3f off = m_geometry.offset(part, m_explode);
        Vec3f lo = part.boundsMin, hi = part.boundsMax;
        if (m_section == Section::Channel)
            hi.x = std::min(hi.x, m_sectionPosition);
        if (m_section == Section::Width)
            hi.z = std::min(hi.z, m_sectionPosition);
        if (hi.x < lo.x || hi.z < lo.z)
            continue;
        for (int c = 0; c < 8; ++c) {
            const Vec3f corner{(c & 1) ? hi.x : lo.x, (c & 2) ? hi.y : lo.y, (c & 4) ? hi.z : lo.z};
            bool inFront = false;
            const QPointF p = project(corner + off, &inFront);
            if (inFront)
                device = device.isNull() ? QRectF(p, QSizeF(1, 1)) : device.united(QRectF(p, QSizeF(1, 1)));
        }
    }
    if (device.isNull())
        return;

    QFont titleFont = painter.font();
    titleFont.setPointSizeF(9.5);
    titleFont.setBold(true);
    QFont subtitleFont = titleFont;
    subtitleFont.setBold(false);
    subtitleFont.setPointSizeF(8.5);
    const QFontMetricsF titleMetrics(titleFont), subtitleMetrics(subtitleFont);

    const mosfet::ClipPlane clip = clipPlane();
    std::vector<Item> items;
    for (PartId id : kLabelledParts) {
        const int i = mosfet::index(id);
        const mosfet::Part *part = m_geometry.part(id);
        if (!part || !m_visible[i])
            continue;
        const Vec3f off = m_geometry.offset(*part, m_explode);

        // Anchor candidates, moved onto the cut face where a section is active.
        std::vector<Vec3f> candidates;
        for (Vec3f a : part->anchors) {
            if (m_section == Section::Width && a.z > m_sectionPosition)
                a.z = m_sectionPosition;
            candidates.push_back(a);
        }
        if (m_section == Section::Channel && part->region.xMin() < m_sectionPosition
            && part->region.xMax() > m_sectionPosition) {
            const float x = m_sectionPosition - 0.002f;
            const float top = part->region.topAt(x), bottom = part->region.bottomAt(x);
            if (top - bottom > 0.02f)
                candidates.insert(candidates.begin(), Vec3f{x, 0.5f * (top + bottom), 0.0f});
        }
        for (const Vec3f &candidate : candidates) {
            const Vec3f world = candidate + off;
            if (clip.distance(world) < -1.0e-3f)
                continue;
            bool inFront = false;
            const QPointF screen = project(world, &inFront);
            if (!inFront || !rect().contains(screen.toPoint()) || !anchorVisible(i, world))
                continue;
            const PartText &text = m_texts[i];
            const qreal w = std::max(titleMetrics.horizontalAdvance(text.title),
                                     subtitleMetrics.horizontalAdvance(text.subtitle)) + 24.0;
            items.push_back({i, screen, screen.x() < device.center().x(), QRectF(0, 0, w, 40)});
            break;
        }
    }

    // Two columns beside the device, labels kept near their anchors' heights.
    for (int side = 0; side < 2; ++side) {
        std::vector<Item *> column;
        for (Item &item : items) {
            if (item.left == (side == 0))
                column.push_back(&item);
        }
        std::sort(column.begin(), column.end(), [](Item *a, Item *b) { return a->anchor.y() < b->anchor.y(); });
        qreal bottom = 64.0;
        for (Item *item : column) {
            const qreal w = item->box.width(), h = item->box.height();
            const qreal x = side == 0 ? std::max(10.0, device.left() - 26.0 - w)
                                      : std::min(width() - 10.0 - w, device.right() + 26.0);
            const qreal y = std::max(item->anchor.y() - h / 2.0, bottom + 6.0);
            item->box = QRectF(x, y, w, h);
            bottom = y + h;
        }
        const qreal overflow = bottom - (height() - 40.0);
        if (overflow > 0.0) {
            for (Item *item : column)
                item->box.translate(0.0, -overflow);
        }
    }

    for (const Item &item : items) {
        const PartText &text = m_texts[item.part];
        const QColor color = partMaterial(static_cast<PartId>(item.part)).color;
        const bool active = item.part == m_selected || item.part == m_hovered;
        const QPointF edge(item.left ? item.box.right() : item.box.left(), item.box.center().y());

        painter.setPen(QPen(active ? highlightColor() : QColor(0xc3, 0xcb, 0xd6, 170), 1.2));
        painter.drawLine(item.anchor, edge);
        painter.setPen(QPen(QColor(10, 12, 16), 1.5));
        painter.setBrush(color);
        painter.drawEllipse(item.anchor, 3.5, 3.5);

        painter.setPen(QPen(active ? highlightColor() : kLabelBorder, 1.0));
        painter.setBrush(kLabelFill);
        painter.drawRoundedRect(item.box, 6, 6);
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        painter.drawRoundedRect(QRectF(item.box.left() + 5, item.box.top() + 7, 3, item.box.height() - 14), 1.5, 1.5);

        painter.setFont(titleFont);
        painter.setPen(kText);
        painter.drawText(item.box.adjusted(14, 4, -6, -20), Qt::AlignLeft | Qt::AlignVCenter, text.title);
        painter.setFont(subtitleFont);
        painter.setPen(kDim);
        painter.drawText(item.box.adjusted(14, 20, -6, -4), Qt::AlignLeft | Qt::AlignVCenter, text.subtitle);
        m_labelBoxes.push_back({item.part, item.box, item.anchor});
    }

    // Terminal badges on the contacts.
    QFont badgeFont = titleFont;
    badgeFont.setPointSizeF(9.0);
    painter.setFont(badgeFont);
    struct Badge { PartId id; const char *letter; float lift; };
    for (const Badge &badge : {Badge{PartId::GateContact, "G", -20}, Badge{PartId::SourceContact, "S", -20},
                               Badge{PartId::DrainContact, "D", -20}, Badge{PartId::BodyContact, "B", 20}}) {
        const mosfet::Part *part = m_geometry.part(badge.id);
        if (!part || !m_visible[mosfet::index(badge.id)])
            continue;
        const Vec3f off = m_geometry.offset(*part, m_explode);
        const Vec3f anchor = (badge.id == PartId::BodyContact ? part->anchors.front() : part->anchors.back()) + off;
        bool inFront = false;
        const QPointF screen = project(anchor, &inFront);
        if (!inFront || !anchorVisible(mosfet::index(badge.id), anchor))
            continue;
        const QPointF centre = screen + QPointF(0, badge.lift);
        const bool active = mosfet::index(badge.id) == m_selected || mosfet::index(badge.id) == m_hovered;
        painter.setPen(QPen(active ? highlightColor() : QColor(0xc3, 0xcb, 0xd6, 140), 1.0));
        painter.drawLine(screen, centre + QPointF(0, badge.lift < 0 ? 10 : -10));
        painter.setPen(QPen(active ? highlightColor() : kLabelBorder, 1.2));
        painter.setBrush(kLabelFill);
        painter.drawEllipse(centre, 10, 10);
        painter.setPen(kText);
        painter.drawText(QRectF(centre.x() - 10, centre.y() - 10, 20, 20), Qt::AlignCenter, QString::fromLatin1(badge.letter));
        m_labelBoxes.push_back({mosfet::index(badge.id), QRectF(centre.x() - 10, centre.y() - 10, 20, 20), screen});
    }
}

// Pinch-off point and probe: lines across the channel surface and down the front face.
void MosfetView::paintMarkers(QPainter &painter)
{
    const mosfet::Part *channel = m_geometry.part(PartId::Channel);
    const mosfet::Part *depletion = m_geometry.part(PartId::Depletion);
    const Vec3f lift = depletion ? m_geometry.offset(*depletion, m_explode) : Vec3f{};
    QFont font = painter.font();
    font.setPointSizeF(8.5);
    font.setBold(true);
    painter.setFont(font);
    auto draw = [&](double metres, const QColor &color, const QString &text, bool dashed) {
        const float x = m_geometry.toDisplayX(metres);
        const float depth = depletion ? -depletion->region.bottomAt(x) : 0.3f;
        const Vec3f a{x, 0.004f, m_geometry.zBack}, b{x, 0.004f, m_geometry.zFront}, c{x, -depth, m_geometry.zFront + 0.002f};
        if (clipPlane().distance(b + lift) < -1.0e-3f)
            return;
        bool f1 = false, f2 = false, f3 = false;
        const QPointF pa = project(a + lift, &f1), pb = project(b + lift, &f2), pc = project(c + lift, &f3);
        if (!f1 || !f2 || !f3)
            return;
        painter.setPen(QPen(color, 1.8, dashed ? Qt::DashLine : Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(pa, pb);
        painter.drawLine(pb, pc);
        painter.setPen(color);
        painter.drawText(QRectF(pc.x() - 70, pc.y() + 4, 140, 16), Qt::AlignCenter, text);
    };
    Q_UNUSED(channel);
    if (m_showPinch && m_pinchOff >= 0.0)
        draw(m_pinchOff, QColor(0xff, 0x7a, 0x6b), tr("pinch-off"), false);
    if (m_showProbe && m_probe >= 0.0)
        draw(m_probe, QColor(0xc3, 0x8b, 0xff), tr("probe"), true);
}

// Electron flow (source -> drain) and conventional current (drain -> source),
// drawn above the device when a drain current flows.
void MosfetView::paintFlow(QPainter &painter)
{
    if (!m_flow || m_current <= 0.0)
        return;
    const float y1 = m_geometry.gateTop + 1.05f + 2.0f * m_explode;
    const float y2 = y1 + 0.55f;
    const float z = m_geometry.zFront + 0.3f;
    QFont font = painter.font();
    font.setPointSizeF(8.5);
    font.setBold(true);
    painter.setFont(font);
    auto arrow = [&](float from, float to, float y, const QColor &color, const QString &text) {
        bool f1 = false, f2 = false;
        const QPointF a = project({from, y, z}, &f1), b = project({to, y, z}, &f2);
        if (!f1 || !f2)
            return;
        painter.setPen(QPen(color, 2.2, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(a, b);
        const QLineF line(b, a);
        QLineF wing1 = line, wing2 = line;
        wing1.setLength(11);
        wing2.setLength(11);
        wing1.setAngle(line.angle() + 25);
        wing2.setAngle(line.angle() - 25);
        painter.setBrush(color);
        painter.setPen(Qt::NoPen);
        painter.drawPolygon(QPolygonF({b, wing1.p2(), wing2.p2()}));
        painter.setPen(color);
        const QPointF mid = (a + b) / 2.0;
        painter.drawText(QRectF(mid.x() - 160, mid.y() - 19, 320, 16), Qt::AlignCenter, text);
    };
    arrow(m_geometry.sourceEdge, m_geometry.drainEdge, y1, QColor(0x6e, 0xc3, 0xff), tr("electron flow: source → drain"));
    arrow(m_geometry.drainEdge, m_geometry.sourceEdge, y2, QColor(0xff, 0xb4, 0x54),
          tr("conventional current ID = %1: drain → source").arg(Format::si(m_current, QStringLiteral("A"), 3)));
}

void MosfetView::paintGizmo(QPainter &painter)
{
    const QPointF origin(width() - 62.0, height() - 58.0);
    const QMatrix4x4 view = m_rig.camera().view();
    struct Axis { QVector3D direction; QColor color; QString name; };
    std::vector<Axis> axes = {{QVector3D(1, 0, 0), QColor(0xff, 0x7a, 0x6b), tr("x  S→D")},
                              {QVector3D(0, 1, 0), QColor(0x7b, 0xdc, 0x8c), tr("y")},
                              {QVector3D(0, 0, 1), QColor(0x6a, 0xb0, 0xff), tr("z  W")}};
    std::sort(axes.begin(), axes.end(), [&](const Axis &a, const Axis &b) {
        return view.mapVector(a.direction).z() < view.mapVector(b.direction).z();
    });
    QFont font = painter.font();
    font.setPointSizeF(8.5);
    font.setBold(true);
    painter.setFont(font);
    for (const Axis &axis : axes) {
        const QVector3D d = view.mapVector(axis.direction);
        const QPointF end = origin + QPointF(d.x(), -d.y()) * 28.0;
        painter.setPen(QPen(axis.color, 2.0, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(origin, end);
        const QPointF textAt = origin + QPointF(d.x(), -d.y()) * 40.0;
        painter.drawText(QRectF(textAt.x() - 30, textAt.y() - 8, 60, 16), Qt::AlignCenter, axis.name);
    }
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------

void MosfetView::mousePressEvent(QMouseEvent *event)
{
    m_pressPosition = m_lastPosition = event->position();
    m_button = event->button();
    m_dragged = false;
    setFocus();
}

void MosfetView::mouseMoveEvent(QMouseEvent *event)
{
    const QPointF delta = event->position() - m_lastPosition;
    m_lastPosition = event->position();
    if (event->buttons() != Qt::NoButton) {
        if (!m_dragged && (event->position() - m_pressPosition).manhattanLength() > 3.0)
            m_dragged = true;
        if (!m_dragged)
            return;
        if (event->buttons() & Qt::LeftButton) {
            setCursor(Qt::ClosedHandCursor);
            m_rig.orbit(-float(delta.x()) * 0.008f, float(delta.y()) * 0.008f);
            emit cameraChanged();
        } else {
            setCursor(Qt::SizeAllCursor);
            m_rig.pan(float(delta.x()), float(delta.y()), float(height()));
            emit cameraChanged();
        }
        update();
        return;
    }
    const int hovered = pickAt(event->position());
    if (hovered != m_hovered) {
        m_hovered = hovered;
        setCursor(hovered >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
        emit partHovered(hovered);
        update();
    }
}

void MosfetView::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == m_button && !m_dragged && event->button() == Qt::LeftButton)
        setSelectedPart(pickAt(event->position()));
    m_button = Qt::NoButton;
    setCursor(m_hovered >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
}

void MosfetView::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return;
    const int id = pickAt(event->position());
    if (id < 0) {
        setPreset(Preset::Perspective);
        return;
    }
    const mosfet::Part *part = m_geometry.part(static_cast<PartId>(id));
    if (!part)
        return;
    const Vec3f centre = (part->boundsMin + part->boundsMax) * 0.5f + m_geometry.offset(*part, m_explode);
    const Vec3f size = part->boundsMax - part->boundsMin;
    Camera camera = m_rig.goal();
    camera.target = toQt(centre);
    camera.distance = std::clamp(std::max({size.x, size.y, size.z}) * 3.2f, 4.0f, m_rig.camera().distance);
    m_rig.animateTo(camera);
    startAnimation();
    setSelectedPart(id);
}

void MosfetView::wheelEvent(QWheelEvent *event)
{
    m_rig.zoom(std::pow(0.88f, float(event->angleDelta().y()) / 120.0f));
    emit cameraChanged();
    update();
    event->accept();
}

void MosfetView::leaveEvent(QEvent *event)
{
    QOpenGLWidget::leaveEvent(event);
    if (m_hovered >= 0) {
        m_hovered = -1;
        emit partHovered(-1);
        update();
    }
}

bool MosfetView::event(QEvent *event)
{
    if (event->type() == QEvent::ToolTip) {
        auto *help = static_cast<QHelpEvent *>(event);
        const int id = pickAt(help->pos());
        if (id >= 0 && !m_texts[id].title.isEmpty()) {
            QToolTip::showText(help->globalPos(), tooltipHtml(m_texts[id]), this);
        } else {
            QToolTip::hideText();
            event->ignore();
        }
        return true;
    }
    return QOpenGLWidget::event(event);
}
