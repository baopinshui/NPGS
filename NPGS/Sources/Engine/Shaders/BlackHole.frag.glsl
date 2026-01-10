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
const float EPSILON = 1e-6;

// [OPTIMIZATION] 预定义单位矩阵对角线，避免构建完整矩阵
// 仅在极少数需要返回 mat4 的外部接口保留，内部计算不再依赖它
const mat4 MINKOWSKI_METRIC = mat4(
    1, 0, 0, 0,
    0, 1, 0, 0,
    0, 0, 1, 0,
    0, 0, 0, -1
);

// 开普勒角速度 (Kerr-Newman) - 保持不变，已足够精简
float GetKeplerianAngularVelocity(float Radius, float Rs, float Spin, float Q) 
{
    float M = 0.5 * Rs; 
    float Mr_minus_Q2 = M * Radius - Q * Q;
    if (Mr_minus_Q2 < 0.0) return 0.0;
    float sqrt_Term = sqrt(Mr_minus_Q2);
    float denominator = Radius * Radius + Spin * sqrt_Term;
    return sqrt_Term / max(EPSILON, denominator);
}

// [OPTIMIZATION] 内联优化的 KerrSchild 半径求解
// 移除了部分冗余分支，利用 r_sign 统一处理逻辑
float KerrSchildRadius(vec3 p, float a, float r_sign) {
    float r_sign_len = r_sign * length(p);
    if (a == 0.0) return r_sign_len; // 快速路径

    float a2 = a * a;
    float rho2 = dot(p.xz, p.xz); // x^2 + z^2
    float y2 = p.y * p.y;
    
    // b = x^2 + y^2 + z^2 - a^2
    float b = rho2 + y2 - a2;
    
    // 判别式 det = sqrt(b^2 + 4 a^2 y^2)
    float det = sqrt(b * b + 4.0 * a2 * y2);
    
    float r2;
    // 数值稳定性优化：避免两数相减造成的精度损失
    if (b >= 0.0) {
        r2 = 0.5 * (b + det);
    } else {
        // 当 b < 0 时，使用共轭分子有理化公式: 
        // (-b + sqrt(b^2 + 4ac)) / 2 = (2ac) / (b + sqrt(...)) -> 这里的形式略有不同
        // r^2 = (2 a^2 y^2) / (det - b) 保证分母远离0
        r2 = (2.0 * a2 * y2) / max(1e-20, det - b);
    }
    return r_sign * sqrt(r2);
}

// [OPTIMIZATION] 结构体用于缓存几何参数，减少重复计算
struct KerrGeometry {
    float r;
    float r2;
    float a2;
    float f;
    vec3  grad_r;     // dr/dx
    vec3  grad_f;     // df/dx
    vec4  l_up;       // l^u = (lx, ly, lz, -1)
    vec4  l_down;     // l_u = (lx, ly, lz,  1)
    float inv_r2_a2;  // 1 / (r^2 + a^2)
};

