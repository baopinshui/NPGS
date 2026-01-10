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
    int   iObserverMode;                 // 0 = Static, 1 = Falling
    float iUniverseSign;                 // +1.0 or -1.0
    float iBlackHoleTime;
    float iBlackHoleMassSol;
    float iSpin;                         // [INPUT] Dimensionless Spin parameter a* (0.0 ~ 1.0)
    float iQ;                            // [INPUT] Dimensionless Charge parameter Q* (0.0 ~ 1.0)
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

const float CONST_M = 0.5; // [PHYS] Mass G*M = 0.5
const float EPSILON = 1e-6;

// [TENSOR] Flat Space Metric eta_uv = diag(1, 1, 1, -1)
const mat4 MINKOWSKI_METRIC = mat4(
    1, 0, 0, 0,
    0, 1, 0, 0,
    0, 0, 1, 0,
    0, 0, 0, -1
);

// [INPUT] Spin/Q: Dimensional (scaled by CONST_M)
float GetKeplerianAngularVelocity(float Radius, float Rs, float Spin, float Q) 
{
    float M = 0.5 * Rs; 
    float Mr_minus_Q2 = M * Radius - Q * Q;
    if (Mr_minus_Q2 < 0.0) return 0.0;
    float sqrt_Term = sqrt(Mr_minus_Q2);
    float denominator = Radius * Radius + Spin * sqrt_Term;
    return sqrt_Term / max(EPSILON, denominator);
}

// [MATH] Solve for r where (x^2 + z^2)/(r^2 + a^2) + y^2/r^2 = 1
// [INPUT] a: Dimensional (scaled by CONST_M)
float KerrSchildRadius(vec3 p, float a, float r_sign) {
    float r_sign_len = r_sign * length(p);
    if (a == 0.0) return r_sign_len; 

    float a2 = a * a;
    float rho2 = dot(p.xz, p.xz); // x^2 + z^2
    float y2 = p.y * p.y;
    
    float b = rho2 + y2 - a2;
    float det = sqrt(b * b + 4.0 * a2 * y2);
    
    float r2;
    if (b >= 0.0) {
        r2 = 0.5 * (b + det);
    } else {
        r2 = (2.0 * a2 * y2) / max(1e-20, det - b);
    }
    return r_sign * sqrt(r2);
}

// [TENSOR] Structure to hold metric components g_uv and g^uv components
// Kerr-Schild form: g_uv = eta_uv + f * l_u * l_v
struct KerrGeometry {
    float r;
    float r2;
    float a2;
    float f;              // [MATH] Scalar function f(r, theta)
    vec3  grad_r;         // [TENSOR] Covariant spatial gradient of r (partial_i r)
    vec3  grad_f;         // [TENSOR] Covariant spatial gradient of f (partial_i f)
    vec4  l_up;           // [TENSOR] Contravariant null vector l^u = (l^x, l^y, l^z, -1)
    vec4  l_down;         // [TENSOR] Covariant null vector l_u = (l_x, l_y, l_z, 1)
    float inv_r2_a2; 
};

// [INPUT] a/Q: Dimensional (scaled by CONST_M)
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
        
        geo.f = (2.0 * CONST_M * inv_r - (Q * Q) * inv_r2) * fade;
        
        geo.grad_r = X * inv_r;
        float df_dr = (-2.0 * CONST_M + 2.0 * Q * Q * inv_r) * inv_r2 * fade;
        geo.grad_f = df_dr * geo.grad_r;
        
        geo.inv_r2_a2 = inv_r2; 
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
    geo.l_down = vec4(lx, ly, lz, 1.0); // Spatial same (eta_ij = delta_ij), Temporal flip (eta_tt = -1)
    
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

// [TENSOR] Returns Contravariant Metric g^uv
// g^uv = eta^uv - f * l^u * l^v
// [INPUT] a/Q: Dimensional (scaled by CONST_M)
mat4 GetInverseKerrMetric(vec4 X, float a, float Q, float fade, float r_sign) {
    KerrGeometry geo;
    ComputeGeometry(X.xyz, a, Q, fade, r_sign, geo);
    return MINKOWSKI_METRIC - geo.f * outerProduct(geo.l_up, geo.l_up);
}

