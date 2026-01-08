#version 450
#pragma shader_stage(fragment)
#extension GL_EXT_samplerless_texture_functions : enable
#include "Common/CoordConverter.glsl"
#include "Common/NumericConstants.glsl"
// =============================================================================
// 输入与输出定义
// =============================================================================
layout(location = 0) out vec4 FragColor;
layout(location = 0) in  vec2 TexCoordFromVert;
layout(set = 0, binding = 0) uniform GameArgs
{
    vec2  iResolution;
    float iFovRadians;
    float iTime;
    float iGameTime;
    float iTimeDelta;
    float iTimeRate;
};
layout(set = 0, binding = 1) uniform BlackHoleArgs
{
    mat4x4 iInverseCamRot;
    vec4  iBlackHoleRelativePosRs;
    vec4  iBlackHoleRelativeDiskNormal; 
    vec4  iBlackHoleRelativeDiskTangen; 
    float iUniverseSign;
    float iBlackHoleTime;
    float iBlackHoleMassSol;
    float iSpin;
    float iMu;
    float iAccretionRate;
    float iInterRadiusRs;
    float iOuterRadiusRs;
    float iThinRs;
    float iHopper;
    float iBrightmut;
    float iDarkmut;
    float iReddening;
    float iSaturation;
    float iBlackbodyIntensityExponent;
    float iRedShiftColorExponent;
    float iRedShiftIntensityExponent;
    float iJetRedShiftIntensityExponent;
    float iJetBrightmut;
    float iJetSaturation;
    float iJetShiftMax;
    
    float iBlendWeight;
};
layout(set = 1, binding = 0) uniform texture2D iHistoryTex;
layout(set = 1, binding = 1) uniform samplerCube iBackground;
layout(set = 1, binding = 2) uniform samplerCube iAntiground;
// =============================================================================
// 工具函数 (噪声、插值、随机) - 保持不变
// =============================================================================
float RandomStep(vec2 Input, float Seed)
{
    return fract(sin(dot(Input + fract(11.4514 * sin(Seed)), vec2(12.9898, 78.233))) * 43758.5453);
}
float CubicInterpolate(float x)
{
    return 3.0 * pow(x, 2.0) - 2.0 * pow(x, 3.0);
}
float PerlinNoise(vec3 Position)
{
    vec3 PosInt   = floor(Position);
    vec3 PosFloat = fract(Position);
    
    float Sx = CubicInterpolate(PosFloat.x);
    float Sy = CubicInterpolate(PosFloat.y);
    float Sz = CubicInterpolate(PosFloat.z);
    float v000 = 2.0 * fract(sin(dot(vec3(PosInt.x,       PosInt.y,       PosInt.z),       vec3(12.9898, 78.233, 213.765))) * 43758.5453) - 1.0;
    float v100 = 2.0 * fract(sin(dot(vec3(PosInt.x + 1.0, PosInt.y,       PosInt.z),       vec3(12.9898, 78.233, 213.765))) * 43758.5453) - 1.0;
    float v010 = 2.0 * fract(sin(dot(vec3(PosInt.x,       PosInt.y + 1.0, PosInt.z),       vec3(12.9898, 78.233, 213.765))) * 43758.5453) - 1.0;
    float v110 = 2.0 * fract(sin(dot(vec3(PosInt.x + 1.0, PosInt.y + 1.0, PosInt.z),       vec3(12.9898, 78.233, 213.765))) * 43758.5453) - 1.0;
    float v001 = 2.0 * fract(sin(dot(vec3(PosInt.x,       PosInt.y,       PosInt.z + 1.0), vec3(12.9898, 78.233, 213.765))) * 43758.5453) - 1.0;
    float v101 = 2.0 * fract(sin(dot(vec3(PosInt.x + 1.0, PosInt.y,       PosInt.z + 1.0), vec3(12.9898, 78.233, 213.765))) * 43758.5453) - 1.0;
    float v011 = 2.0 * fract(sin(dot(vec3(PosInt.x,       PosInt.y + 1.0, PosInt.z + 1.0), vec3(12.9898, 78.233, 213.765))) * 43758.5453) - 1.0;
    float v111 = 2.0 * fract(sin(dot(vec3(PosInt.x + 1.0, PosInt.y + 1.0, PosInt.z + 1.0), vec3(12.9898, 78.233, 213.765))) * 43758.5453) - 1.0;
    return mix(mix(mix(v000, v100, Sx), mix(v010, v110, Sx), Sy),
               mix(mix(v001, v101, Sx), mix(v011, v111, Sx), Sy), Sz);
}
float SoftSaturate(float x)
{
    return 1.0 - 1.0 / (max(x, 0.0) + 1.0);
}
float PerlinNoise1D(float Position)
{
    float PosInt   = floor(Position);
    float PosFloat = fract(Position);
    float v0 = 2.0 * fract(sin(PosInt * 12.9898) * 43758.5453) - 1.0;
    float v1 = 2.0 * fract(sin((PosInt + 1.0) * 12.9898) * 43758.5453) - 1.0;
    return v1 * CubicInterpolate(PosFloat) + v0 * CubicInterpolate(1.0 - PosFloat);
}
float GenerateAccretionDiskNoise(vec3 Position, float NoiseStartLevel, float NoiseEndLevel, float ContrastLevel)
{
    float NoiseAccumulator = 10.0;
    float start = NoiseStartLevel;
    float end = NoiseEndLevel;
    int iStart = int(floor(start));
    int iEnd = int(ceil(end));
    
    int maxIterations = iEnd - iStart;
    for (int delta = 0; delta < maxIterations; delta++)
    {
        int i = iStart + delta;
        float iFloat = float(i);
        float w = max(0.0, min(end, iFloat + 1.0) - max(start, iFloat));
        if (w <= 0.0) continue;
        
        float NoiseFrequency = pow(3.0, iFloat);
        vec3 ScaledPosition = NoiseFrequency * Position;
        float noise = PerlinNoise(ScaledPosition);
        NoiseAccumulator *= (1.0 + 0.1 * noise * w);
    }
    return log(1.0 + pow(0.1 * NoiseAccumulator, ContrastLevel));
}
// =============================================================================
// 数学与颜色转换函数 - 保持不变
// =============================================================================
float Vec2ToTheta(vec2 v1, vec2 v2)
{
    float VecDot   = dot(v1, v2);
    float VecCross = v1.x * v2.y - v1.y * v2.x;
    float Angle    = asin(0.999999 * VecCross / (length(v1) * length(v2)));
    float Dx = step(0.0, VecDot);
    float Cx = step(0.0, VecCross);
    return mix(mix(-kPi - Angle, kPi - Angle, Cx), Angle, Dx);
}
vec3 KelvinToRgb(float Kelvin)
{
    if (Kelvin < 400.01) return vec3(0.0);
    float Teff     = (Kelvin - 6500.0) / (6500.0 * Kelvin * 2.2);
    vec3  RgbColor = vec3(0.0);
    RgbColor.r = exp(2.05539304e4 * Teff);
    RgbColor.g = exp(2.63463675e4 * Teff);
    RgbColor.b = exp(3.30145739e4 * Teff);
    float BrightnessScale = 1.0 / max(max(1.5 * RgbColor.r, RgbColor.g), RgbColor.b);
    if (Kelvin < 1000.0) BrightnessScale *= (Kelvin - 400.0) / 600.0;
    RgbColor *= BrightnessScale;
    return RgbColor;
}
vec3 WavelengthToRgb(float wavelength) {
    vec3 color = vec3(0.0);
    if (wavelength < 380.0 || wavelength > 750.0) return color; 
    if (wavelength >= 380.0 && wavelength < 440.0) {
        color.r = -(wavelength - 440.0) / (440.0 - 380.0); color.g = 0.0; color.b = 1.0;
    } else if (wavelength >= 440.0 && wavelength < 490.0) {
        color.r = 0.0; color.g = (wavelength - 440.0) / (490.0 - 440.0); color.b = 1.0;
    } else if (wavelength >= 490.0 && wavelength < 510.0) {
        color.r = 0.0; color.g = 1.0; color.b = -(wavelength - 510.0) / (510.0 - 490.0);
    } else if (wavelength >= 510.0 && wavelength < 580.0) {
        color.r = (wavelength - 510.0) / (580.0 - 510.0); color.g = 1.0; color.b = 0.0;
    } else if (wavelength >= 580.0 && wavelength < 645.0) {
        color.r = 1.0; color.g = -(wavelength - 645.0) / (645.0 - 580.0); color.b = 0.0;
    } else if (wavelength >= 645.0 && wavelength <= 750.0) {
        color.r = 1.0; color.g = 0.0; color.b = 0.0;
    }
    float factor = 0.0;
    if (wavelength >= 380.0 && wavelength < 420.0) factor = 0.3 + 0.7 * (wavelength - 380.0) / (420.0 - 380.0);
    else if (wavelength >= 420.0 && wavelength < 645.0) factor = 1.0;
    else if (wavelength >= 645.0 && wavelength <= 750.0) factor = 0.3 + 0.7 * (750.0 - wavelength) / (750.0 - 645.0);
    
    return color * factor / pow(color.r * color.r + 2.25 * color.g * color.g + 0.36 * color.b * color.b, 0.5) * (0.1 * (color.r + color.g + color.b) + 0.9);
}
float Shape(float x, float Alpha, float Beta)
{
    float k = pow(Alpha + Beta, Alpha + Beta) / (pow(Alpha, Alpha) * pow(Beta, Beta));
    return k * pow(x, Alpha) * pow(1.0 - x, Beta);
}
// =============================================================================
// 坐标转换与轨道计算
// =============================================================================
float GetKeplerianAngularVelocity(float Radius, float Rs, float Spin) 
{
    float M = 0.5 * Rs; 
    float SqrtM = sqrt(M);
    // Omega = sqrt(M) / (r^1.5 + a * sqrt(M))
    return SqrtM / (pow(Radius, 1.5) + Spin * SqrtM);
}
// =============================================================================
// Kerr-Schild (Spin Axis = Y)
// =============================================================================
const float CONST_M = 0.5; 
const mat4  MINKOWSKI_METRIC = mat4(
    1, 0, 0, 0,
    0, 1, 0, 0,
    0, 0, 1, 0,
    0, 0, 0, -1
);
float KerrSchildRadius(vec3 p, float a, float r_sign) {
    float a2 = a * a;
    float rho2 = p.x * p.x + p.z * p.z;
    float y2 = p.y * p.y;
    float b = rho2 + y2 - a2;
    float r2;
    if (b >= 0.0) {
        r2 = 0.5 * (b + sqrt(b * b + 4.0 * a2 * y2));
    } else {
        float det = sqrt(b * b + 4.0 * a2 * y2);
        r2 = (2.0 * a2 * y2) / max(1e-20, det - b);
    }
    return r_sign * sqrt(r2);
}
mat4 GetInverseKerrMetric(vec4 X, float a, float fade, float r_sign) {
    float r = KerrSchildRadius(X.xyz, a, r_sign); 
    float r2 = r * r;
    float a2 = a * a;
    float denom = 1.0 / (r2 + a2);
    
    float lx = (r * X.x + a * X.z) * denom;
    float ly = X.y / r;
    float lz = (r * X.z - a * X.x) * denom;
    
    vec4 l_up = vec4(lx, ly, lz, -1.0); 
    float f = (2.0 * CONST_M * r * r2) / (r2 * r2 + a2 * X.y * X.y) * fade;
    
    return MINKOWSKI_METRIC - f * outerProduct(l_up, l_up);
}
mat4 GetKerrMetric(vec4 X, float a, float fade, float r_sign) {
    float r = KerrSchildRadius(X.xyz, a, r_sign);
    float r2 = r * r;
    float a2 = a * a;
    float denom = 1.0 / (r2 + a2);
    
    float lx = (r * X.x + a * X.z) * denom;
    float ly = X.y / r;
    float lz = (r * X.z - a * X.x) * denom;
    
    vec4 l_down = vec4(lx, ly, lz, 1.0); 
    float f = (2.0 * CONST_M * r * r2) / (r2 * r2 + a2 * X.y * X.y) * fade;
    
    return MINKOWSKI_METRIC + f * outerProduct(l_down, l_down);
}
float Hamiltonian(vec4 X, vec4 P, float a, float fade, float r_sign) {
    mat4 g_inv = GetInverseKerrMetric(X, a, fade, r_sign);
    return 0.5 * dot(g_inv * P, P);
}
vec4 GradHamiltonian(vec4 X, vec4 P, float a, float fade, float current_r_sign) {
    float rho = length(X.xz); 
    // 稍微放宽梯度采样的eps，因为RK4需要更平滑的导数
    float distToRing = sqrt(X.y * X.y + pow(rho - abs(a), 2.0));
    float eps = max(1e-5, 0.001 * distToRing); 
    
    vec4 G;
    float invTwoEps = 0.5 / eps; 
    
    vec4 dx = vec4(eps, 0.0, 0.0, 0.0);
    vec4 dy = vec4(0.0, eps, 0.0, 0.0);
    vec4 dz = vec4(0.0, 0.0, eps, 0.0);
    
    G.x = (Hamiltonian(X + dx, P, a, fade, current_r_sign) - Hamiltonian(X - dx, P, a, fade, current_r_sign));
    G.z = (Hamiltonian(X + dz, P, a, fade, current_r_sign) - Hamiltonian(X - dz, P, a, fade, current_r_sign));
    
    // Y 方向处理奇环穿越
    vec4 X_plus = X + dy;
    float sign_plus = current_r_sign;
    if (length(X_plus.xz) < abs(a) && X.y * X_plus.y < 0.0) sign_plus = -current_r_sign;
    float H_plus = Hamiltonian(X_plus, P, a, fade, sign_plus);
    
    vec4 X_minus = X - dy;
    float sign_minus = current_r_sign;
    if (length(X_minus.xz) < abs(a) && X.y * X_minus.y < 0.0) sign_minus = -current_r_sign;
    float H_minus = Hamiltonian(X_minus, P, a, fade, sign_minus);
    
    G.y = H_plus - H_minus;
    G.w = 0.0;
    
    return G * invTwoEps;
}
// =============================================================================
// RK4 积分器
// =============================================================================
struct State {
    vec4 X;
    vec4 P;
};
// 获取相空间导数：dX/dlambda = dH/dP, dP/dlambda = -dH/dX
void ApplyHamiltonianCorrection(inout vec4 P, vec4 X, float a, float fade, float r_sign) {
    mat4 g_inv = GetInverseKerrMetric(X, a, fade, r_sign);
    float A = g_inv[3][3];
    float B = 2.0 * dot(vec3(g_inv[0][3], g_inv[1][3], g_inv[2][3]), P.xyz);
    float C = dot(P.xyz, mat3(g_inv) * P.xyz);
    float disc = B * B - 4.0 * A * C;
    if (disc >= 0.0) {
        float sqrtDisc = sqrt(disc);
        float pt1 = (-B - sqrtDisc) / (2.0 * A);
        float pt2 = (-B + sqrtDisc) / (2.0 * A);
        // 选择最接近当前值的一个，保持连续性
        P.w = (abs(pt1 - P.w) < abs(pt2 - P.w)) ? pt1 : pt2;
    }
}
State GetDerivatives(State S, float a, float fade, float r_sign) {
    State deriv;
    // dx/dlambda = g^uv * p_v
    deriv.X = GetInverseKerrMetric(S.X, a, fade, r_sign) * S.P;
    // dp/dlambda = -dH/dx
    deriv.P = -GradHamiltonian(S.X, S.P, a, fade, r_sign);
    return deriv;
}
// RK4 单步推进
void StepGeodesicRK4(inout vec4 X, inout vec4 P, float dt, float a, float fade, float r_sign) {
    State s0; s0.X = X; s0.P = P;
    
    State k1 = GetDerivatives(s0, a, fade, r_sign);
    
    State s1; 
    s1.X = s0.X + 0.5 * dt * k1.X; 
    s1.P = s0.P + 0.5 * dt * k1.P;
    State k2 = GetDerivatives(s1, a, fade, r_sign);
    
    State s2; 
    s2.X = s0.X + 0.5 * dt * k2.X; 
    s2.P = s0.P + 0.5 * dt * k2.P;
    State k3 = GetDerivatives(s2, a, fade, r_sign);
    
    State s3; 
    s3.X = s0.X + dt * k3.X; 
    s3.P = s0.P + dt * k3.P;
    State k4 = GetDerivatives(s3, a, fade, r_sign);
    
    X = s0.X + (dt / 6.0) * (k1.X + 2.0 * k2.X + 2.0 * k3.X + k4.X);
    P = s0.P + (dt / 6.0) * (k1.P + 2.0 * k2.P + 2.0 * k3.P + k4.P);
}
void StepGeodesicEuler(inout vec4 X, inout vec4 P, float dt, float a, float fade, float r_sign) {
    vec4 gradH = GradHamiltonian(X, P, a, fade, r_sign);
    
    // 更新动量
    P = P - dt * gradH;
    
    // 强制哈密顿约束 (修正 P_t)
    // 这一步对于防止在奇环附近发散至关重要
    ApplyHamiltonianCorrection(P, X, a, fade, r_sign);
    
    // 更新位置 (使用新动量)
    mat4 g_inv = GetInverseKerrMetric(X, a, fade, r_sign);
    X = X + dt * (g_inv * P);
     ApplyHamiltonianCorrection(P, X, a, fade, r_sign);
}
// =============================================================================
// 吸积盘与喷流
// =============================================================================


