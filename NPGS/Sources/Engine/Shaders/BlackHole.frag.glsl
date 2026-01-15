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

//vec3 WavelengthToRgb(float wavelength) {
//    vec3 color = vec3(0.0);
//    if (wavelength < 380.0 || wavelength > 750.0) return color; 
//    if (wavelength >= 380.0 && wavelength < 440.0) {
//        color.r = -(wavelength - 440.0) / (440.0 - 380.0); color.g = 0.0; color.b = 1.0;
//    } else if (wavelength >= 440.0 && wavelength < 490.0) {
//        color.r = 0.0; color.g = (wavelength - 440.0) / (490.0 - 440.0); color.b = 1.0;
//    } else if (wavelength >= 490.0 && wavelength < 510.0) {
//        color.r = 0.0; color.g = 1.0; color.b = -(wavelength - 510.0) / (510.0 - 490.0);
//    } else if (wavelength >= 510.0 && wavelength < 580.0) {
//        color.r = (wavelength - 510.0) / (580.0 - 510.0); color.g = 1.0; color.b = 0.0;
//    } else if (wavelength >= 580.0 && wavelength < 645.0) {
//        color.r = 1.0; color.g = -(wavelength - 645.0) / (645.0 - 580.0); color.b = 0.0;
//    } else if (wavelength >= 645.0 && wavelength <= 750.0) {
//        color.r = 1.0; color.g = 0.0; color.b = 0.0;
//    }
//    float factor = 0.0;
//    if (wavelength >= 380.0 && wavelength < 420.0) factor = 0.3 + 0.7 * (wavelength - 380.0) / (420.0 - 380.0);
//    else if (wavelength >= 420.0 && wavelength < 645.0) factor = 1.0;
//    else if (wavelength >= 645.0 && wavelength <= 750.0) factor = 0.3 + 0.7 * (750.0 - wavelength) / (750.0 - 645.0);
//    
//    return color * factor / pow(color.r * color.r + 2.25 * color.g * color.g + 0.36 * color.b * color.b, 0.5) * (0.1 * (color.r + color.g + color.b) + 0.9);
//}
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