// [TENSOR] Returns Covariant Metric g_uv
// g_uv = eta_uv + f * l_u * l_v
// [INPUT] a/Q: Dimensional (scaled by CONST_M)
mat4 GetKerrMetric(vec4 X, float a, float Q, float fade, float r_sign) {
    KerrGeometry geo;
    ComputeGeometry(X.xyz, a, Q, fade, r_sign, geo);
    return MINKOWSKI_METRIC + geo.f * outerProduct(geo.l_down, geo.l_down);
}

// [TENSOR] Calculates Hamiltonian H = 1/2 g^uv P_u P_v
// X: Position x^u (Contravariant)
// P: Momentum p_u (Covariant)
// [INPUT] a/Q: Dimensional (scaled by CONST_M)
float Hamiltonian(vec4 X, vec4 P, float a, float Q, float fade, float r_sign) {
    KerrGeometry geo;
    ComputeGeometry(X.xyz, a, Q, fade, r_sign, geo);
    
    // P^2_minkowski = P_i * P_i - P_t^2 (Using eta^uv)
    float P_sq_minkowski = dot(P.xyz, P.xyz) - P.w * P.w;
    
    // l^u P_u term
    float l_dot_P = dot(geo.l_up.xyz, P.xyz) - P.w;
    
    // H = 1/2 * (eta^uv P_u P_v - f * (l^u P_u)^2)
    return 0.5 * (P_sq_minkowski - geo.f * l_dot_P * l_dot_P);
}

// [INPUT] a/Q: Dimensional (scaled by CONST_M)
float GetGtt(vec3 Pos, float a, float Q, float r_sign) {
    KerrGeometry geo;
    ComputeGeometry(Pos, a, Q, 1.0, r_sign, geo);
    // g_tt = -1 + f * l_t * l_t = -1 + f (since l_t = 1)
    return -1.0 + geo.f;
}

// [MATH] Special Relativity Lorentz Boost
vec4 LorentzBoost(vec4 P_in, vec3 v) {
    float v2 = dot(v, v);
    if (v2 >= 1.0 || v2 < EPSILON) return P_in;
    float gamma = 1.0 / sqrt(1.0 - v2);
    float bp = dot(v, P_in.xyz);
    vec3 P_spatial = P_in.xyz + (gamma - 1.0) * (bp / v2) * v + gamma * P_in.w * v;
    float P_time = gamma * (P_in.w + bp);
    return vec4(P_spatial, P_time);
}

// [TENSOR] Returns Observer 4-Velocity U^u (Contravariant) in Kerr-Schild Frame
// [INPUT] a/Q: Dimensional (scaled by CONST_M)
vec4 GetObserverU(vec4 X, int iObserverMode, float a, float Q, float fade, float r_sign) 
{
    // Mode 1: Falling Observer (Rain Observer, ZAMO limit at infinity)
    // U_u = (0, 0, 0, -1) [Covariant]
    // U^u = g^uv * U_v = -g^u3
    if (iObserverMode == 1) 
    {
        mat4 g_inv = GetInverseKerrMetric(X, a, Q, fade, r_sign);
        return -1.0 * g_inv[3]; 
    }
    
    // Mode 0: Static Observer (Stationary in KS coordinates)
    // U^i = 0, U^t = 1 / sqrt(-g_tt)
    else 
    {
        float gtt = GetGtt(X.xyz, a, Q, r_sign);
        
        if (gtt >= -1e-6) // Ergosphere check
        {
            mat4 g_inv = GetInverseKerrMetric(X, a, Q, fade, r_sign);
            return -1.0 * g_inv[3];
        }
        
        return vec4(0.0, 0.0, 0.0, 1.0 / sqrt(-gtt));
    }
}

