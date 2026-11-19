#pragma once

#include <QMatrix4x4>
#include <QVector3D>

// Orbit camera: looks at `target` from `distance` away. yaw 0 / pitch 0 looks
// straight at the front (cross-section) face, along -z.
struct Camera
{
    QVector3D target{0.0f, -0.4f, 0.0f};
    float yaw = 0.0f;        // radians, around the vertical axis
    float pitch = 0.0f;      // radians, above the horizontal
    float distance = 12.0f;
    float fov = 32.0f;       // vertical field of view, degrees

    QVector3D eye() const;
    QMatrix4x4 view() const;
    QMatrix4x4 projection(float aspect) const;
};

// Holds the current camera and a goal. animateTo() starts a fixed-length
// ease-in-out transition, so preset changes take the same time at any frame rate.
class CameraRig
{
public:
    static constexpr float kMinPitch = -1.50f;   // about +/-86 degrees
    static constexpr float kMaxPitch = 1.50f;
    static constexpr float kMinDistance = 2.5f;
    static constexpr float kMaxDistance = 240.0f;

    const Camera &camera() const { return m_current; }
    const Camera &goal() const { return m_goal; }
    bool animating() const { return m_animating; }

    void jumpTo(const Camera &camera);        // no animation
    void animateTo(const Camera &camera);

    // Direct manipulation (mouse): moves both current and goal.
    void orbit(float deltaYaw, float deltaPitch);
    void pan(float dxPixels, float dyPixels, float viewportHeight);
    void zoom(float factor);

    // Returns true while still moving.
    bool update(float seconds);

    static Camera clamped(Camera camera);

private:
    static constexpr float kTransitionSeconds = 0.65f;
    Camera m_current, m_goal, m_start;
    float m_elapsed = 0.0f;
    bool m_animating = false;
};