// [OPTIMIZATION] 计算几何参数的核心函数
// 将原本分散在各个函数中的 r, f, l 计算逻辑集中处理
void ComputeGeometry(vec3 X, float a, float Q, float fade, float r_sign, out KerrGeometry geo) {
    geo.a2 = a * a;
    
    if (a == 0.0) {
        geo.r = length(X);
        geo.r2 = geo.r * geo.r;
        float inv_r = 1.0 / max(EPSILON, geo.r);
        float inv_r2 = inv_r * inv_r;
        
        // Schwarzschild limit
        geo.l_up = vec4(X * inv_r, -1.0);
        geo.l_down = vec4(X * inv_r, 1.0);
        
        // f = (2M/r - Q^2/r^2)
        geo.f = (2.0 * CONST_M * inv_r - (Q * Q) * inv_r2) * fade;
        
        geo.grad_r = X * inv_r;
        // df/dr = -2M/r^2 + 2Q^2/r^3 = (-2M + 2Q^2/r) / r^2
        float df_dr = (-2.0 * CONST_M + 2.0 * Q * Q * inv_r) * inv_r2 * fade;
        geo.grad_f = df_dr * geo.grad_r;
        
        geo.inv_r2_a2 = inv_r2; // actually 1/r^2
        return;
    }

    geo.r = KerrSchildRadius(X, a, r_sign);
    geo.r2 = geo.r * geo.r;
    float r3 = geo.r2 * geo.r;
    float z_coord = X.y;
    float z2 = z_coord * z_coord;
    
    geo.inv_r2_a2 = 1.0 / (geo.r2 + geo.a2);
    
    // --- 计算 l^u ---
    float lx = (geo.r * X.x + a * X.z) * geo.inv_r2_a2;
    float ly = X.y / geo.r;
    float lz = (geo.r * X.z - a * X.x) * geo.inv_r2_a2;
    
    geo.l_up = vec4(lx, ly, lz, -1.0);
    geo.l_down = vec4(lx, ly, lz, 1.0); // 空间分量相同，时间分量反号
    
    // --- 计算 f ---
    float num_f = 2.0 * CONST_M * r3 - Q * Q * geo.r2;
    float den_f = geo.r2 * geo.r2 + geo.a2 * z2;
    float inv_den_f = 1.0 / max(1e-20, den_f);
    geo.f = (num_f * inv_den_f) * fade;
    
    // --- 计算 grad r ---
    float R2 = dot(X, X);
    float D = 2.0 * geo.r2 - R2 + geo.a2;
    float denom_grad = geo.r * D;
    if (abs(denom_grad) < 1e-9) denom_grad = sign(geo.r) * 1e-9;
    float inv_denom_grad = 1.0 / denom_grad;
    
    geo.grad_r = vec3(
        X.x * geo.r2,
        X.y * (geo.r2 + geo.a2),
        X.z * geo.r2
    ) * inv_denom_grad;
    
    // --- 计算 grad f (解析优化) ---
    // df/dr terms (reduced by factor r)
    float term_M  = -2.0 * CONST_M * geo.r2 * geo.r2 * geo.r;
    float term_Q  = 2.0 * Q * Q * geo.r2 * geo.r2;
    float term_Ma = 6.0 * CONST_M * geo.a2 * geo.r * z2;
    float term_Qa = -2.0 * Q * Q * geo.a2 * z2;
    
    float df_dr_num_reduced = term_M + term_Q + term_Ma + term_Qa;
    float df_dr = (geo.r * df_dr_num_reduced) * (inv_den_f * inv_den_f);
    
    float df_dy = -(num_f * 2.0 * geo.a2 * z_coord) * (inv_den_f * inv_den_f);
    
    geo.grad_f = df_dr * geo.grad_r;
    geo.grad_f.y += df_dy;
    geo.grad_f *= fade;
}

// 原始接口保留，但内部通过结构体加速
// 逆度规 g^uv
mat4 GetInverseKerrMetric(vec4 X, float a, float Q, float fade, float r_sign) {
    KerrGeometry geo;
    ComputeGeometry(X.xyz, a, Q, fade, r_sign, geo);
    // g^uv = eta^uv - f * l^u * l^v
    return MINKOWSKI_METRIC - geo.f * outerProduct(geo.l_up, geo.l_up);
}

// 协变度规 g_uv
mat4 GetKerrMetric(vec4 X, float a, float Q, float fade, float r_sign) {
    KerrGeometry geo;
    ComputeGeometry(X.xyz, a, Q, fade, r_sign, geo);
    // g_uv = eta_uv + f * l_u * l_v
    return MINKOWSKI_METRIC + geo.f * outerProduct(geo.l_down, geo.l_down);
}

// [OPTIMIZATION] 优化后的哈密顿量计算，不构建矩阵
float Hamiltonian(vec4 X, vec4 P, float a, float Q, float fade, float r_sign) {
    KerrGeometry geo;
    ComputeGeometry(X.xyz, a, Q, fade, r_sign, geo);
    
    // P^2_minkowski = P.xyz . P.xyz - P.t^2
    float P_sq_minkowski = dot(P.xyz, P.xyz) - P.w * P.w;
    
    // l^u P_u = l_up . P_down_flat - but P is Covariant (P_u)
    // l^u P_u = l^x P_x + l^y P_y + l^z P_z + l^t P_t
    // l^t = -1
    float l_dot_P = dot(geo.l_up.xyz, P.xyz) - P.w;
    
    // H = 1/2 * (P^2_eta - f * (l.P)^2)
    return 0.5 * (P_sq_minkowski - geo.f * l_dot_P * l_dot_P);
}