// [TENSOR] Returns Initial Covariant Momentum P_u
// [INPUT] iSpin/iQ: Dimensional (scaled by CONST_M)
vec4 GetInitialMomentum(
    vec3 RayDir,
    vec4 X,
    int  iObserverMode,   // 0: static, 1: free-fall (E=1, L=0)
    float universesign,
    float iSpin,          // Dimensional
    float iQ,             // Dimensional
    float GravityFade
) {
    // --- 几何 ---
    KerrGeometry geo;
    ComputeGeometry(X.xyz, iSpin, iQ, GravityFade, universesign, geo);

    // 确保方向单位化
    vec3 n = normalize(RayDir);

    // --- 局域正交系中的光子四动量（逆变，Minkowski） ---
    // 注意：这里的 w 分量是“时间分量”
    vec4 P_local = vec4(n, 1.0);

    // --- 观者速度（Minkowski 意义下） ---
    vec3 v = vec3(0.0);

    if (iObserverMode == 0) {
        // ===== 静止观者 =====
        float g_tt = GetGtt(X.xyz, iSpin, iQ, universesign);
        float alpha = sqrt(max(EPSILON, -g_tt));

        // 仅时间缩放，不需要 boost
        vec4 P_up = vec4(P_local.xyz, P_local.w / alpha);

        // --- 降指标 ---
        mat4 g_uv = GetKerrMetric(X, iSpin, iQ, GravityFade, universesign);
        vec4 P_down = g_uv * P_up;

        // 空间反向
        P_down.xyz = P_down.xyz;
        return P_down;
    }

    else {
        // ===== 自由落体观者 (E=1, L=0) =====
        // KS 中的自然自由落体速度
        float v_mag = sqrt(max(0.0, geo.f));
        v = -v_mag * geo.l_up.xyz;

        // SR boost（在 Minkowski 上）
        vec4 P_up = LorentzBoost(P_local, v);

        // --- 降指标 ---
        mat4 g_uv = GetKerrMetric(X, iSpin, iQ, GravityFade, universesign);
        vec4 P_down = g_uv * P_up;

        // 空间反向
        P_down.xyz = P_down.xyz;
        return P_down;
    }
}
// [TENSOR] Calculates Redshift Ratio E_emit / E_obs
// P_photon: Contravariant P^u (NOTE: Function signature expects this, but caller passes Covariant? See main)
// Actually implementation uses g_cov * P_photon, so input must be Contravariant P^u.
float CalculateFrequencyRatio(vec4 P_photon, vec4 U_emitter, vec4 U_observer, mat4 g_cov)
{
    vec4 P_cov = g_cov * P_photon; 
    float E_emit = -dot(P_cov, U_emitter);
    float E_obs  = -dot(P_cov, U_observer);
    return E_emit / max(EPSILON, E_obs);
}

// -----------------------------------------------------------------------------
// 数值积分核心结构
// -----------------------------------------------------------------------------
struct State {
    vec4 X; // [TENSOR] Contravariant Position x^u
    vec4 P; // [TENSOR] Covariant Momentum p_u
};

// [TENSOR] Enforces Hamiltonian Constraint H = 0 on Covariant Momentum P_u
// P: In/Out Covariant Momentum p_u
// X: Contravariant Position x^u
// [INPUT] a/Q: Dimensional (scaled by CONST_M)
void ApplyHamiltonianCorrection(inout vec4 P, vec4 X, float initial_pt, float a, float Q, float fade, float r_sign) {
    P.w = -initial_pt; // Energy conservation (if applicable)
    vec3 p = P.xyz;    // Spatial components of Covariant Momentum p_i
    
    KerrGeometry geo;
    ComputeGeometry(X.xyz, a, Q, fade, r_sign, geo);
    
    // Solving g^uv p_u p_v = 0 for scaling factor k on p_i
    float p2 = dot(p, p);
    float L_dot_p = dot(geo.l_up.xyz, p);
    float Pt = P.w;
    
    float Coeff_A = p2 - geo.f * L_dot_p * L_dot_p;
    float Coeff_B = 2.0 * geo.f * L_dot_p * Pt;
    float Coeff_C = -Pt * Pt * (1.0 + geo.f);
    
    float disc = Coeff_B * Coeff_B - 4.0 * Coeff_A * Coeff_C;
    
    if (disc >= 0.0) {
        float sqrtDisc = sqrt(disc);
        float denom = 2.0 * Coeff_A;
        if (abs(denom) > 1e-9) {
            float k = (-Coeff_B + sqrtDisc) / denom;
            if (k < 0.0) k = (-Coeff_B - sqrtDisc) / denom;
            if (k > 0.0) P.xyz *= k;
        }
    }
}

