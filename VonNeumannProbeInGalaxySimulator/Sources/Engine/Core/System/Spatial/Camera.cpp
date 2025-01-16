#include "Camera.h"

#include "Engine/Core/Base/Assert.h"

_NPGS_BEGIN
_SYSTEM_BEGIN
_SPATIAL_BEGIN

FCamera::FCamera(const glm::vec3& Position, const glm::vec3& WorldUp, float Sensitivity, float Speed, float Zoom)
    :
    _Orientation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f)),
    _Position(Position),
    _Front(kFront),
    _WorldUp(WorldUp),
    _Sensitivity(Sensitivity),
    _Speed(Speed),
    _Zoom(Zoom),
    _PrevOffsetX(0.0f),
    _PrevOffsetY(0.0f)
{
    UpdateVectors();
}

#pragma warning(push)
#pragma warning(disable : 4715)
const glm::vec3& FCamera::GetCameraVector(EVectorType Type) const
{
    switch (Type)
    {
    case EVectorType::kPosition:
        return _Position;
    case EVectorType::kFront:
        return _Front;
    case EVectorType::kUp:
        return _Up;
    case EVectorType::kRight:
        return _Right;
    default:
        NpgsAssert(false, "Invalid vector type");
    }
}
#pragma warning(pop)

void FCamera::ProcessKeyboard(EMovement Direction, double DeltaTime)
{
    float Velocity = static_cast<float>(_Speed * DeltaTime);

    switch (Direction)
    {
    case EMovement::kForward:
        _Position += _Front * Velocity;
        break;
    case EMovement::kBack:
        _Position -= _Front * Velocity;
        break;
    case EMovement::kLeft:
        _Position -= _Right * Velocity;
        break;
    case EMovement::kRight:
        _Position += _Right * Velocity;
        break;
    case EMovement::kUp:
        _Position += _Up * Velocity;
        break;
    case EMovement::kDown:
        _Position -= _Up * Velocity;
        break;
    case EMovement::kRollLeft:
        ProcessRotation(0.0f, 0.0f, -10.0f * Velocity);
        break;
    case EMovement::kRollRight:
        ProcessRotation(0.0f, 0.0f,  10.0f * Velocity);
        break;
    }

    UpdateVectors();
}

void FCamera::ProcessMouseMovement(double OffsetX, double OffsetY)
{
    static float SmoothCoefficient = 0.1f;
    float SmoothedX = SmoothCoefficient * static_cast<float>(OffsetX) + (1.0f - SmoothCoefficient) * _PrevOffsetX;
    float SmoothedY = SmoothCoefficient * static_cast<float>(OffsetY) + (1.0f - SmoothCoefficient) * _PrevOffsetY;
    _PrevOffsetX = SmoothedX;
    _PrevOffsetY = SmoothedY;

    float HorizontalAngle = static_cast<float>(_Sensitivity *  SmoothedX);
    float VerticalAngle   = static_cast<float>(_Sensitivity * -SmoothedY);

    ProcessRotation(HorizontalAngle, VerticalAngle, 0.0f);
}

void FCamera::ProcessRotation(float Yaw, float Pitch, float Roll)
{
    glm::quat QuatYaw   = glm::angleAxis(glm::radians(Yaw),   glm::vec3(0.0f, 1.0f, 0.0f));
    glm::quat QuatPitch = glm::angleAxis(glm::radians(Pitch), glm::vec3(1.0f, 0.0f, 0.0f));
    glm::quat QuatRoll  = glm::angleAxis(glm::radians(Roll),  glm::vec3(0.0f, 0.0f, 1.0f));

    _Orientation = glm::normalize(QuatYaw * QuatPitch * QuatRoll * _Orientation);

    UpdateVectors();
}

void FCamera::UpdateVectors()
{
    _Orientation = glm::normalize(_Orientation);
    glm::quat ConjugateOrient = glm::conjugate(_Orientation);

    _Front = glm::normalize(ConjugateOrient * glm::vec3(0.0f, 0.0f, -1.0f));
    _Right = glm::normalize(ConjugateOrient * glm::vec3(1.0f, 0.0f,  0.0f));
    _Up    = glm::normalize(ConjugateOrient * glm::vec3(0.0f, 1.0f,  0.0f));
}

_SPATIAL_END
_SYSTEM_END
_NPGS_END