vec4 DiskColor(vec4 BaseColor, float StepLength, vec3 RayPos, vec3 LastRayPos,
               vec3 RayDir, vec3 LastRayDir,
               float InterRadius, float OuterRadius, float Thin, float Hopper, float Brightmut, float Darkmut, float Reddening, float Saturation, float DiskTemperatureArgument,
               float BlackbodyIntensityExponent, float RedShiftColorExponent, float RedShiftIntensityExponent,
               float PeakTemperature, float ShiftMax, float Spin)
{
    // [初始化累积变量]
    vec4 AccumulatedDiskColor = vec4(0.0);
    
    // [光线参数]
    // 在局部Kerr空间中，从 LastRayPos 到 RayPos
    vec3 StartPos = LastRayPos;
    vec3 DirVec   = RayDir; // 假设传入的 RayDir 已经归一化
    float TotalDist = StepLength;
    float TraveledDist = 0.0;
    
    // [包围盒快速剔除]
    // 如果这一整段光线都在吸积盘有效半径之外或高度过高，直接跳过
    // 稍微放宽边界以防插值误差
    float MaxR = max(KerrSchildRadius(StartPos, Spin, 1.0), KerrSchildRadius(RayPos, Spin, 1.0));
    float MinAbsY = min(abs(StartPos.y), abs(RayPos.y));
    float MaxThicknessBounds = Thin * 2.0 + max(0.0, (MaxR - 3.0) * Hopper) * 2.0; // 估算最大厚度
    
    if (MaxR < InterRadius * 0.9 || (MaxR > OuterRadius * 1.1 && TotalDist < 100.0) || MinAbsY > MaxThicknessBounds * 1.5) 
    {
        return BaseColor;
    }

    // [自适应子步进循环]
    // 限制最大循环次数防止死循环 (对于极近处的长步长)
    // 通常情况下，由于RK4步长受限，这里的循环不会很多
    int MaxSubSteps = 32; 
    
    for (int i = 0; i < MaxSubSteps; i++)
    {
        if (TraveledDist >= TotalDist) break;

        vec3 CurrentPos = StartPos + DirVec * TraveledDist;
        
        // 1. 计算特征尺度 (Feature Scale) 以决定子步长 dt
        //    复用原有的自适应逻辑，但用于控制采样密度
        float CurrentR = KerrSchildRadius(CurrentPos, Spin, 1.0);
        float rho = length(CurrentPos.xz);
        
        // 计算到“环”的特征距离，用于估算变化率
        float distToRing = sqrt(CurrentPos.y * CurrentPos.y + pow(rho - abs(Spin), 2.0));
        
        // 应用旧版本的特征尺度缩放公式
        float SmallStepBoundary = max(OuterRadius, 12.0); // 这里的基准尺度可以根据OuterRadius调整
        float t_val = clamp((distToRing - SmallStepBoundary) / SmallStepBoundary, 0.0, 1.0);
        float smoothT = t_val * t_val * t_val * (t_val * (t_val * 6.0 - 15.0) + 10.0);
        
        float FeatureScale = mix(min(1.0 + 0.25 * max(distToRing - 12.0, 0.0), distToRing), distToRing, smoothT);
        FeatureScale *= (0.15 + 0.25 * clamp(0.5 * (0.5 * distToRing / max(10.0, SmallStepBoundary) - 1.0), 0.0, 1.0));
        
        // 针对吸积盘内部的高频噪声，进一步缩放步长
        // 如果在吸积盘厚度范围内，强制更小的步长以捕捉噪声
        float CurrentThin = Thin + max(0.0, (rho - 3.0) * Hopper);
        if (abs(CurrentPos.y) < CurrentThin * 1.5) {
            FeatureScale *= 0.5; // 提高内部采样精度
            FeatureScale = max(FeatureScale, 0.05); // 硬下限，防止步长过小
        } else {
             // 在盘外，可以步进快一点，尽快穿过真空区
             FeatureScale = max(FeatureScale, 0.2);
        }

        // 2. 确定当前步长 dt
        float dt = min(FeatureScale, TotalDist - TraveledDist);
        
        // 3. 采样位置：使用中点法则 (Midpoint Rule) 提高积分精度
        //    采样点位于当前小段的中心
        vec3 SamplePos = CurrentPos + DirVec * dt * 0.5;
        
        // --- 核心物理计算 (针对 SamplePos) ---
        // 这部分逻辑直接复用原有的单点计算，但应用在 SamplePos 上
        
        float PosR = KerrSchildRadius(SamplePos, Spin, 1.0);
        float PosY = SamplePos.y;
        
        // [厚度抖动计算]
        float SampleThin = Thin + max(0.0, (length(SamplePos.xz) - 3.0) * Hopper);
        float NoiseLevel = max(0.0, 2.0 - 0.6 * SampleThin);

        if (abs(PosY) < SampleThin && PosR < OuterRadius && PosR > InterRadius)
        {
            // [计算几何因子]
            float x = (PosR - InterRadius) / (OuterRadius - InterRadius);
            float a_param = max(1.0, (OuterRadius - InterRadius) / 10.0);
            float EffectiveRadius = (-1.0 + sqrt(1.0 + 4.0 * a_param * a_param * x - 4.0 * x * a_param)) / (2.0 * a_param - 2.0);
            if(a_param == 1.0) EffectiveRadius = x;
            
            float InterCloudEffectiveRadius = (PosR - InterRadius) / min(OuterRadius - InterRadius, 12.0);
            float DenAndThiFactor = Shape(EffectiveRadius, 0.9, 1.5);

            // [密度判断]
            if ((abs(PosY) < SampleThin * DenAndThiFactor) || (PosY < SampleThin * (1.0 - 5.0 * pow(InterCloudEffectiveRadius, 2.0))))
            {
                // [物理/螺旋计算]
                float AngularVelocity = GetKeplerianAngularVelocity(PosR, 1.0, Spin);
                float HalfPiTimeInside = kPi / GetKeplerianAngularVelocity(3.0, 1.0, Spin);
                
                // 螺旋计算 (简化调用，保留原逻辑)
                float u = sqrt(PosR);
                float k_cubed = Spin * 0.70710678;
                float SpiralTheta;
                if (abs(k_cubed) < 0.001 * u * u * u) {
                    float inv_u = 1.0 / u; float eps3 = k_cubed * pow(inv_u, 3.0);
                    SpiralTheta = -16.9705627 * inv_u * (1.0 - 0.25 * eps3 + 0.142857 * eps3 * eps3);
                } else {
                    float k = sign(k_cubed) * pow(abs(k_cubed), 0.33333333);
                    SpiralTheta = (5.6568542 / k) * (0.5 * log((PosR - k*u + k*k) / pow(u+k, 2.0)) + 1.7320508 * (atan(2.0*u - k, 1.7320508 * k) - 1.5707963));
                }
                
                float InnerTheta = kPi / HalfPiTimeInside * iBlackHoleTime;
                float PosTheta = Vec2ToTheta(SamplePos.zx, vec2(cos(-SpiralTheta), sin(-SpiralTheta)));
                float PosLogarithmicTheta = Vec2ToTheta(SamplePos.zx, vec2(cos(-2.0 * log(PosR)), sin(-2.0 * log(PosR))));
                float PosThetaForInnerCloud = Vec2ToTheta(SamplePos.zx, vec2(cos(0.666666 * InnerTheta), sin(0.666666 * InnerTheta)));

                // [温度与红移]
                float DiskTemperature = pow(DiskTemperatureArgument * pow(1.0 / PosR, 3.0) * max(1.0 - sqrt(InterRadius / PosR), 0.000001), 0.25);
                vec3 CloudVelocity = AngularVelocity * cross(vec3(0.0, 1.0, 0.0), SamplePos);
                float RelativeVelocity = clamp(dot(-DirVec, CloudVelocity), -0.99, 0.99); // 使用 DirVec
                float Dopler = sqrt((1.0 + RelativeVelocity) / (1.0 - RelativeVelocity));
                float GravitationalRedshift = sqrt(max(1.0 - 1.0 / PosR, 0.000001));
                float RedShift = Dopler * GravitationalRedshift;

                // [噪声生成]
                float RotPosR = PosR + 0.25 / 3.0 * iBlackHoleTime;
                float Density = DenAndThiFactor;
                float Thick = SampleThin * Density * (0.4 + 0.6 * clamp(SampleThin - 0.5, 0.0, 2.5) / 2.5 + (1.0 - (0.4 + 0.6 * clamp(SampleThin - 0.5, 0.0, 2.5) / 2.5)) * SoftSaturate(GenerateAccretionDiskNoise(vec3(1.5 * PosTheta, RotPosR, 0.0), -0.7 + NoiseLevel, 1.3 + NoiseLevel, 80.0))); 
                float ThickM = SampleThin * Density;
                
                vec4 SampleColor = vec4(0.0);
                
                if (abs(PosY) < SampleThin * Density)
                {
                    float Levelmut = 0.91 * log(1.0 + (0.06 / 0.91 * max(0.0, min(1000.0, PosR) - 10.0)));
                    float Conmut = 80.0 * log(1.0 + (0.1 * 0.06 * max(0.0, min(1000000.0, PosR) - 10.0)));
                    
                    SampleColor = vec4(GenerateAccretionDiskNoise(vec3(0.1 * RotPosR, 0.1 * PosY, 0.02 * pow(OuterRadius, 0.7) * PosTheta), NoiseLevel + 2.0 - Levelmut, NoiseLevel + 4.0 - Levelmut, 80.0 - Conmut)); 
                    
                    if(PosTheta + kPi < 0.1 * kPi) {
                        SampleColor *= (PosTheta + kPi) / (0.1 * kPi);
                        SampleColor += (1.0 - ((PosTheta + kPi) / (0.1 * kPi))) * vec4(GenerateAccretionDiskNoise(vec3(0.1 * RotPosR, 0.1 * PosY, 0.02 * pow(OuterRadius, 0.7) * (PosTheta + 2.0 * kPi)), NoiseLevel + 2.0 - Levelmut, NoiseLevel + 4.0 - Levelmut, 80.0 - Conmut));
                    }
                    
                    if(PosR > max(0.15379 * OuterRadius, 0.15379 * 64.0)) {
                        float Spir = (GenerateAccretionDiskNoise(vec3(0.1 * (PosR * (4.65114e-6) - 0.1 / 3.0 * iBlackHoleTime - 0.08 * OuterRadius * PosLogarithmicTheta), 0.1 * PosY, 0.02 * pow(OuterRadius, 0.7) * PosLogarithmicTheta), NoiseLevel + 2.0 - Levelmut, NoiseLevel + 3.0 - Levelmut, 80.0 - Conmut)); 
                        if(PosLogarithmicTheta + kPi < 0.1 * kPi) {
                            Spir *= (PosLogarithmicTheta + kPi) / (0.1 * kPi);
                            Spir += (1.0 - ((PosLogarithmicTheta + kPi) / (0.1 * kPi))) * (GenerateAccretionDiskNoise(vec3(0.1 * (PosR * (4.65114e-6) - 0.1 / 3.0 * iBlackHoleTime - 0.08 * OuterRadius * (PosLogarithmicTheta + 2.0 * kPi)), 0.1 * PosY, 0.02 * pow(OuterRadius, 0.7) * (PosLogarithmicTheta + 2.0 * kPi)), NoiseLevel + 2.0 - Levelmut, NoiseLevel + 3.0 - Levelmut, 80.0 - Conmut));
                        }
                        SampleColor *= (mix(1.0, clamp(0.7 * Spir * 1.5 - 0.5, 0.0, 3.0), 0.5 + 0.5 * max(-1.0, 1.0 - exp(-1.5 * 0.1 * (100.0 * PosR / max(OuterRadius, 64.0) - 20.0)))));
                    }

                    float VerticalMixFactor = max(0.0, (1.0 - abs(PosY) / Thick));
                    Density *= 0.7 * VerticalMixFactor * Density;
                    SampleColor.xyz *= Density * 1.4;
                    SampleColor.a *= (Density) * (Density) / 0.3;
                    SampleColor.xyz *= max(0.0, (0.2 + 2.0 * sqrt(pow(PosY / Thick, 2.0) + 0.001)));
                }
                
                if (abs(PosY) < SampleThin * (1.0 - 5.0 * pow(InterCloudEffectiveRadius, 2.0)))
                {
                    float DustColor = max(1.0 - pow(PosY / (SampleThin * max(1.0 - 5.0 * pow(InterCloudEffectiveRadius, 2.0), 0.0001)), 2.0), 0.0) * GenerateAccretionDiskNoise(vec3(1.5 * fract((1.5 * PosThetaForInnerCloud + kPi / HalfPiTimeInside * iBlackHoleTime) / 2.0 / kPi) * 2.0 * kPi, PosR, PosY), 0.0, 6.0, 80.0);
                    SampleColor += 0.02 * vec4(vec3(DustColor), 0.2 * DustColor) * sqrt(max(0.0001,1.0 - DirVec.y * DirVec.y)) * min(1.0, Dopler * Dopler);
                }

                // [颜色物理应用]
                float BrightWithoutRedshift = 0.05 * min(OuterRadius / (1000.0), 1000.0 / OuterRadius) + 0.55 / exp(5.0 * EffectiveRadius) * mix(0.2 + 0.8 * abs(DirVec.y), 1.0, clamp(Thick - 0.8, 0.2, 1.0)); 
                BrightWithoutRedshift *= pow(DiskTemperature / PeakTemperature, BlackbodyIntensityExponent); 
                
                float VisionTemperature = DiskTemperature * pow(RedShift, RedShiftColorExponent); 
                
                SampleColor.xyz *= BrightWithoutRedshift * KelvinToRgb(VisionTemperature); 
                SampleColor.xyz *= min(pow(RedShift, RedShiftIntensityExponent), ShiftMax); 
                SampleColor.xyz *= min(1.0, 1.8 * (OuterRadius - PosR) / (OuterRadius - InterRadius)); 
                
                SampleColor.a *= 0.125;
                
                // 边缘淡出
                SampleColor *= max(mix(vec4(5.0 / (max(SampleThin, 0.2) + (0.0 + Hopper * 0.5) * OuterRadius)), vec4(vec3(0.3 + 0.7 * 5.0 / (SampleThin + (0.0 + Hopper * 0.5) * OuterRadius)), 1.0), 0.0 * exp(-pow(20.0 * PosR / OuterRadius, 2.0))), 
                             mix(vec4(100.0 / OuterRadius), vec4(vec3(0.3 + 0.7 * 100.0 / OuterRadius), 1.0), exp(-pow(20.0 * PosR / OuterRadius, 2.0))));
                SampleColor.xyz *= mix(1.0, max(1.0, abs(DirVec.y) / 0.2), clamp(0.3 - 0.6 * (ThickM - 1.0), 0.0, 0.3));

                // [积分累加]
                // 这里的关键：颜色乘以当前小步长 dt
                AccumulatedDiskColor += SampleColor * dt;
            }
        }
        
        // 推进光线
        TraveledDist += dt;
    }
    
    // [后期颜色处理 (对累积结果操作)]
    // 如果没有积累到任何颜色，直接返回背景
    if (AccumulatedDiskColor.a <= 0.0 && AccumulatedDiskColor.r <= 0.0) return BaseColor;

    vec4 Color = AccumulatedDiskColor;
    vec4 Result = vec4(0.0);

    Color.xyz *= Brightmut;
    Color.a *= Darkmut;

    // Reddening / Saturation Logic (Copy from original)
    float aR = 1.0 + Reddening * (1.0 - 1.0);
    float aG = 1.0 + Reddening * (3.0 - 1.0);
    float aB = 1.0 + Reddening * (6.0 - 1.0);
    float Sum_rgb = (Color.r + Color.g + Color.b) * pow(1.0 - BaseColor.a, aG);
    Sum_rgb *= 1.0;
    
    float r001 = 0.0; float g001 = 0.0; float b001 = 0.0;
        
    float Denominator = Color.r * pow(1.0 - BaseColor.a, aR) + Color.g * pow(1.0 - BaseColor.a, aG) + Color.b * pow(1.0 - BaseColor.a, aB);
    if (Denominator > 0.000001)
    {
        r001 = Sum_rgb * Color.r * pow(1.0 - BaseColor.a, aR) / Denominator;
        g001 = Sum_rgb * Color.g * pow(1.0 - BaseColor.a, aG) / Denominator;
        b001 = Sum_rgb * Color.b * pow(1.0 - BaseColor.a, aB) / Denominator;
        
        float invSum = 3.0 / (r001 + g001 + b001);
        r001 *= pow(r001 * invSum, Saturation);
        g001 *= pow(g001 * invSum, Saturation);
        b001 *= pow(b001 * invSum, Saturation);
    }

    Result.r = BaseColor.r + r001;
    Result.g = BaseColor.g + g001;
    Result.b = BaseColor.b + b001;
    // 原始逻辑：BaseColor.a + Color.a * pow((1.0 - BaseColor.a), 1.0);
    // 这里 Color.a 已经是积分后的累积值
    Result.a = BaseColor.a + Color.a * (1.0 - BaseColor.a);

    return Result;
}


