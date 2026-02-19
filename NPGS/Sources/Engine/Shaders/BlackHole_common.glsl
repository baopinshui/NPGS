
#extension GL_EXT_samplerless_texture_functions : enable
#include "Common/CoordConverter.glsl"
#include "Common/NumericConstants.glsl"

// =============================================================================
// SECTION 1:  Uniform 定义
// =============================================================================


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
    vec4  iBlackHoleRelativePosRs;       //黑洞在相机系下位置。单位倍Rs
    vec4  iBlackHoleRelativeDiskNormal;  //黑洞在相机系下吸积盘法向兼自旋正方向。单位倍Rs
    vec4  iBlackHoleRelativeDiskTangen;  //黑洞在相机系下吸积盘切向。单位倍Rs


    int   iGrid;                         //绘制网格
    int   iObserverMode;                 //观者模式，0静态，1落体

    float iUniverseSign;                 //相机所在空间侧。 +1.0正宇宙  -1.0反宇宙
                        
    float iBlackHoleTime;                //时间。单位 c*s/Rs
    float iBlackHoleMassSol;             //质量，单位倍太阳质量
    float iSpin;                         //无量纲自旋a*   
    float iQ;                            //无量纲电荷Q* 

    float iMu;                           //吸积物质比荷
    float iAccretionRate;                //吸积率 单位倍爱丁顿吸积率

    float iInterRadiusRs;                //吸积盘内半径。单位倍Rs
    float iOuterRadiusRs;                //吸积盘外半径。单位倍Rs
    float iThinRs;                       //吸积盘半厚度。单位倍Rs
    float iHopper;                       //吸积盘厚度随半径增加斜率
    float iBrightmut;                    //吸积盘亮度乘数
    float iDarkmut;                      //吸积盘不透明度乘数
    float iReddening;                    //吸积盘红化系数
    float iSaturation;                   //吸积盘饱和度
    float iBlackbodyIntensityExponent;   //吸积盘温度——黑体颜色指数
    float iRedShiftColorExponent;        //吸积盘频移——温度指数
    float iRedShiftIntensityExponent;    //吸积盘频移——亮度指数

    float iHeatHaze;				     //热气流扰动强度
    float iBackgroundBrightmut;		     //背景亮度乘数  

    float iPhotonRingBoost;              //光子环亮度增亮
    float iPhotonRingColorTempBoost;     //光子环颜色增蓝
    float iBoostRot;                     //增强在自旋非0的非对称程度
    //吸积盘的亮度限制貌似写死2.2了
    //应该添加背景的频移 亮度和颜色限制系数

    float iJetRedShiftIntensityExponent; //喷流频移——亮度指数
    float iJetBrightmut;                 //喷流亮度乘数
    float iJetSaturation;                //喷流饱和度
    float iJetShiftMax;                  //喷流蓝移限制

    float iBlendWeight;                  //TAA前后帧混合权重。
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

float PerlinNoise(vec3 Position)//以后改成读lut
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
// SECTION 3: 颜色与光谱函数     采样与后处理
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

vec4 SampleBackground(vec3 Dir, float Shift, float Status)
{
    vec4 Backcolor;
    if (iBlackHoleMassSol>=0.0)
    {
        Backcolor = textureLod(iBackground, Dir, min(1.0, textureQueryLod(iBackground, Dir).x));
        if (Status > 1.5) { // Antiverse (Status == 2.0)
            Backcolor = textureLod(iAntiground, Dir, min(1.0, textureQueryLod(iAntiground, Dir).x));
        }
    }else{
        Backcolor = textureLod(iAntiground, Dir, min(1.0, textureQueryLod(iAntiground, Dir).x));
        if (Status > 1.5) { // Antiverse (Status == 2.0)
            Backcolor = textureLod(iBackground, Dir, min(1.0, textureQueryLod(iBackground, Dir).x));
        }
    }
    // 频移着色
    float BackgroundShift = Shift;
    vec3 Rcolor = Backcolor.r * 1.0 * WavelengthToRgb(max(453.0, 645.0 / BackgroundShift));
    vec3 Gcolor = Backcolor.g * 1.5 * WavelengthToRgb(max(416.0, 510.0 / BackgroundShift));
    vec3 Bcolor = Backcolor.b * 0.6 * WavelengthToRgb(max(380.0, 440.0 / BackgroundShift));
    vec3 Scolor = Rcolor + Gcolor + Bcolor;
    float OStrength = 0.3 * Backcolor.r + 0.6 * Backcolor.g + 0.1 * Backcolor.b;
    float RStrength = 0.3 * Scolor.r + 0.6 * Scolor.g + 0.1 * Scolor.b;
    Scolor *= OStrength / max(RStrength, 0.001);
    
    return iBackgroundBrightmut*vec4(Scolor, Backcolor.a) * pow(Shift, 4.0);
}


vec4 ApplyToneMapping(vec4 Result,float shift)
{
    float RedFactor   = 3.0 * Result.r / (Result.r + Result.b + Result.g );
    float BlueFactor  = 3.0 * Result.b / (Result.r + Result.b + Result.g );
    float GreenFactor = 3.0 * Result.g / (Result.r + Result.b + Result.g );
    float BloomMax    = max(8.0,shift);
    
    vec4 Mapped;
    Mapped.r = min(-4.0 * log( 1.0 - pow(Result.r, 2.2)), BloomMax * RedFactor);
    Mapped.g = min(-4.0 * log( 1.0 - pow(Result.g, 2.2)), BloomMax * GreenFactor);
    Mapped.b = min(-4.0 * log( 1.0 - pow(Result.b, 2.2)), BloomMax * BlueFactor);
    Mapped.a = min(-4.0 * log( 1.0 - pow(Result.a, 2.2)), 4.0);
    return Mapped;
}
// =============================================================================
// SECTION 4: 广相计算。Y为自旋方向，ingoing方向笛卡尔形式kerrscild系。+++-。
// =============================================================================

const float CONST_M = 0.5; // [PHYS] Mass M = 0.5
const float EPSILON = 1e-6;

// [TENSOR] Flat Space Metric eta_uv = diag(1, 1, 1, -1)
const mat4 MINKOWSKI_METRIC = mat4(
    1, 0, 0, 0,
    0, 1, 0, 0,
    0, 0, 1, 0,
    0, 0, 0, -1
);

//PhysicalSpinA和PhysicalQ是有量纲量（无量纲量乘M，即乘0.5）。

float GetKeplerianAngularVelocity(float Radius, float Rs, float PhysicalSpinA, float PhysicalQ) 
{
    float M = 0.5 * Rs; 
    float Mr_minus_Q2 = M * Radius - PhysicalQ * PhysicalQ;
    if (Mr_minus_Q2 < 0.0) return 0.0;
    float sqrt_Term = sqrt(Mr_minus_Q2);
    float denominator = Radius * Radius + PhysicalSpinA * sqrt_Term;
    return sqrt_Term / max(EPSILON, denominator);
}

//输入X^mu空间部分，输出bl系参数r
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
// 计算 ZAMO (零角动量观测者) 的角速度 Omega
float GetZamoOmega(float r, float a, float Q, float y) {
    float r2 = r * r;
    float a2 = a * a;
    float y2 = y * y;
    float cos2 = min(1.0, y2 / (r2 + 1e-9)); 
    float sin2 = 1.0 - cos2;
    
    // Delta = r^2 - 2Mr + a^2 + Q^2 (M=0.5)
    float Delta = r2 - r + a2 + Q * Q;
    
    // Sigma = r^2 + a^2 cos^2 theta
    float Sigma = r2 + a2 * cos2;
    
    // metric term A = (r^2+a^2)^2 - Delta * a^2 * sin^2 theta
    float A_metric = (r2 + a2) * (r2 + a2) - Delta * a2 * sin2;
    
    // Omega_ZAMO = 2Mra / A (for Q=0), with Q: a(2Mr - Q^2) / A
    // 2Mr = r (since M=0.5, 2M=1.0) -> r
    return a * (r - Q * Q) / max(1e-9, A_metric);
}

// 求解射线与 Kerr-Schild 常数 r 椭球面的交点
// 方程: (x^2 + z^2)/(r^2 + a^2) + y^2/r^2 = 1
// 返回 vec2(t1, t2)，如果没有交点返回 vec2(-1.0)
vec2 IntersectKerrEllipsoid(vec3 O, vec3 D, float r, float a) {
    float r2 = r * r;
    float a2 = a * a;
    float R_eq_sq = r2 + a2; // 赤道半径平方
    float R_pol_sq = r2;     // 极半径平方
    
    // 椭球方程: B(x^2 + z^2) + A(y^2) = A*B
    // 其中 A = R_eq_sq, B = R_pol_sq
    float A = R_eq_sq;
    float B = R_pol_sq;
    
    // 代入射线 P = O + D*t
    // (B*Dx^2 + B*Dz^2 + A*Dy^2) t^2 + ...
    float qa = B * (D.x * D.x + D.z * D.z) + A * D.y * D.y;
    float qb = 2.0 * (B * (O.x * D.x + O.z * D.z) + A * O.y * D.y);
    float qc = B * (O.x * O.x + O.z * O.z) + A * O.y * O.y - A * B;
    
    if (abs(qa) < 1e-9) return vec2(-1.0); // 线性退化，忽略
    
    float disc = qb * qb - 4.0 * qa * qc;
    if (disc < 0.0) return vec2(-1.0);
    
    float sqrtDisc = sqrt(disc);
    float t1 = (-qb - sqrtDisc) / (2.0 * qa);
    float t2 = (-qb + sqrtDisc) / (2.0 * qa);
    
    return vec2(t1, t2);
}

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
    float inv_den_f;      
    float num_f;          
};

//fade用于在接近包围盒边界时强行过渡为平直时空。直接乘在f上。下文中gravityfade同。

void ComputeGeometryScalars(vec3 X, float PhysicalSpinA, float PhysicalQ, float fade, float r_sign, out KerrGeometry geo) {
    geo.a2 = PhysicalSpinA * PhysicalSpinA;
    
    if (PhysicalSpinA == 0.0) {
        geo.r = r_sign*length(X);
        geo.r2 = geo.r * geo.r;
        float inv_r = 1.0 / geo.r;
        float inv_r2 = inv_r * inv_r;
        
        geo.l_up = vec4(X * inv_r, -1.0);
        geo.l_down = vec4(X * inv_r, 1.0);
        
        geo.num_f = (2.0 * CONST_M * geo.r - PhysicalQ * PhysicalQ);
        geo.f = (2.0 * CONST_M * inv_r - (PhysicalQ * PhysicalQ) * inv_r2) * fade;
        
        geo.inv_r2_a2 = inv_r2; 
        geo.inv_den_f = 0.0; 
        return;
    }

    geo.r = KerrSchildRadius(X, PhysicalSpinA, r_sign);
    geo.r2 = geo.r * geo.r;
    float r3 = geo.r2 * geo.r;
    float z_coord = X.y; 
    float z2 = z_coord * z_coord;
    
    geo.inv_r2_a2 = 1.0 / (geo.r2 + geo.a2);
    
    float lx = (geo.r * X.x - PhysicalSpinA * X.z) * geo.inv_r2_a2;
    float ly = X.y / geo.r;
    float lz = (geo.r * X.z + PhysicalSpinA * X.x) * geo.inv_r2_a2;
    
    geo.l_up = vec4(lx, ly, lz, -1.0);
    geo.l_down = vec4(lx, ly, lz, 1.0); 
    
    geo.num_f = 2.0 * CONST_M * r3 - PhysicalQ * PhysicalQ * geo.r2;
    float den_f = geo.r2 * geo.r2 + geo.a2 * z2;
    geo.inv_den_f = 1.0 / max(1e-20, den_f);
    geo.f = (geo.num_f * geo.inv_den_f) * fade;
}


void ComputeGeometryGradients(vec3 X, float PhysicalSpinA, float PhysicalQ, float fade, inout KerrGeometry geo) {
    float inv_r = 1.0 / geo.r;
    
    if (PhysicalSpinA == 0.0) {

        float inv_r2 = inv_r * inv_r;
        geo.grad_r = X * inv_r;
        float df_dr = (-2.0 * CONST_M + 2.0 * PhysicalQ * PhysicalQ * inv_r) * inv_r2 * fade;
        geo.grad_f = df_dr * geo.grad_r;
        return;
    }

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
    
    float z_coord = X.y;
    float z2 = z_coord * z_coord;
    
    float term_M  = -2.0 * CONST_M * geo.r2 * geo.r2 * geo.r;
    float term_Q  = 2.0 * PhysicalQ * PhysicalQ * geo.r2 * geo.r2;
    float term_Ma = 6.0 * CONST_M * geo.a2 * geo.r * z2;
    float term_Qa = -2.0 * PhysicalQ * PhysicalQ * geo.a2 * z2;
    
    float df_dr_num_reduced = term_M + term_Q + term_Ma + term_Qa;
    float df_dr = (geo.r * df_dr_num_reduced) * (geo.inv_den_f * geo.inv_den_f);
    
    float df_dy = -(geo.num_f * 2.0 * geo.a2 * z_coord) * (geo.inv_den_f * geo.inv_den_f);
    
    geo.grad_f = df_dr * geo.grad_r;
    geo.grad_f.y += df_dy;
    geo.grad_f *= fade;
}

//ks系的形式使得度规与矢量的乘法可以优化
//升指标和降指标。虽然变量名用的P，但是可以用于任何符合变换规则的矢量
//  P^u = g^uv P_v
// g^uv = eta^uv - f * l^u * l^v
vec4 RaiseIndex(vec4 P_cov, KerrGeometry geo) {
    // eta^uv = diag(1, 1, 1, -1)
    vec4 P_flat = vec4(P_cov.xyz, -P_cov.w); 

    float L_dot_P = dot(geo.l_up, P_cov);
    
    return P_flat - geo.f * L_dot_P * geo.l_up;
}

