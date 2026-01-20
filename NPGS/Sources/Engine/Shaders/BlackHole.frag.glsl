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
    float iSpin;                         // [INPUT] Dimensionless Spin parameter a*   
    float iQ;                            // [INPUT] Dimensionless Charge parameter Q* 
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
    if (wavelength <= 380.0 ) {
        color.r = 1.0; color.g = 0.0; color.b = 1.0;
    } else if (wavelength >= 380.0 && wavelength < 440.0) {
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
    } else if (wavelength >= 750.0) {
        color.r = 1.0; color.g = 0.0; color.b = 0.0;
    }
    float factor = 0.3;
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
float GetKeplerianAngularVelocity(float Radius, float Rs, float PhysicalSpinA, float PhysicalQ) 
{
    float M = 0.5 * Rs; 
    float Mr_minus_Q2 = M * Radius - PhysicalQ * PhysicalQ;
    if (Mr_minus_Q2 < 0.0) return 0.0;
    float sqrt_Term = sqrt(Mr_minus_Q2);
    float denominator = Radius * Radius + PhysicalSpinA * sqrt_Term;
    return sqrt_Term / max(EPSILON, denominator);
}

// [MATH] Solve for r where (x^2 + z^2)/(r^2 + a^2) + y^2/r^2 = 1
float KerrSchildRadius(vec3 p, float PhysicalSpinA, float r_sign) {
    float r_sign_len = r_sign * length(p);
    if (PhysicalSpinA == 0.0) return r_sign_len; 

    float a2 = PhysicalSpinA * PhysicalSpinA;
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

// [TENSOR] Structure to hold metric components
struct KerrGeometry {
    float r;
    float r2;
    float a2;
    float f;              
    vec3  grad_r;         
    vec3  grad_f;         
    vec4  l_up;           // l^u = (lx, ly, lz, -1)
    vec4  l_down;         // l_u = (lx, ly, lz, 1)
    float inv_r2_a2;
    float inv_den_f;      // [OPTIMIZATION] Cached for gradient calculation
    float num_f;          // [OPTIMIZATION] Cached numerator of f
};

// [OPTIMIZATION] Level 1: Only computes Scalars (r, f, l_u). No Gradients.
// Used for Metric Raising/Lowering, Hamiltonian, Step Size.
void ComputeGeometryScalars(vec3 X, float PhysicalSpinA, float PhysicalQ, float fade, float r_sign, out KerrGeometry geo) {
    geo.a2 = PhysicalSpinA * PhysicalSpinA;
    
    if (PhysicalSpinA == 0.0) {
        geo.r = length(X);
        geo.r2 = geo.r * geo.r;
        float inv_r = 1.0 / max(EPSILON, geo.r);
        float inv_r2 = inv_r * inv_r;
        
        // Schwarzschild limit
        geo.l_up = vec4(X * inv_r, -1.0);
        geo.l_down = vec4(X * inv_r, 1.0);
        
        geo.num_f = (2.0 * CONST_M * geo.r - PhysicalQ * PhysicalQ); // Modified for consistency logic below
        // For Schwarzschild: f = (2M/r - Q^2/r^2)
        geo.f = (2.0 * CONST_M * inv_r - (PhysicalQ * PhysicalQ) * inv_r2) * fade;
        
        geo.inv_r2_a2 = inv_r2; 
        geo.inv_den_f = 0.0; // Not used in Schw limit gradient path usually, or handled simply
        return;
    }

    geo.r = KerrSchildRadius(X, PhysicalSpinA, r_sign);
    geo.r2 = geo.r * geo.r;
    float r3 = geo.r2 * geo.r;
    float z_coord = X.y; // Standard Kerr-Schild Z is up, here Y is up
    float z2 = z_coord * z_coord;
    
    geo.inv_r2_a2 = 1.0 / (geo.r2 + geo.a2);
    
    // --- Calculate l^u ---
    // Note: Code assumes Y-up convention based on input X
    float lx = (geo.r * X.x + PhysicalSpinA * X.z) * geo.inv_r2_a2;
    float ly = X.y / geo.r;
    float lz = (geo.r * X.z - PhysicalSpinA * X.x) * geo.inv_r2_a2;
    
    geo.l_up = vec4(lx, ly, lz, -1.0);
    geo.l_down = vec4(lx, ly, lz, 1.0); 
    
    // --- Calculate f ---
    geo.num_f = 2.0 * CONST_M * r3 - PhysicalQ * PhysicalQ * geo.r2;
    float den_f = geo.r2 * geo.r2 + geo.a2 * z2;
    geo.inv_den_f = 1.0 / max(1e-20, den_f);
    geo.f = (geo.num_f * geo.inv_den_f) * fade;
}

// [OPTIMIZATION] Level 2: Computes Gradients. Assumes Scalars are already computed in 'geo'.
// Used only inside Geodesic Derivative function.
void ComputeGeometryGradients(vec3 X, float PhysicalSpinA, float PhysicalQ, float fade, inout KerrGeometry geo) {
    float inv_r = 1.0 / geo.r;
    
    if (PhysicalSpinA == 0.0) {
        // Schwarzschild gradients
        float inv_r2 = inv_r * inv_r;
        geo.grad_r = X * inv_r;
        float df_dr = (-2.0 * CONST_M + 2.0 * PhysicalQ * PhysicalQ * inv_r) * inv_r2 * fade;
        geo.grad_f = df_dr * geo.grad_r;
        return;
    }

    // --- Calculate grad r ---
    float R2 = dot(X, X);
    float D = 2.0 * geo.r2 - R2 + geo.a2;
    float denom_grad = geo.r * D;
    // Avoid division by zero
    if (abs(denom_grad) < 1e-9) denom_grad = sign(geo.r) * 1e-9;
    float inv_denom_grad = 1.0 / denom_grad;
    
    geo.grad_r = vec3(
        X.x * geo.r2,
        X.y * (geo.r2 + geo.a2),
        X.z * geo.r2
    ) * inv_denom_grad;
    
    // --- Calculate grad f ---
    float z_coord = X.y;
    float z2 = z_coord * z_coord;
    
    float term_M  = -2.0 * CONST_M * geo.r2 * geo.r2 * geo.r;
    float term_Q  = 2.0 * PhysicalQ * PhysicalQ * geo.r2 * geo.r2;
    float term_Ma = 6.0 * CONST_M * geo.a2 * geo.r * z2;
    float term_Qa = -2.0 * PhysicalQ * PhysicalQ * geo.a2 * z2;
    
    float df_dr_num_reduced = term_M + term_Q + term_Ma + term_Qa;
    // Reuse inv_den_f from scalars
    float df_dr = (geo.r * df_dr_num_reduced) * (geo.inv_den_f * geo.inv_den_f);
    
    float df_dy = -(geo.num_f * 2.0 * geo.a2 * z_coord) * (geo.inv_den_f * geo.inv_den_f);
    
    geo.grad_f = df_dr * geo.grad_r;
    geo.grad_f.y += df_dy;
    geo.grad_f *= fade;
}

// [OPTIMIZATION] Replaces mat4 GetInverseKerrMetric * vector
// Computes P^u = g^uv P_v
// g^uv = eta^uv - f * l^u * l^v
vec4 RaiseIndex(vec4 P_cov, KerrGeometry geo) {
    // eta^uv = diag(1, 1, 1, -1)
    vec4 P_flat = vec4(P_cov.xyz, -P_cov.w); 
    
    // l^u * P_v (Scalar contraction) ? No, we need l^u * l^v * P_v
    // First compute scalar K = l^v P_v = l_up . P_cov (Note: l_up components match l^v)
    // Actually: l^v P_v = l^x P_x + l^y P_y + l^z P_z + l^t P_t
    // geo.l_up = (lx, ly, lz, -1). P_cov = (Px, Py, Pz, Pt).
    // So dot product works directly.
    float L_dot_P = dot(geo.l_up, P_cov);
    
    return P_flat - geo.f * L_dot_P * geo.l_up;
}

// [OPTIMIZATION] Replaces mat4 GetKerrMetric * vector
// Computes P_u = g_uv P^v
// g_uv = eta_uv + f * l_u * l_v
vec4 LowerIndex(vec4 P_contra, KerrGeometry geo) {
    // eta_uv = diag(1, 1, 1, -1)
    vec4 P_flat = vec4(P_contra.xyz, -P_contra.w);
    
    // l_k P^k. geo.l_down = (lx, ly, lz, 1). P_contra = (Px, Py, Pz, Pt).
    // l_k P^k = lx Px + ly Py + lz Pz + 1 * Pt.
    float L_dot_P = dot(geo.l_down, P_contra);
    
    return P_flat + geo.f * L_dot_P * geo.l_down;
}

// [INPUT] a/Q: Dimensional (scaled by CONST_M)
float GetGtt(vec3 Pos, float PhysicalSpinA, float PhysicalQ, float r_sign) {
    KerrGeometry geo;
    ComputeGeometryScalars(Pos, PhysicalSpinA, PhysicalQ, 1.0, r_sign, geo);
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

// [TENSOR] Returns Initial Covariant Momentum P_u   借助ZAMO观者定义一个性质非常良好的标架，再boost到静态/落体观者系
vec4 GetInitialMomentum(
    vec3 RayDir,
    vec4 X,
    int  iObserverMode,   
    float universesign,
    float PhysicalSpinA,  
    float PhysicalQ,      
    float GravityFade
)
{
    // 1. 计算几何
    KerrGeometry geo;
    ComputeGeometryScalars(X.xyz, PhysicalSpinA, PhysicalQ, GravityFade, universesign, geo);

    // -------------------------------------------------------------------------
    // 2. 确定观者四维速度 U_up 并强制归一化
    // -------------------------------------------------------------------------
    vec4 U_up;
    
    if (iObserverMode == 0) { 
        // Static Observer
        float g_tt = -1.0 + geo.f;
        float time_comp = 1.0 / sqrt(max(1e-9, -g_tt));
        U_up = vec4(0.0, 0.0, 0.0, time_comp);
    } else { 
        // Free-Falling Observer
        float r = geo.r; float r2 = geo.r2; float a = PhysicalSpinA; float a2 = geo.a2;
        float y_phys = X.y; 
        
        float rho2 = r2 + a2 * (y_phys * y_phys) / (r2 + 1e-9);
        float Q2 = PhysicalQ * PhysicalQ;
        float MassChargeTerm = 2.0 * CONST_M * r - Q2;
        float Xi = sqrt(max(0.0, MassChargeTerm * (r2 + a2)));
        float DenomPhi = rho2 * (MassChargeTerm + Xi);
        
        float U_phi_KS = (abs(DenomPhi) > 1e-9) ? (-MassChargeTerm * a / DenomPhi) : 0.0;
        float U_r_KS = -Xi / max(1e-9, rho2);
        
        float inv_r2_a2 = 1.0 / (r2 + a2);
        float Ux_rad = (r * X.x + a * X.z) * inv_r2_a2 * U_r_KS;
        float Uz_rad = (r * X.z - a * X.x) * inv_r2_a2 * U_r_KS;
        float Uy_rad = (X.y / r) * U_r_KS;
        float Ux_tan = -X.z * U_phi_KS;
        float Uz_tan =  X.x * U_phi_KS;
        
        vec3 U_spatial = vec3(Ux_rad + Ux_tan, Uy_rad, Uz_rad + Uz_tan);
        
        float l_dot_u_spatial = dot(geo.l_down.xyz, U_spatial);
        float U_spatial_sq = dot(U_spatial, U_spatial);
        float A = -1.0 + geo.f;
        float B = 2.0 * geo.f * l_dot_u_spatial;
        float C = U_spatial_sq + geo.f * (l_dot_u_spatial * l_dot_u_spatial) + 1.0; 
        float Det = max(0.0, B*B - 4.0 * A * C);
        float Ut = (-B - sqrt(Det)) / (2.0 * A); 
        U_up = vec4(U_spatial, Ut);
    }

    // [FIX] 强制归一化 U_up，消除浮点误差
    // U . U = -1
    vec4 U_down = LowerIndex(U_up, geo);
    float U_norm_sq = dot(U_up, U_down); // 应该是 -1
    float U_correction = inversesqrt(abs(U_norm_sq)); // 修正因子，应该极接近 1.0
    U_up *= U_correction;
    U_down *= U_correction; // 更新 U_down 用于后续计算

    // -------------------------------------------------------------------------
    // 3. 构建 ZAMO 参照系并强制归一化
    // -------------------------------------------------------------------------
    vec4 zamo_cov = vec4(0.0, 0.0, 0.0, 1.0);
    vec4 zamo_contra = RaiseIndex(zamo_cov, geo); 
    
    float zamo_norm_sq = dot(zamo_cov, zamo_contra); 
    float norm_factor = inversesqrt(max(1e-9, abs(zamo_norm_sq)));
    vec4 u_ref = zamo_contra * norm_factor;
    if (u_ref.w < 0.0) u_ref = -u_ref;

    // -------------------------------------------------------------------------
    // 4. 定义射线方向 k 并强制归一化
    // -------------------------------------------------------------------------
    vec4 k_naive = vec4(-normalize(RayDir), 0.0);
    vec4 k_naive_down = LowerIndex(k_naive, geo);
    float k_dot_u = dot(k_naive_down, u_ref); 
    vec4 k_ortho = k_naive + k_dot_u * u_ref;
    
    vec4 k_ortho_down = LowerIndex(k_ortho, geo);
    float k_len_sq = dot(k_ortho, k_ortho_down);
    float k_norm = inversesqrt(max(1e-9, k_len_sq));
    vec4 k_phys = k_ortho * k_norm;

    // -------------------------------------------------------------------------
    // 5. 精确 Boost
    // -------------------------------------------------------------------------
    vec4 sum_vel = u_ref + U_up;
    float U_dot_uref = dot(U_down, u_ref); 
    float denom = 1.0 - U_dot_uref; 
    
    float k_dot_U = dot(k_phys, U_down);
    float boost_factor = k_dot_U / denom;
    
    vec4 P_final = k_phys + U_up + boost_factor * sum_vel;

    // 返回协变动量
    return LowerIndex(P_final, geo);
}

// [TENSOR] Calculates Redshift Ratio E_emit / E_obs
float CalculateFrequencyRatio(vec4 P_photon, vec4 U_emitter, vec4 U_observer, KerrGeometry geo)
{
    // P_photon is Contravariant P^u
    vec4 P_cov = LowerIndex(P_photon, geo);
    float E_emit = -dot(P_cov, U_emitter);
    float E_obs  = -dot(P_cov, U_observer);
    return E_emit / max(EPSILON, E_obs);
}

void ReflectPhotonKerrSchild(
    vec4 X, 
    inout vec4 P_mu, 
    float PhysicalSpinA, 
    float PhysicalQ, 
    float GravityFade, 
    float CurrentUniverseSign
) {
    KerrGeometry geo;
    ComputeGeometryScalars(X.xyz, PhysicalSpinA, PhysicalQ, GravityFade, CurrentUniverseSign, geo);

    float f = geo.f;
    vec4 l_lower = geo.l_down;
    float P_v = P_mu.w;

    float denom = 1.0 - f;
    if (abs(denom) < 1e-9) denom = sign(denom) * 1e-9;

    float scale_factor = (2.0 * P_v) / denom;
    vec4 direction_term = vec4(0.0, 0.0, 0.0, 1.0) - f * l_lower;

    P_mu = -P_mu + scale_factor * direction_term;
}

// -----------------------------------------------------------------------------
// 5.数值积分核心结构
// -----------------------------------------------------------------------------
struct State {
    vec4 X; // [TENSOR] Contravariant Position x^u
    vec4 P; // [TENSOR] Covariant Momentum p_u
};

// [TENSOR] Enforces Hamiltonian Constraint H = 0
// Optimized to use pre-calculated scalars if available, otherwise minimal calc.
// Optimized: Uses 'Closest to 1.0' root selection to handle negative energy/ergosphere correctly.
void ApplyHamiltonianCorrection(inout vec4 P, vec4 X, float E, float PhysicalSpinA, float PhysicalQ, float fade, float r_sign) {
    // P.w (Pt) is -E_conserved. 
    // For bound states or negative energy orbits, E might be negative relative to infinity, 
    // but locally P.w just represents the time component covariant momentum.
    P.w = -E; 
    vec3 p = P.xyz;    
    
    KerrGeometry geo;
    ComputeGeometryScalars(X.xyz, PhysicalSpinA, PhysicalQ, fade, r_sign, geo);
    
    // Equation: g^uv P_u P_v = 0
    // Metric:   g^uv = eta^uv - f * l^u * l^v
    // Expand:   (p^2 - Pt^2) - f * (l^u P_u)^2 = 0
    // l^u P_u = l.p + l^t Pt = l.p - Pt (since l^t = -1)
    
    // We want to find scalar k such that P_new = (k*p, Pt) satisfies the constraint.
    // Substitute k*p:
    // (k^2 p^2 - Pt^2) - f * (k * (l.p) - Pt)^2 = 0
    
    // Expand quadratic for k: A*k^2 + B*k + C = 0
    
    float L_dot_p_s = dot(geo.l_up.xyz, p);
    float Pt = P.w; 
    
    // A = p^2 - f * (l.p)^2
    float p2 = dot(p, p);
    float Coeff_A = p2 - geo.f * L_dot_p_s * L_dot_p_s;
    
    // Term inside square: (k * L.p - Pt)^2 = k^2(L.p)^2 - 2k(L.p)Pt + Pt^2
    // Middle term of expansion: -f * (-2k * L.p * Pt) = 2 * f * L.p * Pt
    float Coeff_B = 2.0 * geo.f * L_dot_p_s * Pt;
    
    // Constant term: -Pt^2 - f * Pt^2 = -Pt^2 * (1 + f)
    float Coeff_C = -Pt * Pt * (1.0 + geo.f);
    
    float disc = Coeff_B * Coeff_B - 4.0 * Coeff_A * Coeff_C;
    
    if (disc >= 0.0) {
        float sqrtDisc = sqrt(disc);
        float denom = 2.0 * Coeff_A;
        
        // Avoid division by zero (linear trajectory case)
        if (abs(denom) > 1e-9) {
            float k1 = (-Coeff_B + sqrtDisc) / denom;
            float k2 = (-Coeff_B - sqrtDisc) / denom;
            
            // CRITICAL FIX:
            // Always pick the root closer to 1.0.
            // This assumes the numerical drift is small.
            // It correctly handles negative energy and ergosphere region crossing
            // where coefficients might flip signs.
            float dist1 = abs(k1 - 1.0);
            float dist2 = abs(k2 - 1.0);
            
            float k = (dist1 < dist2) ? k1 : k2;
            
            // Safety: Only apply if k is reasonable (prevent NaN or Explosion)
            // Ideally k should be very close to 1.0 (e.g. 0.99 ~ 1.01)
            //f (k > 0.0 && abs(k - 1.0) < 0.1) {
                P.xyz *= mix(k,1.0,clamp(abs(k-1.0)/0.1-1.0,0.0,1.0));
            //}
        }
    }
}

// [OPTIMIZATION] Derivative function now takes Geometry as input
// To allow reuse of pre-computed geometry.
State GetDerivativesAnalytic(State S, float PhysicalSpinA, float PhysicalQ, float fade, inout KerrGeometry geo) {
    State deriv;
    
    // Note: 'geo' input must have Scalars computed. We verify/compute Gradients here.
    ComputeGeometryGradients(S.X.xyz, PhysicalSpinA, PhysicalQ, fade, geo);
    
    // l^u * P_u
    float l_dot_P = dot(geo.l_up.xyz, S.P.xyz) + geo.l_up.w * S.P.w;
    
    // dx^u/dlambda = g^uv p_v = P_flat^u - f * (l.P) * l^u
    // Inline RaiseIndex logic to reuse l_dot_P
    vec4 P_flat = vec4(S.P.xyz, -S.P.w); 
    deriv.X = P_flat - geo.f * l_dot_P * geo.l_up;
    
    // dp_u/dlambda = -dH/dx^u
    vec3 grad_A = (-2.0 * geo.r * geo.inv_r2_a2) * geo.inv_r2_a2 * geo.grad_r;
    
    float rx_az = geo.r * S.X.x + PhysicalSpinA * S.X.z;
    float rz_ax = geo.r * S.X.z - PhysicalSpinA * S.X.x;
    
    vec3 d_num_lx = S.X.x * geo.grad_r; 
    d_num_lx.x += geo.r; 
    d_num_lx.z += PhysicalSpinA;
    vec3 grad_lx = geo.inv_r2_a2 * d_num_lx + rx_az * grad_A;
    
    vec3 grad_ly = (geo.r * vec3(0.0, 1.0, 0.0) - S.X.y * geo.grad_r) / geo.r2;
    
    vec3 d_num_lz = S.X.z * geo.grad_r;
    d_num_lz.z += geo.r;
    d_num_lz.x -= PhysicalSpinA;
    vec3 grad_lz = geo.inv_r2_a2 * d_num_lz + rz_ax * grad_A;
    
    vec3 P_dot_grad_l = S.P.x * grad_lx + S.P.y * grad_ly + S.P.z * grad_lz;
    
    // Force = 0.5 * [ grad_f * (l.P)^2 + 2f(l.P) * grad(l.P) ]
    vec3 Force = 0.5 * ( (l_dot_P * l_dot_P) * geo.grad_f + (2.0 * geo.f * l_dot_P) * P_dot_grad_l );
    
    deriv.P = vec4(Force, 0.0); 
    
    return deriv;
}

// [HELPER] 检测试探步是否穿过奇环面
float GetIntermediateSign(vec4 StartX, vec4 CurrentX, float CurrentSign, float PhysicalSpinA) {
    if (StartX.y * CurrentX.y < 0.0) {
        float t = StartX.y / (StartX.y - CurrentX.y);
        float rho_cross = length(mix(StartX.xz, CurrentX.xz, t));
        if (rho_cross < abs(PhysicalSpinA)) {
            return -CurrentSign;
        }
    }
    return CurrentSign;
}

// [MATH] RK4 Integrator with Geometry Reuse
// [OPTIMIZATION] Accepts 'geo0' (Geometry at X) to skip first ComputeGeometry call.
// [ADD NEW FUNCTION] Optimized RK4 that reuses pre-calculated k1 and geometry
// 这一步是为了防止为了计算步长多调用一次 GetDerivatives 导致性能下降
void StepGeodesicRK4_Optimized(
    inout vec4 X, inout vec4 P, 
    float E, float dt, 
    float PhysicalSpinA, float PhysicalQ, float fade, float r_sign, 
    KerrGeometry geo0, 
    State k1 // <--- 输入预计算的 k1
) {
    State s0; s0.X = X; s0.P = P;

    // --- k1 Step (SKIPPED - REUSING INPUT) ---
    // State k1 = GetDerivativesAnalytic(s0, PhysicalSpinA, PhysicalQ, fade, geo0);

    // --- k2 Step ---
    State s1; 
    s1.X = s0.X + 0.5 * dt * k1.X; 
    s1.P = s0.P + 0.5 * dt * k1.P;
    float sign1 = GetIntermediateSign(s0.X, s1.X, r_sign, PhysicalSpinA);
    KerrGeometry geo1;
    ComputeGeometryScalars(s1.X.xyz, PhysicalSpinA, PhysicalQ, fade, sign1, geo1);
    State k2 = GetDerivativesAnalytic(s1, PhysicalSpinA, PhysicalQ, fade, geo1);

    // --- k3 Step ---
    State s2; 
    s2.X = s0.X + 0.5 * dt * k2.X; 
    s2.P = s0.P + 0.5 * dt * k2.P;
    float sign2 = GetIntermediateSign(s0.X, s2.X, r_sign, PhysicalSpinA);
    KerrGeometry geo2;
    ComputeGeometryScalars(s2.X.xyz, PhysicalSpinA, PhysicalQ, fade, sign2, geo2);
    State k3 = GetDerivativesAnalytic(s2, PhysicalSpinA, PhysicalQ, fade, geo2);

    // --- k4 Step ---
    State s3; 
    s3.X = s0.X + dt * k3.X; 
    s3.P = s0.P + dt * k3.P;
    float sign3 = GetIntermediateSign(s0.X, s3.X, r_sign, PhysicalSpinA);
    KerrGeometry geo3;
    ComputeGeometryScalars(s3.X.xyz, PhysicalSpinA, PhysicalQ, fade, sign3, geo3);
    State k4 = GetDerivativesAnalytic(s3, PhysicalSpinA, PhysicalQ, fade, geo3);

    // --- Final Integration ---
    vec4 finalX = s0.X + (dt / 6.0) * (k1.X + 2.0 * k2.X + 2.0 * k3.X + k4.X);
    vec4 finalP = s0.P + (dt / 6.0) * (k1.P + 2.0 * k2.P + 2.0 * k3.P + k4.P);

    // --- Hamiltonian Correction Logic ---
    float finalSign = GetIntermediateSign(s0.X, finalX, r_sign, PhysicalSpinA);
    if(finalSign>0){
    ApplyHamiltonianCorrection(finalP, finalX, E, PhysicalSpinA, PhysicalQ, fade, finalSign);
    }
    X = finalX;
    P = finalP;
}
// =============================================================================
// SECTION 6: 体积渲染函数 (吸积盘与喷流)
// =============================================================================

vec4 DiskColor(vec4 BaseColor, float StepLength, vec4 RayPos, vec4 LastRayPos,
               vec3 RayDir, vec3 LastRayDir,vec4 iP_cov, float iE_obs,
               float InterRadius, float OuterRadius, float Thin, float Hopper, float Brightmut, float Darkmut, float Reddening, float Saturation, float DiskTemperatureArgument,
               float BlackbodyIntensityExponent, float RedShiftColorExponent, float RedShiftIntensityExponent,
               float PeakTemperature, float ShiftMax, 
               float PhysicalSpinA, 
               float PhysicalQ     
               ) 
{
    vec4 CurrentResult = BaseColor;

    // --- [性能优化] 多级包围盒检测 ---
    
    // 1. 垂直包围盒 (Vertical Bounds)
    float MaxDiskHalfHeight = Thin + max(0.0, Hopper * OuterRadius) + 2.0; 
    
    if (LastRayPos.y > MaxDiskHalfHeight && RayPos.y > MaxDiskHalfHeight) return BaseColor;
    if (LastRayPos.y < -MaxDiskHalfHeight && RayPos.y < -MaxDiskHalfHeight) return BaseColor;

    // 2. 径向包围盒 (Radial Bounds - Cylinder Check)
    vec2 P0 = LastRayPos.xz;
    vec2 P1 = RayPos.xz;
    vec2 V  = P1 - P0;
    float LenSq = dot(V, V);
    
    float t_closest = (LenSq > 1e-8) ? clamp(-dot(P0, V) / LenSq, 0.0, 1.0) : 0.0;
    vec2 ClosestPoint = P0 + V * t_closest;
    float MinDistSq = dot(ClosestPoint, ClosestPoint);
    
    float BoundarySq = (OuterRadius * 1.1) * (OuterRadius * 1.1);
    
    if (MinDistSq > BoundarySq) return BaseColor;

    // --- 包围盒检测结束 ---

    vec3 StartPos = LastRayPos.xyz; 
    vec3 DirVec   = RayDir; 
    
    float StartTimeLag = LastRayPos.w;
    float EndTimeLag   = RayPos.w;

    float TotalDist = StepLength;
    float TraveledDist = 0.0;
    
    float R_Start = KerrSchildRadius(StartPos, PhysicalSpinA, 1.0);
    float R_End   = KerrSchildRadius(RayPos.xyz, PhysicalSpinA, 1.0);
    float MaxR = max(R_Start, R_End);
    
    if ( MaxR < InterRadius * 0.9) 
    {
        return BaseColor;
    }
   
    int MaxSubSteps = 32; 
    
    for (int i = 0; i < MaxSubSteps; i++)
    {
        if (CurrentResult.a > 0.99) break;
        if (TraveledDist >= TotalDist) break;

        vec3 CurrentPos = StartPos + DirVec * TraveledDist;
        
        float TimeInterpolant = min(1.0, TraveledDist / max(1e-9, TotalDist));
        float CurrentRayTimeLag = mix(StartTimeLag, EndTimeLag, TimeInterpolant);
        float EmissionTime = iBlackHoleTime + CurrentRayTimeLag;

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
        
        float PosR = KerrSchildRadius(SamplePos, PhysicalSpinA, 1.0);
        float PosY = SamplePos.y;
        
        float GeometricThin = Thin + max(0.0, (length(SamplePos.xz) - 3.0) * Hopper);
        
        // [修改 1] 计算内侧云参数与包围盒
        float InterCloudEffectiveRadius = (PosR - InterRadius) / min(OuterRadius - InterRadius, 12.0);
        float InnerCloudBound = max(GeometricThin, Thin * 1.0) * (1.0 - 5.0 * pow(InterCloudEffectiveRadius, 2.0));
        
        // [修改 2] 关键修复：外层包围盒必须取主盘与内侧云的并集
        // GeometricThin * 1.5 是主盘噪声的包围盒，InnerCloudBound 是内侧云的包围盒
        float UnionBound = max(GeometricThin * 1.5, max(0.0, InnerCloudBound));

        if (abs(PosY) < UnionBound && PosR < OuterRadius && PosR > InterRadius)
        {
             float NoiseLevel = max(0.0, 2.0 - 0.6 * GeometricThin);
             float x = (PosR - InterRadius) / max(1e-6, OuterRadius - InterRadius);
             float a_param = max(1.0, (OuterRadius - InterRadius) / 10.0);
             float EffectiveRadius = (-1.0 + sqrt(max(0.0, 1.0 + 4.0 * a_param * a_param * x - 4.0 * x * a_param))) / (2.0 * a_param - 2.0);
             if(a_param == 1.0) EffectiveRadius = x;
             
             float DenAndThiFactor = Shape(EffectiveRadius, 0.9, 1.5);

             float RotPosR_ForThick = PosR + 0.25 / 3.0 * EmissionTime;
             float PosLogTheta_ForThick = Vec2ToTheta(SamplePos.zx, vec2(cos(-2.0 * log(max(1e-6, PosR))), sin(-2.0 * log(max(1e-6, PosR)))));
             float ThickNoise = GenerateAccretionDiskNoise(vec3(1.5 * PosLogTheta_ForThick, RotPosR_ForThick, 0.0), -0.7 + NoiseLevel, 1.3 + NoiseLevel, 80.0);
             
             float PerturbedThickness = max(1e-6, GeometricThin * DenAndThiFactor * (0.4 + 0.6 * clamp(GeometricThin - 0.5, 0.0, 2.5) / 2.5 + (1.0 - (0.4 + 0.6 * clamp(GeometricThin - 0.5, 0.0, 2.5) / 2.5)) * SoftSaturate(ThickNoise)));

             // 使用并集条件进入详细计算
             if ((abs(PosY) < PerturbedThickness) || (abs(PosY) < InnerCloudBound))
             {
                 float AngularVelocity = GetKeplerianAngularVelocity(max(InterRadius, PosR), 1.0, PhysicalSpinA, PhysicalQ);
                 
                 float u = sqrt(max(1e-6, PosR));
                 float k_cubed = PhysicalSpinA * 0.70710678;
                 float SpiralTheta;
                 if (abs(k_cubed) < 0.001 * u * u * u) {
                     float inv_u = 1.0 / u; float eps3 = k_cubed * pow(inv_u, 3.0);
                     SpiralTheta = -16.9705627 * inv_u * (1.0 - 0.25 * eps3 + 0.142857 * eps3 * eps3);
                 } else {
                     float k = sign(k_cubed) * pow(abs(k_cubed), 0.33333333);
                     float logTerm = (PosR - k*u + k*k) / max(1e-9, pow(u+k, 2.0));
                     SpiralTheta = (5.6568542 / k) * (0.5 * log(max(1e-9, logTerm)) + 1.7320508 * (atan(2.0*u - k, 1.7320508 * k) - 1.5707963));
                 }
                 float PosTheta = Vec2ToTheta(SamplePos.zx, vec2(cos(-SpiralTheta), sin(-SpiralTheta)));
                 float PosLogarithmicTheta = Vec2ToTheta(SamplePos.zx, vec2(cos(-2.0 * log(max(1e-6, PosR))), sin(-2.0 * log(max(1e-6, PosR)))));
                 
                 vec3 FluidVel = AngularVelocity * vec3(SamplePos.z, 0.0, -SamplePos.x);
                 vec4 U_fluid_unnorm = vec4(FluidVel, 1.0); 
                 KerrGeometry geo_sample;
                 ComputeGeometryScalars(SamplePos, PhysicalSpinA, PhysicalQ, 1.0, 1.0, geo_sample);
                 vec4 U_fluid_lower = LowerIndex(U_fluid_unnorm, geo_sample);
                 float norm_sq = dot(U_fluid_unnorm, U_fluid_lower);
                 vec4 U_fluid = U_fluid_unnorm * inversesqrt(max(1e-6, abs(norm_sq)));
                 float E_emit = -dot(iP_cov, U_fluid);
                 float FreqRatio = iE_obs / max(1e-6, E_emit);





                 float DiskTemperature = pow(DiskTemperatureArgument * pow(1.0 / max(1e-6, PosR), 3.0) * max(1.0 - sqrt(InterRadius / max(1e-6, PosR)), 0.000001), 0.25);
                 float VisionTemperature = DiskTemperature * pow(FreqRatio, RedShiftColorExponent); 
                 float BrightWithoutRedshift = 0.05 * min(OuterRadius / (1000.0), 1000.0 / OuterRadius) + 0.55 / exp(5.0 * EffectiveRadius) * mix(0.2 + 0.8 * abs(DirVec.y), 1.0, clamp(GeometricThin - 0.8, 0.2, 1.0)); 
                 BrightWithoutRedshift *= pow(DiskTemperature / PeakTemperature, BlackbodyIntensityExponent); 
                 
                 float RotPosR = PosR + 0.25 / 3.0 * EmissionTime;
                 float Density = DenAndThiFactor;
                 
                 vec4 SampleColor = vec4(0.0);

                 // 1. 主盘噪声计算
                 if (abs(PosY) < PerturbedThickness) 
                 {
                     float Levelmut = 0.91 * log(1.0 + (0.06 / 0.91 * max(0.0, min(1000.0, PosR) - 10.0)));
                     float Conmut = 80.0 * log(1.0 + (0.1 * 0.06 * max(0.0, min(1000000.0, PosR) - 10.0)));
                     
                     SampleColor = vec4(GenerateAccretionDiskNoise(vec3(0.1 * RotPosR, 0.1 * PosY, 0.02 * pow(OuterRadius, 0.7) * PosTheta), NoiseLevel + 2.0 - Levelmut, NoiseLevel + 4.0 - Levelmut, 80.0 - Conmut)); 
                     
                     if(PosTheta + kPi < 0.1 * kPi) {
                         SampleColor *= (PosTheta + kPi) / (0.1 * kPi);
                         SampleColor += (1.0 - ((PosTheta + kPi) / (0.1 * kPi))) * vec4(GenerateAccretionDiskNoise(vec3(0.1 * RotPosR, 0.1 * PosY, 0.02 * pow(OuterRadius, 0.7) * (PosTheta + 2.0 * kPi)), NoiseLevel + 2.0 - Levelmut, NoiseLevel + 4.0 - Levelmut, 80.0 - Conmut));
                     }
                     
                     if(PosR > max(0.15379 * OuterRadius, 0.15379 * 64.0)) {
                         float TimeShiftedRadiusTerm = PosR * (4.65114e-6) - 0.1 / 3.0 * EmissionTime;
                         float Spir = (GenerateAccretionDiskNoise(vec3(0.1 * (TimeShiftedRadiusTerm - 0.08 * OuterRadius * PosLogarithmicTheta), 0.1 * PosY, 0.02 * pow(OuterRadius, 0.7) * PosLogarithmicTheta), NoiseLevel + 2.0 - Levelmut, NoiseLevel + 3.0 - Levelmut, 80.0 - Conmut)); 
                         if(PosLogarithmicTheta + kPi < 0.1 * kPi) {
                             Spir *= (PosLogarithmicTheta + kPi) / (0.1 * kPi);
                             Spir += (1.0 - ((PosLogarithmicTheta + kPi) / (0.1 * kPi))) * (GenerateAccretionDiskNoise(vec3(0.1 * (TimeShiftedRadiusTerm - 0.08 * OuterRadius * (PosLogarithmicTheta + 2.0 * kPi)), 0.1 * PosY, 0.02 * pow(OuterRadius, 0.7) * (PosLogarithmicTheta + 2.0 * kPi)), NoiseLevel + 2.0 - Levelmut, NoiseLevel + 3.0 - Levelmut, 80.0 - Conmut));
                         }
                         SampleColor *= (mix(1.0, clamp(0.7 * Spir * 1.5 - 0.5, 0.0, 3.0), 0.5 + 0.5 * max(-1.0, 1.0 - exp(-1.5 * 0.1 * (100.0 * PosR / max(OuterRadius, 64.0) - 20.0)))));
                     }

                     float VerticalMixFactor = max(0.0, (1.0 - abs(PosY) / PerturbedThickness)); 
                     Density *= 0.7 * VerticalMixFactor * Density;
                     SampleColor.xyz *= Density * 1.4;
                     SampleColor.a *= (Density) * (Density) / 0.3;
                     
                     float RelHeight = clamp(abs(PosY) / PerturbedThickness, 0.0, 1.0);
                     SampleColor.xyz *= max(0.0, (0.2 + 2.0 * sqrt(max(0.0, RelHeight * RelHeight + 0.001))));
                 }

                 // [修改 3] 内侧点缀云（Dust）：恢复独立旋转逻辑，并使用修正后的 InnerCloudBound
                 // 计算内侧独立坐标系
                 float InnerAngVel = GetKeplerianAngularVelocity(3.0, 1.0, PhysicalSpinA, PhysicalQ);
                 float InnerCloudTimePhase = kPi / (kPi / max(1e-6, InnerAngVel)) * EmissionTime; 
                 // 注意：这里使用 0.666666 * InnerCloudTimePhase 来匹配 Schwarzschild 版本的速度感
                 float InnerRotArg = 0.666666 * InnerCloudTimePhase;
                 float PosThetaForInnerCloud = Vec2ToTheta(SamplePos.zx, vec2(cos(InnerRotArg), sin(InnerRotArg)));

                 if (abs(PosY) < InnerCloudBound) 
                 {
                     float DustIntensity = max(1.0 - pow(PosY / (GeometricThin  * max(1.0 - 5.0 * pow(InterCloudEffectiveRadius, 2.0), 0.0001)), 2.0), 0.0);
                     
                     if (DustIntensity > 0.0) {
                        float DustNoise = GenerateAccretionDiskNoise(
                            vec3(1.5 * fract((1.5 * PosThetaForInnerCloud + InnerCloudTimePhase) / 2.0 / kPi) * 2.0 * kPi, PosR, PosY), 
                            0.0, 6.0, 80.0
                        );
                        float DustVal = DustIntensity * DustNoise;
                         
                        float ApproxDiskDirY =  DirVec.y; 
                        SampleColor += 0.02 * vec4(vec3(DustVal), 0.2 * DustVal) * sqrt(max(0.0, 1.0001 - ApproxDiskDirY * ApproxDiskDirY) * min(1.0, 1 * 1));
                     }
                 }

                 SampleColor.xyz *= BrightWithoutRedshift * KelvinToRgb(VisionTemperature); 
                 SampleColor.xyz *= min(pow(FreqRatio, RedShiftIntensityExponent), ShiftMax); 
                 SampleColor.xyz *= min(1.0, 1.3 * (OuterRadius - PosR) / (OuterRadius - InterRadius)); 
                 SampleColor.a   *= 0.125;
                 
                 vec4 BoostFactor = max(
                    mix(vec4(5.0 / (max(Thin, 0.2) + (0.0 + Hopper * 0.5) * OuterRadius)), vec4(vec3(0.3 + 0.7 * 5.0 / (Thin + (0.0 + Hopper * 0.5) * OuterRadius)), 1.0), 0.0),
                    mix(vec4(100.0 / OuterRadius), vec4(vec3(0.3 + 0.7 * 100.0 / OuterRadius), 1.0), exp(-pow(20.0 * PosR / OuterRadius, 2.0)))
                 );
                 SampleColor *= BoostFactor;
                 SampleColor.xyz *= mix(1.0, max(1.0, abs(DirVec.y) / 0.2), clamp(0.3 - 0.6 * (PerturbedThickness / max(1e-6, Density) - 1.0), 0.0, 0.3));
                 SampleColor.xyz *=1.0+1.2*max(0.0,max(0.0,min(1.0,3.0-2.0*Thin))*min(0.5,1.0-5.0*Hopper));
                 SampleColor.xyz *= Brightmut;
                 SampleColor.a   *= Darkmut;
                 
                 vec4 StepColor = SampleColor * dt;
                 
                 // [修改 4] 颜色混合逻辑：保持你提供的基于透射率衰减后再计算饱和度的顺序
                 float aR = 1.0 + Reddening * (1.0 - 1.0);
                 float aG = 1.0 + Reddening * (3.0 - 1.0);
                 float aB = 1.0 + Reddening * (6.0 - 1.0);
                 
                 float Sum_rgb = (StepColor.r + StepColor.g + StepColor.b) * pow(1.0 - CurrentResult.a, aG);
                 Sum_rgb *= 1.0;
                 
                 float r001 = 0.0;
                 float g001 = 0.0;
                 float b001 = 0.0;
                     
                 float Denominator = StepColor.r*pow(1.0 - CurrentResult.a, aR) + StepColor.g*pow(1.0 - CurrentResult.a, aG) + StepColor.b*pow(1.0 - CurrentResult.a, aB);
                 
                 if (Denominator > 0.000001)
                 {
                     r001 = Sum_rgb * StepColor.r * pow(1.0 - CurrentResult.a, aR) / Denominator;
                     g001 = Sum_rgb * StepColor.g * pow(1.0 - CurrentResult.a, aG) / Denominator;
                     b001 = Sum_rgb * StepColor.b * pow(1.0 - CurrentResult.a, aB) / Denominator;
                     
                    r001 *= pow(3.0*r001/(r001+g001+b001), Saturation);
                    g001 *= pow(3.0*g001/(r001+g001+b001), Saturation);
                    b001 *= pow(3.0*b001/(r001+g001+b001), Saturation);
                 }
                 
                 CurrentResult.r = CurrentResult.r + r001;
                 CurrentResult.g = CurrentResult.g + g001;
                 CurrentResult.b = CurrentResult.b + b001;
                 CurrentResult.a = CurrentResult.a + StepColor.a * pow((1.0 - CurrentResult.a), 1.0);

            }
        }
        TraveledDist += dt;
    }
    
    return CurrentResult;
}
vec4 JetColor(vec4 BaseColor, float StepLength, vec4 RayPos, vec4 LastRayPos,
              vec3 RayDir, vec3 LastRayDir,vec4 iP_cov, float iE_obs,
              float InterRadius, float OuterRadius, float JetRedShiftIntensityExponent, float JetBrightmut, float JetReddening, float JetSaturation, float AccretionRate, float JetShiftMax, 
              float PhysicalSpinA, 
              float PhysicalQ    
              ) 
{
    vec4 CurrentResult = BaseColor;
    vec3 StartPos = LastRayPos.xyz; 
    vec3 DirVec   = RayDir; 
    
    // [NaN保护] 
    if (any(isnan(StartPos)) || any(isinf(StartPos))) return BaseColor;

    float StartTimeLag = LastRayPos.w;
    float EndTimeLag   = RayPos.w;

    float TotalDist = StepLength;
    float TraveledDist = 0.0;
    
    float R_Start = length(StartPos.xz);
    float R_End   = length(RayPos.xyz); 
    float MaxR_XZ = max(R_Start, R_End);
    float MaxY    = max(abs(StartPos.y), abs(RayPos.y));
    
    if (MaxR_XZ > OuterRadius * 1.5 && MaxY < OuterRadius) return BaseColor;

    int MaxSubSteps = 32; 
    
    for (int i = 0; i < MaxSubSteps; i++)
    {
        if (TraveledDist >= TotalDist) break;

        vec3 CurrentPos = StartPos + DirVec * TraveledDist;
        
        float TimeInterpolant = min(1.0, TraveledDist / max(1e-9, TotalDist));
        float CurrentRayTimeLag = mix(StartTimeLag, EndTimeLag, TimeInterpolant);
        float EmissionTime = iBlackHoleTime + CurrentRayTimeLag;

        float DistanceToBlackHole = length(CurrentPos); 
        float SmallStepBoundary = max(OuterRadius, 12.0);
        float StepSize = 1.0; 
        
        StepSize *= 0.15 + 0.25 * min(max(0.0, 0.5 * (0.5 * DistanceToBlackHole / max(10.0 , SmallStepBoundary) - 1.0)), 1.0);
        if ((DistanceToBlackHole) >= 2.0 * SmallStepBoundary) StepSize *= DistanceToBlackHole;
        else if ((DistanceToBlackHole) >= 1.0 * SmallStepBoundary) StepSize *= ((1.0 + 0.25 * max(DistanceToBlackHole - 12.0, 0.0)) * (2.0 * SmallStepBoundary - DistanceToBlackHole) + DistanceToBlackHole * (DistanceToBlackHole - SmallStepBoundary)) / SmallStepBoundary;
        else StepSize *= min(1.0 + 0.25 * max(DistanceToBlackHole - 12.0, 0.0), DistanceToBlackHole);
        
        float dt = min(StepSize, TotalDist - TraveledDist);
        float Dither = RandomStep(10000.0 * (RayPos.zx / max(1e-6, OuterRadius)), iTime * 4.0 + float(i) * 0.1337);
        vec3 SamplePos = CurrentPos + DirVec * dt * Dither;
        
        float PosR = KerrSchildRadius(SamplePos, PhysicalSpinA, 1.0);
        float PosY = SamplePos.y;
        float RhoSq = dot(SamplePos.xz, SamplePos.xz);
        float Rho = sqrt(RhoSq);
        
        vec4 AccumColor = vec4(0.0);
        bool InJet = false;

        if (RhoSq < 2.0 * InterRadius * InterRadius + 0.03 * 0.03 * PosY * PosY && PosR < sqrt(2.0) * OuterRadius)
        {
            InJet = true;
            float Shape = 1.0 / sqrt(max(1e-9, InterRadius * InterRadius + 0.02 * 0.02 * PosY * PosY));
            
            float noiseInput = 0.3 * (EmissionTime - 1.0 / 0.8 * abs(abs(PosY) + 100.0 * (RhoSq / max(0.1, PosR)))) / max(1e-6, (OuterRadius / 100.0)) / (1.0 / 0.8);
            float a = mix(0.7 + 0.3 * PerlinNoise1D(noiseInput), 1.0, exp(-0.01 * 0.01 * PosY * PosY));
            
            vec4 Col = vec4(1.0, 1.0, 1.0, 0.5) * max(0.0, 1.0 - 5.0 * Shape * abs(1.0 - pow(Rho * Shape, 2.0))) * Shape;
            Col *= a;
            Col *= max(0.0, 1.0 - 1.0 * exp(-0.0001 * PosY / max(1e-6, InterRadius) * PosY / max(1e-6, InterRadius)));
            Col *= exp(-4.0 / (2.0) * PosR / max(1e-6, OuterRadius) * PosR / max(1e-6, OuterRadius));
            Col *= 0.5;
            AccumColor += Col;
        }

        float Wid = abs(PosY);
        if (Rho < 1.3 * InterRadius + 0.25 * Wid && Rho > 0.7 * InterRadius + 0.15 * Wid && PosR < 30.0 * InterRadius)
        {
            InJet = true;
            float InnerTheta = 2.0 * GetKeplerianAngularVelocity(InterRadius, 1.0, PhysicalSpinA, PhysicalQ) * (EmissionTime - 1.0 / 0.8 * abs(PosY));
            float Shape = 1.0 / max(1e-9, (InterRadius + 0.2 * Wid));
            
            float Twist = 0.2 * (1.1 - exp(-0.1 * 0.1 * PosY * PosY)) * (PerlinNoise1D(0.35 * (EmissionTime - 1.0 / 0.8 * abs(PosY)) / (1.0 / 0.8)) - 0.5);
            vec2 TwistedPos = SamplePos.xz + Twist * vec2(cos(0.666666 * InnerTheta), -sin(0.666666 * InnerTheta));
            
            vec4 Col = vec4(1.0, 1.0, 1.0, 0.5) * max(0.0, 1.0 - 2.0 * abs(1.0 - pow(length(TwistedPos) * Shape, 2.0))) * Shape;
            Col *= 1.0 - exp(-PosY / max(1e-6, InterRadius) * PosY / max(1e-6, InterRadius));
            Col *= exp(-0.005 * PosY / max(1e-6, InterRadius) * PosY / max(1e-6, InterRadius));
            Col *= 0.5;
            AccumColor += Col;
        }

        if (InJet)
        {
            vec3  JetVelDir = vec3(0.0, sign(PosY), 0.0);
            vec3 RotVelDir = normalize(vec3(SamplePos.z, 0.0, -SamplePos.x));
            vec3 FinalSpatialVel = JetVelDir * 0.9 + RotVelDir * 0.05; 
            
            vec4 U_jet_unnorm = vec4(FinalSpatialVel, 1.0);
            KerrGeometry geo_sample;
            ComputeGeometryScalars(SamplePos, PhysicalSpinA, PhysicalQ, 1.0, 1.0, geo_sample);
            vec4 U_fluid_lower = LowerIndex(U_jet_unnorm, geo_sample);
            float norm_sq = dot(U_jet_unnorm, U_fluid_lower);
            vec4 U_jet = U_jet_unnorm * inversesqrt(max(1e-6, abs(norm_sq)));
            
            float E_emit = -dot(iP_cov, U_jet);
            float FreqRatio = iE_obs / max(1e-6, E_emit);

            float JetTemperature = 100000.0 * FreqRatio; 
            AccumColor.xyz *= KelvinToRgb(JetTemperature);
            AccumColor.xyz *= min(pow(FreqRatio, JetRedShiftIntensityExponent), JetShiftMax);
            
            AccumColor *= JetBrightmut * (0.5 + 0.5 * tanh(log(max(1e-6, AccretionRate)) + 1.0));
            AccumColor.a *= 0.0; 
                 


                 float aR = 1.0+ JetReddening*(1.0-1.0);
                 float aG = 1.0+ JetReddening*(3.0-1.0);
                 float aB = 1.0+ JetReddening*(6.0-1.0);
                 float Sum_rgb = (AccumColor.r + AccumColor.g + AccumColor.b)*pow(1.0 - CurrentResult.a, aG);
                 Sum_rgb *= 1.0;
                 
                 float r001 = 0.0;
                 float g001 = 0.0;
                 float b001 = 0.0;
                     
                 float Denominator = AccumColor.r*pow(1.0 - CurrentResult.a, aR) + AccumColor.g*pow(1.0 - CurrentResult.a, aG) + AccumColor.b*pow(1.0 - CurrentResult.a, aB);
                 if (Denominator > 0.000001)
                 {
                     r001 = Sum_rgb * AccumColor.r * pow(1.0 - CurrentResult.a, aR) / Denominator;
                     g001 = Sum_rgb * AccumColor.g * pow(1.0 - CurrentResult.a, aG) / Denominator;
                     b001 = Sum_rgb * AccumColor.b * pow(1.0 - CurrentResult.a, aB) / Denominator;
                     
                    r001 *= pow(3.0*r001/(r001+g001+b001),JetSaturation);
                    g001 *= pow(3.0*g001/(r001+g001+b001),JetSaturation);
                    b001 *= pow(3.0*b001/(r001+g001+b001),JetSaturation);
                     
                 }
                 
                 CurrentResult.r=CurrentResult.r + r001;
                 CurrentResult.g=CurrentResult.g + g001;
                 CurrentResult.b=CurrentResult.b + b001;
                 CurrentResult.a=CurrentResult.a + AccumColor.a * pow((1.0 - CurrentResult.a),1.0);
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
    // -------------------------------------------------------------------------
    // 1. 基础输入与屏幕坐标 (Basic Input & Screen Coordinates)
    // -------------------------------------------------------------------------
    vec2 FragUv = gl_FragCoord.xy / iResolution.xy;
    FragUv.y = 1.0 - FragUv.y; 
    float Fov = tan(iFovRadians / 2.0);

    // TAA Jitter
    vec2 Jitter = vec2(RandomStep(FragUv, fract(iTime * 1.0 + 0.5)), RandomStep(FragUv, fract(iTime * 1.0))) / iResolution;
    vec3 ViewDirLocal = FragUvToDir(FragUv + 0.5 * Jitter, Fov, iResolution); 

    // -------------------------------------------------------------------------
    // 2. 物理常数与黑洞参数计算 (Physical Constants & Black Hole Parameters)
    // -------------------------------------------------------------------------
    // [Spin & ISCO Parameters]
    float iSpinclamp = clamp(iSpin, -0.99, 0.99);
    float a2 = iSpinclamp * iSpinclamp;
    float abs_a = abs(iSpinclamp);
    float common_term = pow(1.0 - a2, 1.0/3.0);
    float Z1 = 1.0 + common_term * (pow(1.0 + abs_a, 1.0/3.0) + pow(1.0 - abs_a, 1.0/3.0));
    float Z2 = sqrt(3.0 * a2 + Z1 * Z1);
    float root_term = sqrt(max(0.0, (3.0 - Z1) * (3.0 + Z1 + 2.0 * Z2))); 
    float Rms_M = 3.0 + Z2 - (sign(iSpinclamp) * root_term); 
    float RmsRatio = Rms_M / 2.0; 
    float AccretionEffective = sqrt(max(0.001, 1.0 - (2.0 / 3.0) / Rms_M));

    // [Temperature & Accretion]
    const float kPhysicsFactor = 1.52491e30; 
    float DiskArgument = kPhysicsFactor / iBlackHoleMassSol * (iMu / AccretionEffective) * (iAccretionRate);
    float PeakTemperature = pow(DiskArgument * 0.05665278, 0.25);

    // [Metric Constants]
    float PhysicalSpinA = -iSpin * CONST_M;  
    float PhysicalQ     = iQ * CONST_M; 
    
    // [Horizons]
    float HorizonDiscrim = 0.25 - PhysicalSpinA * PhysicalSpinA - PhysicalQ * PhysicalQ;
    float EventHorizonR = 0.5 + sqrt(max(0.0, HorizonDiscrim));
    float InnerHorizonR = 0.5 - sqrt(max(0.0, HorizonDiscrim));
    bool  bIsNakedSingularity = HorizonDiscrim < 0.0;

    // [Rendering Limits]
    float RaymarchingBoundary = max(iOuterRadiusRs + 1.0, 501.0);
    float BackgroundShiftMax = 2.0;
    float ShiftMax = 1.0; 
    float CurrentUniverseSign = iUniverseSign;

    // -------------------------------------------------------------------------
    // 3. 相机系统与坐标变换 (Camera System & Coordinate Transforms)
    // -------------------------------------------------------------------------
    // World Space
    vec3 CamToBHVecVisual = (iInverseCamRot * vec4(iBlackHoleRelativePosRs.xyz, 0.0)).xyz;
    vec3 RayPosWorld = -CamToBHVecVisual; 
    
    vec3 DiskNormalWorld = normalize((iInverseCamRot * vec4(iBlackHoleRelativeDiskNormal.xyz, 0.0)).xyz);
    vec3 DiskTangentWorld = normalize((iInverseCamRot * vec4(iBlackHoleRelativeDiskTangen.xyz, 0.0)).xyz);
    
    // Basis Construction
    vec3 BH_Y = normalize(DiskNormalWorld);             
    vec3 BH_X = normalize(DiskTangentWorld);            
    BH_X = normalize(BH_X - dot(BH_X, BH_Y) * BH_Y);
    vec3 BH_Z = normalize(cross(BH_X, BH_Y));           
    mat3 LocalToWorldRot = mat3(BH_X, BH_Y, BH_Z);
    mat3 WorldToLocalRot = transpose(LocalToWorldRot);
    
    // Transform to Local Metric Space
    vec3 RayPosLocal = WorldToLocalRot * RayPosWorld;
    vec3 RayDirWorld_Geo = WorldToLocalRot * normalize((iInverseCamRot * vec4(ViewDirLocal, 0.0)).xyz);

    // -------------------------------------------------------------------------
    // 4. 状态初始化与包围盒优化 (State Init & Bounding Optimization)
    // -------------------------------------------------------------------------
    vec4  Result = vec4(0.0);
    bool  bShouldContinueMarchRay = true;
    bool  bWaitCalBack = false;
    float DistanceToBlackHole = length(RayPosLocal);

    // Fast Forward (Bounding Sphere Check)
    if (DistanceToBlackHole > RaymarchingBoundary) 
    {
        vec3 O = RayPosLocal; vec3 D = RayDirWorld_Geo; float r = RaymarchingBoundary - 1.0; 
        float b = dot(O, D); float c = dot(O, O) - r * r; float delta = b * b - c; 
        if (delta < 0.0) { 
            bShouldContinueMarchRay = false; 
            bWaitCalBack = true; 
        } 
        else {
            float tEnter = -b - sqrt(delta); 
            if (tEnter > 0.0) RayPosLocal = O + D * tEnter;
            else if (-b + sqrt(delta) <= 0.0) { 
                bShouldContinueMarchRay = false; 
                bWaitCalBack = true; 
            }
        }
    }

    // Geodesic Variables
    vec4 X = vec4(RayPosLocal, 0.0); // .xyz = Position, .w = Time Lag
    vec4 P_cov = vec4(-1.0);         // Covariant Momentum
    float E_conserved = 1.0;       
    vec3 RayDir = RayDirWorld_Geo;   
    vec3 LastDir = RayDir;
    vec3 LastPos = RayPosLocal;
    float GravityFade = CubicInterpolate(max(min(1.0 - (0.01 * length(RayPosLocal) - 1.0) / 4.0, 1.0), 0.0));
    
    // Calculate Initial Momentum
    if (bShouldContinueMarchRay) 
    {
       P_cov = GetInitialMomentum(RayDir, X, iObserverMode, iUniverseSign, PhysicalSpinA, PhysicalQ, GravityFade);
    }
    E_conserved = -P_cov.w;

    // -------------------------------------------------------------------------
    // 5. 初始合法性检查与终结半径 (Validity Checks & Termination Radius)
    // -------------------------------------------------------------------------
    float TerminationR = -1.0; 
    float CameraStartR = KerrSchildRadius(RayPosLocal, PhysicalSpinA, CurrentUniverseSign);
    
    if (CurrentUniverseSign > 0.0) 
    {
        // 静态观者能层合法性检查
        if (iObserverMode == 0) 
        {
            float CosThetaSq = (RayPosLocal.y * RayPosLocal.y) / (CameraStartR * CameraStartR + 1e-20);
            float SL_Discrim = 0.25 - PhysicalQ * PhysicalQ - PhysicalSpinA * PhysicalSpinA * CosThetaSq;
            
            if (SL_Discrim >= 0.0) {
                float SL_Outer = 0.5 + sqrt(SL_Discrim);
                float SL_Inner = 0.5 - sqrt(SL_Discrim); 
                
                if (CameraStartR < SL_Outer && CameraStartR > SL_Inner) {
                    bShouldContinueMarchRay = false; 
                    bWaitCalBack = false; 
                    Result = vec4(0.0, 0.0, 0.0, 1.0); 
                } 
            }
        }
        else
        {

        }
        // 确定光线追踪终止半径 (非裸奇点)
        if (!bIsNakedSingularity && CurrentUniverseSign > 0.0) 
        {
            if (CameraStartR > EventHorizonR) TerminationR = EventHorizonR; 
            else if (CameraStartR > InnerHorizonR) TerminationR = InnerHorizonR;
            else TerminationR = -1.0;
        }
    }
    // 输入变量：CONST_M (质量), iSpin (无量纲自旋), iQ (无量纲电荷)
    float AbsSpin = abs(CONST_M * iSpin);
    float Q2 = iQ * iQ * CONST_M * CONST_M; // Q^2
    

    float AcosTerm = acos(clamp(-abs(iSpin), -1.0, 1.0));
    float PhCoefficient = 1.0 + cos(0.66666667 * AcosTerm);
    float r_guess = 2.0 * CONST_M * PhCoefficient; 
    float r = r_guess;
    float sign_a = 1.0; 
    
    for(int k=0; k<3; k++) {
        float Mr_Q2 = CONST_M * r - Q2;
        float sqrt_term = sqrt(max(0.0001, Mr_Q2)); 
        
        // 方程 f(r)
        float f = r*r - 3.0*CONST_M*r + 2.0*Q2 + sign_a * 2.0 * AbsSpin * sqrt_term;
        
        // 导数 f'(r)
        float df = 2.0*r - 3.0*CONST_M + sign_a * AbsSpin * CONST_M / sqrt_term;
        
        if(abs(df) < 0.00001) break;
    
        r = r - f / df;
    }
    
    float ProgradePhotonRadius = r;

    float MaxStep=150.0+300.0/(1.0+1000.0*(1.0-iSpin*iSpin-iQ*iQ)*(1.0-iSpin*iSpin-iQ*iQ));
    if(bIsNakedSingularity) MaxStep=450;//150.0+300.0/(1.0+10.0*(1.0-iSpin*iSpin-iQ*iQ)*(1.0-iSpin*iSpin-iQ*iQ));
    // -------------------------------------------------------------------------
    // 6. 光线追踪主循环 (Raymarching Loop)
    // -------------------------------------------------------------------------
    int Count = 0;
    float lastR = 0.0;
    bool bIntoOutHorizon = false;
    bool bIntoInHorizon = false;
    float LastDr = 0.0;           
    int RadialTurningCounts = 0;  
    
    vec3 RayPos = X.xyz; // Sync for loop start

    while (bShouldContinueMarchRay)
    {
        // --- Geometry & Status Update ---
        DistanceToBlackHole = length(RayPos);
        if (DistanceToBlackHole > RaymarchingBoundary)
        { 
            bShouldContinueMarchRay = false; 
            bWaitCalBack = true; 
            break; //离开足够远
        }
        
        KerrGeometry geo;
        ComputeGeometryScalars(X.xyz, PhysicalSpinA, PhysicalQ, GravityFade, CurrentUniverseSign, geo);

        // Horizon & Iteration Checks
        if (CurrentUniverseSign > 0.0 && geo.r < TerminationR && !bIsNakedSingularity && TerminationR != -1.0) 
        { 
            bShouldContinueMarchRay = false;
            bWaitCalBack = false;
            Result = vec4(0.0, 0.3, 0.3, 0.0); 
            break; //直接进入判定区
        }
        if (Count > MaxStep) 
        { 
            bShouldContinueMarchRay = false; 
            bWaitCalBack = false;
            Result = vec4(0.0, 0.3, 0.0, 0.0);
            break; //耗尽步数
        }

        // --- Derivative & Bound State Logic ---
        State s0; s0.X = X; s0.P = P_cov;
        State k1 = GetDerivativesAnalytic(s0, PhysicalSpinA, PhysicalQ, GravityFade, geo);

        // Bound State Detection (Naked Singularity)
        {
            float CurrentDr = dot(geo.grad_r, k1.X.xyz);
            if (Count > 0 && CurrentDr * LastDr < 0.0) RadialTurningCounts++;
            if (RadialTurningCounts > 2) 
            {
                bShouldContinueMarchRay = false; bWaitCalBack = false;
                Result = vec4(0.3, 0.0, 0.3, 1.0); 
                break;//识别剔除束缚态光子轨道
            }
            LastDr = CurrentDr;
        }

        // Horizon Crossing Flags
        if(geo.r > InnerHorizonR && lastR < InnerHorizonR) bIntoInHorizon = true;
        if(geo.r > EventHorizonR && lastR < EventHorizonR) bIntoOutHorizon = true;

        // --- Prograde Photon Sphere Pruning ---
        if (CurrentUniverseSign > 0.0 && !bIsNakedSingularity)
        {


            float SafetyGap = 0.001;
            float PhotonShellLimit = ProgradePhotonRadius - SafetyGap; 
            float preCeiling = min(CameraStartR - SafetyGap, TerminationR + 0.2);
            if(bIntoInHorizon) { preCeiling = InnerHorizonR + 0.2; }//处理 射线从相机出发 -> 向外运动 -> 调头 -> 向内运动 -> 撞击内视界 的光
            if(bIntoOutHorizon) { preCeiling = EventHorizonR + 0.2; }
            
            float PruningCeiling = min(iInterRadiusRs, preCeiling);
            PruningCeiling = min(PruningCeiling, PhotonShellLimit); 

            if (geo.r < PruningCeiling)
            {
                float DrDlambda = dot(geo.grad_r, k1.X.xyz);
                if (DrDlambda > 1e-4) 
                {
                    bShouldContinueMarchRay = false;
                    bWaitCalBack = false;
                    Result = vec4(0.0, 0., 0.3, 0.0);
                    break; //对凝结在视界前的光提前剔除
                }
            }
        }

        // --- Adaptive Step Size Calculation ---
        float rho = length(RayPos.xz);
        float DistRing = sqrt(RayPos.y * RayPos.y + pow(rho - abs(PhysicalSpinA), 2.0));
        float Vel_Mag = length(k1.X); 
        float Force_Mag = length(k1.P);
        float Mom_Mag = length(P_cov);
        
        float ErrorTolerance = 0.5;
        float StepGeo =  DistRing / (Vel_Mag + 1e-9);
        float StepForce = Mom_Mag / (Force_Mag + 1e-15);
        
        float dLambda = ErrorTolerance*min(StepGeo, StepForce);
        dLambda = max(dLambda, 1e-7); 

        // --- Integration (Optimized RK4) ---
        vec4 LastX = X;
        LastPos = X.xyz;
        GravityFade = CubicInterpolate(max(min(1.0 - (0.01 * DistanceToBlackHole - 1.0) / 4.0, 1.0), 0.0));
        
        // Safety Check for Contra Momentum
        vec4 P_contra_step = RaiseIndex(P_cov, geo);
        if (P_contra_step.w > 10000.0 && !bIsNakedSingularity && CurrentUniverseSign > 0.0) 
        { 
            bShouldContinueMarchRay = false; 
            bWaitCalBack = false;
            Result = vec4(0.3, 0.3, 0.2, 0.0);  
            break; //凝结在视界
        }

        StepGeodesicRK4_Optimized(X, P_cov, E_conserved, -dLambda, PhysicalSpinA, PhysicalQ, GravityFade, CurrentUniverseSign, geo, k1);
        lastR = geo.r;

        RayPos = X.xyz;
        vec3 StepVec = RayPos - LastPos;
        float ActualStepLength = length(StepVec);
        RayDir = (ActualStepLength > 1e-7) ? StepVec / ActualStepLength : LastDir;
        
        // Wormhole Check
        if (LastPos.y * RayPos.y < 0.0) {
            float t_cross = LastPos.y / (LastPos.y - RayPos.y);
            float rho_cross = length(mix(LastPos.xz, RayPos.xz, t_cross));
            if (rho_cross < abs(PhysicalSpinA)) CurrentUniverseSign *= -1.0;
        }

        // --- Volumetric Rendering ---
        if (CurrentUniverseSign > 0.0) {
           Result = DiskColor(Result, ActualStepLength, X, LastX, RayDir, LastDir, P_cov, E_conserved,
                             iInterRadiusRs, iOuterRadiusRs, iThinRs, iHopper, iBrightmut, iDarkmut, iReddening, iSaturation, DiskArgument, 
                             iBlackbodyIntensityExponent, iRedShiftColorExponent, iRedShiftIntensityExponent, PeakTemperature, ShiftMax, 
                             clamp(PhysicalSpinA, -0.49, 0.49), 
                             PhysicalQ                          
                             ); 
           
           Result = JetColor(Result, ActualStepLength, X, LastX, RayDir, LastDir, P_cov, E_conserved,
                             iInterRadiusRs, iOuterRadiusRs, iJetRedShiftIntensityExponent, iJetBrightmut, iReddening, iJetSaturation, iAccretionRate, iJetShiftMax, 
                             clamp(PhysicalSpinA, -0.049, 0.049), 
                             PhysicalQ                            
                             ); 
        }

        if (Result.a > 0.99) { bShouldContinueMarchRay = false; bWaitCalBack = false; break; }
        
        LastDir = RayDir;
        Count++;
    }

    // -------------------------------------------------------------------------
    // 7. 背景采样 (Background Sampling)
    // -------------------------------------------------------------------------
    if (bWaitCalBack)
    {
        vec3 EscapeDirWorld = LocalToWorldRot * normalize(RayDir); 
        vec4 Backcolor = textureLod(iBackground, EscapeDirWorld, min(1.0, textureQueryLod(iBackground, EscapeDirWorld).x));
        if (CurrentUniverseSign < 0.0) Backcolor = textureLod(iAntiground, EscapeDirWorld, min(1.0, textureQueryLod(iAntiground, EscapeDirWorld).x));
        
        float FrequencyRatio = 1.0 / max(1e-4, E_conserved);
        float BackgroundShift = clamp(FrequencyRatio, 1.0/BackgroundShiftMax, BackgroundShiftMax);
        
        vec4 TexColor = Backcolor;
        vec3 Rcolor = Backcolor.r * 1.0 * WavelengthToRgb(max(453.0, 645.0 / BackgroundShift));
        vec3 Gcolor = Backcolor.g * 1.5 * WavelengthToRgb(max(416.0, 510.0 / BackgroundShift));
        vec3 Bcolor = Backcolor.b * 0.6 * WavelengthToRgb(max(380.0, 440.0 / BackgroundShift));
        vec3 Scolor = Rcolor + Gcolor + Bcolor;
        float OStrength = 0.3 * Backcolor.r + 0.6 * Backcolor.g + 0.1 * Backcolor.b;
        float RStrength = 0.3 * Scolor.r + 0.6 * Scolor.g + 0.1 * Scolor.b;
        Scolor *= OStrength / max(RStrength, 0.001);
        TexColor = vec4(Scolor, Backcolor.a) * pow(FrequencyRatio, 4.0); 

        Result += 0.9999 * TexColor * (1.0 - Result.a);
    }
    
    // -------------------------------------------------------------------------
    // 8. 色调映射与 TAA (Tone Mapping & TAA)
    // -------------------------------------------------------------------------
    float RedFactor   = 3.0 * Result.r / (Result.g + Result.b + Result.g );
    float BlueFactor  = 3.0 * Result.b / (Result.g + Result.b + Result.g );
    float GreenFactor = 3.0 * Result.g / (Result.g + Result.b + Result.g );
    float BloomMax    = 8.0;
    
    Result.r = min(-4.0 * log( 1.0 - pow(Result.r, 2.2)), BloomMax * RedFactor);
    Result.g = min(-4.0 * log( 1.0 - pow(Result.g, 2.2)), BloomMax * GreenFactor);
    Result.b = min(-4.0 * log( 1.0 - pow(Result.b, 2.2)), BloomMax * BlueFactor);
    Result.a = min(-4.0 * log( 1.0 - pow(Result.a, 2.2)), 4.0);
    
    vec4 PrevColor = texelFetch(iHistoryTex, ivec2(gl_FragCoord.xy), 0);
    FragColor      = (iBlendWeight) * Result + (1.0 - iBlendWeight) * PrevColor;
}