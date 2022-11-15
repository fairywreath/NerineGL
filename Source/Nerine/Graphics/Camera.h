#pragma once

#include <Core/Types.h>

namespace Nerine
{

using quat = glm::quat;

class CameraController
{
public:
    virtual mat4 GetViewMatrix() const = 0;
    virtual vec3 GetPosition() const = 0;
};

class Camera
{
public:
    explicit Camera(CameraController& controller);

    mat4 GetViewMatrix() const;
    vec3 GetPosition() const;

private:
    const CameraController* m_Controller;
};

/*
 * Quaternion based first-person/look-at camera
 */
class FirstPersonCameraController final : public CameraController
{
public:
    FirstPersonCameraController() = default;
    FirstPersonCameraController(const vec3& pos, const vec3& target, const vec3& up);

    mat4 GetViewMatrix() const override;
    vec3 GetPosition() const override;

    void Update(double deltaSeconds, const vec2& mousePos, bool mousePressed);

    void SetPosition(const vec3& pos);
    void ResetMousePosition(const vec2& pos);
    void SetUpVector(const vec3& up);

    inline void LookAt(const vec3& pos, const vec3& target, const vec3& up)
    {
        m_CameraPosition = pos;
        m_CameraOrientation = glm::lookAt(pos, target, up);
    }

public:
    struct Movement
    {
        bool forward{false};
        bool backward{false};
        bool left{false};
        bool right{false};
        bool up{false};
        bool down{false};

        bool fast{false};
    } Movement;

public:
    float MouseSpeed{4.0f};
    float Acceleration{150.0f};
    float Damping{0.1f};
    float MaxSpeed{10.0f};
    float FastCoef{10.0f};

private:
    vec2 m_MousePosition{0.0f, 0.0f};
    vec3 m_CameraPosition{0.0f, 10.0f, 10.0f};
    quat m_CameraOrientation{quat(vec3(0))};
    vec3 m_MoveSpeed{0.0f, 0.0f, 0.0f};
    vec3 m_Up{0.0f, 1.0f, 0.0f};
};

/*
 * Euler angles based free-moving/move-to camera
 */
class FreeMovingCameraController final : public CameraController
{
public:
    FreeMovingCameraController() = default;
    FreeMovingCameraController(const vec3& pos, const vec3& angles);

    mat4 GetViewMatrix() const override final;
    vec3 GetPosition() const override final;

    void Update(float deltaSeconds);

    void SetPosition(const vec3& pos);
    void SetAngles(float pitch, float pan, float roll);
    void SetAngles(const vec3& angles);

    void SetDesiredPosition(const vec3& pos);
    void SetDesiredAngles(float pitch, float pan, float roll);
    void SetDesiredAngles(const vec3& angles);

public:
    float DampingLinear{10.0f};
    vec3 DampingEulerAngles{5.0f, 5.0f, 5.0f};

private:
    vec3 m_PositionCurrent{vec3(0.0f)};
    vec3 m_PositionDesired{vec3(0.0f)};

    // vec3: pitch, pan, roll.
    vec3 m_AnglesCurrent{vec3(0.0f)};
    vec3 m_AnglesDesired{vec3(0.0f)};

    mat4 m_CurrentTransform{mat4(1.0f)};

    static inline float ClipAngle(float d)
    {
        if (d < -180.0f)
            return d + 360.0f;
        if (d > +180.0f)
            return d - 360.f;
        return d;
    }

    static inline vec3 ClipAngles(const vec3& angles)
    {
        return vec3(std::fmod(angles.x, 360.0f), std::fmod(angles.y, 360.0f),
                    std::fmod(angles.z, 360.0f));
    }

    static inline vec3 AngleDelta(const vec3& anglesCurrent, const vec3& anglesDesired)
    {
        const vec3 d = ClipAngles(anglesCurrent) - ClipAngles(anglesDesired);
        return vec3(ClipAngle(d.x), ClipAngle(d.y), ClipAngle(d.z));
    }
};

} // namespace Nerine
