#pragma once

#include "PartInfo.h"
#include "geometry/MosfetGeometry.h"
#include "render/Camera.h"
#include "render/MosfetRenderer.h"

#include <QElapsedTimer>
#include <QOpenGLWidget>
#include <QTimer>

#include <vector>

// Interactive 3D view of the MOSFET: renders through MosfetRenderer and draws
// labels, terminal badges and an axis gizmo on top with QPainter.
//
// Mouse: drag to rotate, right- or middle-drag to pan, wheel to zoom, click to
// select a part, double-click to focus on it (double-click empty space resets).
class MosfetView : public QOpenGLWidget
{
    Q_OBJECT

public:
    enum class Preset { Perspective, CrossSection, Top, DrainSide };
    enum class Section { None, Width, Channel };

    explicit MosfetView(QWidget *parent = nullptr);
    ~MosfetView() override;

    void setDeviceGeometry(const mosfet::MosfetGeometry &geometry);
    void setTexts(const PartTexts &texts) { m_texts = texts; update(); }

    void setPartVisible(mosfet::PartId id, bool visible);
    bool partVisible(mosfet::PartId id) const { return m_visible[mosfet::index(id)]; }
    void setLatticeVisible(bool visible);
    void setLabelsVisible(bool visible);
    void setExplode(float amount);   // 0..1
    // position: display z for Width, display x for Channel.
    void setSection(Section section, float position);
    Section section() const { return m_section; }
    float explode() const { return m_explode; }
    int particleCount() const { return int(m_particleData.size()); }
    float sectionPosition() const { return m_sectionPosition; }

    void setPreset(Preset preset, bool animate = true);
    void setCamera(const Camera &camera);   // jump, without emitting cameraChanged

    // Live content from the page.
    void setCaption(const QString &title, const QString &status, const QColor &statusColor);
    void setMarkers(double pinchOffMetres, double probeMetres);   // negative: none
    void setFlow(bool visible, double drainCurrent);
    void setParticles(const std::vector<SphereInstance> &particles, const QString &legend);
    void setParticlesVisible(bool visible);
    void setPinchMarkerVisible(bool visible) { m_showPinch = visible; update(); }
    void setProbeMarkerVisible(bool visible) { m_showProbe = visible; update(); }
    int selectedPart() const { return m_selected; }
    void setSelectedPart(int id);

    // Used by tests and by the page.
    const mosfet::MosfetGeometry &deviceGeometry() const { return m_geometry; }
    const CameraRig &cameraRig() const { return m_rig; }
    mosfet::ClipPlane clipPlane() const;
    int pickAt(const QPointF &position) const;
    QPointF project(mosfet::Vec3f world, bool *inFront = nullptr) const;
    QString glError() const { return m_glError; }
    struct LabelBox { int part; QRectF box; QPointF anchor; };
    const std::vector<LabelBox> &labelBoxes() const { return m_labelBoxes; }

signals:
    void partSelected(int id);   // -1 when the selection is cleared
    void partHovered(int id);
    void cameraChanged();

protected:
    void initializeGL() override;
    void paintGL() override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    bool event(QEvent *event) override;

private:
    QMatrix4x4 viewProjection() const;
    void ray(const QPointF &position, mosfet::Vec3f &origin, mosfet::Vec3f &direction) const;
    Camera presetCamera(Preset preset) const;
    bool anchorVisible(int part, mosfet::Vec3f world) const;
    void startAnimation();
    void paintOverlay(QPainter &painter);
    void paintLabels(QPainter &painter);
    void paintGizmo(QPainter &painter);
    void paintMarkers(QPainter &painter);
    void paintFlow(QPainter &painter);
    void releaseGl();

    MosfetRenderer m_renderer;
    CameraRig m_rig;
    mosfet::MosfetGeometry m_geometry;
    bool m_hasGeometry = false;
    bool m_geometryDirty = false;
    bool m_latticeDirty = true;
    bool m_framed = false;
    PartTexts m_texts;

    mosfet::PartFlags m_visible{};
    bool m_lattice = false;
    bool m_labels = true;
    float m_explode = 0.0f;
    Section m_section = Section::None;
    float m_sectionPosition = 0.0f;

    int m_hovered = -1;
    int m_selected = -1;
    QPointF m_pressPosition, m_lastPosition;
    Qt::MouseButton m_button = Qt::NoButton;
    bool m_dragged = false;

    QTimer m_timer;
    QElapsedTimer m_clock;
    QString m_glError;
    std::vector<LabelBox> m_labelBoxes;

    QString m_title, m_status;
    QColor m_statusColor;
    double m_pinchOff = -1.0, m_probe = -1.0;
    bool m_showPinch = true, m_showProbe = true;
    bool m_flow = false;
    double m_current = 0.0;
    std::vector<SphereInstance> m_particleData;
    bool m_particlesDirty = false;
    bool m_particles = true;
    QString m_particleLegend;
    float m_framedWidth = 0.0f;   // device x-extent when the camera was last framed
};
