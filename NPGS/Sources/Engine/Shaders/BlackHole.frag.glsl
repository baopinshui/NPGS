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
    float iQ;
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
float GetKeplerianAngularVelocity(float Radius, float Rs, float Spin, float Q) 
{
    float M = 0.5 * Rs; 
    
    // Kerr-Newman 角速度公式:
    // Omega = sqrt(Mr - Q^2) / (r^2 + a * sqrt(Mr - Q^2))
    // 注意: 这里假设测试粒子是电中性的。如果粒子带电，会有洛伦兹力项，但对于吸积盘视觉模拟，通常忽略粒子自身电荷。
    
    float Mr_minus_Q2 = M * Radius - Q * Q;
    
    // 保护：防止在视界内或极端 Q 值导致负根号
    if (Mr_minus_Q2 < 0.0) return 0.0;
    
    float sqrt_Term = sqrt(Mr_minus_Q2);
    
    // 分母: r^2 + a * sqrt(Mr - Q^2)
    float denominator = Radius * Radius + Spin * sqrt_Term;
    
    return sqrt_Term / max(1e-6, denominator);
}
// =============================================================================
// Kerr-Schild (Spin Axis = Y) & Kerr-Newman Support
// =============================================================================
const float CONST_M = 0.5; 
const mat4  MINKOWSKI_METRIC = mat4(
    1, 0, 0, 0,
    0, 1, 0, 0,
    0, 0, 1, 0,
    0, 0, 0, -1
);