// [TENSOR] Calculates derivatives for Geodesic Equation
// S.P is Covariant Momentum p_u
// Output deriv.X is dx^u / dlambda (Contravariant Velocity)
// Output deriv.P is dp_u / dlambda (Covariant Force)
// [INPUT] a/Q: Dimensional (scaled by CONST_M)
State GetDerivativesAnalytic(State S, float a, float Q, float fade, float r_sign) {
    State deriv;
    
    KerrGeometry geo;
    ComputeGeometry(S.X.xyz, a, Q, fade, r_sign, geo);
    
    // l^u * P_u
    float l_dot_P = dot(geo.l_up.xyz, S.P.xyz) + geo.l_up.w * S.P.w;
    
    // dx^u/dlambda = g^uv p_v = P_flat^u - f * (l.P) * l^u
    vec4 P_flat = vec4(S.P.xyz, -S.P.w); // Minkowski raise
    deriv.X = P_flat - geo.f * l_dot_P * geo.l_up;
    
    // dp_u/dlambda = -dH/dx^u
    vec3 grad_A = (-2.0 * geo.r * geo.inv_r2_a2) * geo.inv_r2_a2 * geo.grad_r;
    
    float rx_az = geo.r * S.X.x + a * S.X.z;
    float rz_ax = geo.r * S.X.z - a * S.X.x;
    
    vec3 d_num_lx = S.X.x * geo.grad_r; 
    d_num_lx.x += geo.r; 
    d_num_lx.z += a;
    vec3 grad_lx = geo.inv_r2_a2 * d_num_lx + rx_az * grad_A;
    
    vec3 grad_ly = (geo.r * vec3(0.0, 1.0, 0.0) - S.X.y * geo.grad_r) / geo.r2;
    
    vec3 d_num_lz = S.X.z * geo.grad_r;
    d_num_lz.z += geo.r;
    d_num_lz.x -= a;
    vec3 grad_lz = geo.inv_r2_a2 * d_num_lz + rz_ax * grad_A;
    
    vec3 P_dot_grad_l = S.P.x * grad_lx + S.P.y * grad_ly + S.P.z * grad_lz;
    
    // Force = 0.5 * [ grad_f * (l.P)^2 + 2f(l.P) * grad(l.P) ]
    vec3 Force = 0.5 * ( (l_dot_P * l_dot_P) * geo.grad_f + (2.0 * geo.f * l_dot_P) * P_dot_grad_l );
    
    deriv.P = vec4(Force, 0.0); // Stationary metric => partial_t H = 0
    
    return deriv;
}