float GetGtt(vec3 Pos, float a, float Q, float r_sign) {
    KerrGeometry geo;
    ComputeGeometry(Pos, a, Q, 1.0, r_sign, geo); // fade assumed 1.0 or passed in? Original logic implies internal calc.
    // g_tt = -1 + f * l_t * l_t = -1 + f
    return -1.0 + geo.f;
}

// 辅助函数保持不变
vec4 LorentzBoost(vec4 P_in, vec3 v) {
    float v2 = dot(v, v);
    if (v2 >= 1.0 || v2 < EPSILON) return P_in;
    float gamma = 1.0 / sqrt(1.0 - v2);
    float bp = dot(v, P_in.xyz);
    vec3 P_spatial = P_in.xyz + (gamma - 1.0) * (bp / v2) * v + gamma * P_in.w * v;
    float P_time = gamma * (P_in.w + bp);
    return vec4(P_spatial, P_time);
}

// [FIXED] 获取观测者的逆变 4-速度 U^u
// 这决定了观测者处于什么参考系，直接影响多普勒效应和光行差
vec4 GetObserverU(vec4 X, int iObserverMode, float a, float Q, float fade, float r_sign) 
{
    // Mode 1: Falling Observer (Rain Observer / ZAMO in limit)
    // 在 Kerr-Schild 坐标系中，从无穷远静止下落的观测者拥有非常简单的协变速度: U_u = (0, 0, 0, -1)
    // 我们需要将其转换为逆变速度 U^u = g^uv * U_v
    if (iObserverMode == 1) 
    {
        mat4 g_inv = GetInverseKerrMetric(X, a, Q, fade, r_sign);
        // U^u = g^u0 * 0 + ... + g^u3 * (-1) = -1.0 * (g_inv 的第4列)
        return 1.0 * g_inv[3]; 
    }
    
    // Mode 0: Static Observer (Boyer-Lindquist Stationary)
    // 保持空间坐标不变的观测者: U^i = 0, U^t = N.
    // 归一化条件: g_uv U^u U^v = -1 => g_tt * (U^t)^2 = -1
    // U^t = 1 / sqrt(-g_tt)
    // 注意：在能层（Ergosphere）内 g_tt > 0，静态观测者无法存在（需超光速）。
    else 
    {
        float gtt = GetGtt(X.xyz, a, Q, r_sign);
        
        // 安全检查：如果在能层内或视界内，静态观测者物理上不成立
        // 这里做一个平滑回退到 Falling 模式，防止渲染 NaN
        if (gtt >= -1e-4) 
        {
            mat4 g_inv = GetInverseKerrMetric(X, a, Q, fade, r_sign);
            return vec4(0.0);
        }
        
        return vec4(0.0, 0.0, 0.0, -1.0 / sqrt(-gtt));
    }
}

