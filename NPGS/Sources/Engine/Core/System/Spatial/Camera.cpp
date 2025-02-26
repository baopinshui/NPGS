#include "Camera.h"

#include "Engine/Core/Base/Assert.h"

_NPGS_BEGIN
_SYSTEM_BEGIN
_SPATIAL_BEGIN

FCamera::FCamera(const glm::vec3& Position, float Sensitivity, float Speed, float Zoom)
    :
    _Orientation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f)),
    _Position(Position),
    _Sensitivity(Sensitivity),
    _RotationSmoothCoefficient(9.6f),
    _OrbitDistanceRotationSmoothCoefficient(9.6),
    _Speed(Speed),
    _Zoom(Zoom),
    _OffsetX(0.0f),
    _OffsetY(0.0f),
    _TargetOffsetX(0.0f),
    _TargetOffsetY(0.0f),
    _AxisDir(0.0f, 1.0f, 0.2f),
    _OrbitalCenter(0.0f, 0.0f, 0.0f),
    _Theta(0.0f),
    _Phi(45.0f),
    _DistanceToOrbitalCenter(1.0f),
    _TargetDistanceToOrbitalCenter(0.0001f),
    _bIsOrbiting(false),
    _bAllowCrossZenith(false)
{
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
    _TargetOffsetX   =OffsetX;
    _TargetOffsetY   =OffsetY;
}

void FCamera::ProcessRotation(float Yaw, float Pitch, float Roll)
{
    glm::quat QuatYaw   = glm::angleAxis(glm::radians(Yaw),   glm::vec3(0.0f, 1.0f, 0.0f));
    glm::quat QuatPitch = glm::angleAxis(glm::radians(Pitch), glm::vec3(1.0f, 0.0f, 0.0f));
    glm::quat QuatRoll  = glm::angleAxis(glm::radians(Roll),  glm::vec3(0.0f, 0.0f, 1.0f));

    _Orientation = glm::normalize(QuatYaw * QuatPitch * QuatRoll * _Orientation);

    UpdateVectors();
}
void FCamera::ProcessOrbital(double OffsetX, double OffsetY)
{
    if(!_bIsOrbiting)
    {
        return;
    }
    _AxisDir = glm::normalize(_AxisDir);
    _Theta += static_cast<float>(_Sensitivity * OffsetX);
    if (_Theta >= 360.0f) 
    {
        _Theta -= 360.0f; 
    }
    else if (_Theta < 0.0f)
    {
        _Theta += 360.0f; 
    }
    _Phi += static_cast<float>(_Sensitivity * OffsetY);
    if (_bAllowCrossZenith)
    {
        if (_Phi >= 360.0f) 
        { 
            _Phi -= 360.0f; 
        }
        else if (_Phi < 0.0f) 
        {
            _Phi += 360.0f; 
        }
    }
    else
    {
        if (_Phi > 180.0f) 
        {
            _Phi = 180.f; 
        }
        else if (_Phi < 0.0f) 
        {
            _Phi = 0.0f; 
        }
    }
    glm::vec3 Up = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 Back    = glm::vec3(0.0f, 1.0f, 0.0f);

    glm::quat ThetaRotate = glm::angleAxis(glm::radians(_Theta), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::quat PhiRotate   = glm::angleAxis(glm::radians(_Phi), glm::vec3(1.0f, 0.0f, 0.0f));

    glm::vec3 ToAxisNormal;
    float ToAxisTheta;
    if (_AxisDir.x == 0.0f && _AxisDir.z == 0.0f)
    {
        if (_AxisDir.y > 0.0f) 
        { 
            ToAxisTheta = 0.0f;
            ToAxisNormal = glm::vec3(1.0f, 0.0f, 0.0f);
        } 
        else
        {
            ToAxisTheta = 180.0f;
            ToAxisNormal = glm::vec3(1.0f, 0.0f, 0.0f);
        }
    }
    else
    {
        ToAxisNormal = glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), _AxisDir);
        if (_AxisDir.y >= 0.0f)
        {
            ToAxisTheta = glm::degrees(glm::asin(glm::length(ToAxisNormal)));
        }
        else
        {
            ToAxisTheta = 180.0f-glm::degrees(glm::asin(glm::length(ToAxisNormal)));
        }
    }
    glm::quat ToAxisRotate   = glm::angleAxis(glm::radians(ToAxisTheta), ToAxisNormal);

    Up   = ToAxisRotate *( ThetaRotate *(PhiRotate * Up));
    Back    = ToAxisRotate *( ThetaRotate *(PhiRotate * Back ));
    glm::vec3 Right = glm::cross(Up,Back);
    _Orientation = glm::conjugate(glm::quat_cast(glm::mat3x3(Right, Up, Back)));
    SetCameraVector(FCamera::EVectorType::kPosition, _OrbitalCenter + _DistanceToOrbitalCenter * Back);
    UpdateVectors();
}
void FCamera::ProcessTimeEvolution(double DeltaTime)
{
    _DistanceToOrbitalCenter += (_TargetDistanceToOrbitalCenter - _DistanceToOrbitalCenter) * std::min(1.0, _OrbitDistanceRotationSmoothCoefficient*DeltaTime);
    SetCameraVector(FCamera::EVectorType::kPosition, _OrbitalCenter - _DistanceToOrbitalCenter * _Front);
    _OffsetX += (_TargetOffsetX - _OffsetX) * std::min(1.0, _RotationSmoothCoefficient * DeltaTime);
    _OffsetY += (_TargetOffsetY - _OffsetY) * std::min(1.0, _RotationSmoothCoefficient * DeltaTime);
    _TargetOffsetX = 0.0f;
    _TargetOffsetY = 0.0f;
    if (!_bIsOrbiting)
    {
        float HorizontalAngle = static_cast<float>(_Sensitivity * -_OffsetX);
        float VerticalAngle   = static_cast<float>(_Sensitivity * -_OffsetY);

        ProcessRotation(HorizontalAngle, VerticalAngle, 0.0f);
    }
    else
    {
        ProcessOrbital(_OffsetX, _OffsetY);
    }
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