// 注意：移除了 BlackHolePos, DiskNormal, DiskTangen 参数，增加了 Spin 参数
vec4 JetColor(vec4 BaseColor, float StepLength, vec3 RayPos, vec3 LastRayPos,
              vec3 RayDir, vec3 LastRayDir,
              float InterRadius, float OuterRadius, float JetRedShiftIntensityExponent, float JetBrightmut, float JetReddening, float JetSaturation, float AccretionRate, float JetShiftMax, float Spin)
{
    // [坐标系说明]
    // 输入已处于 Kerr-Schild 局部空间，Y 轴为自旋轴，XZ 为赤道面。
    vec3 PosOnDisk = RayPos;
    vec3 DirOnDisk = RayDir;

    // [物理半径] 使用 Boyer-Lindquist 半径 r
    float PosR = KerrSchildRadius(PosOnDisk, Spin, 1.0);
    
    // 垂直高度 (Kerr-Schild 下 Y 坐标未被混合，可直接作为高度)
    float PosY = PosOnDisk.y;
    
    vec4 Color = vec4(0.0);
    bool NotInJet = true;
    vec4 Result = vec4(0.0);
    
    // [喷流核心 (Core)]
    // 使用 Euclidean 距离 length(PosOnDisk.xz) 来判定圆柱体宽度在视觉上是足够好的近似
    float RhoSq = dot(PosOnDisk.xz, PosOnDisk.xz); // x^2 + z^2
    
    if (RhoSq < 2.0 * InterRadius * InterRadius + 0.03 * 0.03 * PosY * PosY && PosR < sqrt(2.0) * OuterRadius)
    {
        vec4 Color0;
        NotInJet = false;

        // 更新角速度以包含自旋影响，影响喷流内部纹理的旋转速度
        float InnerTheta = 3.0 * GetKeplerianAngularVelocity(InterRadius, 1.0, Spin) * (iBlackHoleTime - 1.0 / 0.8 * abs(PosY));

        float Shape = 1.0 / sqrt(InterRadius * InterRadius + 0.02 * 0.02 * PosY * PosY);
        
        // 噪声采样
        float noiseInput = 0.3 * (iBlackHoleTime - 1.0 / 0.8 * abs(abs(PosY) + 100.0 * (RhoSq / PosR))) / (OuterRadius / 100.0) / (1.0 / 0.8);
        float a = mix(0.7 + 0.3 * PerlinNoise1D(noiseInput), 1.0, exp(-0.01 * 0.01 * PosY * PosY));
        
        Color0 = vec4(1.0, 1.0, 1.0, 0.5) * max(0.0, 1.0 - 5.0 * Shape * abs(1.0 - pow(length(PosOnDisk.xz) * Shape, 2.0))) * Shape;
        Color0 *= a;
        
        // 边缘与距离衰减
        Color0 *= max(0.0, 1.0 - 1.0 * exp(-0.0001 * PosY / InterRadius * PosY / InterRadius));
        Color0 *= exp(-4.0 / (2.0) * PosR / OuterRadius * PosR / OuterRadius);
        Color0 *= 0.5;
        Color += Color0;
    }

    // [喷流外层 (Sheath)]
    float Wid = abs(PosY); // 随高度变宽
    float Rho = sqrt(RhoSq);
    
    if (Rho < 1.3 * InterRadius + 0.25 * Wid && Rho > 0.7 * InterRadius + 0.15 * Wid && PosR < 30.0 * InterRadius)
    {
        vec4 Color1;
        NotInJet = false;

        // 外层旋转速度较慢
        float InnerTheta = 2.0 * GetKeplerianAngularVelocity(InterRadius, 1.0, Spin) * (iBlackHoleTime - 1.0 / 0.8 * abs(PosY));

        float Shape = 1.0 / (InterRadius + 0.2 * Wid);
        
        // 复杂的噪声扭曲形状
        float Twist = 0.2 * (1.1 - exp(-0.1 * 0.1 * PosY * PosY)) * 
                      (PerlinNoise1D(0.35 * (iBlackHoleTime - 1.0 / 0.8 * abs(PosY)) / (1.0 / 0.8)) - 0.5);
        
        vec2 TwistedPos = PosOnDisk.xz + Twist * vec2(cos(0.666666 * InnerTheta), -sin(0.666666 * InnerTheta));
        
        Color1 = vec4(1.0, 1.0, 1.0, 0.5) * max(0.0, 1.0 - 2.0 * abs(1.0 - pow(length(TwistedPos) * Shape, 2.0))) * Shape;
                                             
        Color1 *= 1.0 - exp(-PosY / InterRadius * PosY / InterRadius);
        Color1 *= exp(-0.005 * PosY / InterRadius * PosY / InterRadius);
        Color1 *= 0.5;
        Color += Color1;
    }

    // [物理着色与红移]
    if (!NotInJet)
    {
        float JetTemperature = 100000.0;
        Color.xyz *= KelvinToRgb(JetTemperature);
        
        // 相对速度计算：
        // 喷流物质主要沿 Y 轴向外运动 (sign(PosY))
        // 速度近似为逃逸速度 sqrt(1/r)
        float RelativeVelocity = -(DirOnDisk.y) * sqrt(1.0 / (PosR)) * sign(PosY);
        
        // 狭义相对论多普勒
        float Dopler = sqrt((1.0 + RelativeVelocity) / (1.0 - RelativeVelocity));
        
        // 广义相对论引力红移 (Kerr Metric 近似)
        // 假设观察者在无穷远，红移因子为 sqrt(g_tt)
        float GravitationalRedshift = sqrt(max(1.0 - 1.0 / PosR, 0.000001));
        
        float RedShift = Dopler * GravitationalRedshift;
        
        Color.xyz *= min(pow(RedShift, JetRedShiftIntensityExponent), JetShiftMax);
        
        // 亮度与吸积率挂钩
        Color *= StepLength * JetBrightmut * (0.5 + 0.5 * tanh(log(AccretionRate) + 1.0));
        Color.a *= 0.0; // 喷流也是加法混合，Alpha 设为 0 (或者用于辉光)
    }

    if (NotInJet)
    {
        return BaseColor;
    }

    // [后期处理：红化与饱和度] (保持原有逻辑)
    float aR = 1.0 + JetReddening * (1.0 - 1.0);
    float aG = 1.0 + JetReddening * (2.5 - 1.0);
    float aB = 1.0 + JetReddening * (4.5 - 1.0);
    float Sum_rgb = (Color.r + Color.g + Color.b) * pow(1.0 - BaseColor.a, aG);
    Sum_rgb *= 1.0;
    
    float r001 = 0.0;
    float g001 = 0.0;
    float b001 = 0.0;
        
    float Denominator = Color.r * pow(1.0 - BaseColor.a, aR) + Color.g * pow(1.0 - BaseColor.a, aG) + Color.b * pow(1.0 - BaseColor.a, aB);
    if (Denominator > 0.000001)
    {
        r001 = Sum_rgb * Color.r * pow(1.0 - BaseColor.a, aR) / Denominator;
        g001 = Sum_rgb * Color.g * pow(1.0 - BaseColor.a, aG) / Denominator;
        b001 = Sum_rgb * Color.b * pow(1.0 - BaseColor.a, aB) / Denominator;
        
        r001 *= pow(3.0 * r001 / (r001 + g001 + b001), JetSaturation);
        g001 *= pow(3.0 * g001 / (r001 + g001 + b001), JetSaturation);
        b001 *= pow(3.0 * b001 / (r001 + g001 + b001), JetSaturation);
    }

    Result.r = BaseColor.r + r001;
    Result.g = BaseColor.g + g001;
    Result.b = BaseColor.b + b001;
    Result.a = BaseColor.a + Color.a * pow((1.0 - BaseColor.a), 1.0);

    return Result;
}