// [FIXED] 重写的初始动量计算
// 使用局部投影法代替洛伦兹变换，能够完美处理 Kerr 度规的空间拖曳
vec4 GetInitialMomentum(vec3 RayDir, vec4 X, int iObserverMode, float iSpin, float iQ, float GravityFade) 
{
    // 1. 准备 Kerr-Schild 几何参数
    float r_sign = 1.0; 
    float r = KerrSchildRadius(X.xyz, iSpin, r_sign);
    float r2 = r * r;
    float a2 = iSpin * iSpin;
    
    // 计算零矢量 l 的空间分量 (指向外，r增加的方向)
    float denom_inv = 1.0 / (r2 + a2);
    float lx = (r * X.x + iSpin * X.z) * denom_inv;
    float ly = X.y / r;
    float lz = (r * X.z - iSpin * X.x) * denom_inv;
    vec3 l_spatial = vec3(lx, ly, lz);
    
    float r3 = r2 * r;
    float numerator = 2.0 * CONST_M * r3 - iQ * iQ * r2;
    float denominator = r2 * r2 + a2 * X.y * X.y;
    float f = (numerator / max(1e-20, denominator)) * GravityFade;

    // 2. 根据观测者模式修正光线方向
    vec3 TargetDir = RayDir;

    // Mode 0: 静态观测者 (Static)
    if (iObserverMode == 0 && f < 1.0) {
        float beta = sqrt(f); 
        
        // [BUG FIX]: 
        // 静态观测者相对于网格向外运动，但在光线追踪(逆向)中，
        // 我们需要把相机的“向内视线”在网格系中变得“更向内”，
        // 这样才能模拟出静态者看到的“黑洞变大”的效果。
        // 原代码使用了 +normalize(l_spatial)，导致光线向外偏折（黑洞变小）。
        // 这里改为负号。
        vec3 v_boost = -normalize(l_spatial) * beta;
        
        vec4 P_local = vec4(RayDir, 1.0);
        TargetDir = LorentzBoost(P_local, v_boost).xyz;
        TargetDir = normalize(TargetDir);
    }
    
    // 3. 求解 Kerr-Schild 下的逆变时间分量 P^t
    float v_sq = dot(TargetDir, TargetDir);
    float L = dot(l_spatial, TargetDir); 
    
    float A = 1.0 - f;
    float Delta = v_sq - f * (v_sq - L * L);
    
    float kt = 0.0;
    if (abs(A) < 1e-5) {
        if (abs(L) > 1e-5) kt = (v_sq + f * L * L) / (2.0 * f * L);
        else kt = length(TargetDir);
    } else {
        kt = (f * L + sqrt(max(0.0, Delta))) / A;
    }
    
    vec4 P_contrav = vec4(TargetDir, kt);

    // 4. 转换为协变动量并返回
    mat4 g_cov = GetKerrMetric(X, iSpin, iQ, GravityFade, r_sign);
    return g_cov * P_contrav;
}


float CalculateFrequencyRatio(vec4 P_photon, vec4 U_emitter, vec4 U_observer, mat4 g_cov)
{
    // 这里传入 g_cov 是矩阵，为了兼容性保留，建议外部如果能优化也可以把 g_cov 去掉
    vec4 P_cov = g_cov * P_photon; 
    float E_emit = -dot(P_cov, U_emitter);
    float E_obs  = -dot(P_cov, U_observer);
    return E_emit / max(EPSILON, E_obs);
}

// -----------------------------------------------------------------------------
// 数值积分核心结构
// -----------------------------------------------------------------------------
struct State {
    vec4 X;
    vec4 P;
};