// P_u = g_uv P^v
// g_uv = eta_uv + f * l_u * l_v
vec4 LowerIndex(vec4 P_contra, KerrGeometry geo) {
    // eta_uv = diag(1, 1, 1, -1)
    vec4 P_flat = vec4(P_contra.xyz, -P_contra.w);
    
    float L_dot_P = dot(geo.l_down, P_contra);
    
    return P_flat + geo.f * L_dot_P * geo.l_down;
}


//初始化光子动量P_u，以向心矢量为主轴做施密特正交化
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

    KerrGeometry geo;
    ComputeGeometryScalars(X.xyz, PhysicalSpinA, PhysicalQ, GravityFade, universesign, geo);

    //确定观者四维速度 U_up 
    vec4 U_up;
    // Static Observer
    float g_tt = -1.0 + geo.f;
    float time_comp = 1.0 / sqrt(max(1e-9, -g_tt));
    U_up = vec4(0.0, 0.0, 0.0, time_comp);
    if (iObserverMode == 1) {
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
        float sqrtDet = sqrt(Det);
        
        float Ut;
        if (abs(A) < 1e-7) {
            Ut = -C / max(1e-9, B); 
        } else {
            if (B < 0.0) {
                 Ut = 2.0 * C / (-B + sqrtDet);
            } else {
                 Ut = (-B - sqrtDet) / (2.0 * A);
            }
        }
        U_up = mix(U_up,vec4(U_spatial, Ut),GravityFade);//在包围盒边界回退到静态观者

    }
       
    vec4 U_down = LowerIndex(U_up, geo);

    //构建平直空间参考基
    //主轴，径向
    vec3 m_r = -normalize(X.xyz);

    vec3 WorldUp = vec3(0.0, 1.0, 0.0);
    //副轴，环向或X。注意这个系只是中转，在极点强取方向不会改变视线朝向，只会改变畸变方向，而两极处在平行赤道面上正好没有畸变
    if (abs(dot(m_r, WorldUp)) > 0.999) {
        WorldUp = vec3(1.0, 0.0, 0.0);
    }
    vec3 m_phi = cross(WorldUp, m_r); 
    m_phi = normalize(m_phi);

    vec3 m_theta = cross(m_phi, m_r); 

    // 分解 RayDir 到这组基底
    float k_r     = dot(RayDir, m_r);
    float k_theta = dot(RayDir, m_theta);
    float k_phi   = dot(RayDir, m_phi);

    //构建弯曲时空物理基底

    vec4 e1 = vec4(m_r, 0.0);
    e1 += dot(e1, U_down) * U_up; 
    vec4 e1_d = LowerIndex(e1, geo);
    float n1 = sqrt(max(1e-9, dot(e1, e1_d)));
    e1 /= n1; e1_d /= n1;

    vec4 e2 = vec4(m_theta, 0.0);
    e2 += dot(e2, U_down) * U_up;
    e2 -= dot(e2, e1_d) * e1;
    vec4 e2_d = LowerIndex(e2, geo);
    float n2 = sqrt(max(1e-9, dot(e2, e2_d)));
    e2 /= n2; e2_d /= n2;

    vec4 e3 = vec4(m_phi, 0.0);
    e3 += dot(e3, U_down) * U_up;
    e3 -= dot(e3, e1_d) * e1;
    e3 -= dot(e3, e2_d) * e2;
    vec4 e3_d = LowerIndex(e3, geo);
    float n3 = sqrt(max(1e-9, dot(e3, e3_d)));
    e3 /= n3;



    vec4 P_up = U_up - (k_r * e1 + k_theta * e2 + k_phi * e3);

    // 返回协变动量 P_mu
    return LowerIndex(P_up, geo);
}
// -----------------------------------------------------------------------------
// 5.积分器
// -----------------------------------------------------------------------------
struct State {
    vec4 X; // x^u
    vec4 P; // p_u
};

//通过缩放动量空间部分修正哈密顿量
void ApplyHamiltonianCorrection(inout vec4 P, vec4 X, float E, float PhysicalSpinA, float PhysicalQ, float fade, float r_sign) {
    // P.w (Pt) is -E_conserved. 

    P.w = -E; 
    vec3 p = P.xyz;    
    
    KerrGeometry geo;
    ComputeGeometryScalars(X.xyz, PhysicalSpinA, PhysicalQ, fade, r_sign, geo);
    
    
    float L_dot_p_s = dot(geo.l_up.xyz, p);
    float Pt = P.w; 
    
    float p2 = dot(p, p);
    float Coeff_A = p2 - geo.f * L_dot_p_s * L_dot_p_s;
    
    float Coeff_B = 2.0 * geo.f * L_dot_p_s * Pt;
    
    float Coeff_C = -Pt * Pt * (1.0 + geo.f);
    
    float disc = Coeff_B * Coeff_B - 4.0 * Coeff_A * Coeff_C;
    
    if (disc >= 0.0) {
        float sqrtDisc = sqrt(disc);
        float denom = 2.0 * Coeff_A;
        
        if (abs(denom) > 1e-9) {
            float k1 = (-Coeff_B + sqrtDisc) / denom;
            float k2 = (-Coeff_B - sqrtDisc) / denom;
            

            float dist1 = abs(k1 - 1.0);
            float dist2 = abs(k2 - 1.0);
            
            float k = (dist1 < dist2) ? k1 : k2;
            
            P.xyz *= mix(k,1.0,clamp(abs(k-1.0)/0.1-1.0,0.0,1.0));//修正过强时强行修正会炸，在0.9到0.8过渡回退到不修正
        }
    }
}
//通过缩放动量空间部分修正哈密顿量
void ApplyHamiltonianCorrectionFORTEST(inout vec4 P, vec4 X, float E, float PhysicalSpinA, float PhysicalQ, float fade, float r_sign) {
    // P.w (Pt) is -E_conserved. 

    P.w = -E; 
    vec3 p = P.xyz;    
    
    KerrGeometry geo;
    ComputeGeometryScalars(X.xyz, PhysicalSpinA, PhysicalQ, fade, r_sign, geo);
    
    
    float L_dot_p_s = dot(geo.l_up.xyz, p);
    float Pt = P.w; 
    
    float p2 = dot(p, p);
    float Coeff_A = p2 - geo.f * L_dot_p_s * L_dot_p_s;
    
    float Coeff_B = 2.0 * geo.f * L_dot_p_s * Pt;
    
    float Coeff_C = -Pt * Pt * (1.0 + geo.f);
    
    float disc = Coeff_B * Coeff_B - 4.0 * Coeff_A * Coeff_C;
    
    if (disc >= 0.0) {
        float sqrtDisc = sqrt(disc);
        float denom = 2.0 * Coeff_A;
        
        if (abs(denom) > 1e-9) {
            float k1 = (-Coeff_B + sqrtDisc) / denom;
            float k2 = (-Coeff_B - sqrtDisc) / denom;
            

            float dist1 = abs(k1 - 1.0);
            float dist2 = abs(k2 - 1.0);
            
            float k = (dist1 < dist2) ? k1 : k2;
            
            P.xyz *= k1;
        }
    }
}
//哈密顿量时空导数
State GetDerivativesAnalytic(State S, float PhysicalSpinA, float PhysicalQ, float fade, inout KerrGeometry geo) {
    State deriv;
    
    ComputeGeometryGradients(S.X.xyz, PhysicalSpinA, PhysicalQ, fade, geo);
    
    // l^u * P_u
    float l_dot_P = dot(geo.l_up.xyz, S.P.xyz) + geo.l_up.w * S.P.w;
    
    // dx^u/dlambda = g^uv p_v = P_flat^u - f * (l.P) * l^u
    vec4 P_flat = vec4(S.P.xyz, -S.P.w); 
    deriv.X = P_flat - geo.f * l_dot_P * geo.l_up;
    
    // dp_u/dlambda = -dH/dx^u
    vec3 grad_A = (-2.0 * geo.r * geo.inv_r2_a2) * geo.inv_r2_a2 * geo.grad_r;
    
    float rx_az = geo.r * S.X.x - PhysicalSpinA * S.X.z;
    float rz_ax = geo.r * S.X.z + PhysicalSpinA * S.X.x;
    
    vec3 d_num_lx = S.X.x * geo.grad_r; 
    d_num_lx.x += geo.r; 
    d_num_lx.z -= PhysicalSpinA;
    vec3 grad_lx = geo.inv_r2_a2 * d_num_lx + rx_az * grad_A;
    
    vec3 grad_ly = (geo.r * vec3(0.0, 1.0, 0.0) - S.X.y * geo.grad_r) / geo.r2;
    
    vec3 d_num_lz = S.X.z * geo.grad_r;
    d_num_lz.z += geo.r;
    d_num_lz.x += PhysicalSpinA;
    vec3 grad_lz = geo.inv_r2_a2 * d_num_lz + rz_ax * grad_A;
    
    vec3 P_dot_grad_l = S.P.x * grad_lx + S.P.y * grad_ly + S.P.z * grad_lz;
    
    // Force = 0.5 * [ grad_f * (l.P)^2 + 2f(l.P) * grad(l.P) ]
    vec3 Force = 0.5 * ( (l_dot_P * l_dot_P) * geo.grad_f + (2.0 * geo.f * l_dot_P) * P_dot_grad_l );
    
    deriv.P = vec4(Force, 0.0); 
    
    return deriv;
}

//检测试探步是否穿过奇环面。Rk4里小步的符号需要实时更新不然会被弹飞
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

//Rk4，第一步复用外部结果
void StepGeodesicRK4_Optimized(
    inout vec4 X, inout vec4 P, 
    float E, float dt, 
    float PhysicalSpinA, float PhysicalQ, float fade, float r_sign, 
    KerrGeometry geo0, 
    State k1 //预计算的 k1
) {
    State s0; s0.X = X; s0.P = P;

    // k1 Step
    // State k1 = GetDerivativesAnalytic(s0, PhysicalSpinA, PhysicalQ, fade, geo0);

    // k2 Step
    State s1; 
    s1.X = s0.X + 0.5 * dt * k1.X; 
    s1.P = s0.P + 0.5 * dt * k1.P;
    float sign1 = GetIntermediateSign(s0.X, s1.X, r_sign, PhysicalSpinA);
    KerrGeometry geo1;
    ComputeGeometryScalars(s1.X.xyz, PhysicalSpinA, PhysicalQ, fade, sign1, geo1);
    State k2 = GetDerivativesAnalytic(s1, PhysicalSpinA, PhysicalQ, fade, geo1);

    // k3 Step
    State s2; 
    s2.X = s0.X + 0.5 * dt * k2.X; 
    s2.P = s0.P + 0.5 * dt * k2.P;
    float sign2 = GetIntermediateSign(s0.X, s2.X, r_sign, PhysicalSpinA);
    KerrGeometry geo2;
    ComputeGeometryScalars(s2.X.xyz, PhysicalSpinA, PhysicalQ, fade, sign2, geo2);
    State k3 = GetDerivativesAnalytic(s2, PhysicalSpinA, PhysicalQ, fade, geo2);

    // k4 Step
    State s3; 
    s3.X = s0.X + dt * k3.X; 
    s3.P = s0.P + dt * k3.P;
    float sign3 = GetIntermediateSign(s0.X, s3.X, r_sign, PhysicalSpinA);
    KerrGeometry geo3;
    ComputeGeometryScalars(s3.X.xyz, PhysicalSpinA, PhysicalQ, fade, sign3, geo3);
    State k4 = GetDerivativesAnalytic(s3, PhysicalSpinA, PhysicalQ, fade, geo3);

    vec4 finalX = s0.X + (dt / 6.0) * (k1.X + 2.0 * k2.X + 2.0 * k3.X + k4.X);
    vec4 finalP = s0.P + (dt / 6.0) * (k1.P + 2.0 * k2.P + 2.0 * k3.P + k4.P);

    
    float finalSign = GetIntermediateSign(s0.X, finalX, r_sign, PhysicalSpinA);
    if(finalSign>0){//antiverse侧修正有可能造成数值问题，暂时关停
    ApplyHamiltonianCorrection(finalP, finalX, E, PhysicalSpinA, PhysicalQ, fade, finalSign);
    }
    X = finalX;
    P = finalP;
}
// =============================================================================
// SECTION 6: 热折射，吸积盘与喷流,经纬网
// =============================================================================

#define ENABLE_HEAT_HAZE        1       // 1 = 开启热浪折射
#define HAZE_STRENGTH           0.2    // 折射强度
#define HAZE_SCALE              5.2     // 噪声频率
#define HAZE_DENSITY_THRESHOLD  0.1     // 密度阈值
#define HAZE_LAYER_THICKNESS    0.8     // 厚度范围倍数
#define HAZE_RADIAL_EXPAND      0.8     // 径向范围倍数
#define HAZE_ROT_SPEED          0.2     // 盘热气旋转速度系数 (相对于开普勒速度)
#define HAZE_FLOW_SPEED         0.15     // 喷流速度系数
#define HAZE_PROBE_STEPS        10      // 试探步数
#define HAZE_STEP_SIZE          0.05    // 每步长度 (Rg)
#define HAZE_DEBUG_MASK         0       // 1 = 显示热气遮罩 Debug
#define HAZE_DEBUG_VECTOR       0       // 1 = 显示力场向量 Debug

#define HAZE_DISK_DENSITY_REF   (iBrightmut * 30.0) 
#define HAZE_JET_DENSITY_REF    (iJetBrightmut * 1.0)


//heat haze基本函数
// heat haze热浪折射，噪声准备
float HazeNoise01(vec3 p) {
    return PerlinNoise(p) * 0.5 + 0.5;
}

// 基础 3D 噪声采样
float GetBaseNoise(vec3 p)
{
    float baseScale = HAZE_SCALE * 0.4; 
    vec3 pos = p * baseScale;
    
    // 给采样坐标加一个任意角度的旋转矩阵，防止噪声晶格与吸积盘平面(XZ)对齐
    const mat3 rotNoise = mat3(
         0.80,  0.60,  0.00,
        -0.48,  0.64,  0.60,
        -0.36,  0.48, -0.80
    );
    pos = rotNoise * pos;

    float n1 = HazeNoise01(pos); 
    float n2 = HazeNoise01(pos * 3.0 + vec3(13.5, -2.4, 4.1));

    return n1 * 0.6 + n2 * 0.4; 
}

