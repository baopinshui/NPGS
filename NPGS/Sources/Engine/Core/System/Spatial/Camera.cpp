#include "Camera.h"

#include "Engine/Core/Base/Assert.h"

_NPGS_BEGIN
_SYSTEM_BEGIN
_SPATIAL_BEGIN

FCamera::FCamera(const glm::vec3& Position, float Sensitivity, float Speed, float Zoom)
    :
    _Orientation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f)),
    _Position(Position),
    _OrbitRadius(0.0f),
    _Sensitivity(Sensitivity),
    _Speed(Speed),
    _Zoom(Zoom),
    _PrevOffsetX(0.0f),
    _PrevOffsetY(0.0f),
    _bIsOrbiting(false)
{
    UpdateVectors();
}

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
    if (_bIsOrbiting)
    {
        ProcessOrbital(OffsetX, OffsetY);
    }
    else
    {
        static float SmoothCoefficient = 1.0f;
        float SmoothedX = SmoothCoefficient * static_cast<float>(OffsetX) + (1.0f - SmoothCoefficient) * _PrevOffsetX;
        float SmoothedY = SmoothCoefficient * static_cast<float>(OffsetY) + (1.0f - SmoothCoefficient) * _PrevOffsetY;
        _PrevOffsetX = SmoothedX;
        _PrevOffsetY = SmoothedY;

        float HorizontalAngle = static_cast<float>(_Sensitivity *  SmoothedX);
        float VerticalAngle   = static_cast<float>(_Sensitivity * -SmoothedY);

        ProcessRotation(HorizontalAngle, VerticalAngle, 0.0f);
    }
}

void FCamera::ProcessOrbital(double OffsetX, double OffsetY)
{
    if (!_bIsOrbiting)
    {
        return;
    }

    static float SmoothCoefficient = 1.0f;
    float SmoothedX = SmoothCoefficient * static_cast<float>(OffsetX) + (1.0f - SmoothCoefficient) * _PrevOffsetX;
    float SmoothedY = SmoothCoefficient * static_cast<float>(OffsetY) + (1.0f - SmoothCoefficient) * _PrevOffsetY;
    _PrevOffsetX = SmoothedX;
    _PrevOffsetY = SmoothedY;

    glm::vec3 OrbitAxis         = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 PrevRight         = _Right;
    glm::vec3 DirectionToCamera = _Position - _OrbitTarget;

    float HorizontalAngle = static_cast<float>(_Sensitivity * -SmoothedX);
    float VerticalAngle   = static_cast<float>(_Sensitivity *  SmoothedY);

    glm::quat HorizontalRotation = glm::angleAxis(glm::radians(HorizontalAngle), OrbitAxis);
    glm::quat VerticalRotation   = glm::angleAxis(glm::radians(VerticalAngle), _Right);

    DirectionToCamera = HorizontalRotation * VerticalRotation * DirectionToCamera;
    _Position         = _OrbitTarget + DirectionToCamera;

    glm::vec3 Direction = glm::normalize(_OrbitTarget - _Position);
    glm::vec3 Right     = glm::normalize(glm::cross(Direction, OrbitAxis));

    Right = glm::dot(Right, PrevRight) < 0.0f ? -Right : Right;

    glm::vec3 Up = glm::normalize(glm::cross(Right, Direction));
    glm::mat3 RotationMatrix(Right, Up, -Direction);
    _Orientation = glm::conjugate(glm::quat_cast(RotationMatrix));

    UpdateVectors();
}

void FCamera::SetCameraVector(EVectorType Type, const glm::vec3& NewVector)
{
    switch (Type)
    {
    case EVectorType::kPosition:
        _Position = NewVector;
        break;
    case EVectorType::kFront:
        _Front = NewVector;
        break;
    case EVectorType::kUp:
        _Up = NewVector;
        break;
    case EVectorType::kRight:
        _Right = NewVector;
        break;
    default:
        NpgsAssert(false, "Invalid vector type");
    }
}

void FCamera::SetOrbitTarget(const glm::vec3& Target, float Radius)
{
    _OrbitTarget = Target;
    _OrbitRadius = Radius;
    _bIsOrbiting = true;

    _Position = _OrbitTarget - glm::vec3(0.0f, 0.0f, _OrbitRadius);

    glm::vec3 Direction = glm::normalize(_OrbitTarget - _Position);
    glm::vec3 Right     = glm::normalize(glm::cross(Direction, glm::vec3(0.0f, 1.0f, 0.0f)));
    glm::vec3 Up        = glm::normalize(glm::cross(Right, Direction));
    
    glm::mat3x3 RotationMatrix(Right, Up, -Direction);
    _Orientation = glm::conjugate(glm::quat_cast(RotationMatrix));

    UpdateVectors();
}

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
        return _Position;
    }
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