// =============================================================================
// 主函数
// =============================================================================

void main()
{
    vec4  Result = vec4(0.0);
    vec2  FragUv = gl_FragCoord.xy / iResolution.xy;
    float Fov    = tan(iFovRadians / 2.0);
    // --- 0. 物理参数准备 ---
    float iSpinclamp = clamp(iSpin, 0.0, 0.095);
    float Zx = 1.0 + pow(1.0 - pow(iSpinclamp, 2.0), 0.333333333333333) * (pow(1.0 + pow(iSpinclamp, 2.0), 0.333333333333333) + pow(1.0 - iSpinclamp, 0.333333333333333)); 
    float RmsRatio = (3.0 + sqrt(3.0 * pow(iSpinclamp, 2.0) + Zx * Zx) - sqrt((3.0 - Zx) * (3.0 + Zx + 2.0 * sqrt(3.0 * pow(iSpinclamp, 2.0) + Zx * Zx)))) / 2.0; 
    float AccretionEffective = sqrt(1.0 - 1.0 / RmsRatio); 
    const float kPhysicsFactor = 1.52491e30; 
    float DiskArgument = kPhysicsFactor / iBlackHoleMassSol* (iMu / AccretionEffective) * (iAccretionRate );
    float PeakTemperature = pow(DiskArgument * 0.05665278, 0.25);
    
    float RaymarchingBoundary = max(iOuterRadiusRs + 1.0, 501.0);
    float BackgroundShiftMax = 2.0;
    float BackgroundBlueShift = min(1.0 / sqrt(1.0 - 1.0 /max(length(iBlackHoleRelativePosRs.xyz), 1.001) + 0.005), BackgroundShiftMax);
    
    float PhysicalSpinA = iSpin * CONST_M;
    float EventHorizonR = 0.5 + sqrt(max(0.0, 0.25 - PhysicalSpinA * PhysicalSpinA));
    bool bIsNakedSingularity = abs(PhysicalSpinA) > CONST_M;
    float InnerRegionBoundary = 0.5;
    float TerminationR = EventHorizonR ;
    float CurrentUniverseSign = iUniverseSign;
    float ShiftMax = 1.5;
    // --- 1. 相机光线生成 ---
    vec2 Jitter = vec2(RandomStep(FragUv, fract(iTime * 1.0 + 0.5)), RandomStep(FragUv, fract(iTime * 1.0))) / iResolution;
    vec3 ViewDirLocal = FragUvToDir(FragUv + 0.5 * Jitter, Fov, iResolution);
    
    vec3 RayDirWorld = normalize((iInverseCamRot * vec4(ViewDirLocal, 0.0)).xyz);
    vec3 CamToBHVecVisual = (iInverseCamRot * vec4(iBlackHoleRelativePosRs.xyz, 0.0)).xyz;
    vec3 RayPosVisual = -CamToBHVecVisual;
    vec3 RayPosWorld = RayPosVisual; 
    
    vec3 DiskNormalWorld = normalize((iInverseCamRot * vec4(iBlackHoleRelativeDiskNormal.xyz, 0.0)).xyz);
    vec3 DiskTangentWorld = normalize((iInverseCamRot * vec4(iBlackHoleRelativeDiskTangen.xyz, 0.0)).xyz);
    vec3 BH_Y = normalize(DiskNormalWorld);             
    vec3 BH_X = normalize(DiskTangentWorld);            
    BH_X = normalize(BH_X - dot(BH_X, BH_Y) * BH_Y);
    vec3 BH_Z = normalize(cross(BH_X, BH_Y));           
    mat3 LocalToWorldRot = mat3(BH_X, BH_Y, BH_Z);
    mat3 WorldToLocalRot = transpose(LocalToWorldRot);
    
    RayPosWorld = WorldToLocalRot * RayPosWorld;
    RayDirWorld = WorldToLocalRot * RayDirWorld;
    // --- 2. 优化: 包围球快进 ---
    float DistanceToBlackHole = length(RayPosWorld);
    bool bShouldContinueMarchRay = true;
    bool bWaitCalBack = false;
    float GravityFade = CubicInterpolate(max(min(1.0 - (0.01 * DistanceToBlackHole - 1.0) / 4.0, 1.0), 0.0));
    
    if (DistanceToBlackHole > RaymarchingBoundary) 
    {
        vec3 O = RayPosWorld; vec3 D = RayDirWorld; float r = RaymarchingBoundary - 1.0; 
        float b = dot(O, D); float c = dot(O, O) - r * r; float delta = b * b - c; 
        if (delta < 0.0) { bShouldContinueMarchRay = false; bWaitCalBack = true; } 
        else {
            float tEnter = -b - sqrt(delta); 
            if (tEnter > 0.0) RayPosWorld = O + D * tEnter;
            else if (-b + sqrt(delta) <= 0.0) { bShouldContinueMarchRay = false; bWaitCalBack = true; }
        }
    }
    // --- 3. 物理动量初始化 ---
    vec4 X = vec4(RayPosWorld, 0.0);
    vec4 P_cov = vec4(0.0);
    if (bShouldContinueMarchRay) 
    {
        mat4 g_cov = GetKerrMetric(X, PhysicalSpinA, GravityFade, CurrentUniverseSign); 
        float g_tt = g_cov[3][3];
        if (g_tt < 0) {
            // (Tetrad Basis Construction remains same as improved version in input)
            vec3 R_flat = normalize(RayPosWorld);
            vec3 View_flat = normalize(RayDirWorld);
            float cosTheta = dot(View_flat, R_flat);
            float sinTheta = sqrt(max(1.0 - cosTheta * cosTheta, 0.0));
            vec3 T_flat = (sinTheta < 1e-5) ? normalize(cross(R_flat, (abs(R_flat.y) > 0.9) ? vec3(1,0,0) : vec3(0,1,0))) : (View_flat - cosTheta * R_flat) / sinTheta;
            
            float D = -g_tt; vec3 g_ti = g_cov[3].xyz; mat3 g_sp = mat3(g_cov);
            float K_r = dot(g_ti, R_flat); float K_t = dot(g_ti, T_flat);
            vec3 gS_r = g_sp * R_flat; vec3 gS_t = g_sp * T_flat;
            float S_rr = dot(R_flat, gS_r); float S_tt = dot(T_flat, gS_t); float S_rt = dot(R_flat, gS_t);
            
            vec3 Num_R = K_r * g_ti + D * gS_r;
            float NormSq_R = max(K_r * K_r + D * S_rr, 1e-20);
            float InvNorm_R = inversesqrt(NormSq_R);
            float CrossTerm = K_t * S_rr - K_r * S_rt;
            float Overlap = K_r * K_t + D * S_rt;
            vec3 Res_T = CrossTerm * g_ti + (NormSq_R * gS_t - Overlap * gS_r);
            float Core_Norm = S_tt * K_r * K_r + S_rr * K_t * K_t - 2.0 * S_rt * K_r * K_t;
            float PolyNorm = max(Core_Norm + D * (S_tt * S_rr - S_rt * S_rt), 1e-20);
            float Scale_T = sqrt(D / (NormSq_R * PolyNorm));
            vec3 Total_Spatial = g_ti + cosTheta * Num_R * InvNorm_R + sinTheta * Res_T * Scale_T;
            P_cov.w = -sqrt(D); P_cov.xyz = Total_Spatial * inversesqrt(D);
        } else { bShouldContinueMarchRay = false; Result = vec4(0.0); }
    }
    vec3 RayPos = X.xyz;
    vec3 LastPos = RayPos;
    vec3 RayDir = RayDirWorld;
    vec3 LastDir = RayDirWorld;
    int Count = 0;
    
    // --- 5. 光线追踪主循环 (RK4 + 插值采样) ---
    while (bShouldContinueMarchRay)
    {
        float DistanceToBlackHole = length(RayPos);
        
        if (DistanceToBlackHole > RaymarchingBoundary) { bShouldContinueMarchRay = false; bWaitCalBack = true; break; }
        
        float CurrentKerrR = KerrSchildRadius(RayPos, PhysicalSpinA, CurrentUniverseSign);
        if (CurrentUniverseSign > 0.0 && CurrentKerrR < TerminationR && abs(iSpin) <= 1.0) { bShouldContinueMarchRay = false; bWaitCalBack = false; break; }
        
        // RK4 使得我们可以使用大步长，减少迭代次数
        if ((Count > 150&&!bIsNakedSingularity)||(Count > 450&&bIsNakedSingularity)) { bShouldContinueMarchRay = false; bWaitCalBack = true; break; }

        float rho = length(RayPos.xz);
        float RayStep;
        bool useSymplecticEuler = false;
        if (bIsNakedSingularity && abs(CurrentKerrR) < InnerRegionBoundary) {
            // [区域 1: 裸奇点内部区域] 使用指定的优化公式 + 辛欧拉
            useSymplecticEuler = true;            
            RayStep = mix(0.15, 0.5, smoothstep(0.3, 0.5, abs(CurrentKerrR))) * sqrt(RayPos.y * RayPos.y + pow(rho - abs(PhysicalSpinA), 2.0));

        } else {
            // [区域 2: 外部区域或正常黑洞] 使用默认 RK4 步长策略
            useSymplecticEuler = false;
            // 外部区域使用宽松的 RK4 步长系数
            RayStep = 0.5 * sqrt(RayPos.y * RayPos.y + pow(rho - abs(PhysicalSpinA), 2.0));
            if (Count == 0) RayStep *= RandomStep(FragUv, fract(iTime));
            RayStep = max(RayStep, 0.02);
        }
        LastPos = X.xyz; 
        
        GravityFade = CubicInterpolate(max(min(1.0 - (0.01 * DistanceToBlackHole - 1.0) / 4.0, 1.0), 0.0));
        mat4 g_inv = GetInverseKerrMetric(X, PhysicalSpinA, GravityFade, CurrentUniverseSign);
        vec4 V_contra = g_inv * P_cov;
        
        // 转换空间步长 RayStep 为仿射参数 dLambda
        // |V| dLambda = RayStep  => dLambda = RayStep / |V|
        float V_mag = length(V_contra.xyz); // 空间速度模长
        float dLambda = RayStep / max(V_mag, 1e-4);
        if (useSymplecticEuler) {
            StepGeodesicEuler(X, P_cov, dLambda, PhysicalSpinA, GravityFade, CurrentUniverseSign);
        } else 
        {
            StepGeodesicRK4(X, P_cov, dLambda, PhysicalSpinA, GravityFade, CurrentUniverseSign);
        }
        RayPos = X.xyz;
        vec3 StepVec = RayPos - LastPos;
        float ActualStepLength = length(StepVec);
        RayDir = (ActualStepLength > 1e-7) ? StepVec / ActualStepLength : LastDir;
        // 穿越检测
        if (LastPos.y * RayPos.y < 0.0) {
            float t_cross = LastPos.y / (LastPos.y - RayPos.y);
            float rho_cross = length(mix(LastPos.xz, RayPos.xz, t_cross));
            if (rho_cross < abs(PhysicalSpinA)) CurrentUniverseSign *= -1.0;
        }
        // ==========================================
        // 核心改动 3: 吸积盘插值采样 (Sub-stepping)
        // ==========================================
            if (CurrentUniverseSign > 0.0) {
                // 注意：RayPos 是 RK4 步进的终点，LastPos 是起点
                Result = DiskColor(Result, ActualStepLength, RayPos, LastPos, RayDir, LastDir,
                                   iInterRadiusRs, iOuterRadiusRs, iThinRs, iHopper, iBrightmut, iDarkmut, iReddening, iSaturation, DiskArgument, 
                                   iBlackbodyIntensityExponent, iRedShiftColorExponent, iRedShiftIntensityExponent, PeakTemperature, ShiftMax, clamp(PhysicalSpinA,-0.49,0.49));
                 
                // JetColor 保持原样 (如果需要同样的优化，也可以将循环逻辑复制进去，但通常吸积盘对精度要求最高)
                //Result = JetColor(Result, ActualStepLength, RayPos, LastPos, RayDir, LastDir,
                //                  iInterRadiusRs, iOuterRadiusRs, iJetRedShiftIntensityExponent, iJetBrightmut, iReddening, iJetSaturation, iAccretionRate, iJetShiftMax, clamp(PhysicalSpinA,-0.049,0.049));
            }
        
        if (Result.a > 0.99) { bShouldContinueMarchRay = false; bWaitCalBack = false; break; }
        
        LastDir = RayDir;
        Count++;
    }
    
    // --- 6. 背景采样 (保持不变) ---
    if (bWaitCalBack)
    {
        vec3 EscapeDirWorld = LocalToWorldRot * normalize(RayDir); 
        vec4 Backcolor = textureLod(iBackground, EscapeDirWorld, min(1.0, textureQueryLod(iBackground, EscapeDirWorld).x));
        if (CurrentUniverseSign < 0.0) Backcolor = textureLod(iAntiground, EscapeDirWorld, min(1.0, textureQueryLod(iAntiground, EscapeDirWorld).x));
        
        vec4 TexColor = Backcolor;
        if (length(iBlackHoleRelativePosRs.xyz) < 200.0) {
            vec3 Rcolor = Backcolor.r * 1.0 * WavelengthToRgb(max(453.0, 645.0 / BackgroundBlueShift));
            vec3 Gcolor = Backcolor.g * 1.5 * WavelengthToRgb(max(416.0, 510.0 / BackgroundBlueShift));
            vec3 Bcolor = Backcolor.b * 0.6 * WavelengthToRgb(max(380.0, 440.0 / BackgroundBlueShift));
            vec3 Scolor = Rcolor + Gcolor + Bcolor;
            float OStrength = 0.3 * Backcolor.r + 0.6 * Backcolor.g + 0.1 * Backcolor.b;
            float RStrength = 0.3 * Scolor.r + 0.6 * Scolor.g + 0.1 * Scolor.b;
            Scolor *= OStrength / max(RStrength, 0.001);
            TexColor = vec4(Scolor, Backcolor.a) * pow(BackgroundBlueShift, 4.0);
        }
        Result += 0.9999 * TexColor * (1.0 - Result.a);
    }
    // --- 7. 色调映射与 TAA (保持不变) ---
    float RedFactor   = 3.0 * Result.r / (Result.g + Result.b + Result.g);
    float BlueFactor  = 3.0 * Result.b / (Result.g + Result.b + Result.g);
    float GreenFactor = 3.0 * Result.g / (Result.g + Result.b + Result.g);
    float BloomMax    = 8.0;
    
    Result.r = min(-4.0 * log(1.0 - pow(Result.r, 2.2)), BloomMax * RedFactor);
    Result.g = min(-4.0 * log(1.0 - pow(Result.g, 2.2)), BloomMax * GreenFactor);
    Result.b = min(-4.0 * log(1.0 - pow(Result.b, 2.2)), BloomMax * BlueFactor);
    Result.a = min(-4.0 * log(1.0 - pow(Result.a, 2.2)), 4.0);
    
    float BlendWeight = iBlendWeight;
    vec4 PrevColor = texelFetch(iHistoryTex, ivec2(gl_FragCoord.xy), 0);
    FragColor      = (BlendWeight) * Result + (1.0 - BlendWeight) * PrevColor;
}