// 计算吸积盘热浪遮罩
float GetDiskHazeMask(vec3 pos_Rg, float InterRadius, float OuterRadius, float Thin, float Hopper)
{
    float r = length(pos_Rg.xz);
    float y = abs(pos_Rg.y);
    
    float GeometricThin = Thin + max(0.0, (r - 3.0) * Hopper);
    float diskThickRef = GeometricThin; 
    
    float boundaryY = max(0.2, diskThickRef * HAZE_LAYER_THICKNESS);
    
    float vMaskDisk = 1.0 - smoothstep(boundaryY * 0.5, boundaryY * 1.5, y);
    float rMaskDisk = smoothstep(InterRadius * 0.3, InterRadius * 0.8, r) * 
                      (1.0 - smoothstep(OuterRadius * HAZE_RADIAL_EXPAND * 0.75, OuterRadius * HAZE_RADIAL_EXPAND, r));
    
    return vMaskDisk * rMaskDisk;
}

// 计算喷流热浪遮罩
float GetJetHazeMask(vec3 pos_Rg, float InterRadius, float OuterRadius)
{
    float r = length(pos_Rg.xz);
    float y = abs(pos_Rg.y);
    float RhoSq = r * r;
    
    // 逻辑复用自 JetColor
    // 喷流主要由两部分组成：核心 (Core) 和 外壳 (Shell)
    
    // 1. 核心半径估计 (Jet Core)
    float coreRadiusLimit = sqrt(2.0 * InterRadius * InterRadius + 0.03 * 0.03 * y * y);
    
    // 2. 外壳半径估计 (Jet Shell)
    // 对应 JetColor: Rho < 1.3 * InterRadius + 0.25 * Wid
    float shellRadiusLimit = 1.3 * InterRadius + 0.25 * y;
    
    // 取两者较大的作为热浪边界，并稍微膨胀以覆盖辉光
    float maxJetRadius = max(coreRadiusLimit, shellRadiusLimit) * 1.2;
    
    // 3. 垂直长度限制
    // 喷流在 OuterRadius 附近开始衰减
    float jLen = OuterRadius * 0.8;
    
    // 4. 生成遮罩
    float rMaskJet = 1.0 - smoothstep(maxJetRadius * 0.8, maxJetRadius * 1.1, r);
    float hMaskJet = 1.0 - smoothstep(jLen * 0.75, jLen * 1.0, y);
    
    // 喷流不应在吸积盘内部太强 (y 接近 0 时)，虽然 JetColor 也有处理，
    // 这里加一个平滑淡入防止在盘中心产生奇怪的重叠
    float startYMask = smoothstep(InterRadius * 0.5, InterRadius * 1.5, y);
    
    return rMaskJet * hMaskJet * startYMask;
}

// 包围盒检测优化
bool IsInHazeBoundingVolume(vec3 pos, float probeDist, float OuterRadius) {
    float maxR = OuterRadius * 1.2;
    float maxY = maxR; // 简化包围盒为球或大圆柱
    float r = length(pos);
    // 如果当前点在包围盒内，或者射线方向延伸 probeDist 后能碰到包围盒
    // 这里做个最简单的剔除：如果离原点太远且向外射，则忽略
    if (r > maxR + probeDist) return false;
    return true;
}

// 计算热浪偏移力
vec3 GetHazeForce(vec3 pos_Rg, float time, float PhysicalSpinA, float PhysicalQ, 
                  float InterRadius, float OuterRadius, float Thin, float Hopper,
                  float AccretionRate)
{
    // =========================
    // 1. 吸积盘热浪强度计算
    // =========================
    float dDens = HAZE_DISK_DENSITY_REF;
    float dLimitAbs = 20.0;
    float dFactorAbs = clamp((log(dDens/dLimitAbs)) / 2.302585, 0.0, 1.0);
    // 这里喷流密度仅作为参考
    float jDensRef = HAZE_JET_DENSITY_REF; 
    float dFactorRel = 1.0;
    if (jDensRef > 1e-20) dFactorRel = clamp((log(dDens/jDensRef)) / 2.302585, 0.0, 1.0);
    float diskHazeStrength = dFactorAbs * dFactorRel;

    // =========================
    // 2. 喷流热浪强度计算
    // =========================
    float jetHazeStrength = 0.0;
    float JetThreshold = 1e-2;
    
    // 如果吸积率低于阈值，直接优化跳过
    if (AccretionRate >= JetThreshold)
    {
        // 在 Log 空间内，从 1e-2 到 1.0 进行平滑过渡 (0.0 -> 1.0)
        // 超过 1.0 则保持最大强度
        float logRate = log(AccretionRate);
        float logMin  = log(JetThreshold);
        float logMax  = log(1.0);
        
        float intensity = clamp((logRate - logMin) / (logMax - logMin), 0.0, 1.0);
        jetHazeStrength = intensity;
    }

    // 早期退出优化
    if (diskHazeStrength <= 0.001 && jetHazeStrength <= 0.001) return vec3(0.0);

    vec3 totalForce = vec3(0.0);
    float eps = 0.1;

    // =========================
    // 3. 动态循环周期计算
    // =========================
    float rotSpeedBase = 100.0 * HAZE_ROT_SPEED; 
    float jetSpeedBase = 50.0 * HAZE_FLOW_SPEED;
    
    // 计算内边缘处的基准角速度 (用于确定噪声刷新的物理节奏)
    // M=0.5 -> Rs=1.0. 在 Rg 空间计算.
    float ReferenceOmega = GetKeplerianAngularVelocity(6.0, 1.0, PhysicalSpinA, PhysicalQ);
    
    // 设定一个合理的物理周期，内圈每旋转一定圈数，噪声完成一次淡入淡出循环。
    float AdaptiveFrequency = abs(ReferenceOmega * rotSpeedBase) / (2.0 * kPi * 5.14);
    
    // 限制一下最小频率防止除零或过慢
    AdaptiveFrequency = max(AdaptiveFrequency, 0.1);

    float flowTime = time * AdaptiveFrequency;
    
    float phase1 = fract(flowTime);
    float phase2 = fract(flowTime + 0.5);
    
    // 三角波权重 (0->1->0)
    float weight1 = 1.0 - abs(2.0 * phase1 - 1.0);
    float weight2 = 1.0 - abs(2.0 * phase2 - 1.0);
    
    bool doLayer1 = weight1 > 0.05;
    bool doLayer2 = weight2 > 0.05;
    
    float wTotal = (doLayer1 ? weight1 : 0.0) + (doLayer2 ? weight2 : 0.0);
    float w1_norm = (doLayer1 && wTotal > 0.0) ? (weight1 / wTotal) : 0.0;
    float w2_norm = (doLayer2 && wTotal > 0.0) ? (weight2 / wTotal) : 0.0;

    // 时间偏移
    float t_offset1 = phase1 - 0.5;
    float t_offset2 = phase2 - 0.5;

    // 引入垂直漂移：让噪声采样点随时间在Y轴移动，消除单调重复感
    float VerticalDrift1 = t_offset1 * 1.0; 
    float VerticalDrift2 = t_offset2 * 1.0;
    
    // -----------------------------------------------------
    // A. 吸积盘热浪
    // -----------------------------------------------------
    if (diskHazeStrength > 0.001)
    {
        float maskDisk = GetDiskHazeMask(pos_Rg, InterRadius, OuterRadius, Thin, Hopper);
        
        if (maskDisk > 0.001)
        {
            float r_local = length(pos_Rg.xz);
            float omega = GetKeplerianAngularVelocity(r_local, 1.0, PhysicalSpinA, PhysicalQ);
            
            vec3 gradWorldCombined = vec3(0.0);
            float valCombined = 0.0;

            if (doLayer1)
            {
                float angle1 = omega * rotSpeedBase * t_offset1;
                float c1 = cos(angle1); float s1 = sin(angle1);
                vec3 pos1 = pos_Rg;
                pos1.x = pos_Rg.x * c1 - pos_Rg.z * s1;
                pos1.z = pos_Rg.x * s1 + pos_Rg.z * c1;
                
                float val1 = GetBaseNoise(pos1);
                float nx1 = GetBaseNoise(pos1 + vec3(eps, 0.0, 0.0));
                float ny1 = GetBaseNoise(pos1 + vec3(0.0, eps, 0.0));
                float nz1 = GetBaseNoise(pos1 + vec3(0.0, 0.0, eps));
                vec3 grad1 = vec3(nx1 - val1, ny1 - val1, nz1 - val1);
                
                vec3 gradWorld1;
                gradWorld1.x = grad1.x * c1 + grad1.z * s1;
                gradWorld1.y = grad1.y;
                gradWorld1.z = -grad1.x * s1 + grad1.z * c1;
                
                gradWorldCombined += gradWorld1 * w1_norm;
                valCombined += val1 * w1_norm;
            }
            
            if (doLayer2)
            {
                float angle2 = omega * rotSpeedBase * t_offset2;
                float c2 = cos(angle2); float s2 = sin(angle2);
                vec3 pos2 = pos_Rg;
                pos2.x = pos_Rg.x * c2 - pos_Rg.z * s2;
                pos2.z = pos_Rg.x * s2 + pos_Rg.z * c2;
                
                float val2 = GetBaseNoise(pos2);
                float nx2 = GetBaseNoise(pos2 + vec3(eps, 0.0, 0.0));
                float ny2 = GetBaseNoise(pos2 + vec3(0.0, eps, 0.0));
                float nz2 = GetBaseNoise(pos2 + vec3(0.0, 0.0, eps));
                vec3 grad2 = vec3(nx2 - val2, ny2 - val2, nz2 - val2);
                
                vec3 gradWorld2;
                gradWorld2.x = grad2.x * c2 + grad2.z * s2;
                gradWorld2.y = grad2.y;
                gradWorld2.z = -grad2.x * s2 + grad2.z * c2;
                
                gradWorldCombined += gradWorld2 * w2_norm;
                valCombined += val2 * w2_norm;
            }
            
            float cloud = max(0.0, valCombined - HAZE_DENSITY_THRESHOLD);
            cloud /= (1.0 - HAZE_DENSITY_THRESHOLD);
            cloud = pow(cloud, 1.5);
            
            totalForce += gradWorldCombined * maskDisk * cloud * diskHazeStrength;
        }
    }

    // -----------------------------------------------------
    // B. 喷流热浪
    // -----------------------------------------------------
    if (jetHazeStrength > 0.001)
    {
        float maskJet = GetJetHazeMask(pos_Rg, InterRadius, OuterRadius);
        
        if (maskJet > 0.001)
        {
            float v_jet_mag = 0.9; 
            
            float dist1 = v_jet_mag * jetSpeedBase * t_offset1;
            float dist2 = v_jet_mag * jetSpeedBase * t_offset2;
            
            vec3 gradCombined = vec3(0.0);
            float valCombined = 0.0;
            
            if (doLayer1)
            {
                vec3 pos1 = pos_Rg;
                pos1.y -= sign(pos_Rg.y) * dist1;
                float val1 = GetBaseNoise(pos1);
                float nx1 = GetBaseNoise(pos1 + vec3(eps, 0.0, 0.0));
                float ny1 = GetBaseNoise(pos1 + vec3(0.0, eps, 0.0));
                float nz1 = GetBaseNoise(pos1 + vec3(0.0, 0.0, eps));
                vec3 grad1 = vec3(nx1 - val1, ny1 - val1, nz1 - val1);
                gradCombined += grad1 * w1_norm;
                valCombined += val1 * w1_norm;
            }
            
            if (doLayer2)
            {
                vec3 pos2 = pos_Rg;
                pos2.y -= sign(pos_Rg.y) * dist2;
                float val2 = GetBaseNoise(pos2);
                float nx2 = GetBaseNoise(pos2 + vec3(eps, 0.0, 0.0));
                float ny2 = GetBaseNoise(pos2 + vec3(0.0, eps, 0.0));
                float nz2 = GetBaseNoise(pos2 + vec3(0.0, 0.0, eps));
                vec3 grad2 = vec3(nx2 - val2, ny2 - val2, nz2 - val2);
                gradCombined += grad2 * w2_norm;
                valCombined += val2 * w2_norm;
            }
            
            float cloud = max(0.0, valCombined - 0.3-0.7*HAZE_DENSITY_THRESHOLD); // 喷流的heat haze相比吸积盘需要更多空隙，不然看着怪
            cloud /= clamp((1.0 - 0.3-0.7*HAZE_DENSITY_THRESHOLD),0.0,1.0);
            cloud = pow(cloud, 1.5);
            
            totalForce += gradCombined * maskJet * cloud * jetHazeStrength;
        }
    }

    return totalForce;
}



