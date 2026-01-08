#version 450
#pragma shader_stage(fragment)
#extension GL_EXT_samplerless_texture_functions : enable
#include "Common/CoordConverter.glsl"
#include "Common/NumericConstants.glsl"

// =============================================================================
// SECTION 1: 输入输出与 Uniform 定义
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
    int   iObserverMode; // 0 = Static, 1 = Falling
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
// SECTION 2: 基础工具函数 (噪声、插值、随机)
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

float Vec2ToTheta(vec2 v1, vec2 v2)
{
    float VecDot   = dot(v1, v2);
    float VecCross = v1.x * v2.y - v1.y * v2.x;
    float Angle    = asin(0.999999 * VecCross / (length(v1) * length(v2)));
    float Dx = step(0.0, VecDot);
    float Cx = step(0.0, VecCross);
    return mix(mix(-kPi - Angle, kPi - Angle, Cx), Angle, Dx);
}

float Shape(float x, float Alpha, float Beta)
{
    float k = pow(Alpha + Beta, Alpha + Beta) / (pow(Alpha, Alpha) * pow(Beta, Beta));
    return k * pow(x, Alpha) * pow(1.0 - x, Beta);
}

// =============================================================================
// SECTION 3: 颜色与光谱函数
// =============================================================================

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

// =============================================================================
// SECTION 4: 广义相对论度规与物理核心 (Kerr-Newman)
// =============================================================================

const float CONST_M = 0.5; 
const mat4  MINKOWSKI_METRIC = mat4(
    1, 0, 0, 0,
    0, 1, 0, 0,
    0, 0, 1, 0,
    0, 0, 0, -1
);

// 开普勒角速度 (Kerr-Newman)
float GetKeplerianAngularVelocity(float Radius, float Rs, float Spin, float Q) 
{
    float M = 0.5 * Rs; 
    // Omega = sqrt(Mr - Q^2) / (r^2 + a * sqrt(Mr - Q^2))
    float Mr_minus_Q2 = M * Radius - Q * Q;
    
    if (Mr_minus_Q2 < 0.0) return 0.0;
    
    float sqrt_Term = sqrt(Mr_minus_Q2);
    float denominator = Radius * Radius + Spin * sqrt_Term;
    
    return sqrt_Term / max(1e-6, denominator);
}