// [OPTIMIZATION] 极速版哈密顿修正
// 完全移除 GetInverseKerrMetric 调用，使用代数解
void ApplyHamiltonianCorrection(inout vec4 P, vec4 X, float initial_pt, float a, float Q, float fade, float r_sign) {
    // P is Covariant Momentum (P_u). P.w is P_t.
    // We want to scale P.xyz by k such that H(k*P.xyz, -initial_pt) = 0.
    
    P.w = -initial_pt; // 能量守恒
    vec3 p = P.xyz;
    
    // 获取几何参数
    KerrGeometry geo;
    ComputeGeometry(X.xyz, a, Q, fade, r_sign, geo);
    
    // g^uv = eta^uv - f * l^u * l^v
    // H = 1/2 * g^uv P_u P_v = 0
    // g^uv P_u P_v = (eta^uv P_u P_v) - f * (l^u P_u)^2 = 0
    
    // 分解项:
    // l^u P_u = l^i P_i + l^t P_t = (l_vec . p) + (-1)*P_t
    // Let L_dot_p = dot(geo.l_up.xyz, p)
    // Term L = L_dot_p * k - P.w (Substituting scaled p = k*p_old)
    
    // eta^uv P_u P_v = |p|^2 * k^2 - P_t^2
    
    // Equation:
    // (|p|^2 * k^2 - P_t^2) - f * (L_dot_p * k - P.w)^2 = 0
    
    float p2 = dot(p, p);
    float L_dot_p = dot(geo.l_up.xyz, p);
    float Pt = P.w;
    
    // Expand:
    // k^2 * p2 - Pt^2 - f * ( L_dot_p^2 * k^2 - 2 * L_dot_p * Pt * k + Pt^2 ) = 0
    // k^2 * (p2 - f * L_dot_p^2) + k * (2 * f * L_dot_p * Pt) + (-Pt^2 - f * Pt^2) = 0
    
    float Coeff_A = p2 - geo.f * L_dot_p * L_dot_p;
    float Coeff_B = 2.0 * geo.f * L_dot_p * Pt;
    float Coeff_C = -Pt * Pt * (1.0 + geo.f);
    
    float disc = Coeff_B * Coeff_B - 4.0 * Coeff_A * Coeff_C;
    
    if (disc >= 0.0) {
        float sqrtDisc = sqrt(disc);
        float denom = 2.0 * Coeff_A;
        if (abs(denom) > 1e-9) {
            // 我们寻找接近 1.0 的根。通常是 (-B + sqrt)/2A 或 (-B - sqrt)/2A
            // 取决于 Coeff_A 的符号等。对于光子，k 应该是正数。
            float k = (-Coeff_B + sqrtDisc) / denom;
            
            // 如果得到了负数解或者不合理的解，尝试另一个
            if (k < 0.0) k = (-Coeff_B - sqrtDisc) / denom;
            
            if (k > 0.0) P.xyz *= k;
        }
    }
}

// [OPTIMIZATION] 解析梯度计算 (Matrix-Free & Precomputed)
// 这是整个渲染器最热的代码路径
State GetDerivativesAnalytic(State S, float a, float Q, float fade, float r_sign) {
    State deriv;
    
    // 1. 获取所有几何数据 (r, f, l, gradients)
    KerrGeometry geo;
    ComputeGeometry(S.X.xyz, a, Q, fade, r_sign, geo);
    
    // 2. 计算缩约量 (Contractions)
    // l^u * P_u
    float l_dot_P = dot(geo.l_up.xyz, S.P.xyz) + geo.l_up.w * S.P.w;
    
    // 3. 计算 dX^u / dlambda = P^u
    // P^u = g^uv P_v = (eta^uv - f l^u l^v) P_v
    //     = P_flat^u - f * (l.P) * l^u
    vec4 P_flat = vec4(S.P.xyz, -S.P.w); // eta是对角的 (1,1,1,-1)
    deriv.X = P_flat - geo.f * l_dot_P * geo.l_up;
    
    // 4. 计算 dP_u / dlambda (Force)
    // F_i = 1/2 * partial_i ( g^uv P_u P_v )
    //     = 1/2 * partial_i ( P_flat^2 - f * (l.P)^2 )  (P_flat^2 导数为0)
    //     = -1/2 * [ (l.P)^2 * partial_i(f) + f * 2 * (l.P) * partial_i(l.P) ]
    // partial_i(l.P) = P_u * partial_i(l^u) (P 视为常数)
    
    // --- 计算 P . grad(l) ---
    // l^x = (rx + az)A, l^y = y/r, l^z = (rz - ax)A, where A = 1/(r^2+a^2)
    
    // A 的梯度: grad_A = -2r * A^2 * grad_r
    vec3 grad_A = (-2.0 * geo.r * geo.inv_r2_a2) * geo.inv_r2_a2 * geo.grad_r;
    
    // 预计算 helper
    float rx_az = geo.r * S.X.x + a * S.X.z;
    float rz_ax = geo.r * S.X.z - a * S.X.x;
    
    // 展开 P_k * partial_i (l^k)
    // Term 1: l^x components
    // grad(l^x) = grad(rx+az)*A + (rx+az)*grad_A
    // grad(rx+az) = x*grad_r + r*x_hat + a*z_hat
    vec3 d_num_lx = S.X.x * geo.grad_r; 
    d_num_lx.x += geo.r; 
    d_num_lx.z += a;
    vec3 grad_lx = geo.inv_r2_a2 * d_num_lx + rx_az * grad_A;
    
    // Term 2: l^y components
    // grad(y/r) = (r*y_hat - y*grad_r) / r^2
    vec3 grad_ly = (geo.r * vec3(0.0, 1.0, 0.0) - S.X.y * geo.grad_r) / geo.r2;
    
    // Term 3: l^z components
    vec3 d_num_lz = S.X.z * geo.grad_r;
    d_num_lz.z += geo.r;
    d_num_lz.x -= a;
    vec3 grad_lz = geo.inv_r2_a2 * d_num_lz + rz_ax * grad_A;
    
    // 累加
    vec3 P_dot_grad_l = S.P.x * grad_lx + S.P.y * grad_ly + S.P.z * grad_lz;
    
    // 最终 Force
    // Force = -0.5 * [ (l.P)^2 * grad_f + 2 * f * (l.P) * P_dot_grad_l ]
    // 注意：原代码是 +0.5 * ... 因为 H = 1/2 g^uv...
    // Force = - dH/dX. 
    // H = 0.5 * (const - f * (l.P)^2)
    // dH/dX = -0.5 * [ grad_f * (l.P)^2 + f * 2(l.P) * grad(l.P) ]
    // Force = -dH/dX = 0.5 * [ grad_f * (l.P)^2 + 2f(l.P) * grad(l.P) ]
    // 符号与原代码一致。
    
    vec3 Force = 0.5 * ( (l_dot_P * l_dot_P) * geo.grad_f + (2.0 * geo.f * l_dot_P) * P_dot_grad_l );
    
    deriv.P = vec4(Force, 0.0); // partial_t H = 0
    
    return deriv;
}

