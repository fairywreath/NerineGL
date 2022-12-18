#include "Camera.h"

#include <glm/gtx/euler_angles.hpp>

namespace Nerine
{

Camera::Camera(CameraController& controller) : m_Controller(&controller)
{
}

mat4 Camera::GetViewMatrix() const
{
    return m_Controller->GetViewMatrix();
}

vec3 Camera::GetPosition() const
{
    return m_Controller->GetPosition();
}

FirstPersonCameraController::FirstPersonCameraController(const vec3& pos, const vec3& target,
                                                         const vec3& up)
    : m_CameraPosition(pos), m_CameraOrientation(glm::lookAt(pos, target, up)), m_Up(up)
{
}

mat4 FirstPersonCameraController::GetViewMatrix() const
{
    const glm::mat4 transMatrix = glm::translate(glm::mat4(1.0f), -m_CameraPosition);
    const glm::mat4 rotMatrix = glm::mat4_cast(m_CameraOrientation);
    return rotMatrix * transMatrix;
}

vec3 FirstPersonCameraController::GetPosition() const
{
    return m_CameraPosition;
}

void FirstPersonCameraController::Update(double deltaSeconds, const glm::vec2& mousePos,
                                         bool mousePressed)
{
    if (mousePressed)
    {
        const vec2 delta = mousePos - m_MousePosition;
        const quat deltaQuat = quat(vec3(-MouseSpeed * delta.y, MouseSpeed * delta.x, 0.0f));
        m_CameraOrientation = deltaQuat * m_CameraOrientation;
        m_CameraOrientation = glm::normalize(m_CameraOrientation);
        SetUpVector(m_Up);
    }
    m_MousePosition = mousePos;

    const mat4 v = mat4_cast(m_CameraOrientation);

    const vec3 forward = -vec3(v[0][2], v[1][2], v[2][2]);
    const vec3 right = vec3(v[0][0], v[1][0], v[2][0]);
    const vec3 up = glm::cross(right, forward);

    glm::vec3 accel(0.0f);

    if (Movement.forward)
        accel += forward;
    if (Movement.backward)
        accel -= forward;

    if (Movement.left)
        accel -= right;
    if (Movement.right)
        accel += right;

    if (Movement.up)
        accel += up;
    if (Movement.down)
        accel -= up;

    accel *= FastCoef;

    // if (Movement.fast)
    // accel *= FastCoef;

    if (accel == vec3(0))
    {
        // decelerate naturally according to the damping value
        m_MoveSpeed
            -= m_MoveSpeed * std::min((1.0f / Damping) * static_cast<float>(deltaSeconds), 1.0f);
    }
    else
    {
        // acceleration
        m_MoveSpeed += accel * Acceleration * static_cast<float>(deltaSeconds);
        // const float maxSpeed = Movement.fast ? MaxSpeed * FastCoef : MaxSpeed;
        const float maxSpeed = MaxSpeed * FastCoef;
        if (glm::length(m_MoveSpeed) > maxSpeed)
            m_MoveSpeed = glm::normalize(m_MoveSpeed) * maxSpeed;
    }

    m_CameraPosition += m_MoveSpeed * static_cast<float>(deltaSeconds);
}

void FirstPersonCameraController::SetPosition(const vec3& pos)
{
    m_CameraPosition = pos;
}

void FirstPersonCameraController::ResetMousePosition(const vec2& pos)
{
    m_MousePosition = pos;
};

void FirstPersonCameraController::SetUpVector(const vec3& up)
{
    const mat4 view = GetViewMatrix();
    const vec3 dir = -glm::vec3(view[0][2], view[1][2], view[2][2]);
    m_CameraOrientation = glm::lookAt(m_CameraPosition, m_CameraPosition + dir, up);
}

/*
 * Free moving camera
 */
FreeMovingCameraController::FreeMovingCameraController(const vec3& pos, const vec3& angles)
    : m_PositionCurrent(pos), m_PositionDesired(pos), m_AnglesCurrent(angles),
      m_AnglesDesired(angles)
{
}

mat4 FreeMovingCameraController::GetViewMatrix() const
{
    return m_CurrentTransform;
}

vec3 FreeMovingCameraController::GetPosition() const
{
    return m_PositionCurrent;
}

void FreeMovingCameraController::Update(float deltaSeconds)
{
    m_PositionCurrent += DampingLinear * deltaSeconds * (m_PositionDesired - m_PositionCurrent);

    // clip angles if "spin" is too great
    m_AnglesCurrent = ClipAngles(m_AnglesCurrent);
    m_AnglesDesired = ClipAngles(m_AnglesDesired);

    m_AnglesCurrent
        -= AngleDelta(m_AnglesCurrent, m_AnglesDesired) * DampingEulerAngles * deltaSeconds;

    // normalize/clip angles again if "spin" is too great
    m_AnglesCurrent = ClipAngles(m_AnglesCurrent);

    const vec3 a = glm::radians(m_AnglesCurrent);

    m_CurrentTransform = glm::translate(glm::yawPitchRoll(a.y, a.x, a.z), -m_PositionCurrent);
}

void FreeMovingCameraController::SetPosition(const vec3& pos)
{
    m_PositionCurrent = pos;
}

void FreeMovingCameraController::SetAngles(float pitch, float pan, float roll)
{
    m_AnglesCurrent = {pitch, pan, roll};
}

void FreeMovingCameraController::SetAngles(const vec3& angles)
{
    m_AnglesCurrent = angles;
}

void FreeMovingCameraController::SetDesiredPosition(const vec3& pos)
{
    m_PositionDesired = pos;
}

void FreeMovingCameraController::SetDesiredAngles(float pitch, float pan, float roll)
{
    m_AnglesDesired = {pitch, pan, roll};
}

void FreeMovingCameraController::SetDesiredAngles(const vec3& angles)
{
    m_AnglesDesired = angles;
}

} // namespace Nerine
