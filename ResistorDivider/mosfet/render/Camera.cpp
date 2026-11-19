#include "Camera.h"

#include <QtMath>

#include <algorithm>
#include <cmath>

QVector3D Camera::eye() const
{
    const float cp = std::cos(pitch);
    return target + distance * QVector3D(cp * std::sin(yaw), std::sin(pitch), cp * std::cos(yaw));
}

QMatrix4x4 Camera::view() const
{
    QMatrix4x4 m;
    m.lookAt(eye(), target, QVector3D(0.0f, 1.0f, 0.0f));
    return m;
}

QMatrix4x4 Camera::projection(float aspect) const
{
    QMatrix4x4 m;
    m.perspective(fov, std::max(aspect, 0.1f), distance * 0.05f, distance * 10.0f);
    return m;
}

Camera CameraRig::clamped(Camera camera)
{
    camera.pitch = std::clamp(camera.pitch, kMinPitch, kMaxPitch);
    camera.distance = std::clamp(camera.distance, kMinDistance, kMaxDistance);
    return camera;
}

void CameraRig::jumpTo(const Camera &camera)
{
    m_current = m_goal = clamped(camera);
    m_animating = false;
}

void CameraRig::animateTo(const Camera &camera)
{
    m_start = m_current;
    m_elapsed = 0.0f;
    m_goal = clamped(camera);
    // Take the short way round.
    const float twoPi = 2.0f * float(M_PI);
    while (m_goal.yaw - m_current.yaw > float(M_PI))
        m_goal.yaw -= twoPi;
    while (m_goal.yaw - m_current.yaw < -float(M_PI))
        m_goal.yaw += twoPi;
    m_animating = true;
}

void CameraRig::orbit(float deltaYaw, float deltaPitch)
{
    m_current.yaw += deltaYaw;
    m_current.pitch += deltaPitch;
    m_current = clamped(m_current);
    m_goal = m_current;
    m_animating = false;
}

void CameraRig::pan(float dxPixels, float dyPixels, float viewportHeight)
{
    // Move the target so the scene follows the mouse at the target's depth.
    const float unitsPerPixel = 2.0f * m_current.distance * std::tan(qDegreesToRadians(m_current.fov) / 2.0f)
                                / std::max(viewportHeight, 1.0f);
    const QVector3D forward = (m_current.target - m_current.eye()).normalized();
    const QVector3D right = QVector3D::crossProduct(forward, QVector3D(0, 1, 0)).normalized();
    const QVector3D up = QVector3D::crossProduct(right, forward).normalized();
    m_current.target += (-dxPixels * right + dyPixels * up) * unitsPerPixel;
    m_goal = m_current;
    m_animating = false;
}

void CameraRig::zoom(float factor)
{
    if (m_animating) {   // zoom the whole transition
        m_start.distance = std::clamp(m_start.distance * factor, kMinDistance, kMaxDistance);
        m_goal.distance = std::clamp(m_goal.distance * factor, kMinDistance, kMaxDistance);
    }
    m_current.distance *= factor;
    m_current = clamped(m_current);
    if (!m_animating)
        m_goal = m_current;
}

bool CameraRig::update(float seconds)
{
    if (!m_animating)
        return false;
    m_elapsed += seconds;
    const float u = std::clamp(m_elapsed / kTransitionSeconds, 0.0f, 1.0f);
    const float s = u * u * u * (u * (u * 6.0f - 15.0f) + 10.0f);   // smootherstep
    auto mix = [s](float a, float b) { return a + (b - a) * s; };
    m_current.yaw = mix(m_start.yaw, m_goal.yaw);
    m_current.pitch = mix(m_start.pitch, m_goal.pitch);
    // Distance eases geometrically so zooming in and out feel the same.
    m_current.distance = m_start.distance * std::pow(m_goal.distance / m_start.distance, s);
    m_current.target = m_start.target + (m_goal.target - m_start.target) * s;
    m_current.fov = mix(m_start.fov, m_goal.fov);
    if (u >= 1.0f) {
        m_current = m_goal;
        m_animating = false;
    }
    return m_animating;
}