// Kerr-Schild 几何半径
float KerrSchildRadius(vec3 p, float a, float r_sign) {
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

// 逆度规 (Inverse Kerr-Newman)
mat4 GetInverseKerrMetric(vec4 X, float a, float Q, float fade, float r_sign) {
    if (a == 0.0) {
        float r = length(X.xyz);
        if (r < 1e-6) return MINKOWSKI_METRIC;

        vec4 l_up = vec4(X.xyz / r, -1.0);
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
    
    // Kerr-Newman-Schild Scalar: f = (2Mr^3 - Q^2 r^2) / (r^4 + a^2 z^2)
    float numerator = 2.0 * CONST_M * r3 - Q * Q * r2;
    float denominator = r2 * r2 + a2 * X.y * X.y;
    float f = (numerator / denominator) * fade;
    
    return MINKOWSKI_METRIC - f * outerProduct(l_up, l_up);
}

// 协变度规 (Kerr-Newman Metric Tensor)
mat4 GetKerrMetric(vec4 X, float a, float Q, float fade, float r_sign) {
    if (a == 0.0) {
        float r = length(X.xyz);
        if (r < 1e-6) return MINKOWSKI_METRIC;

        vec4 l_down = vec4(X.xyz / r, 1.0); 
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
    
    float numerator = 2.0 * CONST_M * r3 - Q * Q * r2;
    float denominator = r2 * r2 + a2 * X.y * X.y;
    float f = (numerator / denominator) * fade;
    
    return MINKOWSKI_METRIC + f * outerProduct(l_down, l_down);
}

// 哈密顿量 (用于测地线方程)
float Hamiltonian(vec4 X, vec4 P, float a, float Q, float fade, float r_sign) {
    if (a == 0.0) {
        float r = length(X.xyz);
        if (r < 1e-6) return 0.0; 

        float P_sq_minkowski = dot(P.xyz, P.xyz) - P.w * P.w;
        float l_dot_P_euclid = dot(X.xyz, P.xyz) / r - P.w;
        float f = (2.0 * CONST_M / r - (Q * Q) / (r * r)) * fade;

        return 0.5 * (P_sq_minkowski - f * l_dot_P_euclid * l_dot_P_euclid);
    }

    mat4 g_inv = GetInverseKerrMetric(X, a, Q, fade, r_sign);
    return 0.5 * dot(g_inv * P, P);
}

// 哈密顿量梯度 (用于运动方程)
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

// Gtt 分量 (用于引力红移估计)
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

// 获取观测者 4-速度 (Static or Falling)
vec4 GetObserver4Velocity(vec4 Pos, float a, float Q, float r_sign, int mode) {
    // 1. 计算基础几何参数（保持与度规函数一致）
    float r = KerrSchildRadius(Pos.xyz, a, r_sign); 
    float r2 = r * r;
    float r3 = r2 * r;
    float a2 = a * a;
    float denom = 1.0 / (r2 + a2);

    // 计算 Kerr-Schild 零矢量 l_mu 的空间分量 (注意 y 是旋转轴)
    float lx = (r * Pos.x + a * Pos.z) * denom;
    float ly = Pos.y / r;
    float lz = (r * Pos.z - a * Pos.x) * denom;
    // 根据度规函数定义，l_t = 1.0

    // 2. 计算标量函数 f (包含电荷 Q 和 fade)
    float numerator = 2.0 * CONST_M * r3 - Q * Q * r2;
    float denominator = r2 * r2 + a2 * Pos.y * Pos.y;
    float f = (numerator / denominator);
    // 这里的 f 必须考虑外部传入的变量，如果度规使用了 fade，这里也必须同步
    // 假设在视界附近计算物理观测者，我们取 f = f_raw * fade;

    vec4 U = vec4(0.0);

    if (mode == 0) {
        /* 
           静态观者 (Static Observer):

        */
        if (f < 1.0) { // 仅存在于静止极限（能层）之外
            float ut = 1.0 / sqrt(1.0 - f);
            U = vec4(0.0, 0.0, 0.0, ut);
        } else {
            // 能层内部不存在静态观者（g_tt >= 0），返回零向量或进行错误处理
            U = vec4(0.0); 
        }
    } 
    else if (mode == 1) {
        /* 
           落体观者 (Falling/Rain Observer):

        */
        float norm = sqrt(1.0 + f);
        U.x = -f * lx / norm;
        U.y = -f * ly / norm;
        U.z = -f * lz / norm;
        U.w = (1.0 + f) / norm;
    }

    return U;
}


// 频率比计算 (Redshift/Blueshift factor)
float CalculateFrequencyRatio(vec4 P_photon, vec4 U_emitter, vec4 U_observer, mat4 g_cov)
{
    vec4 P_cov = g_cov * P_photon;
    float E_emit = -dot(P_cov, U_emitter);
    float E_obs  = -dot(P_cov, U_observer);
    return E_emit / max(1e-6, E_obs);
}

// =============================================================================
// SECTION 5: 数值积分器 (RK4)
// =============================================================================

struct State {
    vec4 X;
    vec4 P;
};

// 修正动量以保证零测地线性质 (光子 P.P=0)
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

State GetDerivatives(State S, float a, float Q, float fade, float r_sign) {
    State deriv;
    deriv.X = GetInverseKerrMetric(S.X, a, Q, fade, r_sign) * S.P;
    deriv.P = -GradHamiltonian(S.X, S.P, a, Q, fade, r_sign);
    return deriv;
}

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

// =============================================================================
// SECTION 6: 体积渲染函数 (吸积盘与喷流)
// =============================================================================

vec4 DiskColor(vec4 BaseColor, float StepLength, vec3 RayPos, vec3 LastRayPos,
               vec3 RayDir, vec3 LastRayDir,
               float InterRadius, float OuterRadius, float Thin, float Hopper, float Brightmut, float Darkmut, float Reddening, float Saturation, float DiskTemperatureArgument,
               float BlackbodyIntensityExponent, float RedShiftColorExponent, float RedShiftIntensityExponent,
               float PeakTemperature, float ShiftMax, float Spin, float Q, 
               vec4 iP_cov, float iE_obs) 
{
    vec4 CurrentResult = BaseColor;
    vec3 StartPos = LastRayPos;
    vec3 DirVec   = RayDir; 
    float TotalDist = StepLength;
    float TraveledDist = 0.0;
    
    // --- 包围盒剔除 ---
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

    int MaxSubSteps = 64; 
    
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
        vec3 SamplePos;

        if (CurrentPos.y * (CurrentPos + DirVec * dt).y < 0.0) 
        {
            float t_cross = CurrentPos.y / (CurrentPos.y - (CurrentPos + DirVec * dt).y);
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
        
        if (abs(PosY) < SampleThin * 1.2 && PosR < OuterRadius && PosR > InterRadius)
        {
             float NoiseLevel = max(0.0, 2.0 - 0.6 * SampleThin);
             float x = (PosR - InterRadius) / (OuterRadius - InterRadius);
             float a_param = max(1.0, (OuterRadius - InterRadius) / 10.0);
             float EffectiveRadius = (-1.0 + sqrt(1.0 + 4.0 * a_param * a_param * x - 4.0 * x * a_param)) / (2.0 * a_param - 2.0);
             if(a_param == 1.0) EffectiveRadius = x;
             float InterCloudEffectiveRadius = (PosR - InterRadius) / min(OuterRadius - InterRadius, 12.0);
             float DenAndThiFactor = Shape(EffectiveRadius, 0.9, 1.5);

             if ((abs(PosY) < SampleThin * DenAndThiFactor) || (PosY < SampleThin * (1.0 - 5.0 * pow(InterCloudEffectiveRadius, 2.0))))
             {
                 float AngularVelocity = GetKeplerianAngularVelocity(PosR, 1.0, Spin, Q);
                 
                 // --- 螺旋与噪声计算 ---
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
                 float PosTheta = Vec2ToTheta(SamplePos.zx, vec2(cos(-SpiralTheta), sin(-SpiralTheta)));
                 float PosLogarithmicTheta = Vec2ToTheta(SamplePos.zx, vec2(cos(-2.0 * log(PosR)), sin(-2.0 * log(PosR))));
                 
                 // --- 物理红移计算 ---
                 vec3 FluidVel = AngularVelocity * vec3(SamplePos.z, 0.0, -SamplePos.x);
                 vec4 U_fluid_unnorm = vec4(FluidVel, 1.0); 
                 mat4 g_cov_sample = GetKerrMetric(vec4(SamplePos, 0.0), Spin, Q, 1.0, 1.0);
                 float norm_sq = dot(U_fluid_unnorm, g_cov_sample * U_fluid_unnorm);
                 vec4 U_fluid = U_fluid_unnorm * inversesqrt(max(1e-6, abs(norm_sq)));
                 
                 float E_emit = -dot(iP_cov, U_fluid);
                 float FreqRatio = iE_obs / max(1e-6, E_emit);
                 
                 // --- 颜色合成 ---
                 float DiskTemperature = pow(DiskTemperatureArgument * pow(1.0 / PosR, 3.0) * max(1.0 - sqrt(InterRadius / PosR), 0.000001), 0.25);
                 float VisionTemperature = DiskTemperature * pow(FreqRatio, RedShiftColorExponent); 
                 float BrightWithoutRedshift = 0.05 * min(OuterRadius / (1000.0), 1000.0 / OuterRadius) + 0.55 / exp(5.0 * EffectiveRadius) * mix(0.2 + 0.8 * abs(DirVec.y), 1.0, clamp(Thin - 0.8, 0.2, 1.0)); 
                 BrightWithoutRedshift *= pow(DiskTemperature / PeakTemperature, BlackbodyIntensityExponent); 
                 
                 float RotPosR = PosR + 0.25 / 3.0 * iBlackHoleTime;
                 float Density = DenAndThiFactor;
                 float Levelmut = 0.91 * log(1.0 + (0.06 / 0.91 * max(0.0, min(1000.0, PosR) - 10.0)));
                 float Conmut = 80.0 * log(1.0 + (0.1 * 0.06 * max(0.0, min(1000000.0, PosR) - 10.0)));
                 
                 vec4 SampleColor = vec4(GenerateAccretionDiskNoise(vec3(0.1 * RotPosR, 0.1 * PosY, 0.02 * pow(OuterRadius, 0.7) * PosTheta), NoiseLevel + 2.0 - Levelmut, NoiseLevel + 4.0 - Levelmut, 80.0 - Conmut)); 
                 
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

                 float VerticalMixFactor = max(0.0, (1.0 - abs(PosY) / (SampleThin * Density * 1.4))); 
                 Density *= 0.7 * VerticalMixFactor * Density;
                 SampleColor.xyz *= Density * 1.4;
                 SampleColor.a *= (Density) * (Density) / 0.3;
                 
                 SampleColor.xyz *= BrightWithoutRedshift * KelvinToRgb(VisionTemperature); 
                 SampleColor.xyz *= min(pow(FreqRatio, RedShiftIntensityExponent), ShiftMax); 
                 SampleColor.xyz *= min(1.0, 1.8 * (OuterRadius - PosR) / (OuterRadius - InterRadius)); 
                 SampleColor.a *= 0.125;
                 
                 vec4 StepColor = SampleColor * dt;
                 
                 float aR = 1.0 + Reddening * (1.0 - 1.0);
                 float aG = 1.0 + Reddening * (3.0 - 1.0);
                 float aB = 1.0 + Reddening * (6.0 - 1.0);
                 float Sum_rgb = (StepColor.r + StepColor.g + StepColor.b) * pow(1.0 - CurrentResult.a, aG);
                 
                 float r001 = 0.0; float g001 = 0.0; float b001 = 0.0;
                 float Denominator = StepColor.r * pow(1.0 - CurrentResult.a, aR) + StepColor.g * pow(1.0 - CurrentResult.a, aG) + StepColor.b * pow(1.0 - CurrentResult.a, aB);
                 
                 if (Denominator > 1e-6)
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
              float InterRadius, float OuterRadius, float JetRedShiftIntensityExponent, float JetBrightmut, float JetReddening, float JetSaturation, float AccretionRate, float JetShiftMax, float Spin, float Q, 
              vec4 iP_cov, float iE_obs) 
{
    vec4 CurrentResult = BaseColor;
    vec3 StartPos = LastRayPos;
    vec3 DirVec   = RayDir; 
    float TotalDist = StepLength;
    float TraveledDist = 0.0;
    
    float R_Start = length(StartPos.xz);
    float R_End   = length(RayPos.xz);
    float MaxR_XZ = max(R_Start, R_End);
    float MaxY    = max(abs(StartPos.y), abs(RayPos.y));
    
    if (MaxR_XZ > OuterRadius * 1.5 && MaxY < OuterRadius) return BaseColor;

    int MaxSubSteps = 64; 
    
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

        // --- 几何形状 (Jet Funnel) ---
        float JetCoreShape = 0.0;
        
        if (RhoSq < 2.0 * InterRadius * InterRadius + 0.03 * 0.03 * PosY * PosY && PosR < sqrt(2.0) * OuterRadius)
        {
            InJet = true;
            float InnerTheta = 3.0 * GetKeplerianAngularVelocity(InterRadius, 1.0, Spin, Q) * (iBlackHoleTime - 1.0 / 0.8 * abs(PosY));
            float Shape = 1.0 / sqrt(InterRadius * InterRadius + 0.02 * 0.02 * PosY * PosY);
            float noiseInput = 0.3 * (iBlackHoleTime - 1.0 / 0.8 * abs(abs(PosY) + 100.0 * (RhoSq / max(0.1, PosR)))) / (OuterRadius / 100.0) / (1.0 / 0.8);
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
            float InnerTheta = 2.0 * GetKeplerianAngularVelocity(InterRadius, 1.0, Spin, Q) * (iBlackHoleTime - 1.0 / 0.8 * abs(PosY));
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
            float JetVelocityMag = 0.9; 
            vec3  JetVelDir = vec3(0.0, sign(PosY), 0.0);
            vec3 RotVelDir = normalize(vec3(SamplePos.z, 0.0, -SamplePos.x));
            vec3 FinalSpatialVel = JetVelDir * JetVelocityMag + RotVelDir * 0.05; 
            
            vec4 U_jet_unnorm = vec4(FinalSpatialVel, 1.0);
            mat4 g_cov_sample = GetKerrMetric(vec4(SamplePos, 0.0), Spin, Q, 1.0, 1.0);
            float norm_sq = dot(U_jet_unnorm, g_cov_sample * U_jet_unnorm);
            vec4 U_jet = U_jet_unnorm * inversesqrt(max(1e-6, abs(norm_sq)));
            
            float E_emit = -dot(iP_cov, U_jet);
            float FreqRatio = iE_obs / max(1e-6, E_emit);

            float JetTemperature = 100000.0 * FreqRatio; 
            AccumColor.xyz *= KelvinToRgb(JetTemperature);
            AccumColor.xyz *= min(pow(FreqRatio, JetRedShiftIntensityExponent), JetShiftMax);
            
            AccumColor *= dt * JetBrightmut * (0.5 + 0.5 * tanh(log(AccretionRate) + 1.0));
            AccumColor.a *= 0.0; 

            float aR = 1.0 + JetReddening * (1.0 - 1.0);
            float aG = 1.0 + JetReddening * (2.5 - 1.0);
            float aB = 1.0 + JetReddening * (4.5 - 1.0);
            float Sum_rgb = (AccumColor.r + AccumColor.g + AccumColor.b) * pow(1.0 - BaseColor.a, aG);
            
            float r001 = 0.0; float g001 = 0.0; float b001 = 0.0;
            float Denominator = AccumColor.r * pow(1.0 - BaseColor.a, aR) + AccumColor.g * pow(1.0 - BaseColor.a, aG) + AccumColor.b * pow(1.0 - BaseColor.a, aB);
            if (Denominator > 1e-6)
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
// SECTION 7: 主函数 (Main)
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
    
    // [物理参数]
    float PhysicalSpinA = iSpin * CONST_M;
    float PhysicalQ = iQ * CONST_M;
    
    // 视界半径计算 (Kerr-Newman)
    float HorizonDiscrim = 0.25 - PhysicalSpinA * PhysicalSpinA - PhysicalQ * PhysicalQ;
    float EventHorizonR = 0.5 + sqrt(max(0.0, HorizonDiscrim));
    bool bIsNakedSingularity = HorizonDiscrim < 0.0;
    float TerminationR = EventHorizonR;
    float CurrentUniverseSign = iUniverseSign;
    float ShiftMax = iJetShiftMax; 

    // --- 1. 相机光线生成 (几何部分) ---
    vec2 Jitter = vec2(RandomStep(FragUv, fract(iTime * 1.0 + 0.5)), RandomStep(FragUv, fract(iTime * 1.0))) / iResolution;
    vec3 ViewDirLocal = FragUvToDir(FragUv + 0.5 * Jitter, Fov, iResolution);
    
    // 计算世界空间中的相机位置
    vec3 CamToBHVecVisual = (iInverseCamRot * vec4(iBlackHoleRelativePosRs.xyz, 0.0)).xyz;
    vec3 RayPosWorld = -CamToBHVecVisual; 
    
    // 计算吸积盘坐标系旋转矩阵
    vec3 DiskNormalWorld = normalize((iInverseCamRot * vec4(iBlackHoleRelativeDiskNormal.xyz, 0.0)).xyz);
    vec3 DiskTangentWorld = normalize((iInverseCamRot * vec4(iBlackHoleRelativeDiskTangen.xyz, 0.0)).xyz);
    vec3 BH_Y = normalize(DiskNormalWorld);             
    vec3 BH_X = normalize(DiskTangentWorld);            
    BH_X = normalize(BH_X - dot(BH_X, BH_Y) * BH_Y);
    vec3 BH_Z = normalize(cross(BH_X, BH_Y));           
    mat3 LocalToWorldRot = mat3(BH_X, BH_Y, BH_Z);
    mat3 WorldToLocalRot = transpose(LocalToWorldRot);
    
    // 转换到黑洞局部坐标系 (Kerr-Schild 坐标系的几何方向)
    vec3 RayPosLocal = WorldToLocalRot * RayPosWorld;
    vec3 RayDirWorld_Geo = WorldToLocalRot * normalize((iInverseCamRot * vec4(ViewDirLocal, 0.0)).xyz);

    // --- 2. 优化: 包围球快进 ---
    float DistanceToBlackHole = length(RayPosLocal);
    bool bShouldContinueMarchRay = true;
    bool bWaitCalBack = false;
    
    if (DistanceToBlackHole > RaymarchingBoundary) 
    {
        vec3 O = RayPosLocal; vec3 D = RayDirWorld_Geo; float r = RaymarchingBoundary - 1.0; 
        float b = dot(O, D); float c = dot(O, O) - r * r; float delta = b * b - c; 
        if (delta < 0.0) { bShouldContinueMarchRay = false; bWaitCalBack = true; } 
        else {
            float tEnter = -b - sqrt(delta); 
            if (tEnter > 0.0) RayPosLocal = O + D * tEnter;
            else if (-b + sqrt(delta) <= 0.0) { bShouldContinueMarchRay = false; bWaitCalBack = true; }
        }
    }

    // --- 3. 物理动量初始化 (Physics Initialization) ---
    // 基于观测者 4-速度构建局域标架
// --- 3. 物理动量初始化 (Physics Initialization) ---
    // [修正版] 采用直接投影法，消除旋转摇摆，保证各向同性
    
    vec4 X = vec4(RayPosLocal, 0.0); // 初始时空坐标 (t=0)
    vec4 P_cov = vec4(0.0);          // 协变动量
    float E_obs_camera = 1.0;        // 归一化观测能量
    vec3 RayDir = RayDirWorld_Geo;   // 用于后续近似
    vec3 LastDir = RayDir;
    vec3 LastPos = RayPosLocal;

    if (bShouldContinueMarchRay) 
    {
        float GravityFade = CubicInterpolate(max(min(1.0 - (0.01 * length(RayPosLocal) - 1.0) / 4.0, 1.0), 0.0));
        
        // A. 获取度规张量
        mat4 g_cov = GetKerrMetric(X, PhysicalSpinA, PhysicalQ, GravityFade, CurrentUniverseSign);
        
        // B. 获取观测者 4-速度 U (逆变)
        vec4 U_obs = GetObserver4Velocity(X, PhysicalSpinA, PhysicalQ, CurrentUniverseSign, iObserverMode);
        
        mat3 CamToBHSpace = WorldToLocalRot * mat3(iInverseCamRot);
        // 2. 提取相机的原始基向量 (假设平直空间时的基底)
        // 对应 iInverseCamRot 的第 0, 1, 2 列
        vec3 vRight_Raw = CamToBHSpace * vec3(1.0, 0.0, 0.0);
        vec3 vUp_Raw    = CamToBHSpace * vec3(0.0, 1.0, 0.0);
        vec3 vFwd_Raw   = CamToBHSpace * vec3(0.0, 0.0, 1.0);
        // 3. 定义度规点积宏 (方便计算 g_uv * A * B)
        #define GDot(v1, v2) dot(v1, g_cov * v2)
        // 4. Gram-Schmidt 正交化过程
        // 目标：构建一组物理基底 {e1, e2, e3}，满足 e_i 垂直于 U，且 e_i 之间正交归一
        
        // --- 构建 Forward 基底 (e3) ---
        vec4 e3 = vec4(vFwd_Raw, 0.0);
        // 投影到 U 的正交补空间: v_perp = v - (v.u)/(u.u) * u
        // 因为 U是类时向量 u.u = -1, 所以 v_perp = v + (v.u)*u
        e3 += U_obs * GDot(e3, U_obs); 
        // 归一化 (防止除零)
        e3 *= inversesqrt(max(1e-20, GDot(e3, e3)));
        // --- 构建 Up 基底 (e2) ---
        vec4 e2 = vec4(vUp_Raw, 0.0);
        e2 += U_obs * GDot(e2, U_obs); // 去除时间分量
        e2 -= e3 * GDot(e2, e3);       // 去除 Forward 分量
        e2 *= inversesqrt(max(1e-20, GDot(e2, e2)));
        // --- 构建 Right 基底 (e1) ---
        vec4 e1 = vec4(vRight_Raw, 0.0);
        e1 += U_obs * GDot(e1, U_obs); // 去除时间分量
        e1 -= e3 * GDot(e1, e3);       // 去除 Forward 分量
        e1 -= e2 * GDot(e1, e2);       // 去除 Up 分量
        e1 *= inversesqrt(max(1e-20, GDot(e1, e1)));
        
        #undef GDot
        // 5. 根据 main() 中计算的 ViewDirLocal 合成物理空间动量 S
        // ViewDirLocal 包含了视场角(FOV)和屏幕坐标信息，它是相对于相机基底的权重
        vec4 S_contra = ViewDirLocal.x * e1 + ViewDirLocal.y * e2 + ViewDirLocal.z * e3;
        // 6. 组合最终 4-动量 (逆变)
        // P = E * (U + S)，这里 E 归一化为 1
        vec4 P_contra = U_obs + S_contra;
        // D. 转换为协变动量
        P_cov = g_cov * P_contra;
        // E. 最终数值修正 (保证 P.P = 0)
        ApplyHamiltonianCorrection(P_cov, X, PhysicalSpinA, PhysicalQ, GravityFade, CurrentUniverseSign);
        // F. 记录观测能量
        E_obs_camera = -dot(P_cov, U_obs);
    }
    P_cov = vec4(RayDir,-1.0);
    vec3 RayPos = X.xyz;
    LastPos = RayPos;
    int Count = 0;
    float CameraStartR = KerrSchildRadius(RayPos, PhysicalSpinA, CurrentUniverseSign);
 // 默认不阻挡 (-1.0)
    TerminationR = -1.0; 
    // 仅在黑洞（非裸奇点）且位于正宇宙时应用视界阻挡逻辑
    if (CurrentUniverseSign > 0.0 && !bIsNakedSingularity) 
    {
        if (iObserverMode == 0) // 0 = Static Observer (静态观者)
        {
            // 计算静态极限/无限红移面 (Infinite Redshift Surfaces / Static Limits)
            // r_S = M +/- sqrt(M^2 - Q^2 - a^2 * cos^2(theta))
            // 在 Kerr-Schild 坐标系中 (y轴为旋转轴): cos^2(theta) = y^2 / r^2
            float CosThetaSq = (RayPos.y * RayPos.y) / (CameraStartR * CameraStartR + 1e-10);
            
            // 判别式 D_S = M^2 - Q^2 - a^2 * cos^2(theta)
            // 注意: CONST_M = 0.5
            float SL_Discrim = 0.25 - PhysicalQ * PhysicalQ - PhysicalSpinA * PhysicalSpinA * CosThetaSq;
            
            // 如果判别式<0 (极其罕见的高自旋/高电荷特定角度)，意味着静界不存在，退化为视界逻辑
            float SL_Outer = 0.5 + sqrt(max(0.0, SL_Discrim)); // 外无限红移面 (Ergosphere Outer Boundary)
            float SL_Inner = 0.5 - sqrt(max(0.0, SL_Discrim)); // 内无限红移面
            if (CameraStartR > SL_Outer) 
            {
                // 1. 静态观者位于静界外 (正常黑洞外观)
                // 判定条件：光进入外视界 (Event Horizon) 即停止
                TerminationR = EventHorizonR; 
            } 
            else if (CameraStartR > SL_Inner) 
            {
                // 2. 静态观者位于静界内 (Ergosphere) 但在内红移面外
                // 物理上静态观者无法存在于能层 (需超光速)，视为非法状态 -> 立刻终止渲染
                bShouldContinueMarchRay = false;
                bWaitCalBack = false;
                Result = vec4(0.0, 0.0, 0.0, 1.0); // 输出全黑或报错色
            } 
            else 
            {
                // 3. 静态观者位于内无限红移面内 (Ring/Wormhole Region)
                // 不判定奇点 (TerminationR = -1.0)，允许光穿过视界和环
                TerminationR = -1.0;
            }
        }
        else // 1 = Falling Observer (自由落体/雨滴观者)
        {
            // 计算内视界 (Inner Horizon / Cauchy Horizon)
            float InnerHorizonR = 0.5 - sqrt(max(0.0, HorizonDiscrim));
            if (CameraStartR > InnerHorizonR) 
            {
                // 1. 自由落体观者位于内视界外
                // 可以穿越外视界，但在接触柯西视界(内视界)时停止 (模拟柯西不稳定性或盲区)
                TerminationR = InnerHorizonR;
            } 
            else 
            {
                // 2. 自由落体观者位于内视界内
                // 不判定奇点，允许看到奇环或穿越虫洞
                TerminationR = -1.0;
            }
        }
    }
    

    // --- 4. 光线追踪主循环 ---
    while (bShouldContinueMarchRay)
    {
        float DistanceToBlackHole = length(RayPos);
            if (DistanceToBlackHole > RaymarchingBoundary) { bShouldContinueMarchRay = false; bWaitCalBack = true; break; }
        float CurrentKerrR = KerrSchildRadius(RayPos, PhysicalSpinA, CurrentUniverseSign);
        
        // 视界/奇点判定
       if (CurrentUniverseSign > 0.0 && CurrentKerrR < TerminationR && !bIsNakedSingularity) 
        { 
            bShouldContinueMarchRay = false; 
            bWaitCalBack = false; 
            break; 
        }
        if ((Count > 150 && !bIsNakedSingularity) || (Count > 450 && bIsNakedSingularity)) { 
            bShouldContinueMarchRay = false; bWaitCalBack = true; break; 
        }
        
        // 步长控制
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
        float GravityFade = CubicInterpolate(max(min(1.0 - (0.01 * DistanceToBlackHole - 1.0) / 4.0, 1.0), 0.0));
        
        // RK4 积分
        mat4 g_inv = GetInverseKerrMetric(X, PhysicalSpinA, PhysicalQ, GravityFade, CurrentUniverseSign);
        vec4 P_contra_step = g_inv * P_cov;
        float V_mag = length(P_contra_step.xyz); 
        float dLambda = RayStep / max(V_mag, 1e-6);
        
        StepGeodesicRK4(X, P_cov, dLambda, PhysicalSpinA, PhysicalQ, GravityFade, CurrentUniverseSign);
        ApplyHamiltonianCorrection(P_cov, X, PhysicalSpinA, PhysicalQ, GravityFade, CurrentUniverseSign);
        
        RayPos = X.xyz;
        vec3 StepVec = RayPos - LastPos;
        float ActualStepLength = length(StepVec);
        RayDir = (ActualStepLength > 1e-7) ? StepVec / ActualStepLength : LastDir;
        
        // 穿越环状奇点检测
        if (LastPos.y * RayPos.y < 0.0) {
            float t_cross = LastPos.y / (LastPos.y - RayPos.y);
            float rho_cross = length(mix(LastPos.xz, RayPos.xz, t_cross));
            if (rho_cross < abs(PhysicalSpinA)) CurrentUniverseSign *= -1.0;
        }
        
        // --- 渲染积累 (取消了原来的注释) ---
        //if (CurrentUniverseSign > 0.0) {
        //   Result = DiskColor(Result, ActualStepLength, RayPos, LastPos, RayDir, LastDir,
        //                     iInterRadiusRs, iOuterRadiusRs, iThinRs, iHopper, iBrightmut, iDarkmut, iReddening, iSaturation, DiskArgument, 
        //                     iBlackbodyIntensityExponent, iRedShiftColorExponent, iRedShiftIntensityExponent, PeakTemperature, ShiftMax, clamp(PhysicalSpinA,-0.49,0.49),
        //                     PhysicalQ, 
        //                     P_cov, E_obs_camera); 
        //   
        //   Result = JetColor(Result, ActualStepLength, RayPos, LastPos, RayDir, LastDir,
        //                     iInterRadiusRs, iOuterRadiusRs, iJetRedShiftIntensityExponent, iJetBrightmut, iReddening, iJetSaturation, iAccretionRate, iJetShiftMax, clamp(PhysicalSpinA,-0.049,0.049),
        //                     PhysicalQ, 
        //                     P_cov, E_obs_camera); 
        //}
        
        if (Result.a > 0.99) { bShouldContinueMarchRay = false; bWaitCalBack = false; break; }
        
        LastDir = RayDir;
        Count++;
    }
    
    // --- 5. 背景采样 (含红移) ---
    if (bWaitCalBack)
    {
        vec3 EscapeDirWorld = LocalToWorldRot * normalize(RayDir); 
        vec4 Backcolor = textureLod(iBackground, EscapeDirWorld, min(1.0, textureQueryLod(iBackground, EscapeDirWorld).x));
        if (CurrentUniverseSign < 0.0) Backcolor = textureLod(iAntiground, EscapeDirWorld, min(1.0, textureQueryLod(iAntiground, EscapeDirWorld).x));
        
        float BackgroundShift = max(1e-4, E_obs_camera); 
        BackgroundShift = clamp(BackgroundShift, 1.0/BackgroundShiftMax, BackgroundShiftMax);
        
        vec4 TexColor = Backcolor;
        vec3 Rcolor = Backcolor.r * 1.0 * WavelengthToRgb(max(453.0, 645.0 / BackgroundShift));
        vec3 Gcolor = Backcolor.g * 1.5 * WavelengthToRgb(max(416.0, 510.0 / BackgroundShift));
        vec3 Bcolor = Backcolor.b * 0.6 * WavelengthToRgb(max(380.0, 440.0 / BackgroundShift));
        vec3 Scolor = Rcolor + Gcolor + Bcolor;
        float OStrength = 0.3 * Backcolor.r + 0.6 * Backcolor.g + 0.1 * Backcolor.b;
        float RStrength = 0.3 * Scolor.r + 0.6 * Scolor.g + 0.1 * Scolor.b;
        Scolor *= OStrength / max(RStrength, 0.001);
        TexColor = vec4(Scolor, Backcolor.a) * pow(BackgroundShift, 4.0); 

        Result += 0.9999 * TexColor * (1.0 - Result.a);
    }
    
    // --- 6. 色调映射与 TAA ---
    float RedFactor   = 3.0 * Result.r / (Result.g + Result.b + Result.g );
    float BlueFactor  = 3.0 * Result.b / (Result.g + Result.b + Result.g );
    float GreenFactor = 3.0 * Result.g / (Result.g + Result.b + Result.g );
    float BloomMax    = 8.0;
    
    Result.r = min(-4.0 * log( 1.0 - pow(Result.r, 2.2)), BloomMax * RedFactor);
    Result.g = min(-4.0 * log( 1.0 - pow(Result.g, 2.2)), BloomMax * GreenFactor);
    Result.b = min(-4.0 * log( 1.0 - pow(Result.b, 2.2)), BloomMax * BlueFactor);
    Result.a = min(-4.0 * log( 1.0 - pow(Result.a, 2.2)), 4.0);
    
    float BlendWeight = iBlendWeight;
    vec4 PrevColor = texelFetch(iHistoryTex, ivec2(gl_FragCoord.xy), 0);
    FragColor      = (BlendWeight) * Result + (1.0 - BlendWeight) * PrevColor;
}