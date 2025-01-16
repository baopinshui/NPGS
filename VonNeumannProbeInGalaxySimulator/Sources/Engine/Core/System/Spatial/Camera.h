#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include "Engine/Core/Base/Base.h"

_NPGS_BEGIN
_SYSTEM_BEGIN
_SPATIAL_BEGIN

constexpr glm::vec3 kPosition    = glm::vec3(0.0f);
constexpr glm::vec3 kFront       = glm::vec3(0.0f, 0.0f, -1.0f);
constexpr glm::vec3 kWorldUp     = glm::vec3(0.0f, 1.0f, 0.0f);
constexpr float     kSensitivity = 0.05f;
constexpr float     kSpeed       = 2.5f;
constexpr float     kZoom        = 45.0f;

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
    FCamera(const glm::vec3& Position = kPosition, const glm::vec3& WorldUp = kWorldUp, float Sensitivity = kSensitivity,
            float Speed = kSpeed, float Zoom = kZoom);

    ~FCamera() = default;

    const glm::vec3& GetCameraVector(EVectorType Type) const;
    void ProcessKeyboard(EMovement Direction, double DeltaTime);
    void ProcessMouseMovement(double OffsetX, double OffsetY);
    void ProcessMouseScroll(double OffsetY);
    float GetCameraZoom() const;
    glm::mat4x4 GetViewMatrix() const;
    void SetOrientation(const glm::quat& Orientation);
    const glm::quat& GetOrientation() const;

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
    float     _Sensitivity;
    float     _Speed;
    float     _Zoom;
    float     _PrevOffsetX;
    float     _PrevOffsetY;
};

_SPATIAL_END
_SYSTEM_END
_NPGS_END

#include "Camera.inl"