// KerrSchildRadius 仅计算几何半径，不依赖于 Mass 或 Charge
float KerrSchildRadius(vec3 p, float a, float r_sign) {
    // [优化] 史瓦西/Reissner-Nordstrom 极限 (a=0)
    if (a == 0.0) {
        return r_sign * length(p);
    }

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

// 修改: 增加 Q 参数 (Physical Q)
mat4 GetInverseKerrMetric(vec4 X, float a, float Q, float fade, float r_sign) {
    // [优化] 史瓦西/Reissner-Nordstrom 极限 (a=0)
    if (a == 0.0) {
        float r = length(X.xyz);
        if (r < 1e-6) return MINKOWSKI_METRIC;

        vec4 l_up = vec4(X.xyz / r, -1.0);
        
        // Kerr-Newman: f = (2Mr - Q^2) / r^2 = 2M/r - Q^2/r^2
        float f = (2.0 * CONST_M / r - (Q * Q) / (r * r)) * fade;
        
        return MINKOWSKI_METRIC - f * outerProduct(l_up, l_up);
    }

    float r = KerrSchildRadius(X.xyz, a, r_sign); 
    float r2 = r * r;
    float r3 = r2 * r;
    float a2 = a * a;
    float denom = 1.0 / (r2 + a2);
    
    float lx = (r * X.x + a * X.z) * denom;
    float ly = X.y / r;
    float lz = (r * X.z - a * X.x) * denom;
    
    vec4 l_up = vec4(lx, ly, lz, -1.0); 
    
    // Kerr-Newman-Schild Scalar:
    // f = (2Mr^3 - Q^2 r^2) / (r^4 + a^2 z^2)
    float numerator = 2.0 * CONST_M * r3 - Q * Q * r2;
    float denominator = r2 * r2 + a2 * X.y * X.y;
    float f = (numerator / denominator) * fade;
    
    return MINKOWSKI_METRIC - f * outerProduct(l_up, l_up);
}

// 修改: 增加 Q 参数
mat4 GetKerrMetric(vec4 X, float a, float Q, float fade, float r_sign) {
    if (a == 0.0) {
        float r = length(X.xyz);
        if (r < 1e-6) return MINKOWSKI_METRIC;

        vec4 l_down = vec4(X.xyz / r, 1.0); 
        // Kerr-Newman
        float f = (2.0 * CONST_M / r - (Q * Q) / (r * r)) * fade;
        
        return MINKOWSKI_METRIC + f * outerProduct(l_down, l_down);
    }

    float r = KerrSchildRadius(X.xyz, a, r_sign);
    float r2 = r * r;
    float r3 = r2 * r;
    float a2 = a * a;
    float denom = 1.0 / (r2 + a2);
    
    float lx = (r * X.x + a * X.z) * denom;
    float ly = X.y / r;
    float lz = (r * X.z - a * X.x) * denom;
    
    vec4 l_down = vec4(lx, ly, lz, 1.0); 
    
    // Kerr-Newman-Schild Scalar
    float numerator = 2.0 * CONST_M * r3 - Q * Q * r2;
    float denominator = r2 * r2 + a2 * X.y * X.y;
    float f = (numerator / denominator) * fade;
    
    return MINKOWSKI_METRIC + f * outerProduct(l_down, l_down);
}

// 修改: 增加 Q 参数
float Hamiltonian(vec4 X, vec4 P, float a, float Q, float fade, float r_sign) {
    if (a == 0.0) {
        float r = length(X.xyz);
        if (r < 1e-6) return 0.0; 

        float P_sq_minkowski = dot(P.xyz, P.xyz) - P.w * P.w;
        float l_dot_P_euclid = dot(X.xyz, P.xyz) / r - P.w;

        // Kerr-Newman
        float f = (2.0 * CONST_M / r - (Q * Q) / (r * r)) * fade;

        return 0.5 * (P_sq_minkowski - f * l_dot_P_euclid * l_dot_P_euclid);
    }

    mat4 g_inv = GetInverseKerrMetric(X, a, Q, fade, r_sign);
    return 0.5 * dot(g_inv * P, P);
}

// 修改: 增加 Q 参数
vec4 GradHamiltonian(vec4 X, vec4 P, float a, float Q, float fade, float current_r_sign) {
    float distToRing;
    if (a == 0.0) {
        distToRing = length(X.xyz);
    } else {
        float rho = length(X.xz); 
        distToRing = sqrt(X.y * X.y + pow(rho - abs(a), 2.0));
    }
    
    float eps = max(1e-5, 0.001 * distToRing); 
    
    vec4 G;
    float invTwoEps = 0.5 / eps; 
    
    vec4 dx = vec4(eps, 0.0, 0.0, 0.0);
    vec4 dy = vec4(0.0, eps, 0.0, 0.0);
    vec4 dz = vec4(0.0, 0.0, eps, 0.0);
    
    G.x = (Hamiltonian(X + dx, P, a, Q, fade, current_r_sign) - Hamiltonian(X - dx, P, a, Q, fade, current_r_sign));
    G.z = (Hamiltonian(X + dz, P, a, Q, fade, current_r_sign) - Hamiltonian(X - dz, P, a, Q, fade, current_r_sign));
    
    if (a == 0.0) {
        G.y = (Hamiltonian(X + dy, P, a, Q, fade, current_r_sign) - Hamiltonian(X - dy, P, a, Q, fade, current_r_sign));
    } else {
        vec4 X_plus = X + dy;
        float sign_plus = current_r_sign;
        if (length(X_plus.xz) < abs(a) && X.y * X_plus.y < 0.0) sign_plus = -current_r_sign;
        float H_plus = Hamiltonian(X_plus, P, a, Q, fade, sign_plus);
        
        vec4 X_minus = X - dy;
        float sign_minus = current_r_sign;
        if (length(X_minus.xz) < abs(a) && X.y * X_minus.y < 0.0) sign_minus = -current_r_sign;
        float H_minus = Hamiltonian(X_minus, P, a, Q, fade, sign_minus);
        
        G.y = H_plus - H_minus;
    }
    
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

// 修改: 增加 Q 参数
void ApplyHamiltonianCorrection(inout vec4 P, vec4 X, float a, float Q, float fade, float r_sign) {
    mat4 g_inv = GetInverseKerrMetric(X, a, Q, fade, r_sign);
    float A = g_inv[3][3];
    float B = 2.0 * dot(vec3(g_inv[0][3], g_inv[1][3], g_inv[2][3]), P.xyz);
    float C = dot(P.xyz, mat3(g_inv) * P.xyz);
    float disc = B * B - 4.0 * A * C;
    if (disc >= 0.0) {
        float sqrtDisc = sqrt(disc);
        float pt1 = (-B - sqrtDisc) / (2.0 * A);
        float pt2 = (-B + sqrtDisc) / (2.0 * A);
        P.w = (abs(pt1 - P.w) < abs(pt2 - P.w)) ? pt1 : pt2;
    }
}

// 修改: 增加 Q 参数
State GetDerivatives(State S, float a, float Q, float fade, float r_sign) {
    State deriv;
    deriv.X = GetInverseKerrMetric(S.X, a, Q, fade, r_sign) * S.P;
    deriv.P = -GradHamiltonian(S.X, S.P, a, Q, fade, r_sign);
    return deriv;
}

// 修改: 增加 Q 参数
void StepGeodesicRK4(inout vec4 X, inout vec4 P, float dt, float a, float Q, float fade, float r_sign) {
    State s0; s0.X = X; s0.P = P;
    
    State k1 = GetDerivatives(s0, a, Q, fade, r_sign);
    
    State s1; 
    s1.X = s0.X + 0.5 * dt * k1.X; 
    s1.P = s0.P + 0.5 * dt * k1.P;
    State k2 = GetDerivatives(s1, a, Q, fade, r_sign);
    
    State s2; 
    s2.X = s0.X + 0.5 * dt * k2.X; 
    s2.P = s0.P + 0.5 * dt * k2.P;
    State k3 = GetDerivatives(s2, a, Q, fade, r_sign);
    
    State s3; 
    s3.X = s0.X + dt * k3.X; 
    s3.P = s0.P + dt * k3.P;
    State k4 = GetDerivatives(s3, a, Q, fade, r_sign);
    
    X = s0.X + (dt / 6.0) * (k1.X + 2.0 * k2.X + 2.0 * k3.X + k4.X);
    P = s0.P + (dt / 6.0) * (k1.P + 2.0 * k2.P + 2.0 * k3.P + k4.P);
}

// 修改: 增加 Q 参数以正确计算引力红移
float GetGtt(vec3 Pos, float a, float Q, float r_sign)
{
    float r = KerrSchildRadius(Pos, a, r_sign); 
    float r2 = r * r;
    float r3 = r2 * r;
    float a2 = a * a;
    float z2 = Pos.y * Pos.y;
    
    float rho2 = dot(Pos.xz, Pos.xz);
    float denom = r2 * r2 + a2 * z2;
    // Kerr-Newman-Schild Scalar
    float f = (2.0 * CONST_M * r3 - Q * Q * r2) / max(1e-6, denom);
    
    return -1.0 + f;
}
// =============================================================================
// 吸积盘与喷流 (参数列表更新)
// =============================================================================
vec4 DiskColor(vec4 BaseColor, float StepLength, vec3 RayPos, vec3 LastRayPos,
               vec3 RayDir, vec3 LastRayDir,
               float InterRadius, float OuterRadius, float Thin, float Hopper, float Brightmut, float Darkmut, float Reddening, float Saturation, float DiskTemperatureArgument,
               float BlackbodyIntensityExponent, float RedShiftColorExponent, float RedShiftIntensityExponent,
               float PeakTemperature, float ShiftMax, float Spin, float Q, vec4 CameraPos) // 增加 Q
{
    vec4 CurrentResult = BaseColor;
    
    vec3 StartPos = LastRayPos;
    vec3 DirVec   = RayDir; 
    float TotalDist = StepLength;
    float TraveledDist = 0.0;
    
    // Gtt 包含 Q
    float GttObs = GetGtt(CameraPos.xyz, Spin, Q, CameraPos.w);

    // --- 包围盒剔除 (无变化) ---
    float R_Start = KerrSchildRadius(StartPos, Spin, 1.0);
    float R_End   = KerrSchildRadius(RayPos, Spin, 1.0);
    float MinR = min(R_Start, R_End);
    float MaxR = max(R_Start, R_End);
    float MinAbsY = min(abs(StartPos.y), abs(RayPos.y));
    float BoundsR = min(MaxR, OuterRadius * 1.5); 
    float MaxThicknessBounds = Thin * 2.0 + max(0.0, (BoundsR - 3.0) * Hopper) * 2.0; 
    bool IsCrossingPlane = (StartPos.y * RayPos.y < 0.0);

    if (MaxR < InterRadius * 0.9 || 
        MinR > OuterRadius * 1.1 || 
       (MinAbsY > MaxThicknessBounds * 1.5 && !IsCrossingPlane)) 
    {
        return BaseColor;
    }

    int MaxSubSteps = 128; 
    
    for (int i = 0; i < MaxSubSteps; i++)
    {
        if (CurrentResult.a > 0.99) break;
        if (TraveledDist >= TotalDist) break;

        vec3 CurrentPos = StartPos + DirVec * TraveledDist;
        
        float DistanceToBlackHole = length(CurrentPos); 
        float SmallStepBoundary = max(OuterRadius, 12.0);
        float StepSize = 1.0; 
        
        StepSize *= 0.15 + 0.25 * min(max(0.0, 0.5 * (0.5 * DistanceToBlackHole / max(10.0 , SmallStepBoundary) - 1.0)), 1.0);
        if ((DistanceToBlackHole) >= 2.0 * SmallStepBoundary) StepSize *= DistanceToBlackHole;
        else if ((DistanceToBlackHole) >= 1.0 * SmallStepBoundary) StepSize *= ((1.0 + 0.25 * max(DistanceToBlackHole - 12.0, 0.0)) * (2.0 * SmallStepBoundary - DistanceToBlackHole) + DistanceToBlackHole * (DistanceToBlackHole - SmallStepBoundary)) / SmallStepBoundary;
        else StepSize *= min(1.0 + 0.25 * max(DistanceToBlackHole - 12.0, 0.0), DistanceToBlackHole);
        
        float dt = min(StepSize, TotalDist - TraveledDist);
        
        float Dither = RandomStep(10000.0*(RayPos.zx/OuterRadius), iTime * 4.0 + float(i) * 0.1337);
        vec3 NextPos = CurrentPos + DirVec * dt; 
        vec3 SamplePos;

        if (CurrentPos.y * NextPos.y < 0.0) 
        {
            float t_cross = CurrentPos.y / (CurrentPos.y - NextPos.y);
            vec3 CPoint = CurrentPos + DirVec * (dt * t_cross);
            float JitterOffset = -1.0 + 2.0 * Dither; 
            SamplePos = CPoint + min(Thin, dt) * DirVec * JitterOffset;
        }
        else
        {
            SamplePos = CurrentPos + DirVec * dt * Dither;
        }
        
        float PosR = KerrSchildRadius(SamplePos, Spin, 1.0);
        float PosY = SamplePos.y;
        float SampleThin = Thin + max(0.0, (length(SamplePos.xz) - 3.0) * Hopper);
        float NoiseLevel = max(0.0, 2.0 - 0.6 * SampleThin);

        if (abs(PosY) < SampleThin * 1.2 && PosR < OuterRadius && PosR > InterRadius)
        {
            float x = (PosR - InterRadius) / (OuterRadius - InterRadius);
            float a_param = max(1.0, (OuterRadius - InterRadius) / 10.0);
            float EffectiveRadius = (-1.0 + sqrt(1.0 + 4.0 * a_param * a_param * x - 4.0 * x * a_param)) / (2.0 * a_param - 2.0);
            if(a_param == 1.0) EffectiveRadius = x;
            float InterCloudEffectiveRadius = (PosR - InterRadius) / min(OuterRadius - InterRadius, 12.0);
            float DenAndThiFactor = Shape(EffectiveRadius, 0.9, 1.5);

            if ((abs(PosY) < SampleThin * DenAndThiFactor) || (PosY < SampleThin * (1.0 - 5.0 * pow(InterCloudEffectiveRadius, 2.0))))
            {
                 float AngularVelocity = GetKeplerianAngularVelocity(PosR, 1.0, Spin,Q);
                 float HalfPiTimeInside = kPi / GetKeplerianAngularVelocity(3.0, 1.0, Spin,Q);
                 
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
                 
                 float DiskTemperature = pow(DiskTemperatureArgument * pow(1.0 / PosR, 3.0) * max(1.0 - sqrt(InterRadius / PosR), 0.000001), 0.25);
                 
                 vec3 CloudVelocity = AngularVelocity * cross(vec3(0.0, 1.0, 0.0), SamplePos);
                 float RelativeVelocity = clamp(dot(-DirVec, CloudVelocity), -0.99, 0.99); 
                 float Dopler = sqrt((1.0 + RelativeVelocity) / (1.0 - RelativeVelocity));
                 
                 // Gtt 包含 Q
                 float GttEmit = GetGtt(SamplePos, Spin, Q, 1.0);
                 float GravitationalRedshift = sqrt(abs(GttEmit) / max(1e-6, abs(GttObs)));
                 
                 float RedShift = Dopler * GravitationalRedshift;
                 
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

                 float BrightWithoutRedshift = 0.05 * min(OuterRadius / (1000.0), 1000.0 / OuterRadius) + 0.55 / exp(5.0 * EffectiveRadius) * mix(0.2 + 0.8 * abs(DirVec.y), 1.0, clamp(Thick - 0.8, 0.2, 1.0)); 
                 BrightWithoutRedshift *= pow(DiskTemperature / PeakTemperature, BlackbodyIntensityExponent); 
                 float VisionTemperature = DiskTemperature * pow(RedShift, RedShiftColorExponent); 
                 SampleColor.xyz *= BrightWithoutRedshift * KelvinToRgb(VisionTemperature); 
                 SampleColor.xyz *= min(pow(RedShift, RedShiftIntensityExponent), ShiftMax); 
                 SampleColor.xyz *= min(1.0, 1.8 * (OuterRadius - PosR) / (OuterRadius - InterRadius)); 
                 SampleColor.a *= 0.125;
                 
                 SampleColor *= max(mix(vec4(5.0 / (max(SampleThin, 0.2) + (0.0 + Hopper * 0.5) * OuterRadius)), vec4(vec3(0.3 + 0.7 * 5.0 / (SampleThin + (0.0 + Hopper * 0.5) * OuterRadius)), 1.0), 0.0 * exp(-pow(20.0 * PosR / OuterRadius, 2.0))), 
                              mix(vec4(100.0 / OuterRadius), vec4(vec3(0.3 + 0.7 * 100.0 / OuterRadius), 1.0), exp(-pow(20.0 * PosR / OuterRadius, 2.0))));
                 SampleColor.xyz *= mix(1.0, max(1.0, abs(DirVec.y) / 0.2), clamp(0.3 - 0.6 * (ThickM - 1.0), 0.0, 0.3));

                 vec4 StepColor = SampleColor * dt;
                 
                 float aR = 1.0 + Reddening * (1.0 - 1.0);
                 float aG = 1.0 + Reddening * (3.0 - 1.0);
                 float aB = 1.0 + Reddening * (6.0 - 1.0);
                 float Sum_rgb = (StepColor.r + StepColor.g + StepColor.b) * pow(1.0 - CurrentResult.a, aG);
                 
                 float r001 = 0.0; float g001 = 0.0; float b001 = 0.0;
                 float Denominator = StepColor.r * pow(1.0 - CurrentResult.a, aR) + StepColor.g * pow(1.0 - CurrentResult.a, aG) + StepColor.b * pow(1.0 - CurrentResult.a, aB);
                 
                 if (Denominator > 0.000001)
                 {
                     r001 = Sum_rgb * StepColor.r * pow(1.0 - CurrentResult.a, aR) / Denominator;
                     g001 = Sum_rgb * StepColor.g * pow(1.0 - CurrentResult.a, aG) / Denominator;
                     b001 = Sum_rgb * StepColor.b * pow(1.0 - CurrentResult.a, aB) / Denominator;
                     
                     float invSum = 3.0 / (r001 + g001 + b001);
                     r001 *= pow(r001 * invSum, Saturation);
                     g001 *= pow(g001 * invSum, Saturation);
                     b001 *= pow(b001 * invSum, Saturation);
                 }

                 CurrentResult.r += r001;
                 CurrentResult.g += g001;
                 CurrentResult.b += b001;
                 CurrentResult.a += StepColor.a * (1.0 - CurrentResult.a);
            }
        }
        TraveledDist += dt;
    }
    
    return CurrentResult;
}

vec4 JetColor(vec4 BaseColor, float StepLength, vec3 RayPos, vec3 LastRayPos,
              vec3 RayDir, vec3 LastRayDir,
              float InterRadius, float OuterRadius, float JetRedShiftIntensityExponent, float JetBrightmut, float JetReddening, float JetSaturation, float AccretionRate, float JetShiftMax, float Spin, float Q, vec4 CameraPos) // 增加 Q
{
    vec4 CurrentResult = BaseColor;
    
    vec3 StartPos = LastRayPos;
    vec3 DirVec   = RayDir; 
    float TotalDist = StepLength;
    float TraveledDist = 0.0;

    // Gtt 包含 Q
    float GttObs = GetGtt(CameraPos.xyz, Spin, Q, CameraPos.w);
    
    float R_Start = length(StartPos.xz);
    float R_End   = length(RayPos.xz);
    float MaxR_XZ = max(R_Start, R_End);
    float MaxY    = max(abs(StartPos.y), abs(RayPos.y));
    
    if (MaxR_XZ > OuterRadius * 1.5 && MaxY < OuterRadius) return BaseColor;

    int MaxSubSteps = 128; 
    
    for (int i = 0; i < MaxSubSteps; i++)
    {
        if (TraveledDist >= TotalDist) break;

        vec3 CurrentPos = StartPos + DirVec * TraveledDist;
        
        float DistanceToBlackHole = length(CurrentPos); 
        float SmallStepBoundary = max(OuterRadius, 12.0);
        float StepSize = 1.0; 
        
        StepSize *= 0.15 + 0.25 * min(max(0.0, 0.5 * (0.5 * DistanceToBlackHole / max(10.0 , SmallStepBoundary) - 1.0)), 1.0);
        if ((DistanceToBlackHole) >= 2.0 * SmallStepBoundary) StepSize *= DistanceToBlackHole;
        else if ((DistanceToBlackHole) >= 1.0 * SmallStepBoundary) StepSize *= ((1.0 + 0.25 * max(DistanceToBlackHole - 12.0, 0.0)) * (2.0 * SmallStepBoundary - DistanceToBlackHole) + DistanceToBlackHole * (DistanceToBlackHole - SmallStepBoundary)) / SmallStepBoundary;
        else StepSize *= min(1.0 + 0.25 * max(DistanceToBlackHole - 12.0, 0.0), DistanceToBlackHole);
        
        float dt = min(StepSize, TotalDist - TraveledDist);
        
        float Dither = RandomStep(10000.0 * (RayPos.zx / OuterRadius), iTime * 4.0 + float(i) * 0.1337);
        vec3 SamplePos = CurrentPos + DirVec * dt * Dither;
        
        float PosR = KerrSchildRadius(SamplePos, Spin, 1.0);
        float PosY = SamplePos.y;
        float RhoSq = dot(SamplePos.xz, SamplePos.xz);
        float Rho = sqrt(RhoSq);
        
        vec4 AccumColor = vec4(0.0);
        bool InJet = false;

        if (RhoSq < 2.0 * InterRadius * InterRadius + 0.03 * 0.03 * PosY * PosY && PosR < sqrt(2.0) * OuterRadius)
        {
            InJet = true;
            float InnerTheta = 3.0 * GetKeplerianAngularVelocity(InterRadius, 1.0, Spin,Q) * (iBlackHoleTime - 1.0 / 0.8 * abs(PosY));
            float Shape = 1.0 / sqrt(InterRadius * InterRadius + 0.02 * 0.02 * PosY * PosY);
            float noiseInput = 0.3 * (iBlackHoleTime - 1.0 / 0.8 * abs(abs(PosY) + 100.0 * (RhoSq / PosR))) / (OuterRadius / 100.0) / (1.0 / 0.8);
            float a = mix(0.7 + 0.3 * PerlinNoise1D(noiseInput), 1.0, exp(-0.01 * 0.01 * PosY * PosY));
            
            vec4 Col = vec4(1.0, 1.0, 1.0, 0.5) * max(0.0, 1.0 - 5.0 * Shape * abs(1.0 - pow(Rho * Shape, 2.0))) * Shape;
            Col *= a;
            Col *= max(0.0, 1.0 - 1.0 * exp(-0.0001 * PosY / InterRadius * PosY / InterRadius));
            Col *= exp(-4.0 / (2.0) * PosR / OuterRadius * PosR / OuterRadius);
            Col *= 0.5;
            AccumColor += Col;
        }

        float Wid = abs(PosY);
        if (Rho < 1.3 * InterRadius + 0.25 * Wid && Rho > 0.7 * InterRadius + 0.15 * Wid && PosR < 30.0 * InterRadius)
        {
            InJet = true;
            float InnerTheta = 2.0 * GetKeplerianAngularVelocity(InterRadius, 1.0, Spin,Q) * (iBlackHoleTime - 1.0 / 0.8 * abs(PosY));
            float Shape = 1.0 / (InterRadius + 0.2 * Wid);
            float Twist = 0.2 * (1.1 - exp(-0.1 * 0.1 * PosY * PosY)) * (PerlinNoise1D(0.35 * (iBlackHoleTime - 1.0 / 0.8 * abs(PosY)) / (1.0 / 0.8)) - 0.5);
            vec2 TwistedPos = SamplePos.xz + Twist * vec2(cos(0.666666 * InnerTheta), -sin(0.666666 * InnerTheta));
            
            vec4 Col = vec4(1.0, 1.0, 1.0, 0.5) * max(0.0, 1.0 - 2.0 * abs(1.0 - pow(length(TwistedPos) * Shape, 2.0))) * Shape;
            Col *= 1.0 - exp(-PosY / InterRadius * PosY / InterRadius);
            Col *= exp(-0.005 * PosY / InterRadius * PosY / InterRadius);
            Col *= 0.5;
            AccumColor += Col;
        }

        if (InJet)
        {
            float JetTemperature = 100000.0;
            AccumColor.xyz *= KelvinToRgb(JetTemperature);

            float RelativeVelocity = -(DirVec.y) * sqrt(1.0 / max(0.1, PosR)) * sign(PosY);
            float Dopler = sqrt((1.0 + RelativeVelocity) / (1.0 - RelativeVelocity));
            
            // Gtt 包含 Q
            float GttEmit = GetGtt(SamplePos, Spin, Q, 1.0);
            float GravitationalRedshift = sqrt(abs(GttEmit) / max(1e-6, abs(GttObs)));
            float RedShift = Dopler * GravitationalRedshift;

            AccumColor.xyz *= min(pow(RedShift, JetRedShiftIntensityExponent), JetShiftMax);
            AccumColor *= dt * JetBrightmut * (0.5 + 0.5 * tanh(log(AccretionRate) + 1.0));
            AccumColor.a *= 0.0; 

            float aR = 1.0 + JetReddening * (1.0 - 1.0);
            float aG = 1.0 + JetReddening * (2.5 - 1.0);
            float aB = 1.0 + JetReddening * (4.5 - 1.0);
            float Sum_rgb = (AccumColor.r + AccumColor.g + AccumColor.b) * pow(1.0 - BaseColor.a, aG);
            
            float r001 = 0.0; float g001 = 0.0; float b001 = 0.0;
            float Denominator = AccumColor.r * pow(1.0 - BaseColor.a, aR) + AccumColor.g * pow(1.0 - BaseColor.a, aG) + AccumColor.b * pow(1.0 - BaseColor.a, aB);
            if (Denominator > 0.000001)
            {
                r001 = Sum_rgb * AccumColor.r * pow(1.0 - BaseColor.a, aR) / Denominator;
                g001 = Sum_rgb * AccumColor.g * pow(1.0 - BaseColor.a, aG) / Denominator;
                b001 = Sum_rgb * AccumColor.b * pow(1.0 - BaseColor.a, aB) / Denominator;
                
                float invSum = 3.0 / (r001 + g001 + b001);
                r001 *= pow(r001 * invSum, JetSaturation);
                g001 *= pow(g001 * invSum, JetSaturation);
                b001 *= pow(b001 * invSum, JetSaturation);
            }
            
            CurrentResult.r += r001;
            CurrentResult.g += g001;
            CurrentResult.b += b001;
        }

        TraveledDist += dt;
    }

    return CurrentResult;
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
    
    // [物理参数计算]
    float PhysicalSpinA = iSpin * CONST_M;
    float PhysicalQ = iQ * CONST_M; // 计算物理电荷量 Q (假设 iQ 是 Q/M)
    
    // 视界半径计算 (Kerr-Newman): r+ = M + sqrt(M^2 - a^2 - Q^2)
    // 假设 CONST_M = M = 0.5, 则 M^2 = 0.25
    float HorizonDiscrim = 0.25 - PhysicalSpinA * PhysicalSpinA - PhysicalQ * PhysicalQ;
    float EventHorizonR = 0.5 + sqrt(max(0.0, HorizonDiscrim));
    
    // 裸奇点判断: a^2 + Q^2 > M^2
    bool bIsNakedSingularity = HorizonDiscrim < 0.0;
    
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
    
    // 计算相机处的红移
    vec4 CameraPosLocal = vec4(RayPosWorld,iUniverseSign); 
    float CamGtt = GetGtt(CameraPosLocal.xyz, PhysicalSpinA, PhysicalQ, CameraPosLocal.w);
    
    float BackgroundShift = sqrt(1.0 / max(1e-6, abs(CamGtt)));
    BackgroundShift = clamp(BackgroundShift, 1.0/BackgroundShiftMax, BackgroundShiftMax);
    
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
        // 传入 PhysicalQ
        mat4 g_cov = GetKerrMetric(X, PhysicalSpinA, PhysicalQ, GravityFade, CurrentUniverseSign); 
        float g_tt = g_cov[3][3];
        if (g_tt < 0) {
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
        // 使用更新后的 EventHorizonR
        if (CurrentUniverseSign > 0.0 && CurrentKerrR < TerminationR && !bIsNakedSingularity) { bShouldContinueMarchRay = false; bWaitCalBack = false; break; }
        
        if ((Count > 150 && !bIsNakedSingularity) || (Count > 450 && bIsNakedSingularity)) { 
            bShouldContinueMarchRay = false; bWaitCalBack = true; break; 
        }
        float rho = length(RayPos.xz);
        float DistRing = sqrt(RayPos.y * RayPos.y + pow(rho - abs(PhysicalSpinA), 2.0));
        
        float StepFactor;
        if (bIsNakedSingularity) {
            float GeoFactor = smoothstep(0.0, 0.6, DistRing);
            float RFactor   = smoothstep(0.0, 0.8, abs(CurrentKerrR)); 
            float Safety = min(GeoFactor, RFactor);
            float BaseSpeed = mix(0.1, 0.5, pow(Safety, 0.6));
            StepFactor = BaseSpeed;
        } else 
        {
            StepFactor = 0.5;
        }
        float RayStep = StepFactor * DistRing;

        if (Count == 0) RayStep *= RandomStep(FragUv, fract(iTime));
        RayStep = max(RayStep, 0.001); 
      
        LastPos = X.xyz; 
        
        GravityFade = CubicInterpolate(max(min(1.0 - (0.01 * DistanceToBlackHole - 1.0) / 4.0, 1.0), 0.0));

        // 传入 PhysicalQ
        mat4 g_inv = GetInverseKerrMetric(X, PhysicalSpinA, PhysicalQ, GravityFade, CurrentUniverseSign);
        vec4 V_contra = g_inv * P_cov;
        
        float V_mag = length(V_contra.xyz); 
        float dLambda_Total = RayStep / max(V_mag, 1e-6);
        StepGeodesicRK4(X, P_cov, dLambda_Total, PhysicalSpinA, PhysicalQ, GravityFade, CurrentUniverseSign);
        ApplyHamiltonianCorrection(P_cov, X, PhysicalSpinA, PhysicalQ, GravityFade, CurrentUniverseSign);
        
        RayPos = X.xyz;
        vec3 StepVec = RayPos - LastPos;
        float ActualStepLength = length(StepVec);
        RayDir = (ActualStepLength > 1e-7) ? StepVec / ActualStepLength : LastDir;
        
        if (LastPos.y * RayPos.y < 0.0) {
            float t_cross = LastPos.y / (LastPos.y - RayPos.y);
            float rho_cross = length(mix(LastPos.xz, RayPos.xz, t_cross));
            if (rho_cross < abs(PhysicalSpinA)) CurrentUniverseSign *= -1.0;
        }
        
        // 传递 PhysicalQ 到吸积盘/喷流渲染函数
        if (CurrentUniverseSign > 0.0) {
            Result = DiskColor(Result, ActualStepLength, RayPos, LastPos, RayDir, LastDir,
                              iInterRadiusRs, iOuterRadiusRs, iThinRs, iHopper, iBrightmut, iDarkmut, iReddening, iSaturation, DiskArgument, 
                              iBlackbodyIntensityExponent, iRedShiftColorExponent, iRedShiftIntensityExponent, PeakTemperature, ShiftMax, clamp(PhysicalSpinA,-0.49,0.49),
                              PhysicalQ, CameraPosLocal); 
            
            Result = JetColor(Result, ActualStepLength, RayPos, LastPos, RayDir, LastDir,
                              iInterRadiusRs, iOuterRadiusRs, iJetRedShiftIntensityExponent, iJetBrightmut, iReddening, iJetSaturation, iAccretionRate, iJetShiftMax, clamp(PhysicalSpinA,-0.049,0.049),
                              PhysicalQ, CameraPosLocal); 
        }
        
        if (Result.a > 0.99) { bShouldContinueMarchRay = false; bWaitCalBack = false; break; }
        
        LastDir = RayDir;
        Count++;
    }
    
    // --- 6. 背景采样 ---
    if (bWaitCalBack)
    {
        vec3 EscapeDirWorld = LocalToWorldRot * normalize(RayDir); 
        vec4 Backcolor = textureLod(iBackground, EscapeDirWorld, min(1.0, textureQueryLod(iBackground, EscapeDirWorld).x));
        if (CurrentUniverseSign < 0.0) Backcolor = textureLod(iAntiground, EscapeDirWorld, min(1.0, textureQueryLod(iAntiground, EscapeDirWorld).x));
        
        vec4 TexColor = Backcolor;
        if (length(iBlackHoleRelativePosRs.xyz) < 200.0) {
            vec3 Rcolor = Backcolor.r * 1.0 * WavelengthToRgb(max(453.0, 645.0 / BackgroundShift));
            vec3 Gcolor = Backcolor.g * 1.5 * WavelengthToRgb(max(416.0, 510.0 / BackgroundShift));
            vec3 Bcolor = Backcolor.b * 0.6 * WavelengthToRgb(max(380.0, 440.0 / BackgroundShift));
            vec3 Scolor = Rcolor + Gcolor + Bcolor;
            float OStrength = 0.3 * Backcolor.r + 0.6 * Backcolor.g + 0.1 * Backcolor.b;
            float RStrength = 0.3 * Scolor.r + 0.6 * Scolor.g + 0.1 * Scolor.b;
            Scolor *= OStrength / max(RStrength, 0.001);
            TexColor = vec4(Scolor, Backcolor.a) * pow(BackgroundShift, 4.0);
        }
        Result += 0.9999 * TexColor * (1.0 - Result.a);
    }
    // --- 7. 色调映射与 TAA ---
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
