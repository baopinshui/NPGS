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
    _RotationSmoothCoefficient(30.0f),
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
    _Phi(45.0f),
    _DistanceToOrbitalCenter(1.0f),
    _ObjectivetTargetDistanceToOrbitalCenter(0.0001f),
    _TimeSinceModeChange(10.0),
    _bIsOrbiting(true),
    _ObjectiveSwayYaw(0.0f),
    _ObjectiveSwayPitch(0.0f),
    _CurrentSwayYaw(0.0f),
    _CurrentSwayPitch(0.0f)
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
    case EMovement::kForward: // W键
        _InputTranslationVector += _Front;
        _InputOrbitAxis.y -= 1.0f; // 向上绕转 (纬度)
        break;
    case EMovement::kBack:    // S键
        _InputTranslationVector -= _Front;
        _InputOrbitAxis.y += 1.0f; // 向下绕转 (纬度)
        break;
    case EMovement::kLeft:    // A键
        _InputTranslationVector -= _Right;
        _InputOrbitAxis.x -= 1.0f; // 向左绕转 (经度)
        break;
    case EMovement::kRight:   // D键
        _InputTranslationVector += _Right;
        _InputOrbitAxis.x += 1.0f; // 向右绕转 (经度)
        break;
    case EMovement::kUp:
        _InputTranslationVector += _Up;
        break;
    case EMovement::kDown:
        _InputTranslationVector -= _Up;
        break;
    case EMovement::kRollLeft:
        _InputRollValue -= 1.0f;
        break;
    case EMovement::kRollRight:
        _InputRollValue += 1.0f;
        break;
    }
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
    if (!_bIsOrbiting) return;

    _AxisDir = glm::normalize(_AxisDir);
    _Theta += static_cast<float>(OffsetX);
    if (_Theta >= 360.0f) { _Theta -= 360.0f; }
    else if (_Theta < 0.0f) { _Theta += 360.0f; }

    _Phi += static_cast<float>(OffsetY);
    if (_Phi > 180.0f) { _Phi = 180.f; }
    else if (_Phi < 0.0f) { _Phi = 0.0f; }

    glm::quat ThetaRotate = glm::angleAxis(glm::radians(_Theta), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::quat PhiRotate = glm::angleAxis(glm::radians(_Phi), glm::vec3(1.0f, 0.0f, 0.0f));
    glm::quat ToAxisRotate = CalculateToAxisRotate();
    // 1. 获取基础 Camera-To-World 四元数
    glm::quat Cam2WorldBase = ToAxisRotate * (ThetaRotate * (PhiRotate * glm::angleAxis(glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f))));
    // 2. [终极魔法：绝对零滚转的无奇点摆头 - 欧拉角版本]
    // 偏航角绕局部 Y 轴旋转，俯仰角绕局部 X 轴旋转
    // 顺序为 Yaw * Pitch，确保在看左右的同时上下看不会发生奇怪的扭曲
    glm::quat SwayYawQuat = glm::angleAxis(glm::radians(_CurrentSwayYaw), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::quat SwayPitchQuat = glm::angleAxis(glm::radians(_CurrentSwayPitch), glm::vec3(1.0f, 0.0f, 0.0f));
    glm::quat SwayQuat = SwayYawQuat * SwayPitchQuat;
    // 3. 将摆头应用在基础姿态的局部空间 (右乘 Camera-To-World)
    glm::quat Cam2WorldFinal = Cam2WorldBase * SwayQuat;
    // 4. 更新 _Orientation (View 旋转是 Camera-To-World 的共轭)
    _Orientation = glm::conjugate(Cam2WorldFinal);
    UpdateVectors();
    // 5. 相机在世界中的位置严格由 Base 状态决定，确保相机一直在球面轨道上
    glm::vec3 BaseFront = glm::normalize(Cam2WorldBase * glm::vec3(0.0f, 0.0f, -1.0f));
    SetCameraVector(FCamera::EVectorType::kPosition, _OrbitalCenter - _DistanceToOrbitalCenter * BaseFront);
}
void FCamera::ProcessTimeEvolution(double DeltaTime)
{
    // ================= 1. 新增：处理键盘轨道输入 =================
    if (_bIsOrbiting)
    {
        // 设定键盘控制的恒定绕转速度（例如 90度/秒，可根据需求提取到 .h 中作为成员变量）
        constexpr double kOrbitKeyboardSpeed = 90.0;

        // S = V * T (计算本帧因按键产生的理想角位移)
        double keyDeltaX = _InputOrbitAxis.x * kOrbitKeyboardSpeed * DeltaTime;
        double keyDeltaY = _InputOrbitAxis.y * kOrbitKeyboardSpeed * DeltaTime;

        if (_Sensitivity > 1e-6) // 防止除零危险
        {
            // 【核心魔法】：将键盘的位移逆向转换为鼠标像素单位，直接注入待处理池
            // 这样键盘输入就能完美蹭到鼠标的平滑阻尼算法，产生平滑启停效果
            _ObjectivetOffsetX += (keyDeltaX / _Sensitivity);
            _ObjectivetOffsetY += (keyDeltaY / _Sensitivity);
        }
    }
    // ==============================================================

    // ================= 2. 计算平滑消耗（上次修复的核心） =================
    double smoothFactor = 1.0f - exp(-_RotationSmoothCoefficient * static_cast<double>(DeltaTime));

    double ConsumeX = _ObjectivetOffsetX * smoothFactor;
    double ConsumeY = _ObjectivetOffsetY * smoothFactor;

    _ObjectivetOffsetX -= ConsumeX;
    _ObjectivetOffsetY -= ConsumeY;

    _CurrentSwayYaw += (_ObjectiveSwayYaw - _CurrentSwayYaw) * static_cast<float>(smoothFactor);
    _CurrentSwayPitch += (_ObjectiveSwayPitch - _CurrentSwayPitch) * static_cast<float>(smoothFactor);
    // ==============================================================
    if (!_bIsOrbiting)
    {
        // 非环绕模式下，复位摆头角度
        _ObjectiveSwayYaw = 0.0f;
        _ObjectiveSwayPitch = 0.0f;
        _CurrentSwayYaw = 0.0f;
        _CurrentSwayPitch = 0.0f;
        if (glm::length(_InputTranslationVector) > 0.001f)
        {
            glm::vec3 NormalizedDirection = glm::normalize(_InputTranslationVector);
            float Velocity = static_cast<float>(_Speed * DeltaTime);
            _Position += NormalizedDirection * Velocity;
        }

        float RollVelocity = 300.0f * 0.25f * static_cast<float>(DeltaTime);
        float HorizontalAngle = static_cast<float>(_Sensitivity * -ConsumeX);
        float VerticalAngle = static_cast<float>(_Sensitivity * -ConsumeY);

        ProcessRotation(HorizontalAngle, VerticalAngle, _InputRollValue * RollVelocity);
    }
    else
    {
        _DistanceToOrbitalCenter += (_ObjectivetTargetDistanceToOrbitalCenter - _DistanceToOrbitalCenter) * std::min(1.0, _OrbitDistanceRotationSmoothCoefficient * DeltaTime);
        if (glm::length(_ObjectivetAxisDir - _AxisDir) > 0 && abs(asin(glm::length((_ObjectivetAxisDir - _AxisDir)) / 2.0f)) > 1e-10)
        {
            _AxisDir = glm::normalize(_AxisDir + (std::min(1.0f, _OrbitAxisChangeSmoothCoefficient * static_cast<float>(DeltaTime)) * 2.0f * asin(glm::length((_ObjectivetAxisDir - _AxisDir)) / 2.0f) * glm::normalize(glm::cross(glm::cross(_AxisDir, (_ObjectivetAxisDir - _AxisDir)), _AxisDir))));
        }
        _OrbitalCenter += (_ObjectivetOrbitalCenter - _OrbitalCenter) * std::min(1.0f, _OrbitCenterChangeSmoothCoefficient * static_cast<float>(DeltaTime));

        ProcessOrbital(_Sensitivity * ConsumeX, _Sensitivity * ConsumeY);
    }

    _InputTranslationVector = glm::vec3(0.0f);
    _InputRollValue = 0.0f;
    _InputOrbitAxis = glm::vec2(0.0f); 

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
            _AxisDir = glm::normalize(_Up - _Front);
            glm::quat ToAxisRotate = glm::conjugate(CalculateToAxisRotate());
                glm::mat3 R = glm::mat3_cast(ToAxisRotate * glm::conjugate(_Orientation) *glm::angleAxis(glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)));
                _Theta = glm::degrees(std::atan2(-R[0][2], R[0][0]));
                _Phi = glm::degrees(std::atan2(-R[2][1], R[1][1]));

                
                ProcessOrbital(0.0, 0.0);
        }
    }
}
void FCamera::ProcessSwayMovement(double OffsetX, double OffsetY)
{
    if (_bIsOrbiting)
    {
        // 直接累加到目标偏航角和俯仰角上 (带负号以匹配原有的直觉)
        _ObjectiveSwayYaw -= static_cast<float>(OffsetX * _Sensitivity);
        _ObjectiveSwayPitch -= static_cast<float>(OffsetY * _Sensitivity);
        // [极其重要] 限制俯仰角，防止“看破自己的肚子”或者导致翻转 (死锁)
        // 限制在 -89.0 到 89.0 度之间
        _ObjectiveSwayPitch = std::clamp(_ObjectiveSwayPitch, -89.0f, 89.0f);
    }
}
void FCamera::ResetSway()
{
    // 复位时，只需将目标角度归零
    _ObjectiveSwayYaw = 0.0f;
    _ObjectiveSwayPitch = 0.0f;
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
