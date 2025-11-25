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
    _RotationSmoothCoefficient(100.0f),
    _OrbitDistanceRotationSmoothCoefficient(9.6),
    _OrbitAxisChangeSmoothCoefficient(3.0),
    _OrbitCenterChangeSmoothCoefficient(3.0),
    _Speed(Speed),
    _Zoom(Zoom),
    _OffsetX(0.0f),
    _OffsetY(0.0f),
    _ObjectivetOffsetX(0.0f),
    _ObjectivetOffsetY(0.0f),
    _AxisDir(0.0f, 1.0f, 0.0f),
    _ObjectivetAxisDir(0.0f, 1.0f, 0.0f),
    _OrbitalCenter(0.0f, 0.0f, 0.0f),
    _ObjectivetOrbitalCenter(0.0f, 0.0f, 0.0f),
    _Theta(0.0f),
    _Phi(135.0f),
    _DistanceToOrbitalCenter(1.0f),
    _ObjectivetTargetDistanceToOrbitalCenter(0.0001f),
    _TimeSinceModeChange(10.0),
    _bIsOrbiting(true)
{
    SetTargetOrbitAxis(glm::vec3(0., 1., 0.));
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
void FCamera::ProcessKeyboard(EMovement Direction)
{
    switch (Direction)
    {
    case EMovement::kForward:
        _InputTranslationVector += _Front;
        break;
    case EMovement::kBack:
        _InputTranslationVector -= _Front;
        break;
    case EMovement::kLeft:
        _InputTranslationVector -= _Right;
        break;
    case EMovement::kRight:
        _InputTranslationVector += _Right;
        break;
    case EMovement::kUp:
        _InputTranslationVector += _Up;
        break;
    case EMovement::kDown:
        _InputTranslationVector -= _Up;
        break;
    case EMovement::kRollLeft:
        _InputRollValue -= 1.0f; // 向左滚
        break;
    case EMovement::kRollRight:
        _InputRollValue += 1.0f; // 向右滚
        break;
    }
    // 注意：这里不再调用 UpdateVectors，因为位置还没变
}

void FCamera::ProcessMouseMovement(double OffsetX, double OffsetY)
{
    _ObjectivetOffsetX   +=OffsetX;
    _ObjectivetOffsetY   +=OffsetY;
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
    _Theta += static_cast<float>( OffsetX);
    if (_Theta >= 360.0f) 
    {
        _Theta -= 360.0f; 
    }
    else if (_Theta < 0.0f)
    {
        _Theta += 360.0f; 
    }
    _Phi += static_cast<float>(OffsetY);
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
    glm::quat ThetaRotate = glm::angleAxis(glm::radians(_Theta), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::quat PhiRotate   = glm::angleAxis(glm::radians(_Phi), glm::vec3(1.0f, 0.0f, 0.0f));
    glm::quat ToAxisRotate   = CalculateToAxisRotate();
    glm::quat ToCenterOrient = ToAxisRotate * (ThetaRotate * (PhiRotate * glm::angleAxis(glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f)))) ;
    _Orientation = glm::conjugate(ToCenterOrient);
    UpdateVectors();
    SetCameraVector(FCamera::EVectorType::kPosition, _OrbitalCenter - _DistanceToOrbitalCenter * _Front);
    glm::quat sway = glm::angleAxis(glm::radians(-0.0f), glm::vec3(_Up));
    _Orientation = glm::conjugate(sway*ToCenterOrient);
    UpdateVectors();
}
void FCamera::ProcessTimeEvolution(double DeltaTime)
{
    double smoothFactor = 1.0f - exp(-_RotationSmoothCoefficient * static_cast<double>(DeltaTime));

    _OffsetX += (_ObjectivetOffsetX - _OffsetX) * smoothFactor;
    _OffsetY += (_ObjectivetOffsetY - _OffsetY) * smoothFactor;


    _ObjectivetOffsetX = 0.0f;
    _ObjectivetOffsetY = 0.0f;
    if (!_bIsOrbiting)
    {
        if (glm::length(_InputTranslationVector) > 0.001f)
        {
            glm::vec3 NormalizedDirection = glm::normalize(_InputTranslationVector);

            // S = V * t (位移 = 速度 * 时间)
            float Velocity = static_cast<float>(_Speed * DeltaTime);
            _Position += NormalizedDirection * Velocity;
        }

            // 300.0f * 0.25f 是你原来的旋转速度系数
        float RollVelocity = 300.0f * 0.25f * static_cast<float>(DeltaTime);
        
        float HorizontalAngle = static_cast<float>(_Sensitivity * -_OffsetX);
        float VerticalAngle   = static_cast<float>(_Sensitivity * -_OffsetY);

        ProcessRotation(HorizontalAngle, VerticalAngle, _InputRollValue * RollVelocity);
    }
    else
    {

        _DistanceToOrbitalCenter += (_ObjectivetTargetDistanceToOrbitalCenter - _DistanceToOrbitalCenter) * std::min(1.0, _OrbitDistanceRotationSmoothCoefficient * DeltaTime);
        if (glm::length(_ObjectivetAxisDir - _AxisDir) > 0 && abs(asin(glm::length((_ObjectivetAxisDir - _AxisDir)) / 2.0f)) > 1e-10)
        {
            _AxisDir = glm::normalize(_AxisDir + (std::min(1.0f, _OrbitAxisChangeSmoothCoefficient * static_cast<float>(DeltaTime)) * 2.0f * asin(glm::length((_ObjectivetAxisDir - _AxisDir)) / 2.0f) * glm::normalize(glm::cross(glm::cross(_AxisDir, (_ObjectivetAxisDir - _AxisDir)), _AxisDir))));
        }//绕转轴更新平滑
        _OrbitalCenter += (_ObjectivetOrbitalCenter - _OrbitalCenter) * std::min(1.0f, _OrbitCenterChangeSmoothCoefficient * static_cast<float>(DeltaTime));
        ProcessOrbital(_Sensitivity * _OffsetX, _Sensitivity * _OffsetY);
    }//鼠标拖动平滑更新

    _InputTranslationVector = glm::vec3(0.0f);
    _InputRollValue = 0.0f;
    if (_TimeSinceModeChange < 10.0)
    {
        _TimeSinceModeChange += DeltaTime;
    }
}
void FCamera::ProcessModeChange()
{
    if (_TimeSinceModeChange > 0.5)
    {
        _TimeSinceModeChange = 0.0;
        if (_bIsOrbiting)
        {
            _bIsOrbiting = false;
        }
        else
        {
            _bIsOrbiting = true;
            _OrbitalCenter = _Position + _DistanceToOrbitalCenter * _Front;
            _AxisDir = glm::normalize(_Up + _Front);
            glm::quat ToAxisRotate = glm::conjugate(CalculateToAxisRotate());
                glm::mat3 R = glm::mat3_cast(ToAxisRotate * glm::conjugate(_Orientation) *glm::angleAxis(glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)));
                _Theta = glm::degrees(std::atan2(-R[0][2], R[0][0]));
                _Phi = glm::degrees(std::atan2(-R[2][1], R[1][1]));

                
                ProcessOrbital(0.0, 0.0);
        }
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

glm::quat FCamera::CalculateToAxisRotate()
{
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
        ToAxisNormal = (glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), _AxisDir));
        if (_AxisDir.y >= 0.0f)
        {
            ToAxisTheta = glm::degrees(glm::asin(glm::length(ToAxisNormal)));
        }
        else
        {
            ToAxisTheta = 180.0f - glm::degrees(glm::asin(glm::length(ToAxisNormal)));
        }
    }
    glm::quat ToAxisRotate = glm::angleAxis(glm::radians(ToAxisTheta), glm::normalize(ToAxisNormal));
    return ToAxisRotate;
}

_SPATIAL_END
_SYSTEM_END
_NPGS_END