vec4 DiskColor(vec4 BaseColor, float StepLength, vec4 RayPos, vec4 LastRayPos,
               vec3 RayDir, vec3 LastRayDir,vec4 iP_cov, float iE_obs,
               float InterRadius, float OuterRadius, float Thin, float Hopper, float Brightmut, float Darkmut, float Reddening, float Saturation, float DiskTemperatureArgument,
               float BlackbodyIntensityExponent, float RedShiftColorExponent, float RedShiftIntensityExponent,
               float PeakTemperature, float ShiftMax, 
               float PhysicalSpinA, 
               float PhysicalQ,
               float ThetaInShell,
               inout float RayMarchPhase 
               ) 
{
    vec4 CurrentResult = BaseColor;
    

    float MaxDiskHalfHeight = Thin + max(0.0, Hopper * OuterRadius) + 2.0; 
    if (LastRayPos.y > MaxDiskHalfHeight && RayPos.y > MaxDiskHalfHeight) return BaseColor;
    if (LastRayPos.y < -MaxDiskHalfHeight && RayPos.y < -MaxDiskHalfHeight) return BaseColor;

    vec2 P0 = LastRayPos.xz;
    vec2 P1 = RayPos.xz;
    vec2 V  = P1 - P0;
    float LenSq = dot(V, V);
    float t_closest = (LenSq > 1e-8) ? clamp(-dot(P0, V) / LenSq, 0.0, 1.0) : 0.0;
    vec2 ClosestPoint = P0 + V * t_closest;
    if (dot(ClosestPoint, ClosestPoint) > (OuterRadius * 1.1) * (OuterRadius * 1.1)) return BaseColor;

    vec3 StartPos = LastRayPos.xyz; 
    vec3 DirVec   = RayDir; 
    float StartTimeLag = LastRayPos.w;
    float EndTimeLag   = RayPos.w;

    float R_Start = KerrSchildRadius(StartPos, PhysicalSpinA, 1.0);
    float R_End   = KerrSchildRadius(RayPos.xyz, PhysicalSpinA, 1.0);
    if (max(R_Start, R_End) < InterRadius * 0.9) return BaseColor;

    
    float TotalDist = StepLength;
    float TraveledDist = 0.0;
    
    int SafetyLoopCount = 0;
    const int MaxLoops = 114514; 

    while (TraveledDist < TotalDist && SafetyLoopCount < MaxLoops)
    {
        if (CurrentResult.a > 0.99) break;
        SafetyLoopCount++;

        vec3 CurrentPos = StartPos + DirVec * TraveledDist;
        float DistanceToBlackHole = length(CurrentPos); 
        
        // 计算局部密度
        float SmallStepBoundary = max(OuterRadius, 12.0);
        float StepSize = 1.0; 
        
        StepSize *= 0.15 + 0.25 * min(max(0.0, 0.5 * (0.5 * DistanceToBlackHole / max(10.0 , SmallStepBoundary) - 1.0)), 1.0);
        if ((DistanceToBlackHole) >= 2.0 * SmallStepBoundary) StepSize *= DistanceToBlackHole;
        else if ((DistanceToBlackHole) >= 1.0 * SmallStepBoundary) StepSize *= ((1.0 + 0.25 * max(DistanceToBlackHole - 12.0, 0.0)) * (2.0 * SmallStepBoundary - DistanceToBlackHole) + DistanceToBlackHole * (DistanceToBlackHole - SmallStepBoundary)) / SmallStepBoundary;
        else StepSize *= min(1.0 + 0.25 * max(DistanceToBlackHole - 12.0, 0.0), DistanceToBlackHole);
        
        StepSize = max(0.01, StepSize); 

        //  相位与距离计算
        float DistToNextSample = RayMarchPhase * StepSize;
        float DistRemainingInRK4 = TotalDist - TraveledDist;

        if (DistToNextSample > DistRemainingInRK4)
        {
            // 情况 A：下一个采样点超出了当前的 RK4 步长范围
            // 我们走完这段剩余距离，但不进行采样
            // 并更新相位，表示我们已经走了一部分路程
            
            float PhaseProgress = DistRemainingInRK4 / StepSize;
            RayMarchPhase -= PhaseProgress; // 消耗相位
            
            // 确保相位数值稳定
            if(RayMarchPhase < 0.0) RayMarchPhase = 0.0; // 理论不应发生，除非精度误差
            
            TraveledDist = TotalDist; // 结束本段积分
            break;
        }

        float dt = DistToNextSample;
        
        // 移动到采样点
        TraveledDist += dt;
        vec3 SamplePos = StartPos + DirVec * TraveledDist;
        
        float TimeInterpolant = min(1.0, TraveledDist / max(1e-9, TotalDist));
        float CurrentRayTimeLag = mix(StartTimeLag, EndTimeLag, TimeInterpolant);
        float EmissionTime = iBlackHoleTime + CurrentRayTimeLag;

        // 薄盘优化
        vec3 PreviousPos = CurrentPos; // 这一步的起点
        //if(PreviousPos.y * SamplePos.y < 0.0)
        //{
        //
        //    vec3 CPoint=(-SamplePos*PreviousPos.y+PreviousPos*SamplePos.y)/(SamplePos.y-PreviousPos.y);
        //    SamplePos=CPoint+min(Thin,length(CPoint-PreviousPos))*DirVec*(-1.0+2.0*RandomStep(10000.0*(SamplePos.zx/OuterRadius), fract(iTime * 1.0 + 0.5)));
        //    
        //
        //}
        float PosR = KerrSchildRadius(SamplePos, PhysicalSpinA, 1.0);
        float PosY = SamplePos.y;
        
        float GeometricThin = Thin + max(0.0, (length(SamplePos.xz) - 3.0) * Hopper);
        
        // 计算内侧云参数与包围盒
        float InterCloudEffectiveRadius = (PosR - InterRadius) / min(OuterRadius - InterRadius, 12.0);
        float InnerCloudBound = max(GeometricThin, Thin * 1.0) * max(0.0,1.0 - 5.0 * pow(InterCloudEffectiveRadius, 2.0));
        
        // 外层包围盒取主盘与内侧云的并集
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
                 
                 
                 //vec3 FluidVel = AngularVelocity * vec3(SamplePos.z, 0.0, -SamplePos.x);
                 //vec4 U_fluid_unnorm = vec4(FluidVel, 1.0); 
                 //KerrGeometry geo_sample;
                 //ComputeGeometryScalars(SamplePos, PhysicalSpinA, PhysicalQ, 1.0, 1.0, geo_sample);
                 //vec4 U_fluid_lower = LowerIndex(U_fluid_unnorm, geo_sample);
                 //float norm_sq = dot(U_fluid_unnorm, U_fluid_lower);
                 //vec4 U_fluid = U_fluid_unnorm * inversesqrt(max(1e-6, abs(norm_sq)));
                 //float E_emit = -dot(iP_cov, U_fluid);
                 //float FreqRatio =  1.0/ max(1e-6, E_emit);
                 

                 // [修改]解析法计算红移
                 float inv_r = 1.0 / max(1e-6, PosR);
                 float inv_r2 = inv_r * inv_r;
                 
                 // 无量纲势能项 (M=0.5 -> 2M=1.0)
                 float V_pot = inv_r - (PhysicalQ * PhysicalQ) * inv_r2;
                 
                 // 赤道面度规分量 g_uv
                 float g_tt = -(1.0 - V_pot);
                 float g_tphi = -PhysicalSpinA * V_pot; 
                 float g_phiphi = PosR * PosR + PhysicalSpinA * PhysicalSpinA + PhysicalSpinA * PhysicalSpinA * V_pot;
                 
                 // 归一化条件 U.U = -1 => norm * (u^t)^2 = -1
                 float norm_metric = g_tt + 2.0 * AngularVelocity * g_tphi + AngularVelocity * AngularVelocity * g_phiphi;
                 
                 // 防止超光速区域 (norm >= 0) 导致崩溃
                 float min_norm = -0.01; 
                 float u_t = inversesqrt(max(abs(min_norm), -norm_metric));
                 
                 // 计算角动量 P_phi = x*P_z - z*P_x (这里符号要反一下)
                 float P_phi = - SamplePos.x * iP_cov.z + SamplePos.z * iP_cov.x;
                 
                 // 计算发射能量 E_emit = -u^mu P_mu = u^t * (iE_obs - Omega * P_phi)
                 // 注：iE_obs 即为传入的守恒能量 E_conserved
                 float E_emit = u_t * (iE_obs - AngularVelocity * P_phi);
                 float FreqRatio = 1.0 / max(1e-6, E_emit);
                 // [修改]解析法计算红移结束





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
    
                 SampleColor.xyz *=1.0+    clamp(  iPhotonRingBoost        ,0.0,10.0)  *clamp(0.3*ThetaInShell-0.1,0.0,1.0);
                 VisionTemperature *= 1.0 +clamp( iPhotonRingColorTempBoost,0.0,10.0) * clamp(0.3*ThetaInShell-0.1,0.0,1.0);
                 // 内侧点缀云
                 // 计算内侧独立坐标系
                 float InnerAngVel = GetKeplerianAngularVelocity(3.0, 1.0, PhysicalSpinA, PhysicalQ);
                 float InnerCloudTimePhase = kPi / (kPi / max(1e-6, InnerAngVel)) * EmissionTime; 
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
                 SampleColor.xyz *= Brightmut*clamp(4.0-18.0*(PosR-InterRadius)/(OuterRadius - InterRadius),1.0,4.0);
                 SampleColor.a   *= Darkmut*clamp(5.0-24.0*(PosR-InterRadius)/(OuterRadius - InterRadius),1.0,5.0);
                 
                 vec4 StepColor = SampleColor * StepSize;
                 
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
        RayMarchPhase = 1.0;
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
            float FreqRatio = 1.0/max(1e-6, E_emit);

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

// 空间坐标网格
vec4 GridColor(vec4 BaseColor, vec4 RayPos, vec4 LastRayPos,
               vec4 iP_cov, float iE_obs,
               float PhysicalSpinA, float PhysicalQ,
               float EndStepSign) 
{
    vec4 CurrentResult = BaseColor;
    if (CurrentResult.a > 0.99) return CurrentResult;

    const int MaxGrids = 12; 
    float SignedGridRadii[MaxGrids]; 
    int GridCount = 0;
    
    float StartStepSign = EndStepSign;
    bool bHasCrossed = false;
    float t_cross = -1.0;
    vec3 DiskHitPos = vec3(0.0);
    
    if (LastRayPos.y * RayPos.y < 0.0) {
        float denom = (LastRayPos.y - RayPos.y);
        if(abs(denom) > 1e-9) {
            t_cross = LastRayPos.y / denom;
            DiskHitPos = mix(LastRayPos.xyz, RayPos.xyz, t_cross);
            
            if (length(DiskHitPos.xz) < abs(PhysicalSpinA)) {
                StartStepSign = -EndStepSign;
                bHasCrossed = true;
            }
        }
    }

    bool CheckPositive = (StartStepSign > 0.0) || (EndStepSign > 0.0);
    bool CheckNegative = (StartStepSign < 0.0) || (EndStepSign < 0.0);

    float HorizonDiscrim = 0.25 - PhysicalSpinA * PhysicalSpinA - PhysicalQ * PhysicalQ;
    float RH_Outer = 0.5 + sqrt(max(0.0, HorizonDiscrim));
    float RH_Inner = 0.5 - sqrt(max(0.0, HorizonDiscrim));

    if (CheckPositive) {
        SignedGridRadii[GridCount++] = RH_Outer * 1.05; 
        SignedGridRadii[GridCount++] = 20.0;
        
        if (HorizonDiscrim >= 0.0) {
           SignedGridRadii[GridCount++] = RH_Inner * 0.95; 
        }
    }
    
    if (CheckNegative) {
        SignedGridRadii[GridCount++] = -3.0;  
        SignedGridRadii[GridCount++] = -10.0; 
    }

    vec3 O = LastRayPos.xyz;
    vec3 D_vec = RayPos.xyz - LastRayPos.xyz;

    for (int i = 0; i < GridCount; i++) {
        if (CurrentResult.a > 0.99) break;

        float TargetSignedR = SignedGridRadii[i];
        float TargetGeoR = abs(TargetSignedR); 

        vec2 roots = IntersectKerrEllipsoid(O, D_vec, TargetGeoR, PhysicalSpinA);
        
        float t_hits[2];
        t_hits[0] = roots.x;
        t_hits[1] = roots.y;
        
        if (t_hits[0] > t_hits[1]) {
            float temp = t_hits[0]; t_hits[0] = t_hits[1]; t_hits[1] = temp;
        }
        
        for (int j = 0; j < 2; j++) {
            float t = t_hits[j];
            
            if (t >= 0.0 && t <= 1.0) {
                
                float HitPointSign = StartStepSign;
                if (bHasCrossed) {
                    if (t > t_cross) {
                        HitPointSign = EndStepSign;
                    }
                }

                if (HitPointSign * TargetSignedR < 0.0) continue;

                vec3 HitPos = O + D_vec * t;
                float CheckR = KerrSchildRadius(HitPos, PhysicalSpinA, HitPointSign);
                if (abs(CheckR - TargetSignedR) > 0.1 * TargetGeoR + 0.1) continue; 

                // 计算物理量
                float Omega = GetZamoOmega(TargetSignedR, PhysicalSpinA, PhysicalQ, HitPos.y);
                vec3 VelSpatial = Omega * vec3(HitPos.z, 0.0, -HitPos.x);
                vec4 U_zamo_unnorm = vec4(VelSpatial, 1.0); 
                
                KerrGeometry geo_hit;
                ComputeGeometryScalars(HitPos, PhysicalSpinA, PhysicalQ, 1.0, HitPointSign, geo_hit);
                
                vec4 U_zamo_lower = LowerIndex(U_zamo_unnorm, geo_hit);
                float norm_sq = dot(U_zamo_unnorm, U_zamo_lower);
                float norm = sqrt(max(1e-9, abs(norm_sq)));
                vec4 U_zamo = U_zamo_unnorm / norm;

                float E_emit = -dot(iP_cov, U_zamo);
                float Shift = 1.0/ max(1e-6, abs(E_emit)); 

                // 纹理计算
                float Phi = Vec2ToTheta(normalize(HitPos.zx), vec2(0.0, 1.0));
                float CosTheta = clamp(HitPos.y / TargetGeoR, -1.0, 1.0);
                float Theta = acos(CosTheta);
                float SinTheta = sqrt(max(0.0, 1.0 - CosTheta * CosTheta));

                float DensityPhi = 24.0;
                float DensityTheta = 12.0;
                float DistFactor = length(HitPos);
                float LineWidth = 0.001 * DistFactor;
                LineWidth = clamp(LineWidth, 0.01, 0.1); 

                float PatternPhi = abs(fract(Phi / (2.0 * kPi) * DensityPhi) - 0.5);
                float GridPhi = smoothstep(LineWidth / max(0.005, SinTheta), 0.0, PatternPhi);

                float PatternTheta = abs(fract(Theta / kPi * DensityTheta) - 0.5);
                float GridTheta = smoothstep(LineWidth, 0.0, PatternTheta);
                
                float GridIntensity = max(GridPhi, GridTheta);

                if (GridIntensity > 0.01) {
                    // 常规网格着色
                    float BaseTemp = 6500.0;
                    vec3 BlackbodyColor = KelvinToRgb(BaseTemp * Shift);
                    float Intensity = min(1.5 * pow(Shift, 4.0), 20.0);
                    vec4 GridCol = vec4(BlackbodyColor * Intensity, 1.0);
                    
                    float Alpha = GridIntensity * 0.5; 
                    CurrentResult.rgb += GridCol.rgb * Alpha * (1.0 - CurrentResult.a);
                    CurrentResult.a   += Alpha * (1.0 - CurrentResult.a);
                }
            }
        }
    }

    //  单独处理 r=0 
    if (bHasCrossed && CurrentResult.a < 0.99) {
        
        
        float HitRho = length(DiskHitPos.xz);
        float a_abs = abs(PhysicalSpinA);
        
        float Phi = Vec2ToTheta(normalize(DiskHitPos.zx), vec2(0.0, 1.0));
        
        float DensityPhi = 24.0;
        float DistFactor = length(DiskHitPos); 
        float LineWidth = 0.001 * DistFactor;
        LineWidth = clamp(LineWidth, 0.01, 0.1);

        float PatternPhi = abs(fract(Phi / (2.0 * kPi) * DensityPhi) - 0.5);
        float GridPhi = smoothstep(LineWidth / max(0.1, HitRho / a_abs), 0.0, PatternPhi);

        float NormalizedRho = HitRho / max(1e-6, a_abs);
        float DensityRho = 5.0; 
        float PatternRho = abs(fract(NormalizedRho * DensityRho) - 0.5);
        float GridRho = smoothstep(LineWidth, 0.0, PatternRho);
        
        float GridIntensity = max(GridPhi, GridRho);


        if (GridIntensity > 0.01) {
            float Omega0 = 0.0; 
            
            vec3 VelSpatial = vec3(0.0); 
            vec4 U_zero = vec4(0.0, 0.0, 0.0, 1.0); 
            
            float E_emit = -dot(iP_cov, U_zero); 
            float Shift = 1.0 / max(1e-6, abs(E_emit));
            
            float BaseTemp = 6500.0; 
            vec3 BlackbodyColor = KelvinToRgb(BaseTemp * Shift);
            float Intensity = min(2.0 * pow(Shift, 4.0), 30.0);
            
            vec4 GridCol = vec4(BlackbodyColor * Intensity, 1.0);
            
            float Alpha = GridIntensity * 0.5;
            CurrentResult.rgb += GridCol.rgb * Alpha * (1.0 - CurrentResult.a);
            CurrentResult.a   += Alpha * (1.0 - CurrentResult.a);
        }
    }

    return CurrentResult;
}


vec4 GridColorSimple(vec4 BaseColor, vec4 RayPos, vec4 LastRayPos,
               float PhysicalSpinA, float PhysicalQ,
               float EndStepSign) 
{
    vec4 CurrentResult = BaseColor;
    if (CurrentResult.a > 0.99) return CurrentResult;

    const int MaxGrids = 5; 
    
    float SignedGridRadii[MaxGrids]; 
    vec3  GridColors[MaxGrids];
    int   GridCount = 0;
    
    float StartStepSign = EndStepSign;
    bool bHasCrossed = false;
    float t_cross = -1.0;
    vec3 DiskHitPos = vec3(0.0);
    
    if (LastRayPos.y * RayPos.y < 0.0) {
        float denom = (LastRayPos.y - RayPos.y);
        if(abs(denom) > 1e-9) {
            t_cross = LastRayPos.y / denom;
            DiskHitPos = mix(LastRayPos.xyz, RayPos.xyz, t_cross);
            
            if (length(DiskHitPos.xz) < abs(PhysicalSpinA)) {
                StartStepSign = -EndStepSign;
                bHasCrossed = true;
            }
        }
    }

    bool CheckPositive = (StartStepSign > 0.0) || (EndStepSign > 0.0);
    bool CheckNegative = (StartStepSign < 0.0) || (EndStepSign < 0.0);

    float HorizonDiscrim = 0.25 - PhysicalSpinA * PhysicalSpinA - PhysicalQ * PhysicalQ;
    float RH_Outer = 0.5 + sqrt(max(0.0, HorizonDiscrim));
    float RH_Inner = 0.5 - sqrt(max(0.0, HorizonDiscrim));
    bool HasHorizon = HorizonDiscrim >= 0.0;

    if (CheckPositive) {
        SignedGridRadii[GridCount] = 20.0;
        GridColors[GridCount] = 0.3*vec3(0.0, 1.0, 1.0); 
        GridCount++;

        if (HasHorizon) {
            SignedGridRadii[GridCount] = RH_Outer * 1.01 + 0.05; 
            GridColors[GridCount] = 0.3*vec3(0.0, 1.0, 0.0); 
            GridCount++;
            
            SignedGridRadii[GridCount] = RH_Inner * 0.99 - 0.05; 
            GridColors[GridCount] =0.3* vec3(1.0, 0.0, 0.0); 
            GridCount++;
        }
    }
    
    if (CheckNegative) {
        SignedGridRadii[GridCount] = -20.0;  
        GridColors[GridCount] = 0.3*vec3(1.0, 0.0, 1.0); 
        GridCount++;
    }

    vec3 O = LastRayPos.xyz;
    vec3 D_vec = RayPos.xyz - LastRayPos.xyz;

    for (int i = 0; i < GridCount; i++) {
        if (CurrentResult.a > 0.99) break;

        float TargetSignedR = SignedGridRadii[i];
        float TargetGeoR = abs(TargetSignedR); 
        vec3  TargetColor = GridColors[i];

        vec2 roots = IntersectKerrEllipsoid(O, D_vec, TargetGeoR, PhysicalSpinA);
        
        float t_hits[2];
        t_hits[0] = roots.x;
        t_hits[1] = roots.y;
        if (t_hits[0] > t_hits[1]) {
            float temp = t_hits[0]; t_hits[0] = t_hits[1]; t_hits[1] = temp;
        }
        
        for (int j = 0; j < 2; j++) {
            float t = t_hits[j];
            
            if (t >= 0.0 && t <= 1.0) {
                
                float HitPointSign = StartStepSign;
                if (bHasCrossed) {
                    if (t > t_cross) {
                        HitPointSign = EndStepSign;
                    }
                }

                if (HitPointSign * TargetSignedR < 0.0) continue;

                vec3 HitPos = O + D_vec * t;
                
                float CheckR = KerrSchildRadius(HitPos, PhysicalSpinA, HitPointSign);
                if (abs(CheckR - TargetSignedR) > 0.1 * TargetGeoR + 0.1) continue; 

                float Phi = Vec2ToTheta(normalize(HitPos.zx), vec2(0.0, 1.0));
                float CosTheta = clamp(HitPos.y / TargetGeoR, -1.0, 1.0);
                float Theta = acos(CosTheta);
                float SinTheta = sqrt(max(0.0, 1.0 - CosTheta * CosTheta));

                float DensityPhi = 24.0;
                float DensityTheta = 12.0;
                float DistFactor = length(HitPos);
                float LineWidth = 0.002 * DistFactor; 
                LineWidth = clamp(LineWidth, 0.01, 0.15); 

                float PatternPhi = abs(fract(Phi / (2.0 * kPi) * DensityPhi) - 0.5);
                float GridPhi = smoothstep(LineWidth / max(0.005, SinTheta), 0.0, PatternPhi);

                float PatternTheta = abs(fract(Theta / kPi * DensityTheta) - 0.5);
                float GridTheta = smoothstep(LineWidth, 0.0, PatternTheta);
                
                float GridIntensity = max(GridPhi, GridTheta);

                if (GridIntensity > 0.01) {
                    vec4 GridCol = vec4(TargetColor * 2.0, 1.0);
                    
                    float Alpha = GridIntensity * 0.8; 
                    CurrentResult.rgb += GridCol.rgb * Alpha * (1.0 - CurrentResult.a);
                    CurrentResult.a   += Alpha * (1.0 - CurrentResult.a);
                }
            }
        }
    }

    if (bHasCrossed && CurrentResult.a < 0.99) {
        
        float HitRho = length(DiskHitPos.xz);
        float a_abs = abs(PhysicalSpinA);
        
        float Phi = Vec2ToTheta(normalize(DiskHitPos.zx), vec2(0.0, 1.0));
        
        float DensityPhi = 24.0;
        float DistFactor = length(DiskHitPos); 
        float LineWidth = 0.002 * DistFactor;
        LineWidth = clamp(LineWidth, 0.01, 0.1);

        float PatternPhi = abs(fract(Phi / (2.0 * kPi) * DensityPhi) - 0.5);
        float GridPhi = smoothstep(LineWidth / max(0.1, HitRho / a_abs), 0.0, PatternPhi);

        float NormalizedRho = HitRho / max(1e-6, a_abs);
        float DensityRho = 5.0; 
        float PatternRho = abs(fract(NormalizedRho * DensityRho) - 0.5);
        float GridRho = smoothstep(LineWidth, 0.0, PatternRho);
        
        float GridIntensity = max(GridPhi, GridRho);

        if (GridIntensity > 0.01) {
            vec3 RingColor = 0.3*vec3(1.0, 1.0, 1.0);
            vec4 GridCol = vec4(RingColor * 5.0, 1.0);
            
            float Alpha = GridIntensity * 0.8;
            CurrentResult.rgb += GridCol.rgb * Alpha * (1.0 - CurrentResult.a);
            CurrentResult.a   += Alpha * (1.0 - CurrentResult.a);
        }
    }

    return CurrentResult;
}


// =============================================================================
// SECTION7: KN阴影计算
// =============================================================================

// 判断吸积盘是否“视觉上存在”
bool IsAccretionDiskVisible(float InterR, float OuterR, float Thin, float Hopper, float Bright, float Dark)
{
    // 条件1: 内半径大于等于外半径 -> 不存在
    if (InterR >= OuterR) return false;
    // 条件2: 几何厚度为0 (Thin<=0 且 Hopper==0) -> 不存在
    if (Thin <= 0.0 && Hopper == 0.0) return false;
    // 条件3: 既不发光也不遮挡 (Bright<=0 且 Dark<0) -> 不存在
    // 注意: Darkmut通常是正数表示遮挡能力，题目说 Dark<0 视为“没有”，遵照执行
    if (Bright <= 0.0 && Dark < 0.0) return false;
    
    return true;
}

// 判断喷流是否“视觉上存在”
bool IsJetVisible(float AccretionRate, float JetBright)
{
    // 吸积率过低 或 亮度<=0 -> 不存在
    if (AccretionRate < 1e-2) return false;
    if (JetBright <= 0.0) return false;
    return true;
}

// 求解极轴视角的临界半径 (三次方程最大实根)
// x^3 + Px + K = 0, x = r - M
float SolveCubicMaxReal(float P, float K) {
    if (P >= 0.0) return 0.0; // 理论上黑洞情形P均为负
    float sqrt_term = sqrt(-P / 3.0);
    // 限制 acos 输入在 [-1, 1] 防止 NaN
    float val = (3.0 * K) / (2.0 * P) * sqrt(-3.0 / P);
    float acos_term = acos(clamp(val, -1.0, 1.0));
    return 2.0 * sqrt_term * cos(acos_term / 3.0);
}

// 求解赤道视角光子球参数 u (四次方程)
float SolveQuarticU(float M, float Q, float a, float sign_term, bool is_max_root) {
    float M2 = M * M;
    float Q2 = Q * Q;
    
    // 系数
    float c2 = 2.0 * Q2 - 3.0 * M2;
    float c1 = sign_term * (-2.0 * a * M2); // 注意：文档说"减号版本"即系数为负
    float c0 = Q2 * Q2 - M2 * Q2;
    
    // 初始猜测：
    // 对于顺行(A, 小根)，u 较小 (r 接近 M 或 2M)
    // 对于逆行(B, 大根)，u 较大 (r 接近 3M 或 4M)
    float u = is_max_root ? 2.2 * M : 0.8 * M; 
    
    // 牛顿迭代求解多项式根 (多项式非常平滑，收敛极快)
    for(int i=0; i<8; i++) {
        float u2 = u * u;
        float u3 = u2 * u;
        
        float f  = u2 * u2 + c2 * u2 + c1 * u + c0;
        float df = 4.0 * u3 + 2.0 * c2 * u + c1;
        
        if (abs(df) < 1e-6) break;
        u = u - f / df;
    }
    return abs(u); // u = sqrt(...) 必须为正
}

// 将静态观测者的正弦值转换为落体观测者
float GetDropFrameAngle(float SinThetaStat, float CosThetaStat, float r, float M, float Q, float a, int ObserverMode) {
    // 1. 静态观者 (ObserverMode == 0)
    // 用 atan2 计算角度，自动处理 CosThetaStat < 0 (钝角) 的情况
    if (ObserverMode == 0) {
        return atan(SinThetaStat, CosThetaStat);
    }
    
    // 2. 自由落体观者 (ObserverMode == 1)
    // 虽然Static Observer 是固定在 KS 坐标系下的，而非 ZAMO，但因为一些误差，这里denominator_v直接使用r*r会出问题。
    float numerator_v = 2.0 * M * r - Q * Q;
    float denominator_v = r * r - 2.0*r*a*a; // -2.0*r*a*a是为了修正误差
    
    float v_sq = numerator_v / max(1e-9, denominator_v);
    v_sq = min(0.9999, max(0.0, v_sq)); 
    float v = sqrt(v_sq);
    
    // 应用相对论光行差
    // sin(θ') = sin(θ) * sqrt(1-v^2) / (1 + v*cos(θ))
    // cos(θ') = (cos(θ) + v) / (1 + v*cos(θ))
    
    float denom = 1.0 + v * CosThetaStat;
    float sin_fall = SinThetaStat * sqrt(max(0.0, 1.0 - v_sq));
    float cos_fall = CosThetaStat + v;
    
    // 使用 atan2 自动处理所有象限和归一化问题
    return atan(sin_fall, cos_fall);
}

// 计算 Reissner-Nordstrom (a=0) 黑洞的阴影半张角 (弧度)
float GetShadowHalfAngleRN(float r, float M, float Q, int ObserverMode)
{
    float M2 = M * M;
    float Q2 = Q * Q;
    float r2 = r * r;
    
    // 1. 光子球半径 r_ps
    float term_root = sqrt(max(0.0, 9.0 * M2 - 8.0 * Q2));
    float r_ps = 0.5 * (3.0 * M + term_root);
    
    // 2. 临界碰撞参数 b_c
    float metric_factor_ps = 1.0 - 2.0 * M / r_ps + Q2 / (r_ps * r_ps);
    float b_c = r_ps / sqrt(max(1e-6, metric_factor_ps));
    
    // 3. 计算静态观者的 Sine 和 Cosine
    // f(r) = 1 - 2M/r + Q^2/r^2
    float f_r = 1.0 - 2.0 * M / r + Q2 / r2;
    float sqrt_f = sqrt(max(0.0, f_r));
    
    // Sin = (b_c / r) * sqrt(f)
    float sin_theta_stat = (b_c / r) * sqrt_f;
    
    // 判断光子球内外来决定 Cos 的符号
    // r < r_ps 时，阴影遮挡超过半个天空，为钝角 (Cos < 0)
    // 增加一个微小的 epsilon 防止 r == r_ps 时闪烁
    float cos_sign = (r >= r_ps - 1e-4) ? 1.0 : -1.0;
    
    // 计算 Cos: sqrt(1 - sin^2)
    float cos_theta_stat = cos_sign * sqrt(max(0.0, 1.0 - sin_theta_stat * sin_theta_stat));
    
    // 4. 转换坐标系
    return GetDropFrameAngle(sin_theta_stat, cos_theta_stat, r, M, Q, 0.0, ObserverMode);
}


// =============================================================================
// SECTION8: main
// =============================================================================

struct TraceResult {
    vec3  EscapeDir;      // 最终逸出方向 (World Space)
    float FreqShift;      // 频移 (E_emit / E_obs)
    float Status;         // 0=Stop, 1=Sky, 2=Antiverse,3=不透明体积
    vec4  AccumColor;     // 体积光颜色 (吸积盘+喷流)
    float CurrentSign;    // 最终宇宙符号
};

TraceResult TraceRay(vec2 FragUv, vec2 Resolution)
{
    TraceResult res;
    res.EscapeDir = vec3(0.0);
    res.FreqShift = 0.0;
    res.Status    = 0.0; // Default: Stop
    res.AccumColor = vec4(0.0);

    bool bDeferredShadowCulling = false;

    FragUv.y = 1.0 - FragUv.y; 
    float Fov = tan(iFovRadians / 2.0);
    vec2 Jitter = vec2(RandomStep(FragUv, fract(iTime * 1.0 + 0.5)), RandomStep(FragUv, fract(iTime * 1.0))) / Resolution;
    vec3 ViewDirLocal = FragUvToDir(FragUv + 0.25 * Jitter, Fov, Resolution); 

    // -------------------------------------------------------------------------
    // 物理常数与黑洞参数
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
    float PhysicalSpinA = iSpin * CONST_M;  
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
    if (iBlackHoleMassSol<0.0)
    {
        CurrentUniverseSign=-CurrentUniverseSign;
    }

    // -------------------------------------------------------------------------
    // 相机系统与坐标变换
    // -------------------------------------------------------------------------
    // World Space
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

    vec4 Result = vec4(0.0);
    bool bShouldContinueMarchRay = true;
    bool bWaitCalBack = false;
    float DistanceToBlackHole = length(RayPosLocal);
    
    if (DistanceToBlackHole > RaymarchingBoundary) //跳过与包围盒间空隙
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


    vec4 X = vec4(RayPosLocal, 0.0);
    
    //[修改5]初始化一个默认的 P_cov 防止未初始化报错，但不调用 GetInitialMomentum，因为 RayDir 还没被 Heat Haze 扭曲
    vec4 P_cov = vec4(0.0);

    float E_conserved = 1.0;
    vec3 RayDir = RayDirWorld_Geo;
    vec3 LastDir = RayDir;
    vec3 LastPos = RayPosLocal;
    float GravityFade = CubicInterpolate(max(min(1.0 - (length(RayPosLocal) - 100.0) / (RaymarchingBoundary - 100.0), 1.0), 0.0));

    
    #if ENABLE_HEAT_HAZE == 1
    {
        // 1. 坐标与参数准备 (Rg 空间)
        vec3 pos_Rg_Start = X.xyz; 
        vec3 rayDirNorm = normalize(RayDir);

        float totalProbeDist = float(HAZE_PROBE_STEPS) * HAZE_STEP_SIZE;
        
        // [适配] 使用 iTime，取模防止噪声溢出
        float hazeTime = mod(iBlackHoleTime, 1000.0); 

        // --- Debug: 体积光线步进可视化 ---
        #if HAZE_DEBUG_MASK == 1
        {
            float debugAccum = 0.0;
            float debugStep = 1.0; 
            vec3 debugPos = pos_Rg_Start;
            
            // 为了Debug重算一遍参数，与 GetHazeForce 保持一致
            float rotSpeedBase = 100.0 * HAZE_ROT_SPEED;
            float jetSpeedBase = 50.0 * HAZE_FLOW_SPEED;
            
            // 参数与 GetHazeForce 同步
            float ReferenceOmega = GetKeplerianAngularVelocity(6.0, 1.0, PhysicalSpinA, PhysicalQ);
            float AdaptiveFrequency = abs(ReferenceOmega * rotSpeedBase) / (2.0 * kPi * 5.14);
            AdaptiveFrequency = max(AdaptiveFrequency, 0.1);
            float flowTime = hazeTime * AdaptiveFrequency;

            float phase1 = fract(flowTime); float phase2 = fract(flowTime + 0.5);
            float weight1 = 1.0 - abs(2.0 * phase1 - 1.0); float weight2 = 1.0 - abs(2.0 * phase2 - 1.0);
            float t_offset1 = phase1 - 0.5; float t_offset2 = phase2 - 0.5;
            
            // 引入垂直漂移
            float VerticalDrift1 = t_offset1 * 1.0; 
            float VerticalDrift2 = t_offset2 * 1.0;

            bool doLayer1 = weight1 > 0.05;
            bool doLayer2 = weight2 > 0.05;
            
            float wTotal = (doLayer1 ? weight1 : 0.0) + (doLayer2 ? weight2 : 0.0);
            float w1_norm = (doLayer1 && wTotal > 0.0) ? (weight1 / wTotal) : 0.0;
            float w2_norm = (doLayer2 && wTotal > 0.0) ? (weight2 / wTotal) : 0.0;

            for(int k=0; k<100; k++)
            {
                float valCombined = 0.0;

                // A. 盘部分 Debug
                float maskDisk = GetDiskHazeMask(debugPos, iInterRadiusRs, iOuterRadiusRs, iThinRs, iHopper);
                if (maskDisk > 0.001) {
                    float r_local = length(debugPos.xz);
                    float omega = GetKeplerianAngularVelocity(r_local, 1.0, PhysicalSpinA, PhysicalQ);
                    
                    float vDisk = 0.0;
                    if (doLayer1) {
                        float angle1 = omega * rotSpeedBase * t_offset1;
                        float c1 = cos(angle1); float s1 = sin(angle1);
                        vec3 pos1 = debugPos;
                        pos1.x = debugPos.x * c1 - debugPos.z * s1;
                        pos1.z = debugPos.x * s1 + debugPos.z * c1;
                        pos1.y += VerticalDrift1; 
                        vDisk += GetBaseNoise(pos1) * w1_norm;
                    }
                    if (doLayer2) {
                        float angle2 = omega * rotSpeedBase * t_offset2;
                        float c2 = cos(angle2); float s2 = sin(angle2);
                        vec3 pos2 = debugPos;
                        pos2.x = debugPos.x * c2 - debugPos.z * s2;
                        pos2.z = debugPos.x * s2 + debugPos.z * c2;
                        pos2.y += VerticalDrift2;
                        vDisk += GetBaseNoise(pos2) * w2_norm;
                    }
                    valCombined += maskDisk * max(0.0, vDisk - HAZE_DENSITY_THRESHOLD);
                }

                // B. 喷流部分 Debug
                float maskJet = GetJetHazeMask(debugPos, iInterRadiusRs, iOuterRadiusRs);
                if (maskJet > 0.001) {
                    float v_jet_mag = 0.9;
                    float vJet = 0.0;
                    
                    if (doLayer1) {
                        float dist1 = v_jet_mag * jetSpeedBase * t_offset1;
                        vec3 pos1 = debugPos; pos1.y -= sign(debugPos.y) * dist1;
                        vJet += GetBaseNoise(pos1) * w1_norm;
                    }
                    if (doLayer2) {
                        float dist2 = v_jet_mag * jetSpeedBase * t_offset2;
                        vec3 pos2 = debugPos; pos2.y -= sign(debugPos.y) * dist2;
                        vJet += GetBaseNoise(pos2) * w2_norm;
                    }
                    valCombined += maskJet * max(0.0, vJet - HAZE_DENSITY_THRESHOLD);
                }
                
                debugAccum += valCombined * 0.1; 
                debugPos += rayDirNorm * debugStep;
            }
            
            res.Status = 3.0; // Opaque
            res.AccumColor = vec4(vec3(min(1.0, debugAccum)), 1.0);
            return res;
        }
        #endif

        // 2. 几何剔除优化
        if (IsInHazeBoundingVolume(pos_Rg_Start, totalProbeDist, iOuterRadiusRs)) 
        {
            vec3 accumulatedForce = vec3(0.0);
            float totalWeight = 0.0;

            // 3. 循环累积探测
            for (int i = 0; i < HAZE_PROBE_STEPS; i++)
            {
                float marchDist = float(i + 1) * HAZE_STEP_SIZE; 
                vec3 probePos_Rg = pos_Rg_Start + rayDirNorm * marchDist;

                float t = float(i+1) / float(HAZE_PROBE_STEPS);
                float weight = min(min(3.0*t, 1.0), 3.05 - 3.0*t);
                
                vec3 forceSample = GetHazeForce(probePos_Rg, hazeTime, PhysicalSpinA, PhysicalQ,
                                              iInterRadiusRs, iOuterRadiusRs, iThinRs, iHopper,
                                              iAccretionRate);
                
                accumulatedForce += forceSample * weight;
                totalWeight += weight;
            }

            vec3 avgHazeForce = accumulatedForce / max(0.001, totalWeight);

            // --- Debug: 向量可视化 ---
            #if HAZE_DEBUG_VECTOR == 1
                if (length(avgHazeForce) > 1e-4) {
                    res.Status = 3.0;
                    vec3 debugVec = normalize(avgHazeForce) * 0.5 + 0.5;
                    debugVec *= (0.5 + 10.0 * length(avgHazeForce)); 
                    res.AccumColor = vec4(debugVec, 1.0);
                    return res;
                }
            #endif

            // [修改7]替换掉下面这节
            // 4. 物理偏转应用
            float forceMagSq = dot(avgHazeForce, avgHazeForce);
            if (forceMagSq > 1e-10)
            {
                // 投影到垂直平面，确保只改变方向不改变速度大小
                vec3 forcePerp = avgHazeForce - dot(avgHazeForce, rayDirNorm) * rayDirNorm;
                
                // 计算总偏转量。可能需要微调系数以保持视觉强度正常
                vec3 deflection = forcePerp * HAZE_STRENGTH * 25.0; 

                // 系数 0.1 是为了防止极端扭曲导致数值爆炸，可以根据画面效果微调这个 0.1
                RayDir = normalize(RayDir + deflection * 0.1); 
                
                // 同步更新 LastDir
                LastDir = RayDir;
            }
        }
    }
    #endif
    
    if (bShouldContinueMarchRay) {
       P_cov = GetInitialMomentum(RayDir, X, iObserverMode, iUniverseSign, PhysicalSpinA, PhysicalQ, GravityFade);
    }
    E_conserved = -P_cov.w;

    // -------------------------------------------------------------------------
    // 初始合法性检查与终结半径
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
        // 落体观者能层合法性检查 todo
        }
        // 确定光线追踪终止半径 (非裸奇点)
        if (!bIsNakedSingularity && CurrentUniverseSign > 0.0) 
        {
            if (CameraStartR > EventHorizonR) TerminationR = EventHorizonR; 
            else if (CameraStartR > InnerHorizonR) TerminationR = InnerHorizonR;
            else TerminationR = -1.0;
        }
    }
    
    //计算光子壳最窄处作为回落剔除判断
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


#define ENABLE_SHADOW_CULLING     1       // 1:开启剔除优化, 0:关闭 (To compare performance)
#define DEBUG_SHADOW_CULLING      1       // 1:显示绿色调试层, 0:正常黑色 (To visualize culling)
#define SHADOW_SIZE_MULTIPLIER    0.995     // 阴影半径微调系数 (Multiplier for shadow radius)


    // -------------------------------------------------------------------------
    // 阴影剔除逻辑
    // -------------------------------------------------------------------------
    #if ENABLE_SHADOW_CULLING == 1
    // 条件：非裸奇点、在universe侧、距离足够远(>RN视界或KN逆行光子轨道)、当前还需要继续步进
    float AbsSpinA = abs(CONST_M * iSpin);
    bool bIsRot = AbsSpinA > 1e-5;
    
    // 初步判定是否需要尝试剔除：
    // 1. 非裸奇点 (视界存在)
    // 2. 在正宇宙 (CurrentUniverseSign > 0)
    // 3. 当前光线还需要继续步进 (bShouldContinueMarchRay)
    if (!bIsNakedSingularity && CurrentUniverseSign > 0.0 && bShouldContinueMarchRay && iGrid==0)
    {
        // 预计算剔除启动的阈值半径
        float CullingStartRadius;
        
        if (!bIsRot) {
            // 纯RN/史瓦西黑洞：允许进入光子球内部，直到非常接近视界
            CullingStartRadius = 1.005 * EventHorizonR;
        } else {
            // 旋转黑洞：计算逆行光子轨道半径 r_B (凸出侧)
            // 使用 SolveQuarticU 计算 r_B (对应参数 +1.0)
            float u_B_calc = SolveQuarticU(CONST_M, PhysicalQ, AbsSpinA, 1.0, true);
            float r_B_calc = (u_B_calc * u_B_calc + PhysicalQ * PhysicalQ) / CONST_M;

            CullingStartRadius = r_B_calc + 0.05;
        }
        // 只有当相机(或当前光线起点)在安全半径外，才进行剔除
        if (CameraStartR > CullingStartRadius)
        {
 // 计算视线与黑洞中心的夹角
            vec3 ToCenterDir = -normalize(RayPosLocal); // 局部系下黑洞在原点
            float CosAlpha = dot(normalize(RayDir), ToCenterDir);
            float RayAngle = acos(clamp(CosAlpha, -1.0, 1.0)); // 当前像素视线与黑洞中心的夹角

            // 估算阴影的大致可能张角，仅在这个区域内进行进一步计算
            float SafetyFactor = 2.5 + 1.1 * abs(iSpin) - iQ;
            float MaxShadowAngleEstimate = SafetyFactor * (2.0 * CONST_M) / max(1e-6, CameraStartR);
            if (RayAngle < MaxShadowAngleEstimate || CameraStartR < 3.0*EventHorizonR) // 只有大致朝向黑洞或在光子球里才计算
            {
                float RayAngle = acos(CosAlpha); // 当前像素视线与黑洞中心的夹角
                bool bHitShadow = false; // 是否命中阴影
                
                if (!bIsRot)
                {
                    // === 情况 1: 纯RN或纯史瓦西黑洞 (a=0) ===
                    float ShadowHalfAngle = GetShadowHalfAngleRN(CameraStartR, CONST_M, PhysicalQ, iObserverMode);
                    ShadowHalfAngle *= SHADOW_SIZE_MULTIPLIER;
                    
                    if (RayAngle < ShadowHalfAngle) bHitShadow = true;
                }
                else
                {
                    // === 情况 2: 有自旋黑洞 (a!=0) ===
                    float M = CONST_M;
                    float Q = PhysicalQ;
                    float a = PhysicalSpinA; // 保留原始符号用于手性判断
                    float a_abs = AbsSpinA;  // 几何计算绝对值
                    float Q2 = Q*Q;
                    float a2 = a_abs*a_abs;
                    float r = CameraStartR;
                    
                    // --- 1. 极轴视角 ---
                    float P = a2 + 2.0*Q2 - 3.0*M*M;
                    float K = 2.0*Q2*M + 2.0*M*a2 - 2.0*M*M*M;
                    float x_pole = SolveCubicMaxReal(P, K);
                    float r_p = M + x_pole;
                    float b_pole = sqrt(max(0.0, (2.0*r_p*(r_p*r_p + a2))/(r_p - M))); // 碰撞参数
                    
                    float Delta_r = r*r - 2.0*M*r + a2 + Q2;
                    
                    float SinOF_Stat = b_pole * sqrt(max(0.0, Delta_r)) / (r*r + a2);
                    // 极轴视角在剔除区(r > r_B > r_ps) 总是锐角
                    float CosOF_Stat = sqrt(max(0.0, 1.0 - SinOF_Stat * SinOF_Stat));
                    
                    float AngleOF = GetDropFrameAngle(SinOF_Stat, CosOF_Stat, r, M, Q, a_abs, iObserverMode);
                    float LatFactor = abs(X.y) / length(X.xyz);

                    if (LatFactor > 0.9999)
                    {
                        float effectiveMult = SHADOW_SIZE_MULTIPLIER ;
                        if (RayAngle < AngleOF * effectiveMult) bHitShadow = true;
                    }
                    else
                    {
                        
                        // --- 2. 赤道视角 (Equator View) ---
                        // A点 (缺口/顺行): 对应方程减号项(-2a...), 且取较小根
                        float u_A = SolveQuarticU(M, Q, a_abs, -1.0, true); 
                        float r_A_rad = (u_A * u_A + Q2) / M;
                        
                        float u_B = SolveQuarticU(M, Q, a_abs, 1.0, true); 
                        float r_B_rad = (u_B * u_B + Q2) / M;
                        
                        float safe_a = max(1e-5, a_abs);
                        // Xi_A (缺口侧)
                        // Formula: xi = (r^2(3M-r) - a^2(M+r) - 2Q^2r) / (a(r-M))
                        float num_A = r_A_rad * r_A_rad * (3.0 * M - r_A_rad) - a2 * (M + r_A_rad) - 2.0 * Q2 * r_A_rad;
                        float xi_A = num_A / max(1e-9, safe_a * (r_A_rad - M));
                        // Xi_B (凸起侧)
                        float num_B = r_B_rad * r_B_rad * (3.0 * M - r_B_rad) - a2 * (M + r_B_rad) - 2.0 * Q2 * r_B_rad;
                        float xi_B = num_B / max(1e-9, safe_a * (r_B_rad - M));
                        
                        float Mr_Q2_Shadow = 2.0 * M * r - Q2;
                        float Sigma_Shadow = r * r; 
                        // 计算 BL 度规分量 (用于静态投影公式)
                        float g_tt_stat = -(1.0 - Mr_Q2_Shadow / Sigma_Shadow);
                        float gtphi_stat = -a_abs * Mr_Q2_Shadow / Sigma_Shadow; 
                        float D_cyl = gtphi_stat * gtphi_stat - g_tt_stat * (Sigma_Shadow + a2 + Mr_Q2_Shadow * a2 / Sigma_Shadow);
                        float InvSqrtD = 1.0 / sqrt(max(1e-9, D_cyl));
                        // 计算坐标系扭曲近似修正，Kerr-Schild 坐标系的径向与 Boyer-Lindquist 不同，存在 phi 方向的偏移。近似修正 a * r / Delta
                        float TwistCorrection = safe_a * r / max(1e-5, Delta_r);
                        
                        float SinOA_Stat = abs((xi_A + TwistCorrection) * g_tt_stat + gtphi_stat) * InvSqrtD;
                        float SinOB_Stat = abs((xi_B + TwistCorrection) * g_tt_stat + gtphi_stat) * InvSqrtD;
                        
                        // 计算 Cos (锐角)
                        float CosOA_Stat = sqrt(max(0.0, 1.0 - SinOA_Stat * SinOA_Stat));
                        float CosOB_Stat = sqrt(max(0.0, 1.0 - SinOB_Stat * SinOB_Stat));
                        
                        // 中心偏移 E
                        // 经测试，若使用近似公式 a*(rE+M)/(rE-M) ，则结果偏大 (偏向B点)，且a*越大、相机r越小，偏差越明显。此外，使用2.0*a将偏B，使用1.0*a将偏A。故取中间值 a(近时)到a*5/3(远时) 作为基准。
                        // 同时也叠加 TwistCorrection (因为坐标系扭曲是全局的)。
                        float xi_E_Corrected = (1.6666-2.0/r) * safe_a + TwistCorrection;
                        float SinOE_Stat = abs(xi_E_Corrected * g_tt_stat + gtphi_stat) * InvSqrtD;
                        float CosOE_Stat = sqrt(max(0.0, 1.0 - SinOE_Stat * SinOE_Stat));
                        
                        // 转换为落体视角角度
                        float AngleOA0 = GetDropFrameAngle(SinOA_Stat, CosOA_Stat, r, M, Q, a_abs, iObserverMode);
                        float AngleOB0 = GetDropFrameAngle(SinOB_Stat, CosOB_Stat, r, M, Q, a_abs, iObserverMode);
                        float AngleOE0 = GetDropFrameAngle(SinOE_Stat, CosOE_Stat, r, M, Q, a_abs, iObserverMode);
                        // 垂直半轴 EC 在数学上可证明和相同Q的RN黑洞完全一致
                        float AngleEC0 = GetShadowHalfAngleRN(r, M, Q, iObserverMode);
                        
                        // --- 3. 混合 ---
                        // 调试：如何选择混合函数。需要是一些凹函数。
                        float MixWA = clamp(tan(LatFactor*1.48)/10.98338,0.0,1.0);//指数函数在这里会前期太小、后期太大，所以用tan
                        float MixWB = pow(LatFactor, 2.5);//基本确定2.5
                        float MixWE = pow(LatFactor, 6.0);//基本确定6.0
                        float MixWCD = pow(LatFactor, 0.75);//基本确定0.75
                        
                        float AngleOA = mix(AngleOA0, AngleOF, MixWA);
                        float AngleOB = mix(AngleOB0, AngleOF, MixWB);
                        float AngleEC = mix(AngleEC0, AngleOF, MixWCD);
                        float AngleOE = mix(AngleOE0, 0.0,     MixWE);
                        
                        // --- 4. 视平面判定 (Screen Space Check) ---
                        // 局部系，Y是自旋轴。ToCenterDir是视线反向
                        vec3 SpinAxis = vec3(0.0, 1.0, 0.0);
                        vec3 ScreenUp = normalize(SpinAxis - dot(SpinAxis, ToCenterDir) * ToCenterDir);
                        vec3 ScreenRight = cross(ToCenterDir, ScreenUp);
                        vec3 VecToPixel = normalize(RayDir - dot(RayDir, ToCenterDir) * ToCenterDir);
                        float ProjU = dot(VecToPixel, ScreenRight);
                        float ProjV = dot(VecToPixel, ScreenUp);
                        float x_ang = ProjU * RayAngle;
                        float y_ang = ProjV * RayAngle;
                        
                        // 手性：a>0 时，凸起(B)在U轴正向(Right)，缺口(A)在U轴负向(Left)，E 点向 B 侧偏移
                        float SignChirality = sign(a); 
                        if (abs(a) < 1e-9) SignChirality = 1.0;
                        // E 的位置 (在 U 轴上的坐标)
                        float CenterEx = SignChirality * AngleOE;
                        float dx = x_ang - CenterEx;
                        float dy = y_ang;
                        
                        float RadiusA_from_E = AngleOA + AngleOE;
                        float RadiusB_from_E = max(1e-5, AngleOB - AngleOE);
                        
                        float CurrentHRadius;
                        float CurrentVRadius = AngleEC;
                        
                        // DEBUG：绘制关键点 A, B, C, D, E, O
                        #if DEBUG_SHADOW_CULLING == 1
                        vec2 currP = vec2(x_ang, y_ang);
                        float dotSize = 0.002; // 调试点大小（弧度）
                        
                        // O是白色
                        vec2 ptO = vec2(0.0, 0.0);
                        if (length(currP - ptO) < dotSize) {
                            res.AccumColor = vec4(1.0, 1.0, 1.0, 1.0);
                            res.Status = 3.0; return res;
                        }
                        
                        // C、D、E是蓝色
                        vec2 ptE = vec2(CenterEx, 0.0);
                        vec2 ptC = vec2(CenterEx,  AngleEC);
                        vec2 ptD = vec2(CenterEx, -AngleEC);
                        if (length(currP - ptE) < dotSize || length(currP - ptC) < dotSize || length(currP - ptD) < dotSize) {
                            res.AccumColor = vec4(0.0, 0.5, 1.0, 1.0);
                            res.Status = 3.0; return res;
                        }
                        
                        // A、B是红色
                        vec2 ptA = vec2(CenterEx - SignChirality * RadiusA_from_E, 0.0);
                        vec2 ptB = vec2(CenterEx + SignChirality * RadiusB_from_E, 0.0);
                        if (length(currP - ptA) < dotSize || length(currP - ptB) < dotSize) {
                            res.AccumColor = vec4(1.0, 0.0, 0.0, 1.0);
                            res.Status = 3.0; return res;
                        }
                        #endif
                        
                        // 判断是在 E 的 "A侧" 还是 "B侧"
                        if (dx * SignChirality > 0.0) {
                            CurrentHRadius = RadiusB_from_E; // B侧 (凸起)
                        } else {
                            CurrentHRadius = RadiusA_from_E; // A侧 (缺口)
                            // A侧修正系数
                            float a_star = a_abs / CONST_M; 
                            float f4 = clamp(1.0-((r-30.0)/(80.0-30.0)),0.0,1.0); // 相机距离较远时，避免拉伸
                            float f3 = clamp((a_star - 0.9) / 0.1, 0.0, 1.0); // a*不高时，D形不明显，边缘还是接近椭圆的。所以，修正仅在 a* > 0.9 时生效，1.0时达到最大
                            float f2 = pow(1.0 - LatFactor, 1.0); // 随相机纬度变化。在到达极轴时，应完全没有修正，变回圆形
                            float u = clamp(abs(dx) / RadiusA_from_E, 0.0, 1.0); // u=1表示在A点(边缘)，u=0表示在E点。
                            float f1 = 0.36 * pow(u, 3.5); // 使用 pow 确保靠近中心时修正迅速消失
                            // 缩放使得原本在椭圆外的点被包含进阴影，形成比半椭圆更丰满的"D"形
                            CurrentVRadius *= (1.0 + f1 * f2 * f3 * f4);
                            float f5 = (1.0-2.0*LatFactor)*(1.0-pow(abs(iQ),0.1));
                            CurrentHRadius *= 1.0+25.0*f4*f5*clamp(a_star - 0.98,0.0,0.02)*clamp(a_star - 0.98,0.0,0.02);
                        }
                        
                        float dist_sq = (dx*dx) / (CurrentHRadius*CurrentHRadius) + (dy*dy) / (CurrentVRadius*CurrentVRadius);
                        if (dist_sq < SHADOW_SIZE_MULTIPLIER * SHADOW_SIZE_MULTIPLIER) bHitShadow = true;
                    }
                }
                
                // --- 执行剔除 ---

                if (bHitShadow)
                {
                    bool bHasDisk = IsAccretionDiskVisible(iInterRadiusRs, iOuterRadiusRs, iThinRs, iHopper, iBrightmut, iDarkmut);
                    bool bHasJet  = IsJetVisible(iAccretionRate, iJetBrightmut);
                    
                    if (!bHasDisk && !bHasJet)
                    {
                        // 纯黑洞，无盘无喷流：立即返回黑色
                        #if DEBUG_SHADOW_CULLING == 1
                            res.AccumColor = vec4(0.0, 0.5, 0.0, 1.0); 
                            res.Status = 3.0; 
                        #else
                            res.AccumColor = vec4(0.0, 0.0, 0.0, 1.0); 
                            res.Status = 3.0;
                        #endif
                        res.CurrentSign = CurrentUniverseSign;
                        res.EscapeDir = vec3(0.0);
                        res.FreqShift = 0.0;
                        return res; 
                    }
                    else
                    {
                        // 有盘或喷流，改终结半径，延迟剔除
                        float SafeCullRadius = max(iInterRadiusRs, 1.05 * EventHorizonR);
                        if (SafeCullRadius > TerminationR)
                        {
                            TerminationR = SafeCullRadius;
                            bDeferredShadowCulling = true; // 标记：这是因为剔除而提升的终结半径
                        }
                    }
                }
            }
        }
    }
    #endif



    float MaxStep=150.0+300.0/(1.0+1000.0*(1.0-iSpin*iSpin-iQ*iQ)*(1.0-iSpin*iSpin-iQ*iQ));
    if(bIsNakedSingularity) MaxStep=450;//150.0+300.0/(1.0+10.0*(1.0-iSpin*iSpin-iQ*iQ)*(1.0-iSpin*iSpin-iQ*iQ));
    // -------------------------------------------------------------------------
    // 主循环
    // -------------------------------------------------------------------------
    int Count = 0;
    float lastR = 0.0;
    bool bIntoOutHorizon = false;
    bool bIntoInHorizon = false;
    float LastDr = 0.0;           
    int RadialTurningCounts = 0;  
    float RayMarchPhase = RandomStep(FragUv, iTime); 
    vec3 RayPos = X.xyz; 
    float ThetaInShell=0;
    while (bShouldContinueMarchRay)
    {
        DistanceToBlackHole = length(RayPos);
        if (DistanceToBlackHole > RaymarchingBoundary)
        { 
            bShouldContinueMarchRay = false; 
            bWaitCalBack = true; 
            break; //离开足够远
        }
        
        KerrGeometry geo;
        ComputeGeometryScalars(X.xyz, PhysicalSpinA, PhysicalQ, GravityFade, CurrentUniverseSign, geo);

        if (CurrentUniverseSign > 0.0 && geo.r < TerminationR && !bIsNakedSingularity && TerminationR != -1.0) 
        { 
            bShouldContinueMarchRay = false;
            bWaitCalBack = false;
            //Result = vec4(0.0, 0.3, 0.3, 0.0); 
            break; //视界判定情况1，直接进入视界判定区
        }
        if (Count > MaxStep) 
        { 
            bShouldContinueMarchRay = false; 
            bWaitCalBack = false;
            if(bIsNakedSingularity&&RadialTurningCounts <= 2) bWaitCalBack = true;
            //Result = vec4(0.0, 0.3, 0.0, 0.0);
            break; //耗尽步数
        }

        State s0; s0.X = X; s0.P = P_cov;
        State k1 = GetDerivativesAnalytic(s0, PhysicalSpinA, PhysicalQ, GravityFade, geo);

        float CurrentDr = dot(geo.grad_r, k1.X.xyz);
        if (Count > 0 && CurrentDr * LastDr < 0.0) RadialTurningCounts++;
        LastDr = CurrentDr;

        if(iGrid==0)
        {
            
            if (RadialTurningCounts > 2) 
            {
                bShouldContinueMarchRay = false; bWaitCalBack = false;
                //Result = vec4(0.3, 0.0, 0.3, 1.0); 
                break;//识别剔除奇环附近束缚态光子轨道
            }
            
            
            if(geo.r > InnerHorizonR && lastR < InnerHorizonR) bIntoInHorizon = true;     //检测穿过内视界 
            if(geo.r > EventHorizonR && lastR < EventHorizonR) bIntoOutHorizon = true;    //检测穿过外视界 
            
            if (CurrentUniverseSign > 0.0 && !bIsNakedSingularity)
            {
            
            
                float SafetyGap = 0.001;
                float PhotonShellLimit = ProgradePhotonRadius - SafetyGap; 
                float preCeiling = min(CameraStartR - SafetyGap, TerminationR + 0.2);
                if(bIntoInHorizon) { preCeiling = InnerHorizonR + 0.2; } //处理 射线从相机出发 -> 向外运动 -> 调头 -> 向内运动 -> 撞击内视界 的光
                if(bIntoOutHorizon) { preCeiling = EventHorizonR + 0.2; }//处理 射线从相机出发 -> 向外运动 -> 调头 -> 向内运动 -> 撞击外视界 的光
                
                float PruningCeiling = min(iInterRadiusRs, preCeiling);
                PruningCeiling = min(PruningCeiling, PhotonShellLimit); 
            
                if (geo.r < PruningCeiling)
                {
                    float DrDlambda = dot(geo.grad_r, k1.X.xyz);
                    if (DrDlambda > 1e-4) 
                    {
                        bShouldContinueMarchRay = false;
                        bWaitCalBack = false;
                        //Result = vec4(0.0, 0., 0.3, 0.0);
                        break; //视界判定情况2，对凝结在视界前的光提前剔除
                    }
                }
            }
        }
        
        //对动量和位置及其导数做自适应步长。对电荷做自适应步长（Q贡献r^-2项
        float rho = length(RayPos.xz);
        float DistRing = sqrt(RayPos.y * RayPos.y + pow(rho - abs(PhysicalSpinA), 2.0));
        float Vel_Mag = length(k1.X); 
        float Force_Mag = length(k1.P);
        float Mom_Mag = length(P_cov);
        
        float PotentialTerm = (PhysicalQ * PhysicalQ) / (geo.r2 + 0.01);
        float QDamping = 1.0 / (1.0 + 1.0 * PotentialTerm); 
        
      
        float ErrorTolerance = 0.5 * QDamping;
        float StepGeo =  DistRing / (Vel_Mag + 1e-9);
        float StepForce = Mom_Mag / (Force_Mag + 1e-15);
        
        float dLambda = ErrorTolerance*min(StepGeo, StepForce);
        dLambda = max(dLambda, 1e-7); 

        vec4 LastX = X;
        LastPos = X.xyz;
        GravityFade = CubicInterpolate(max(min(1.0 - ( DistanceToBlackHole - 100.0) / (RaymarchingBoundary - 100.0), 1.0), 0.0));
        
        vec4 P_contra_step = RaiseIndex(P_cov, geo);
        if (P_contra_step.w > 10000.0 && !bIsNakedSingularity && CurrentUniverseSign > 0.0) 
        { 
            bShouldContinueMarchRay = false; 
            bWaitCalBack = false;
            //Result = vec4(0.3, 0.3, 0.2, 0.0);  
            break; //视界判定情况3，凝结在视界
        }

        //if (Count == 0)
        //{
        //    dLambda = RandomStep(FragUv, fract(iTime)); // 光起步步长抖动,但是会让高层光子环变糊，考虑到现在吸积盘的层纹去除逻辑已经挪进体积云噪声内部，建议关着。
        //}
        StepGeodesicRK4_Optimized(X, P_cov, E_conserved, -dLambda, PhysicalSpinA, PhysicalQ, GravityFade, CurrentUniverseSign, geo, k1);
        float deltar=geo.r-lastR;
        

        RayPos = X.xyz;
        vec3 StepVec = RayPos - LastPos;
        float ActualStepLength = length(StepVec);
        float drdl=deltar/max(ActualStepLength,1e-9);

        float rotfact=clamp(1.0   +   iBoostRot* dot(-StepVec,vec3(X.z,0,-X.x)) /ActualStepLength/length(X.xz)  *clamp(iSpin,-1.0,1.0)   ,0.0,2.0)   ;
        if( geo.r<1.6+pow(abs(iSpin),0.666666)){
        ThetaInShell+=ActualStepLength/(0.5*lastR + 0.5*geo.r)/(1.0+1000.0*drdl*drdl)*rotfact*clamp(11.0-10.0*(iSpin*iSpin+iQ*iQ),0.0,1.0);
        }
        lastR = geo.r;
        RayDir = (ActualStepLength > 1e-7) ? StepVec / ActualStepLength : LastDir;
        
        //穿过奇环面
        if (LastPos.y * RayPos.y < 0.0) {
            float t_cross = LastPos.y / (LastPos.y - RayPos.y);
            float rho_cross = length(mix(LastPos.xz, RayPos.xz, t_cross));
            if (rho_cross < abs(PhysicalSpinA)) CurrentUniverseSign *= -1.0;
        }

        //吸积盘和喷流
        if (CurrentUniverseSign > 0.0&& iBlackHoleMassSol>0.0) 
        {
           if(IsAccretionDiskVisible(iInterRadiusRs, iOuterRadiusRs, iThinRs, iHopper, iBrightmut, iDarkmut))
           {
           Result = DiskColor(Result, ActualStepLength, X, LastX, RayDir, LastDir, P_cov, E_conserved,
                             iInterRadiusRs, iOuterRadiusRs, iThinRs, iHopper, iBrightmut, iDarkmut, iReddening, iSaturation, DiskArgument, 
                             iBlackbodyIntensityExponent, iRedShiftColorExponent, iRedShiftIntensityExponent, PeakTemperature, ShiftMax, 
                             clamp(PhysicalSpinA, -0.49, 0.49), 
                             PhysicalQ,
                             ThetaInShell,
                             RayMarchPhase 
                             ); 
           }
           if(IsJetVisible(iAccretionRate, iJetBrightmut)){
           Result = JetColor(Result, ActualStepLength, X, LastX, RayDir, LastDir, P_cov, E_conserved,
                             iInterRadiusRs, iOuterRadiusRs, iJetRedShiftIntensityExponent, iJetBrightmut, iReddening, iJetSaturation, iAccretionRate, iJetShiftMax, 
                             clamp(PhysicalSpinA, -0.049, 0.049), 
                             PhysicalQ                            
                             ); 
           }
        }
        if(iGrid==1)
        {
            Result = GridColor(Result, X, LastX, 
                        P_cov, E_conserved,
                        PhysicalSpinA, 
                        PhysicalQ, 
                        CurrentUniverseSign);
        }
        else if(iGrid==2)
        {
            Result = GridColorSimple(Result, X, LastX, 
                        PhysicalSpinA, 
                        PhysicalQ, 
                        CurrentUniverseSign);
        }
        if (Result.a > 0.99) { bShouldContinueMarchRay = false; bWaitCalBack = false; break; }
        
        LastDir = RayDir;
        Count++;
    }

    //结果打包
    res.CurrentSign = CurrentUniverseSign;
    res.AccumColor  = Result;

        // [修改10] 阴影剔除的 Debug 颜色
    // 如果被剔除，且 Result 的透明度没满（说明没被盘完全挡住），则补上剔除色
    #if ENABLE_SHADOW_CULLING == 1
    if (bDeferredShadowCulling && !bIsNakedSingularity)
    {
        // 检查是否是因为撞到了我们设定的 TerminationR 而退出的
        // 使用 length(RayPos) 而不是 geo.r，确保变量可见性
        float FinalR = length(RayPos);
        
        // 判定条件：位置在截断半径内，或者已经不再继续步进（说明撞击了物体）
        // 宽松一点的容差 +0.1，防止边界闪烁
        if (FinalR <= TerminationR + 0.1 || !bShouldContinueMarchRay)
        {
            #if DEBUG_SHADOW_CULLING == 1
                // 混合前 clamp Alpha，防止负数
                // 逻辑：(已有颜色) + (绿色 * 剩余透明度)
                float RemainingAlpha = max(0.0, 1.0 - res.AccumColor.a);
                res.AccumColor.rgb += vec3(0.0, 0.5, 0.0) * RemainingAlpha;
                res.AccumColor.a = 1.0; // 强制不透明
                
                res.Status = 3.0; // 标记为实体
            #else
                // 正常模式：补齐黑色
                res.AccumColor.a = 1.0;
                res.Status = 3.0;
            #endif
            
            // 如果触发了剔除，直接返回，不再执行下面的 Status 1.0/2.0 判断
            return res;
        }
    }
    #endif


    // 状态位定义:
    // 0.0 = Absorbed/Lost (视界/超时，且未被体积光完全遮挡)
    // 1.0 = Sky (Universe +1)
    // 2.0 = Antiverse (Universe -1)
    // 3.0 = Opaque (Result.a > 0.99，体积光完全遮挡，其边界与前三者交界不做检查)

    if (Result.a > 0.99) {
        // 状态 3：不透明体积
        res.Status = 3.0; 
        res.EscapeDir = vec3(0.0); 
        res.FreqShift = 0.0;
    } 
    else if (bWaitCalBack) {
        // 状态 1 或 2：命中背景
        res.EscapeDir = LocalToWorldRot * normalize(RayDir);
        res.FreqShift = clamp(1.0 / max(1e-4, E_conserved), 1.0/2.0, 2.0); 
        
        if (CurrentUniverseSign  > 0.0) res.Status = 1.0; // Sky
        else res.Status = 2.0; // Antiverse
    } 
    else {
        // 状态 0：该方向无吸积盘和喷流以外任何光
        res.Status = 0.0; 
        res.EscapeDir = vec3(0.0);
        res.FreqShift = 0.0;
    }

    return res;
}