// [TENSOR] Returns Initial Covariant Momentum P_u
// [TENSOR] Returns Initial Covariant Momentum P_u
// Updated: True E=1, L=0 Free-fall Observer & Cartesian-aligned Tetrad
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
    // 1. 计算当前位置的几何信息
    KerrGeometry geo;
    ComputeGeometryScalars(X.xyz, PhysicalSpinA, PhysicalQ, GravityFade, universesign, geo);

    // -------------------------------------------------------------------------
    // 2. 确定观者的四维速度 U (Target Frame)
    // -------------------------------------------------------------------------
    vec4 U_up;
    
    if (iObserverMode == 0) { 
        // --- Static Observer ---
        float g_tt = -1.0 + geo.f;
        float time_comp = 1.0 / sqrt(max(1e-9, -g_tt));
        U_up = vec4(0.0, 0.0, 0.0, time_comp);
    
    } else { 
        // --- True Free-Falling Observer (E=1, L=0, Q=0) ---
        // 代码保持不变，这是解析解
        float r = geo.r; float r2 = geo.r2; float a = PhysicalSpinA; float a2 = geo.a2;
        float y_phys = X.y; // Spin Axis Y
        float rho2 = r2 + a2 * (y_phys * y_phys) / (r2 + 1e-9);
        float TwoMr = 2.0 * CONST_M * r;
        float Xi = sqrt(max(0.0, TwoMr * (r2 + a2)));
        float DenomPhi = rho2 * (TwoMr + Xi);
        float U_phi_KS = (abs(DenomPhi) > 1e-9) ? (-TwoMr * a / DenomPhi) : 0.0;
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

    // -------------------------------------------------------------------------
    // 3. 构建严格的局域参照系 (Reference Frame: ZAMO)
    // -------------------------------------------------------------------------
    // 我们需要一个物理上严格定义的“静止”参考系来对齐相机的 -RayDir。
    // ZAMO (Zero Angular Momentum Observer) 是由 t=const 超曲面的法线 n 定义的。
    // n_cov = (-1, 0, 0, 0)。它是几何上唯一确定的，不依赖人为坐标轴选择。
    
    vec4 n_cov = vec4(0.0, 0.0, 0.0, -1.0); // 注意：Metric signature (-+++) vs 代码习惯
    // 代码中 Metric = eta + f l l. eta = diag(1,1,1,-1).
    // 这里 T 轴是 w 分量。通常 Riemann 几何中 n_cov = (-1,0,0,0) 或 (0,0,0,1) 取决于坐标位。
    // 在本 Shader 坐标系中，w 是时间。
    // ZAMO 的余切矢量是 dt (也就是梯度 t)。在代码坐标系中对应 vec4(0,0,0,1)。
    // 让我们确认符号：U^t > 0，metric signature +++- ?
    // MINKOWSKI_METRIC = diag(1, 1, 1, -1). 
    // 所以时间分量是第 4 个。
    // 协变矢量 (One-form) n_u = (0, 0, 0, 1)。
    // 为什么是正？因为 inverse metric g^tt 通常是负的，为了让 u^u 指向未来(正)，我们需要调整符号。
    // 只要保证 u_ref^t > 0 即可。
    
    vec4 zamo_cov = vec4(0.0, 0.0, 0.0, 1.0); // dt 方向
    vec4 zamo_contra = RaiseIndex(zamo_cov, geo); // g^uv n_v
    
    // 归一化得到 ZAMO 四速度 u_ref
    // u . u = -1
    float zamo_norm_sq = dot(zamo_cov, zamo_contra); // g^tt
    // 注意：在视界外 g^tt < 0。
    float norm_factor = inversesqrt(max(1e-9, abs(zamo_norm_sq)));
    vec4 u_ref = zamo_contra * norm_factor;
    // 确保指向未来 (t分量 > 0)
    if (u_ref.w < 0.0) u_ref = -u_ref;

    // -------------------------------------------------------------------------
    // 4. 在 ZAMO 系中定义物理的射线方向 k
    // -------------------------------------------------------------------------
    // 相机的 -RayDir 定义了它在“平直空间”想看的方向。
    // 我们将其映射到 ZAMO 的空间切片上。
    // 初始猜测：Coordinate basis 中的空间方向。
    vec4 k_naive = vec4(-normalize(RayDir), 0.0);
    
    // 投影：剔除平行于 u_ref 的分量，确保 k 正交于 u_ref (k . u_ref = 0)
    // k_ortho = k - (k . u) * u / (u . u) = k + (k . u) * u
    vec4 k_naive_down = LowerIndex(k_naive, geo);
    float k_dot_u = dot(k_naive_down, u_ref); // Covariant dot
    vec4 k_ortho = k_naive + k_dot_u * u_ref;
    
    // 归一化：确保 k 是单位空间向量 (k . k = 1)
    vec4 k_ortho_down = LowerIndex(k_ortho, geo);
    float k_len_sq = dot(k_ortho, k_ortho_down);
    float k_norm = inversesqrt(max(1e-9, k_len_sq));
    vec4 k_phys = k_ortho * k_norm;

    // -------------------------------------------------------------------------
    // 5. 精确 Pure Boost (ZAMO -> Observer)
    // -------------------------------------------------------------------------
    // 我们在 ZAMO 系中构建了光子 P_ref = u_ref + k_phys (Energy=1)。
    // 现在用无旋转 Boost 将其变换到 Observer (U) 系。
    // 变换公式：P = P_ref - [ (P_ref . (U + u_ref)) / (1 - U . u_ref) ] * (U + u_ref) - 2 * (P_ref . U) * U
    // 由于 P_ref . P_ref = 0 且变换保持内积，P 也是 Null。
    // 由于 P_ref . u_ref = -1 且变换映射 u_ref -> U，故 P . U = -1 (能量守恒)。
    
    vec4 P_ref = u_ref + k_phys;
    
    // 优化计算
    vec4 sum_vel = u_ref + U_up;
    vec4 sum_vel_down = LowerIndex(sum_vel, geo); // = u_ref_down + U_down
    vec4 U_down = LowerIndex(U_up, geo);
    
    float P_dot_sum = dot(P_ref, sum_vel_down);
    float U_dot_uref = dot(U_down, u_ref); // Gamma factor roughly
    
    // 分母是 1 - (-U.u_ref)。由于两个都是未来指向的类时矢量，点积 < -1。
    // 1 - (-|val|) = 1 + |val| > 2。非常安全。
    float denom = 1.0 - U_dot_uref; 
    
    float factor = P_dot_sum / denom;
    
    // 公式中的第二项：2 * (P_ref . U) * U
    // 实际上对于 Pure Boost 将 u_ref 映射到 U：
    // P = P_ref - (P_ref . (U + u_ref))/(1 - u . U) * (U + u_ref) - 2(P_ref . U)U ?? 
    // 不，那是反射公式的通用形式。
    // 对于将 u_ref 映射到 U 的特定 Boost，且 P_ref 由 u_ref + k 组成：
    // 正确的推导形式 (保证 E=1) 是：
    // P^u = k^u + U^u + [ (k^u . U_u) / (1 - U . u_ref) ] * (u_ref^u + U^u)
    
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
void ApplyHamiltonianCorrection(inout vec4 P, vec4 X, float E, float PhysicalSpinA, float PhysicalQ, float fade, float r_sign) {
    P.w = -E; 
    vec3 p = P.xyz;    
    
    KerrGeometry geo;
    ComputeGeometryScalars(X.xyz, PhysicalSpinA, PhysicalQ, fade, r_sign, geo);
    
    // Solving g^uv p_u p_v = 0
    float p2 = dot(p, p);
    // l^u * p_u (spatial part) + l^u * p_t
    // geo.l_up = (lx, ly, lz, -1)
    float L_dot_p_spatial = dot(geo.l_up.xyz, p);
    float L_dot_P_full = L_dot_p_spatial - P.w; // l^t * p_t = (-1)*(-E) = E? Wait.
    // l^u = (l, -1). P_u = (p, -E).
    // l^u P_u = l.p + (-1)(-E) = l.p + E.
    // BUT Hamiltonian function uses: l^u P_u term.
    // In code below: dot(geo.l_up.xyz, p) is spatial.
    // Let's stick to the algebraic expansion used in original code for safety.
    
    // Original: float L_dot_P = dot(geo.l_up.xyz, p); (Variable name reuse confusion in orig code?)
    // In original code: `float L_dot_P = dot(geo.l_up.xyz, p);` -> this only dotted spatial parts?
    // Let's trace original `ApplyHamiltonianCorrection`.
    // It used `L_dot_P` (spatial) in `Coeff_B` and `Coeff_A`.
    
    // Correct expansion of g^uv P_u P_v = 0:
    // eta^uv P_u P_v - f (l^u P_u)^2 = 0
    // (p^2 - E^2) - f (l.p + E)^2 = 0  (since l^t=-1, P_t=-E => l^u P_u = l.p - (-E) = l.p + E)
    
    // Let P_new = k * p.
    // (k^2 p^2 - E^2) - f (k l.p + E)^2 = 0
    // k^2 p^2 - E^2 - f (k^2 (l.p)^2 + 2k E (l.p) + E^2) = 0
    // k^2 [p^2 - f(l.p)^2] + k [-2 f E (l.p)] + [-E^2 - f E^2] = 0
    
    float L_dot_p_s = dot(geo.l_up.xyz, p);
    float Pt = P.w; // -E
    
    // Aligning with original variable names to ensure same logic
    // Original: Coeff_A = p2 - geo.f * L_dot_P * L_dot_P; (where L_dot_P was spatial dot)
    float Coeff_A = p2 - geo.f * L_dot_p_s * L_dot_p_s;
    
    // Original: Coeff_B = 2.0 * geo.f * L_dot_P * Pt;
    // Note: Pt is negative energy.
    float Coeff_B = 2.0 * geo.f * L_dot_p_s * Pt;
    
    // Original: Coeff_C = -Pt * Pt * (1.0 + geo.f);
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
void StepGeodesicRK4(inout vec4 X, inout vec4 P, float E, float dt, float PhysicalSpinA, float PhysicalQ, float fade, float r_sign, KerrGeometry geo0) {
    State s0; s0.X = X; s0.P = P;

    // --- k1 Step ---
    // Reuse geo0 (Scalars already computed in main loop). 
    // We pass it to GetDerivatives, which adds Gradients.
    State k1 = GetDerivativesAnalytic(s0, PhysicalSpinA, PhysicalQ, fade, geo0);

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
    ApplyHamiltonianCorrection(finalP, finalX, E, PhysicalSpinA, PhysicalQ, fade, finalSign);

    X = finalX;
    P = finalP;
}

// =============================================================================
// SECTION 6: 体积渲染函数 (吸积盘与喷流)
// =============================================================================

vec4 DiskColor(vec4 BaseColor, float StepLength, vec3 RayPos, vec3 LastRayPos,
               vec3 RayDir, vec3 LastRayDir,
               float InterRadius, float OuterRadius, float Thin, float Hopper, float Brightmut, float Darkmut, float Reddening, float Saturation, float DiskTemperatureArgument,
               float BlackbodyIntensityExponent, float RedShiftColorExponent, float RedShiftIntensityExponent,
               float PeakTemperature, float ShiftMax, 
               float PhysicalSpinA, // [INPUT] Dimensional Spin (scaled by CONST_M)
               float PhysicalQ,     // [INPUT] Dimensional Charge (scaled by CONST_M)
               vec4 iP_cov, float iE_obs) 
{
    vec4 CurrentResult = BaseColor;
    vec3 StartPos = LastRayPos; // [COORD] KS Spatial Position
    vec3 DirVec   = RayDir; 
    float TotalDist = StepLength;
    float TraveledDist = 0.0;
    
    float R_Start = KerrSchildRadius(StartPos, PhysicalSpinA, 1.0);
    float R_End   = KerrSchildRadius(RayPos, PhysicalSpinA, 1.0);
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
        
        float PosR = KerrSchildRadius(SamplePos, PhysicalSpinA, 1.0);
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
                 float AngularVelocity = GetKeplerianAngularVelocity(PosR, 1.0, PhysicalSpinA, PhysicalQ);
                 
                 float u = sqrt(PosR);
                 float k_cubed = PhysicalSpinA * 0.70710678;
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
                 KerrGeometry geo_sample;
                 // 注意：最后的参数根据上下文可能是 1.0 或者 CurrentUniverseSign，你原代码里写的是 1.0
                 ComputeGeometryScalars(SamplePos, PhysicalSpinA, PhysicalQ, 1.0, 1.0, geo_sample);
                 
                 // 2. 利用辅助函数下降指标：计算协变速度 U_u = g_uv * U^v
                 vec4 U_fluid_lower = LowerIndex(U_fluid_unnorm, geo_sample);
                 
                 // 3. 计算模长平方： U^u * U_u
                 float norm_sq = dot(U_fluid_unnorm, U_fluid_lower);
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
              float PhysicalSpinA, // [INPUT] Dimensional Spin (scaled by CONST_M)
              float PhysicalQ,     // [INPUT] Dimensional Charge (scaled by CONST_M)
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
        
        float PosR = KerrSchildRadius(SamplePos, PhysicalSpinA, 1.0);
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
            float InnerTheta = 3.0 * GetKeplerianAngularVelocity(InterRadius, 1.0, PhysicalSpinA, PhysicalQ) * (iBlackHoleTime - 1.0 / 0.8 * abs(PosY));
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
            float InnerTheta = 2.0 * GetKeplerianAngularVelocity(InterRadius, 1.0, PhysicalSpinA, PhysicalQ) * (iBlackHoleTime - 1.0 / 0.8 * abs(PosY));
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
            KerrGeometry geo_sample;
            // 注意：最后的参数根据上下文可能是 1.0 或者 CurrentUniverseSign，你原代码里写的是 1.0
            ComputeGeometryScalars(SamplePos, PhysicalSpinA, PhysicalQ, 1.0, 1.0, geo_sample);
            
            // 2. 利用辅助函数下降指标：计算协变速度 U_u = g_uv * U^v
            vec4 U_fluid_lower = LowerIndex(U_jet_unnorm, geo_sample);
            
            // 3. 计算模长平方： U^u * U_u
            float norm_sq = dot(U_jet_unnorm, U_fluid_lower);
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
    float PhysicalSpinA = -iSpin * CONST_M;  
    float PhysicalQ = iQ * CONST_M;         
    
    // Event Horizon
    float HorizonDiscrim = 0.25 - PhysicalSpinA * PhysicalSpinA - PhysicalQ * PhysicalQ;
    float EventHorizonR = 0.5 + sqrt(max(0.0, HorizonDiscrim));
    bool bIsNakedSingularity = HorizonDiscrim < 0.0;
    float TerminationR = EventHorizonR;
    float CurrentUniverseSign = iUniverseSign;
    float ShiftMax = iJetShiftMax; 

    // --- 1. 相机光线生成 ---
    vec2 Jitter = vec2(RandomStep(FragUv, fract(iTime * 1.0 + 0.5)), RandomStep(FragUv, fract(iTime * 1.0))) / iResolution;
    vec3 ViewDirLocal = FragUvToDir(FragUv + 0.5 * Jitter, Fov, iResolution); 
    
    vec3 CamToBHVecVisual = (iInverseCamRot * vec4(iBlackHoleRelativePosRs.xyz, 0.0)).xyz;
    vec3 RayPosWorld = -CamToBHVecVisual; 
    
    vec3 DiskNormalWorld = normalize((iInverseCamRot * vec4(iBlackHoleRelativeDiskNormal.xyz, 0.0)).xyz);
    vec3 DiskTangentWorld = normalize((iInverseCamRot * vec4(iBlackHoleRelativeDiskTangen.xyz, 0.0)).xyz);
    vec3 BH_Y = normalize(DiskNormalWorld);             
    vec3 BH_X = normalize(DiskTangentWorld);            
    BH_X = normalize(BH_X - dot(BH_X, BH_Y) * BH_Y);
    vec3 BH_Z = normalize(cross(BH_X, BH_Y));           
    mat3 LocalToWorldRot = mat3(BH_X, BH_Y, BH_Z);
    mat3 WorldToLocalRot = transpose(LocalToWorldRot);
    
    vec3 RayPosLocal = WorldToLocalRot * RayPosWorld;
    vec3 RayDirWorld_Geo = WorldToLocalRot * normalize((iInverseCamRot * vec4(ViewDirLocal, 0.0)).xyz);

    // --- 2. 包围球快进 ---
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

    // --- 3. 物理初始化 ---
    vec4 X = vec4(RayPosLocal, 0.0); 
    vec4 P_cov = vec4(-1.0);          
    float E_conserved = 1.0;       
    
    vec3 RayDir = RayDirWorld_Geo;   
    vec3 LastDir = RayDir;
    vec3 LastPos = RayPosLocal;
    float GravityFade = CubicInterpolate(max(min(1.0 - (0.01 * length(RayPosLocal) - 1.0) / 4.0, 1.0), 0.0));
        
    if (bShouldContinueMarchRay) 
    {
       // Uses Optimized Scalar Geometry internally
       P_cov = GetInitialMomentum(RayDir, X, iObserverMode, iUniverseSign, PhysicalSpinA, PhysicalQ, GravityFade);
       //P_cov = vec4(-RayDir, -1.0); // Override as per original code behavior? Keeping logic.
    }
    
    E_conserved = -P_cov.w;
    
    // Uses Optimized Scalar Geometry internally
    ApplyHamiltonianCorrection(P_cov, X, E_conserved, PhysicalSpinA, PhysicalQ, GravityFade, CurrentUniverseSign);
    
    vec3 RayPos = X.xyz;
    LastPos = RayPos;
    int Count = 0;
    float CameraStartR = KerrSchildRadius(RayPos, PhysicalSpinA, CurrentUniverseSign);
    
    TerminationR = -1.0; 
    
    if (CurrentUniverseSign > 0.0 && !bIsNakedSingularity) 
    {
        if (iObserverMode == 0) 
        {
            float CosThetaSq = (RayPos.y * RayPos.y) / (CameraStartR * CameraStartR + 1e-10);
            float SL_Discrim = 0.25 - PhysicalQ * PhysicalQ - PhysicalSpinA * PhysicalSpinA * CosThetaSq;
            
            float SL_Outer = 0.5 + sqrt(max(0.0, SL_Discrim)); 
            float SL_Inner = 0.5 - sqrt(max(0.0, SL_Discrim)); 
            if (CameraStartR > SL_Outer) TerminationR = EventHorizonR; 
            else if (CameraStartR > SL_Inner) {
                bShouldContinueMarchRay = false; bWaitCalBack = false; Result = vec4(0.0, 0.0, 0.0, 1.0); 
            } 
        }
        else // Falling Observer
        {
            float InnerHorizonR = 0.5 - sqrt(max(0.0, HorizonDiscrim));
            if (CameraStartR > InnerHorizonR) TerminationR = InnerHorizonR;
        }
    }
    
    // --- 4. 光线追踪主循环 ---
    while (bShouldContinueMarchRay)
    {
        float DistanceToBlackHole = length(RayPos);
        if (DistanceToBlackHole > RaymarchingBoundary) { bShouldContinueMarchRay = false; bWaitCalBack = true; break; }
        
        // [OPTIMIZATION] Compute Scalars Only Once per Loop
        KerrGeometry geo;
        ComputeGeometryScalars(X.xyz, PhysicalSpinA, PhysicalQ, GravityFade, CurrentUniverseSign, geo);

        if (CurrentUniverseSign > 0.0 && geo.r < TerminationR && !bIsNakedSingularity) 
        { 
            bShouldContinueMarchRay = false; bWaitCalBack = false; 
            //Result=vec4(0.0,0.,0.3,1.0); 
            break; 
        }

        if ((Count > 150 && !bIsNakedSingularity) || (Count > 450 && bIsNakedSingularity)) { 
            bShouldContinueMarchRay = false; bWaitCalBack = false; 
            Result=vec4(0.0,0.3,0.0,1.0);
            break; 
        }
        
        // Step Size Control
        float rho = length(RayPos.xz);
        float DistRing = sqrt(RayPos.y * RayPos.y + pow(rho - abs(PhysicalSpinA), 2.0));
        
        float GeoFactor = smoothstep(0.0, 0.6, DistRing);
        float RFactor   = smoothstep(0.0, 0.8, abs(geo.r)); 
        float Safety = min(GeoFactor, RFactor);
        float BaseSpeed = mix(0.03, 0.5, Safety);
        float RayStep = BaseSpeed * DistRing;

        LastPos = X.xyz;
        GravityFade = CubicInterpolate(max(min(1.0 - (0.01 * DistanceToBlackHole - 1.0) / 4.0, 1.0), 0.0));
        
        // [OPTIMIZATION] Use RaiseIndex instead of Matrix
        // P^u = g^uv P_v
        vec4 P_contra_step = RaiseIndex(P_cov, geo);
 
        if (P_contra_step.w > 10000.0 && !bIsNakedSingularity) 
        { 
            bShouldContinueMarchRay = false; bWaitCalBack = false; 
            //Result=vec4(0.3,0.0,0.0,1.0);
            break; 
        }

        float V_mag = length(vec4(P_contra_step.xyz, max(0.0, 1.0 - 0.8 * (iSpin*iSpin + iQ*iQ)) * P_contra_step.w)); 
        float dLambda = RayStep / max(V_mag, 0.0); 

        // [OPTIMIZATION] Pass pre-computed 'geo' to RK4 to skip first ComputeGeometry call
        StepGeodesicRK4(X, P_cov, E_conserved, -dLambda, PhysicalSpinA, PhysicalQ, GravityFade, CurrentUniverseSign, geo);

        RayPos = X.xyz;
        vec3 StepVec = RayPos - LastPos;
        float ActualStepLength = length(StepVec);


        RayDir = (ActualStepLength > 1e-7) ? StepVec / ActualStepLength : LastDir;
        
        if (LastPos.y * RayPos.y < 0.0) {
            float t_cross = LastPos.y / (LastPos.y - RayPos.y);
            float rho_cross = length(mix(LastPos.xz, RayPos.xz, t_cross));
            if (rho_cross < abs(PhysicalSpinA)) CurrentUniverseSign *= -1.0;
        }

       // if (CurrentUniverseSign > 0.0) {
       //    Result = DiskColor(Result, ActualStepLength, RayPos, LastPos, RayDir, LastDir,
       //                      iInterRadiusRs, iOuterRadiusRs, iThinRs, iHopper, iBrightmut, iDarkmut, iReddening, iSaturation, DiskArgument, 
       //                      iBlackbodyIntensityExponent, iRedShiftColorExponent, iRedShiftIntensityExponent, PeakTemperature, ShiftMax, 
       //                      clamp(PhysicalSpinA,-0.49,0.49), // Passed as Dimensional
       //                      PhysicalQ,                       // Passed as Dimensional
       //                      P_cov, E_conserved); 
       //    
       //    Result = JetColor(Result, ActualStepLength, RayPos, LastPos, RayDir, LastDir,
       //                      iInterRadiusRs, iOuterRadiusRs, iJetRedShiftIntensityExponent, iJetBrightmut, iReddening, iJetSaturation, iAccretionRate, iJetShiftMax, 
       //                      clamp(PhysicalSpinA,-0.049,0.049), // Passed as Dimensional
       //                      PhysicalQ,                         // Passed as Dimensional
       //                      P_cov, E_conserved); 
       // }
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
        TexColor = vec4(Scolor, Backcolor.a) * pow(FrequencyRatio, 4.0); 

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