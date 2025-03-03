#pragma once

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "Engine/Core/Base/Base.h"

_NPGS_BEGIN
_SYSTEM_BEGIN
_SPATIAL_BEGIN

constexpr glm::vec3 kPosition    = glm::vec3(0.0f);
constexpr float     kSensitivity = 0.05f;
constexpr float     kSpeed       = 0.00000025f;
constexpr float     kZoom        = 60.0f;

class FCamera
{
public:
    enum class EMovement
    {
        kForward,
        kBack,
        kLeft,
        kRight,
        kUp,
        kDown,
        kRollLeft,
        kRollRight
    };

    enum class EVectorType
    {
        kPosition,
        kFront,
        kUp,
        kRight
    };

public:
    FCamera() = delete;
    FCamera(const glm::vec3& Position = kPosition, float Sensitivity = kSensitivity, float Speed = kSpeed, float Zoom = kZoom);

    ~FCamera() = default;

    void ProcessKeyboard(EMovement Direction, double DeltaTime);
    void ProcessMouseMovement(double OffsetX, double OffsetY);
    void ProcessMouseScroll(double OffsetY);
    void ProcessOrbital(double OffsetX, double OffsetY);
    void ProcessTimeEvolution(double DeltaTime);
    void ProcessModeChange();
    void SetOrientation(const glm::quat& Orientation);
    void SetCameraVector(EVectorType Type, const glm::vec3& NewVector);
    void SetCameraMode(bool bIsOrbiting);
    void SetOrbitMode(bool bAllowCrossRotZenith);
    void SetTargetOrbitCenter(glm::vec3 Center);
    void SetTargetOrbitAxis(glm::vec3 Axis);
    void SetFov(float Fov);
    float GetFov() const;
    const glm::quat& GetOrientation() const;
    const glm::vec3& GetCameraVector(EVectorType Type) const;
    glm::mat4x4 GetViewMatrix() const;
    glm::mat4x4 GetProjectionMatrix(float WindowAspect, float Near) const;
    float GetCameraZoom() const;
    bool GetCameraMode() const;

private:
    void ProcessRotation(float Yaw, float Pitch, float Roll);
    void UpdateVectors();

private:
    glm::quat _Orientation;
    glm::vec3 _Position;
    glm::vec3 _Front;
    glm::vec3 _Up;
    glm::vec3 _Right;
    glm::vec3 _WorldUp;

    glm::vec3 _AxisDir;//绕转轴方向
    glm::vec3 _TargetAxisDir;//绕转轴方向
    glm::vec3 _OrbitalCenter;
    glm::vec3 _TargetOrbitalCenter;
    float     _Theta;
    float     _Phi;
    float     _DistanceToOrbitalCenter;
    float     _TargetDistanceToOrbitalCenter;

    float     _RotationSmoothCoefficient;
    float     _OrbitDistanceRotationSmoothCoefficient;
    float     _OrbitCenterChangeSmoothCoefficient;
    float     _OrbitAxisChangeSmoothCoefficient;
    float     _Sensitivity;
    float     _Speed;
    float     _Zoom;
    float     _TargetOffsetX;
    float     _TargetOffsetY;
    float     _OffsetX;
    float     _OffsetY;
    float     _TimeSinceModeChange;
    bool      _bIsOrbiting;
    bool      _bAllowCrossZenith;
};

_SPATIAL_END
_SYSTEM_END
_NPGS_END

#include "Camera.inl"