// [MATH] RK4 Integrator
// X: Contravariant Position x^u
// P: Covariant Momentum p_u
// [INPUT] a/Q: Dimensional (scaled by CONST_M)
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
               float PeakTemperature, float ShiftMax, 
               float Spin, // [INPUT] Dimensional Spin (scaled by CONST_M)
               float Q,    // [INPUT] Dimensional Charge (scaled by CONST_M)
               vec4 iP_cov, float iE_obs) 
{
    vec4 CurrentResult = BaseColor;
    vec3 StartPos = LastRayPos; // [COORD] KS Spatial Position
    vec3 DirVec   = RayDir; 
    float TotalDist = StepLength;
    float TraveledDist = 0.0;
    
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
                 
                 // [TENSOR] Physical Redshift Calculation
                 // FluidVel is v^i in KS frame
                 vec3 FluidVel = AngularVelocity * vec3(SamplePos.z, 0.0, -SamplePos.x);
                 vec4 U_fluid_unnorm = vec4(FluidVel, 1.0); 
                 mat4 g_cov_sample = GetKerrMetric(vec4(SamplePos, 0.0), Spin, Q, 1.0, 1.0);
                 
                 // Normalize Fluid 4-Velocity U^u
                 float norm_sq = dot(U_fluid_unnorm, g_cov_sample * U_fluid_unnorm);
                 vec4 U_fluid = U_fluid_unnorm * inversesqrt(max(1e-6, abs(norm_sq)));
                 
                 // E_emit = - P_u * U^u (Scalar invariant)
                 // iP_cov is Covariant Momentum p_u
                 // U_fluid is Contravariant Velocity U^u
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
              float InterRadius, float OuterRadius, float JetRedShiftIntensityExponent, float JetBrightmut, float JetReddening, float JetSaturation, float AccretionRate, float JetShiftMax, 
              float Spin, // [INPUT] Dimensional Spin (scaled by CONST_M)
              float Q,    // [INPUT] Dimensional Charge (scaled by CONST_M)
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
    
    // [PHYS] Scaled Physical Constants
    // 这里将无量纲的输入自旋和电荷乘以质量常数 (CONST_M = 0.5)
    float PhysicalSpinA = iSpin * CONST_M;  // Dimensional (Length units)
    float PhysicalQ = iQ * CONST_M;         // Dimensional (Length units)
    
    // Event Horizon (Kerr-Newman)
    float HorizonDiscrim = 0.25 - PhysicalSpinA * PhysicalSpinA - PhysicalQ * PhysicalQ;
    float EventHorizonR = 0.5 + sqrt(max(0.0, HorizonDiscrim));
    bool bIsNakedSingularity = HorizonDiscrim < 0.0;
    float TerminationR = EventHorizonR;
    float CurrentUniverseSign = iUniverseSign;
    float ShiftMax = iJetShiftMax; 

    // --- 1. 相机光线生成 (几何部分) ---
    vec2 Jitter = vec2(RandomStep(FragUv, fract(iTime * 1.0 + 0.5)), RandomStep(FragUv, fract(iTime * 1.0))) / iResolution;
    vec3 ViewDirLocal = FragUvToDir(FragUv + 0.5 * Jitter, Fov, iResolution); // [COORD] Camera Space
    
    // [COORD] World Space
    vec3 CamToBHVecVisual = (iInverseCamRot * vec4(iBlackHoleRelativePosRs.xyz, 0.0)).xyz;
    vec3 RayPosWorld = -CamToBHVecVisual; 
    
    // Coordinate Transform Basis Construction
    vec3 DiskNormalWorld = normalize((iInverseCamRot * vec4(iBlackHoleRelativeDiskNormal.xyz, 0.0)).xyz);
    vec3 DiskTangentWorld = normalize((iInverseCamRot * vec4(iBlackHoleRelativeDiskTangen.xyz, 0.0)).xyz);
    vec3 BH_Y = normalize(DiskNormalWorld);             
    vec3 BH_X = normalize(DiskTangentWorld);            
    BH_X = normalize(BH_X - dot(BH_X, BH_Y) * BH_Y);
    vec3 BH_Z = normalize(cross(BH_X, BH_Y));           
    mat3 LocalToWorldRot = mat3(BH_X, BH_Y, BH_Z);
    mat3 WorldToLocalRot = transpose(LocalToWorldRot);
    
    // [COORD] Kerr-Schild Local Space
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
    vec4 X = vec4(RayPosLocal, 0.0); // [TENSOR] Contravariant Position X^u (t=0)
    vec4 P_cov = vec4(0.0);          // [TENSOR] Covariant Momentum P_u
    float E_conserved = 1.0;       
    
    vec3 RayDir = RayDirWorld_Geo;   
    vec3 LastDir = RayDir;
    vec3 LastPos = RayPosLocal;
    float GravityFade = CubicInterpolate(max(min(1.0 - (0.01 * length(RayPosLocal) - 1.0) / 4.0, 1.0), 0.0));
        
    if (bShouldContinueMarchRay) 
    {
       // 1. Get Geometry-Correct Momentum Direction (Magnitude unknown yet)
       // Returns P_u (Covariant)
       P_cov = GetInitialMomentum(RayDir, X, iObserverMode,iUniverseSign, PhysicalSpinA, PhysicalQ, GravityFade);

       // 2. Get Observer 4-Velocity U^u (Contravariant)
       //vec4 U_obs = GetObserverU(X, iObserverMode, PhysicalSpinA, PhysicalQ, GravityFade, CurrentUniverseSign);


    }
    
    // 5. P_cov.w is now -E_conserved (scaled)
    E_conserved = -P_cov.w;
    
    // 6. Enforce Hamiltonian constraint (Null Geodesic P^2 = 0)
    ApplyHamiltonianCorrection(P_cov, X, E_conserved, PhysicalSpinA, PhysicalQ, GravityFade, CurrentUniverseSign);
    
    vec3 RayPos = X.xyz;
    LastPos = RayPos;
    int Count = 0;
    float CameraStartR = KerrSchildRadius(RayPos, PhysicalSpinA, CurrentUniverseSign);
    
    TerminationR = -1.0; 
    
    if (CurrentUniverseSign > 0.0 && !bIsNakedSingularity) 
    {
        if (iObserverMode == 0) // Static Observer
        {
            float CosThetaSq = (RayPos.y * RayPos.y) / (CameraStartR * CameraStartR + 1e-10);
            float SL_Discrim = 0.25 - PhysicalQ * PhysicalQ - PhysicalSpinA * PhysicalSpinA * CosThetaSq;
            
            float SL_Outer = 0.5 + sqrt(max(0.0, SL_Discrim)); 
            float SL_Inner = 0.5 - sqrt(max(0.0, SL_Discrim)); 
            if (CameraStartR > SL_Outer) 
            {
                TerminationR = EventHorizonR; 
            } 
            else if (CameraStartR > SL_Inner) 
            {
                // Illegal State: Static observer inside Ergosphere
                bShouldContinueMarchRay = false;
                bWaitCalBack = false;
                Result = vec4(0.0, 0.0, 0.0, 1.0); 
            } 
            else 
            {
                TerminationR = -1.0;
            }
        }
        else // Falling Observer
        {
            float InnerHorizonR = 0.5 - sqrt(max(0.0, HorizonDiscrim));
            if (CameraStartR > InnerHorizonR) 
            {
                TerminationR = InnerHorizonR;
            } 
            else 
            {
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
        
       if (CurrentUniverseSign > 0.0 && CurrentKerrR < TerminationR && !bIsNakedSingularity) 
        { 
            bShouldContinueMarchRay = false; 
            bWaitCalBack = false; 
            break; 
        }
        if ((Count > 150 && !bIsNakedSingularity) || (Count > 450 && bIsNakedSingularity)) { 
            bShouldContinueMarchRay = false; bWaitCalBack = true; break; 
        }
        
        // Step Size Control
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

        LastPos = X.xyz;
        GravityFade = CubicInterpolate(max(min(1.0 - (0.01 * DistanceToBlackHole - 1.0) / 4.0, 1.0), 0.0));
        
        // [TENSOR] RK4 Integration
        mat4 g_inv = GetInverseKerrMetric(X, PhysicalSpinA, PhysicalQ, GravityFade, CurrentUniverseSign);
        vec4 P_contra_step = g_inv * P_cov; // Raise index: P^u = g^uv P_v
        float V_mag = length(P_contra_step.xyz); 
        float dLambda = RayStep / max(V_mag, 0.0); // Convert spatial step to affine parameter
        
        StepGeodesicRK4(X, P_cov, E_conserved,dLambda, PhysicalSpinA, PhysicalQ, GravityFade, CurrentUniverseSign);
        
        RayPos = X.xyz;
        vec3 StepVec = RayPos - LastPos;
        float ActualStepLength = length(StepVec);
        RayDir = (ActualStepLength > 1e-7) ? StepVec / ActualStepLength : LastDir;
        
        if (LastPos.y * RayPos.y < 0.0) {
            float t_cross = LastPos.y / (LastPos.y - RayPos.y);
            float rho_cross = length(mix(LastPos.xz, RayPos.xz, t_cross));
            if (rho_cross < abs(PhysicalSpinA)) CurrentUniverseSign *= -1.0;
        }
        
        // --- 渲染积累 (取消了原来的注释) ---
        //if (CurrentUniverseSign > 0.0) {
        //   Result = DiskColor(Result, ActualStepLength, RayPos, LastPos, RayDir, LastDir,
        //                     iInterRadiusRs, iOuterRadiusRs, iThinRs, iHopper, iBrightmut, iDarkmut, iReddening, iSaturation, DiskArgument, 
        //                     iBlackbodyIntensityExponent, iRedShiftColorExponent, iRedShiftIntensityExponent, PeakTemperature, ShiftMax, 
        //                     clamp(PhysicalSpinA,-0.49,0.49), // Passed as Dimensional
        //                     PhysicalQ,                       // Passed as Dimensional
        //                     P_cov, E_obs_camera); 
        //   
        //   Result = JetColor(Result, ActualStepLength, RayPos, LastPos, RayDir, LastDir,
        //                     iInterRadiusRs, iOuterRadiusRs, iJetRedShiftIntensityExponent, iJetBrightmut, iReddening, iJetSaturation, iAccretionRate, iJetShiftMax, 
        //                     clamp(PhysicalSpinA,-0.049,0.049), // Passed as Dimensional
        //                     PhysicalQ,                         // Passed as Dimensional
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
        
        // [PHYS] Gravitational Redshift Calculation
        // Frequency Ratio = E_camera / E_infinity
        // E_camera is normalized to 1.0 at start
        // E_infinity = E_conserved (-P_t at infinity)
        float FrequencyRatio = 1.0 / max(1e-4, E_conserved);
        
        float BackgroundShift = FrequencyRatio; 
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