// RK4 积分器 (逻辑不变，调用优化后的函数)
void StepGeodesicRK4(inout vec4 X, inout vec4 P, float E, float dt, float a, float Q, float fade, float r_sign) {
    State s0; s0.X = X; s0.P = P;
    
    State k1 = GetDerivativesAnalytic(s0, a, Q, fade, r_sign);
    
    State s1; 
    s1.X = s0.X + 0.5 * dt * k1.X; 
    s1.P = s0.P + 0.5 * dt * k1.P;
    State k2 = GetDerivativesAnalytic(s1, a, Q, fade, r_sign);
    
    State s2; 
    s2.X = s0.X + 0.5 * dt * k2.X; 
    s2.P = s0.P + 0.5 * dt * k2.P;
    State k3 = GetDerivativesAnalytic(s2, a, Q, fade, r_sign);
    
    State s3; 
    s3.X = s0.X + dt * k3.X; 
    s3.P = s0.P + dt * k3.P;
    State k4 = GetDerivativesAnalytic(s3, a, Q, fade, r_sign);
    
    X = s0.X + (dt / 6.0) * (k1.X + 2.0 * k2.X + 2.0 * k3.X + k4.X);
    P = s0.P + (dt / 6.0) * (k1.P + 2.0 * k2.P + 2.0 * k3.P + k4.P);
    
    ApplyHamiltonianCorrection(P, X, E, a, Q, fade, r_sign);
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
                 // [TENSOR ANNOTATION]
                 // 构造流体(吸积盘物质)的逆变 4-速度 U_fluid^u.
                 // FluidVel 是空间速度 v^i.
                 // U^u = gamma * (v^x, v^y, v^z, 1).
                 vec3 FluidVel = AngularVelocity * vec3(SamplePos.z, 0.0, -SamplePos.x);
                 vec4 U_fluid_unnorm = vec4(FluidVel, 1.0); 
                 mat4 g_cov_sample = GetKerrMetric(vec4(SamplePos, 0.0), Spin, Q, 1.0, 1.0);
                 
                 // 计算归一化因子 gamma
                 // g_uv * U^u * U^v = -1
                 float norm_sq = dot(U_fluid_unnorm, g_cov_sample * U_fluid_unnorm);
                 vec4 U_fluid = U_fluid_unnorm * inversesqrt(max(1e-6, abs(norm_sq)));
                 
                 // 计算发射能量 E_emit = - P_u(photon) * U^u(fluid)
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
                 SampleColor.a   *= 0.125;
                 SampleColor.xyz *=Brightmut;
                 SampleColor.a   *=Darkmut;
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
    // [TENSOR ANNOTATION]
    // 在弯曲时空中初始化光子动量。
    // 我们需要在相机位置构建一个局域四维标架 (Tetrad/Vierbein) {e_mu}.
    // 相机测量到的光线方向 S 实际上是动量在局域平直空间(切空间)的分量。
    
    vec4 X = vec4(RayPosLocal, 0.0); // 初始时空坐标 (t=0)
    vec4 P_cov = vec4(0.0);         
    float E_obs_camera = 1.0;       
    vec3 RayDir = RayDirWorld_Geo;   
    vec3 LastDir = RayDir;
    vec3 LastPos = RayPosLocal;
    float GravityFade = CubicInterpolate(max(min(1.0 - (0.01 * length(RayPosLocal) - 1.0) / 4.0, 1.0), 0.0));
        
    if (bShouldContinueMarchRay) 
    {
       // [FIXED] 使用新的投影法计算初始动量
       P_cov = GetInitialMomentum(RayDir, X, iObserverMode, PhysicalSpinA, PhysicalQ, GravityFade);
    }
    
    // [FIXED] 正确计算相机观测到的能量
    // E_obs = - P_u * U^u_camera
    // 之前直接使用 -P_cov.w 是假设了观测者 U=(1,0,0,0)，这对于静态观测者(需归一化)和落体观测者(需变换)都是错的。
    vec4 U_camera = GetObserverU(X, iObserverMode, PhysicalSpinA, PhysicalQ, GravityFade, CurrentUniverseSign);
    E_obs_camera = dot(P_cov, U_camera);
    
    // 强制修正哈密顿量，消除数值误差 (Input E should be E_infinity = -P_t)
    // 注意：Hamiltonian correction 保持动量的时间分量守恒（能量守恒），即 P_t (P_cov.w)
    // P_cov.w 是无穷远处的能量，E_obs_camera 是本地能量，两者是不同的，但积分器修正需要 P_t
    ApplyHamiltonianCorrection(P_cov, X, E_obs_camera, PhysicalSpinA, PhysicalQ, GravityFade, CurrentUniverseSign);
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
        float RayStep = StepFactor * DistRing*smoothstep(0.0,0.5,DistRing);

       // if (Count == 0) RayStep *= RandomStep(FragUv, fract(iTime));
        

        LastPos = X.xyz;
        GravityFade = CubicInterpolate(max(min(1.0 - (0.01 * DistanceToBlackHole - 1.0) / 4.0, 1.0), 0.0));
        
        // RK4 积分
        // [TENSOR ANNOTATION]
        // 动量 P_u 的更新需要 P^u = g^uv * P_v
        mat4 g_inv = GetInverseKerrMetric(X, PhysicalSpinA, PhysicalQ, GravityFade, CurrentUniverseSign);
        vec4 P_contra_step = g_inv * P_cov; // Contravariant P
        float V_mag = length(P_contra_step.xyz); 
        float dLambda = RayStep / max(V_mag, 1e-6); // 将步长转换为仿射参数 lambda
        
        StepGeodesicRK4(X, P_cov, E_obs_camera,dLambda, PhysicalSpinA, PhysicalQ, GravityFade, CurrentUniverseSign);
        
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
        //
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
        
        float E_emit_at_infinity = -P_cov.w;
        
        // 防止除零 (E_emit 不应为负或零，但在数值误差下需保护)
        float FrequencyRatio = 1.0 / max(1e-4, E_emit_at_infinity);
        
        // BackgroundShift 实际上就是频率比
        float BackgroundShift = FrequencyRatio; 
        BackgroundShift =clamp(BackgroundShift, 1.0/BackgroundShiftMax, BackgroundShiftMax);
        
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