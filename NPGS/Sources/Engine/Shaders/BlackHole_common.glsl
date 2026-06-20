
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

    vec4  iCameraVelocity;               //相机坐标速度

    int   iDEBUG;
    int   iPrepass;                      //使用低分辨率插值
    int   iWhitehole;                    //最大延拓  
    int   iInWhichUniverse;              //当前最大延拓宇宙编号
    int   iGrid;                         //绘制网格
    int   iEnableHeatHaze;               //热折射
    int   iEnableShadowCulling;          //剔除
    int   iObserverMode;                 //观者模式，0静态，1落体，2使用外传相机三维平直坐标速度，自动解析为ingoing分量或outgoing分量
    int   iPolarization;                 //输出偏振

    float iQuality;                      

    float iUniverseSign;                 //相机所在空间侧。 +1.0正宇宙  -1.0反宇宙
                        
    float iBlackHoleTime;                //时间。单位 c*s/Rs
    float iBlackHoleMassSol;             //质量，单位倍太阳质量
    float iSpin;                         //无量纲自旋a*   
    float iQ;                            //无量纲电荷Q* 

    float iMu;                           //吸积物质比荷
    float iAccretionRate;                //吸积率 单位倍爱丁顿吸积率

    float iBackShiftMax;			     //背景最大频移

    float iDensestarsurfaceR;            //致密星表面半径。非0值为启用。单位倍Rs
    float iDensestarBlackbodyIntensityExponent;   //致密星表面温度——黑体颜色指数
    float iDensestarRedShiftColorExponent;        //致密星表面频移——温度指数
    float iDensestarRedShiftIntensityExponent;    //致密星表面频移——亮度指数
    float iDensestarBrightmut;                    //致密星表面亮度乘数

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

    float iPolarizationAngle;            //偏振片模式偏振片角度

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
layout(set = 1, binding = 1) uniform samplerCube iBackground0;
layout(set = 1, binding = 2) uniform samplerCube iAntiground0;
layout(set = 1, binding = 3) uniform samplerCube iBackground1;
layout(set = 1, binding = 4) uniform samplerCube iAntiground1;
layout(set = 1, binding = 5) uniform samplerCube iBackground2;
layout(set = 1, binding = 6) uniform samplerCube iAntiground2;


layout(set = 1, binding = 9) uniform sampler2D iImageTexture;
// =============================================================================
// SECTION 2: 基础工具函数 (噪声、插值、随机)
// =============================================================================
float det3(vec3 a, vec3 b, vec3 c) {
    return dot(a, cross(b, c));
}
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

    
    float rStatus = round(Status);
    int offset = 0;
    if (rStatus > 3.0) {
        offset = int(round((rStatus - 1.0) / 3.0));
    }
    int useContground = -offset;
    
    //  Background 和 Antiground 逻辑
    bool isNegativeMass = (iBlackHoleMassSol < 0.0);
    bool isAntiverse    = (mod(rStatus, 3.0) == 2.0); // 完美囊括 2, 5, 8... 所有反宇宙

    if (isNegativeMass != isAntiverse) 
    {
        if      (int(iInWhichUniverse+3+useContground)%3==0) Backcolor = textureLod(iAntiground0, Dir, min(1.0, textureQueryLod(iAntiground0, Dir).x));
        else if (int(iInWhichUniverse+3+useContground)%3==1) Backcolor = textureLod(iAntiground1, Dir, min(1.0, textureQueryLod(iAntiground1, Dir).x));
        else if (int(iInWhichUniverse+3+useContground)%3==2) Backcolor = textureLod(iAntiground2, Dir, min(1.0, textureQueryLod(iAntiground2, Dir).x));
    } 
    else 
    {
        
        if      (int(iInWhichUniverse+3+useContground)%3==0) Backcolor = textureLod(iBackground0, Dir, min(1.0, textureQueryLod(iBackground0, Dir).x));
        else if (int(iInWhichUniverse+3+useContground)%3==1) Backcolor = textureLod(iBackground1, Dir, min(1.0, textureQueryLod(iBackground1, Dir).x));
        else if (int(iInWhichUniverse+3+useContground)%3==2) Backcolor = textureLod(iBackground2, Dir, min(1.0, textureQueryLod(iBackground2, Dir).x));
    }

    float BackgroundShift = Shift;
    vec3 Rcolor = Backcolor.r * 1.0 * WavelengthToRgb(max(453.0, 645.0 / BackgroundShift));
    vec3 Gcolor = Backcolor.g * 1.5 * WavelengthToRgb(max(416.0, 510.0 / BackgroundShift));
    vec3 Bcolor = Backcolor.b * 0.6 * WavelengthToRgb(max(380.0, 440.0 / BackgroundShift));
    vec3 Scolor = Rcolor + Gcolor + Bcolor;
    float OStrength = 0.3 * Backcolor.r + 0.6 * Backcolor.g + 0.1 * Backcolor.b;
    float RStrength = 0.3 * Scolor.r + 0.6 * Scolor.g + 0.1 * Scolor.b;
    Scolor *= OStrength / max(RStrength, 0.001);
    
    return iBackgroundBrightmut * vec4(Scolor, Backcolor.a) * pow(Shift, 4.0);
}


vec4 ApplyToneMapping(vec4 Result,float shift)
{
    float RedFactor   = 3.0 * Result.r / (Result.r + Result.b + Result.g );
    float BlueFactor  = 3.0 * Result.b / (Result.r + Result.b + Result.g );
    float GreenFactor = 3.0 * Result.g / (Result.r + Result.b + Result.g );
    float BloomMax    = max(8.0,shift)+log(max(shift-8.0+1.0,1.0));
    vec4 Mapped;
    Mapped.r = min(-4.0 * log( 1.0 - pow(Result.r, 2.2)), BloomMax * RedFactor);
    Mapped.g = min(-4.0 * log( 1.0 - pow(Result.g, 2.2)), BloomMax * GreenFactor);
    Mapped.b = min(-4.0 * log( 1.0 - pow(Result.b, 2.2)), BloomMax * BlueFactor);
    Mapped.a = min(-4.0 * log( 1.0 - pow(Result.a, 2.2)), 4.0);
    return Mapped;
    //return Result;
}
// =============================================================================
// SECTION 4: 广相计算。Y为自旋方向，ins/outgoing方向笛卡尔形式kerrscild系。+++-。
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

void ComputeGeometryScalars(vec3 X, float PhysicalSpinA, float PhysicalQ, float fade, float r_sign, bool isOutgoing, out KerrGeometry geo) {
    geo.a2 = PhysicalSpinA * PhysicalSpinA;
    
    // 决定流入还是流出
    float dirSign = isOutgoing ? -1.0 : 1.0;
    
    if (PhysicalSpinA == 0.0) {
        geo.r = r_sign * length(X);
        geo.r2 = geo.r * geo.r;
        float inv_r = 1.0 / geo.r;
        float inv_r2 = inv_r * inv_r;
        
        // 乘上 dirSign
        geo.l_up = vec4(dirSign * X * inv_r, -1.0);
        geo.l_down = vec4(dirSign * X * inv_r, 1.0);
        
        geo.num_f = (2.0 * CONST_M * geo.r - PhysicalQ * PhysicalQ);
        geo.f = (2.0 * CONST_M * inv_r - (PhysicalQ * PhysicalQ) * inv_r2) * fade;
        
        geo.inv_r2_a2 = inv_r2; 
        geo.inv_den_f = inv_r2 * inv_r2; 
        return;
    }

    geo.r = KerrSchildRadius(X, PhysicalSpinA, r_sign);
    geo.r2 = geo.r * geo.r;
    float r3 = geo.r2 * geo.r;
    float z_coord = X.y; 
    float z2 = z_coord * z_coord;
    
    geo.inv_r2_a2 = 1.0 / (geo.r2 + geo.a2);
    
    // 修改处：向外/向内仅反转径向成分，保留自旋成分
    float lx = (dirSign * geo.r * X.x - PhysicalSpinA * X.z) * geo.inv_r2_a2;
    float ly = (dirSign * X.y) / geo.r;
    float lz = (dirSign * geo.r * X.z + PhysicalSpinA * X.x) * geo.inv_r2_a2;
    
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

    float inv_denom_grad = geo.r * geo.inv_den_f;
    
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


//初始化ingoing系下光子动量P_u，以向心矢量为主轴做施密特正交化
vec4 GetInitialMomentum(
    vec3 RayDir,          
    vec4 X,               
    int  iObserverMode,   
    float universesign,
    float PhysicalSpinA,  
    float PhysicalQ,      
    float GravityFade,
    bool isOutgoing
)
{

    KerrGeometry geo;
    ComputeGeometryScalars(X.xyz, PhysicalSpinA, PhysicalQ, GravityFade, universesign,isOutgoing, geo);

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
        float Ux_rad = (r * X.x - a * X.z) * inv_r2_a2 * U_r_KS;
        float Uz_rad = (r * X.z + a * X.x) * inv_r2_a2 * U_r_KS;
        float Uy_rad = (X.y / r) * U_r_KS;
        float Ux_tan =  X.z * U_phi_KS;
        float Uz_tan = -X.x * U_phi_KS;
        
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
            Ut = -C / max(1e-19, B); 
        } else {
            if (B < 0.0) {
                 Ut = 2.0 * C / (-B + sqrtDet);
            } else {
                 Ut = (-B - sqrtDet) / (2.0 * A);
            }
        }
        U_up = mix(U_up,vec4(U_spatial, Ut),GravityFade);//在包围盒边界回退到静态观者

    }else if (iObserverMode == 2) {
        // --- 模式 2: 任意四速观测者 (基于坐标速度) ---
        vec3 v_in = iCameraVelocity.xyz;
        if (any(isnan(v_in)) || any(isinf(v_in))) {v_in=vec3(0.0);}
        vec4 V_up = vec4(v_in, 1.0);
        vec4 V_down = LowerIndex(V_up, geo);
        float V_sq = dot(V_up, V_down);
        if (V_sq < 0) {
            U_up = V_up * inversesqrt(-V_sq);
        } else {
            return vec4(114514.0);
        }
    }else if (iObserverMode == 3) {
        vec3 v_in = -iCameraVelocity.xyz;
        if (any(isnan(v_in)) || any(isinf(v_in))) {v_in=vec3(0.0);}
        vec4 V_up = vec4(v_in, 1.0);
        vec4 V_down = LowerIndex(V_up, geo);
        float V_sq = dot(V_up, V_down);
        if (V_sq < 0) {
            U_up = V_up * inversesqrt(-V_sq);
        } else {
            return vec4(114514.0);
        }
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
// =============================================================================
// 调试函数：检查初始动量合法性并可视化局部动量方向
// =============================================================================
vec3 DebugInitialMomentum(
    vec4 P_cov, 
    vec4 X, 
    int ObserverMode, 
    float universesign, 
    float PhysicalSpinA, 
    float PhysicalQ, 
    float GravityFade, 
    bool isOutgoing, 
    vec3 CameraVelocity
) {
    if (P_cov == vec4(114514.0)) return vec3(0.0);

    KerrGeometry geo;
    ComputeGeometryScalars(X.xyz, PhysicalSpinA, PhysicalQ, GravityFade, universesign, isOutgoing, geo);

    // 升指标得到逆变动量，计算模长平方（测试类光条件）
    vec4 P_up = RaiseIndex(P_cov, geo);
    float norm_sq = dot(P_cov, P_up);

    // ====================================
    // 重建观者四维速度和局部平直标架
    // ====================================
    vec4 U_up;
    float g_tt = -1.0 + geo.f;
    float time_comp = 1.0 / sqrt(max(1e-9, -g_tt));
    U_up = vec4(0.0, 0.0, 0.0, time_comp);
    
    if (ObserverMode == 1) {
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
        float Ux_rad = (r * X.x - a * X.z) * inv_r2_a2 * U_r_KS;
        float Uz_rad = (r * X.z + a * X.x) * inv_r2_a2 * U_r_KS;
        float Uy_rad = (X.y / r) * U_r_KS;
        float Ux_tan =  X.z * U_phi_KS;
        float Uz_tan = -X.x * U_phi_KS;
        
        vec3 U_spatial = vec3(Ux_rad + Ux_tan, Uy_rad, Uz_rad + Uz_tan);
        float l_dot_u_spatial = dot(geo.l_down.xyz, U_spatial);
        float U_spatial_sq = dot(U_spatial, U_spatial);
        float A = -1.0 + geo.f;
        float B = 2.0 * geo.f * l_dot_u_spatial;
        float C = U_spatial_sq + geo.f * (l_dot_u_spatial * l_dot_u_spatial) + 1.0; 
        float Det = max(0.0, B*B - 4.0 * A * C);
        float Ut = (abs(A) < 1e-7) ? (-C / max(1e-19, B)) : ((B < 0.0) ? (2.0 * C / (-B + sqrt(Det))) : ((-B - sqrt(Det)) / (2.0 * A)));
        
        U_up = mix(vec4(0.0, 0.0, 0.0, time_comp), vec4(U_spatial, Ut), GravityFade);
    } else if (ObserverMode == 2) {
        vec3 v_in = CameraVelocity;
        if (any(isnan(v_in)) || any(isinf(v_in))) v_in = vec3(0.0);
        vec4 V_up = vec4(v_in, 1.0);
        vec4 V_down = LowerIndex(V_up, geo);
        float V_sq = dot(V_up, V_down);
        if (V_sq < 0.0) U_up = V_up * inversesqrt(-V_sq);
    }
    vec4 U_down = LowerIndex(U_up, geo);

    vec3 m_r = -normalize(X.xyz);
    vec3 WorldUp = vec3(0.0, 1.0, 0.0);
    if (abs(dot(m_r, WorldUp)) > 0.999) WorldUp = vec3(1.0, 0.0, 0.0);
    vec3 m_phi = normalize(cross(WorldUp, m_r)); 
    vec3 m_theta = cross(m_phi, m_r); 

    vec4 e1 = vec4(m_r, 0.0); e1 += dot(e1, U_down) * U_up; vec4 e1_d = LowerIndex(e1, geo); float n1 = sqrt(max(1e-9, dot(e1, e1_d))); e1 /= n1; e1_d /= n1;
    vec4 e2 = vec4(m_theta, 0.0); e2 += dot(e2, U_down) * U_up; e2 -= dot(e2, e1_d) * e1; vec4 e2_d = LowerIndex(e2, geo); float n2 = sqrt(max(1e-9, dot(e2, e2_d))); e2 /= n2; e2_d /= n2;
    vec4 e3 = vec4(m_phi, 0.0); e3 += dot(e3, U_down) * U_up; e3 -= dot(e3, e1_d) * e1; e3 -= dot(e3, e2_d) * e2; vec4 e3_d = LowerIndex(e3, geo); e3 /= sqrt(max(1e-9, dot(e3, e3_d)));

    // ====================================
    // 合法性检查与局部方向投影
    // ====================================
    // 计算光在观者局部正交标架下的空间动量: P_local^i = P_mu e_i^mu
    vec3 p_local = vec3(dot(P_cov, e1), dot(P_cov, e2), dot(P_cov, e3));
    vec3 local_dir = normalize(p_local); 

    // r通道：检查所有约束条件是否满足
    bool is_lightlike = abs(norm_sq) < 1e-4;    // 约束1：必须严格类光
    float E_loc = -dot(P_cov, U_up);
    bool is_energy_pos = true;//E_loc > 0.0;           // 约束2：观者测量的局部能量必须为正
    bool is_forward = true;//P_up.w > 0.0;             // 约束3：坐标时间分量一般向未来流动

    float valid_r = (is_lightlike && is_energy_pos && is_forward) ? 1.0 : 0.0;

    // g,b通道：将局部光线的x、y方向投影到 [0, 1] 颜色区间
    float g_chan = local_dir.x * 0.5 + 0.5;
    float b_chan = local_dir.y * 0.5 + 0.5;

    return vec3(valid_r, g_chan, b_chan);
}
//inout换系

// 物理常量与数值容差定义
const float EPS = 1e-16; // 防止除0和对数域爆炸的安全容差

//
// 笛卡尔 Kerr-Schild 坐标系 Ingoing/Outgoing 相互变换 (Y轴自旋版)
// 
// @param X          inout: 四坐标 (x, y, z, T)  (注：y为自旋轴)
// @param r_sign     in:    径向符号 (+1.0 或 -1.0)
// @param P          inout: 协变四动量 (p_x, p_y, p_z, p_T)
// @param M, a, Q    in:    黑洞三毛参数
// @param out_to_in  in:    false: In->Out;  true: Out->In
//
void transformKerrSchild_YSpin(inout vec4 X, in float r_sign, inout vec4 P, 
                               in float M, in float a, in float Q, in bool out_to_in) 
{
    // 1. 提取变量
    float x = X.x, y = X.y, z = X.z, t = X.w;
    float px = P.x, py = P.y, pz = P.z, pt = P.w;
    
    float a2 = a * a;
    float M2 = M * M;
    float Q2 = Q * Q;

    // 2. 解径向方程求 r (Y 为自旋轴)
    float R2 = x*x + y*y + z*z;
    float u = R2 - a2;
    float v = 4.0 * a2 * y * y; 
    
    float r2;
    if (u >= 0.0) {
        r2 = 0.5 * (u + sqrt(u*u + v));
    } else {
        r2 = 0.5 * v / max(1e-20, sqrt(u*u + v) - u);
    }
    
    float r = r_sign * sqrt(max(r2, 0.0));

    // 3. 计算视界函数 Delta 与空间底度规项 D
    float Delta = r*r - 2.0*M*r + a2 + Q2;
    float safe_Delta = sign(Delta) * max(abs(Delta), EPS);
    if (safe_Delta == 0.0) safe_Delta = EPS;

    float r3 = r * r * r;
    float D = r3 * r + a2 * y * y; // r^4 + a^2 y^2
    float safe_D = max(D, 1e-12);

    // 4. 计算径向坐标的梯度 r_i (经过 x->z, y->x, z->y 置换)
    vec3 grad_r = vec3(
        r3 * x / safe_D,                 // 对应原 y 的梯度形式
        r * (r*r + a2) * y / safe_D,     // 对应原 z 的梯度形式 (极轴方向)
        r3 * z / safe_D                  // 对应原 x 的梯度形式
    );

    // 5. 分析黑洞类型并计算积分函数 F(r) 和 g(r)
    float delta_disc = M2 - a2 - Q2; 
    float F_r = 0.0;
    float g_r = 0.0;
    float abs_Delta_safe = max(abs(Delta), EPS);

    if (delta_disc > EPS) {
        // [情形 A: 非极端黑洞]
        float K = sqrt(delta_disc);
        float r_plus = M + K;
        float r_minus = M - K;
        
        float frac = abs(r - r_plus) / max(abs(r - r_minus), EPS);
        float ln_arg = log(max(frac, EPS));
        float ln_Delta = log(abs_Delta_safe);

        F_r = 2.0 * M * ln_Delta + ((2.0 * M2 - Q2) / K) * ln_arg;
        g_r = (a / K) * ln_arg;
        
    } else if (delta_disc < -EPS) {
        // [情形 B: 裸奇点 (无视界)]
        float K = sqrt(-delta_disc);
        float ln_Delta = log(abs_Delta_safe);
        float atan_arg = atan((r - M) / K);

        F_r = 2.0 * M * ln_Delta + (2.0 * (2.0 * M2 - Q2) / K) * atan_arg;
        g_r = (2.0 * a / K) * atan_arg;
        
    } else {
        // [情形 C: 极端黑洞]
        float rM = r - M;
        float safe_rM = sign(rM) * max(abs(rM), EPS);
        if (safe_rM == 0.0) safe_rM = EPS;
        float ln_rM = log(max(abs(rM), EPS));

        F_r = 4.0 * M * ln_rM - 2.0 * (2.0 * M2 - Q2) / safe_rM;
        g_r = -2.0 * a / safe_rM;
    }

    g_r += 2.0 * atan(a, r); 
    // 6. 计算导数与动量标量 K_p
    float F_prime = 2.0 * (2.0 * M * r - Q2) / safe_Delta;
    
    float g_prime = 2.0 * a / safe_Delta - 2.0 * a / (r * r + a * a);

    // 角动量 Ly (自旋轴为 y，对应的轨道角动量守恒量)
    float Ly = z * px - x * pz; 
    float K_p = F_prime * pt + g_prime * Ly;

    // 7. 处理换系方向逻辑
    float dir = out_to_in ? -1.0 : 1.0; 
    
    // 旋转角与时间延迟
    float angle      = -dir * g_r;
    float time_shift = -dir * F_r;
    
    // 8. 修正空间动量 (利用新的径向梯度)
    vec3 P_tilde = vec3(px, py, pz) + dir * grad_r * K_p;

    // 9. 旋转矩阵 (作用于 Z-X 平面，绕 Y 轴)
    float cos_a = cos(angle);
    float sin_a = sin(angle);

    // 10. 赋值回原变量 (应用 Z-X 平面的旋转和时间偏移)
    X.x = x * cos_a + z * sin_a; // 原公式的 y'
    X.y = y;                     // Y 坐标(自旋轴)不变
    X.z = z * cos_a - x * sin_a; // 原公式的 x'
    X.w = t + time_shift;

    P.x =   P_tilde.z * sin_a + P_tilde.x * cos_a;
    P.y =   P_tilde.y;             // P_y(自旋方向动量)不变
    P.z = - P_tilde.x * sin_a + P_tilde.z * cos_a;
    P.w = pt;                    // 能量 Pt 不变
}



// =============================================================================
// Walker-Penrose 常数计算 (Kerr-Newman 适配版)
// 输入:
//   X:     光子位置 (Cartesian KS, Y轴为自旋轴)
//   P_cov: 光子下指标动量 P_mu
//   F_cov: 光子下指标偏振 f_mu
//   a:     有量纲自旋参数
//   Q:     有量纲电荷参数
//   r:     由 KerrSchildRadius 算得的有符号半径
// 返回:
//   vec2:  Walker-Penrose 常数的 (实部, 虚部)
// =============================================================================
// Walker-Penrose常数计算
// 输入：X = (x, y, z, t) 为ingoing Kerr-Schild笛卡尔坐标
//       P_cov = 协变四动量 (P_x, P_y, P_z, P_t)
//       F_cov = 协变偏振矢量 (f_x, f_y, f_z, f_t)，实数
//       Physicala = 自旋参数a，PhysicalQ = 电荷Q，r = 径向椭球坐标（可正负）
// 输出：vec2(K_re, K_im)，即复常数 K = k_μν P^μ f^ν + i h_μν P^μ f^ν
vec2 GetWalkerPenrose(vec4 X, vec4 P_cov, vec4 F_cov, float Physicala, float PhysicalQ, float r) {
    if (abs(r) < 1e-6) return vec2(0.0); // 避免奇点

    float a = Physicala;
    float Q = PhysicalQ;
    float a2 = a * a;
    float r2 = r * r;
    float r2a2 = r2 + a2;
    float sqrt_r2a2 = sqrt(r2a2);

    float x = X.x, y = X.y, z = X.z;

    // 用赤道投影 R 计算角度，避免 sqrt(1-cos²)
    float R2 = x * x + z * z;
    float R = sqrt(R2);
    float sinTheta = R / sqrt_r2a2;               // 自然为 0 在极轴
    float cosTheta = clamp(y / r, -1.0, 1.0);     // y = r cosθ
    float sinTheta2 = sinTheta * sinTheta;

    bool onAxis = (R < 1e-12);   // 极轴判定

    float Delta = r2 - r + a2 + Q * Q;            // M=0.5, 2Mr = r
    float Sigma = r2 + a2 * cosTheta * cosTheta;
    float invSigma = 1.0 / max(1e-20, Sigma);
    float invDelta = 1.0 / max(1e-20, Delta);

    // ========== 1. 转换 P_cov (KS) → BL 协变分量 ==========
    float P_t = P_cov.w;
    float P_x = P_cov.x, P_y = P_cov.y, P_z = P_cov.z;

    float P_r_BL = (r2a2) * invDelta * P_t
                 + (x * r / r2a2) * P_x
                 + (y / r) * P_y
                 + (z * r / r2a2) * P_z;

    // P_φ (无奇异)
    float P_phi_BL = x * P_z - z * P_x;

    // P_θ (无奇异)
    float P_theta_BL = 0.0;
    float xPx_zPz = x * P_x + z * P_z;
    if (!onAxis) {
        P_theta_BL = cosTheta * sqrt_r2a2 * (xPx_zPz / R) - r * sinTheta * P_y;
    } else {
        // 极轴上 x=z=0，sinθ=0，该项为 0
        P_theta_BL = 0.0;
    }

    // ========== 2. 转换 f_cov (KS) → BL 协变分量 ==========
    float f_t = F_cov.w;
    float f_x = F_cov.x, f_y = F_cov.y, f_z = F_cov.z;

    float f_r_BL = (r2a2) * invDelta * f_t
                 + (x * r / r2a2) * f_x
                 + (y / r) * f_y
                 + (z * r / r2a2) * f_z;

    float f_phi_BL = x * f_z - z * f_x;

    float f_theta_BL = 0.0;
    float xfx_zfz = x * f_x + z * f_z;
    if (!onAxis) {
        f_theta_BL = cosTheta * sqrt_r2a2 * (xfx_zfz / R) - r * sinTheta * f_y;
    } else {
        f_theta_BL = 0.0;
    }

    // ========== 3. 度规逆分量（无奇异） ==========
    float g_tphi = -a * (r - Q * Q) * invSigma * invDelta;   // 2Mr - Q^2
    float g_tt = -((r2a2) * (r2a2) - Delta * a2 * sinTheta2) * invSigma * invDelta;
    float g_rr = Delta * invSigma;
    float g_thth = invSigma;

    // ========== 4. 升指标（BL 逆变分量） ==========
    float P_t_con = g_tt * P_t + g_tphi * P_phi_BL;
    float P_r_con = g_rr * P_r_BL;
    float P_theta_con = g_thth * P_theta_BL;

    float f_t_con = g_tt * f_t + g_tphi * f_phi_BL;
    float f_r_con = g_rr * f_r_BL;
    float f_theta_con = g_thth * f_theta_BL;

    // ========== 5. 正则化 φ 逆变分量 (乘以 sinθ 或 sin²θ) ==========
    float P_phi_s1 = 0.0, P_phi_s2 = 0.0;
    float f_phi_s1 = 0.0, f_phi_s2 = 0.0;

    if (!onAxis) {
        float invR = 1.0 / R;
        float xPz_zPx = x * P_z - z * P_x;   // = P_phi_BL
        float xfz_zfx = x * f_z - z * f_x;   // = f_phi_BL

        // P_φ / sinθ = √(r²+a²) * (x P_z - z P_x) / R
        float P_phi_over_sin = sqrt_r2a2 * xPz_zPx * invR;
        float f_phi_over_sin = sqrt_r2a2 * xfz_zfx * invR;

        float common_ = (Delta - a2 * sinTheta2) * invSigma * invDelta;

        P_phi_s2 = sinTheta2 * g_tphi * P_t + common_ * P_phi_BL;
        P_phi_s1 = sinTheta  * g_tphi * P_t + common_ * P_phi_over_sin;

        f_phi_s2 = sinTheta2 * g_tphi * f_t + common_ * f_phi_BL;
        f_phi_s1 = sinTheta  * g_tphi * f_t + common_ * f_phi_over_sin;
    }

    // ========== 6. Walker-Penrose 常数 ==========
    float tr = P_t_con * f_r_con - P_r_con * f_t_con;
    float ttheta = P_t_con * f_theta_con - P_theta_con * f_t_con;
    float rphi2 = P_r_con * f_phi_s2 - P_phi_s2 * f_r_con;
    float thetaphi1 = P_theta_con * f_phi_s1 - P_phi_s1 * f_theta_con;

    float K_re = r * tr
               + a2 * cosTheta * sinTheta * ttheta
               + a * r * rphi2
               + a * (r2a2) * cosTheta * thetaphi1;

    float K_im = a * cosTheta * tr
               - a * r * sinTheta * ttheta
               + a2 * cosTheta * rphi2
               - r * (r2a2) * thetaphi1;

    return vec2(K_re, K_im);
}
// 求解偏振在屏幕XY轴的投影
//vec2 SolvePolarization(vec2 W_emit, vec2 W_X, vec2 W_Y) {
//    float det = W_X.x * W_Y.y - W_X.y * W_Y.x;
//    if (abs(det) < 1e-9) return vec2(1.0, 0.0); // 奇异退化保护
//    float c1 = (W_emit.x * W_Y.y - W_emit.y * W_Y.x) / det;
//    float c2 = (W_X.x * W_emit.y - W_X.y * W_emit.x) / det;
//    return vec2(c1, c2);
//}
// =============================================================================
// 解算偏振在相机屏幕上的二维投影
// 输入:
//   K_photon: 当前光子射线携带的 Walker-Penrose 常数 (x:实部, y:虚部)
//   K_right:  相机“右”基向量 (Camera Right) 对应的 WP 常数
//   K_up:     相机“上”基向量 (Camera Up) 对应的 WP 常数
// 返回:
//   vec2:     偏振在相机屏幕上的投影系数 (alpha, beta)，即 (X屏幕分量, Y屏幕分量)
// =============================================================================
// =============================================================================
// 解算偏振在相机屏幕上的二维投影 (科学严谨版：基于守恒律的投影归一化)
// =============================================================================
vec2 SolvePolarization(vec2 K_photon, vec2 K_right, vec2 K_up) {
    float det = K_right.x * K_up.y - K_right.y * K_up.x;
    
    // 降低退化阈值。在 Float32 下，det 可能会小到 1e-20 量级
    if (abs(det) < 1e-25) {
        return vec2(1.0, 0.0); 
    }
    
    float inv_det = 1.0 / det;
    
    float alpha = ( K_up.y * K_photon.x - K_up.x * K_photon.y) * inv_det;
    float beta  = (-K_right.y * K_photon.x + K_right.x * K_photon.y) * inv_det;
    
    vec2 result = vec2(alpha, beta);
    
    // 【核心修复】：物理守恒律约束
    // 平行移动保持极化矢量的模长不变。因为发射点和相机处的极化基底均已归一化，
    // 理论上必定有 alpha^2 + beta^2 = 1。
    // Float32 在处理趋近径向光线的 WP 常数时发生灾难性相消，导致振幅失真甚至爆炸。
    // 在此将结果重新归一化，既消除了数值发散，又保留了精确的偏振方向（相位）信息。
    float mag = length(result);
    if (mag > 1e-19) {
        result /= mag;
    } else {
        // 如果连方向都被噪声完全摧毁(极其罕见)，指定一个默认方向
        result = vec2(1.0, 0.0);
    }
    
    return result;
}


// =============================================================================
// 5.积分器
// =============================================================================
struct State {
    vec4 X; // x^u
    vec4 P; // p_u
};

//通过缩放动量空间部分修正哈密顿量
void ApplyHamiltonianCorrection(inout vec4 P, vec4 X, float E, float PhysicalSpinA, float PhysicalQ, float fade, float r_sign, bool isOutgoing) {
    P.w = -E; 
    vec3 p = P.xyz;    
    
    KerrGeometry geo;
    // 增加入参 isOutgoing
    ComputeGeometryScalars(X.xyz, PhysicalSpinA, PhysicalQ, fade, r_sign, isOutgoing, geo);
    
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
            P.xyz *= mix(k, 1.0, clamp(abs(k - 1.0) / 0.1 - 1.0, 0.0, 1.0));
        }
    }
}
//哈密顿量时空导数
State GetDerivativesAnalytic(State S, float PhysicalSpinA, float PhysicalQ, float fade, bool isOutgoing, inout KerrGeometry geo) {
    State deriv;
    
    ComputeGeometryGradients(S.X.xyz, PhysicalSpinA, PhysicalQ, fade, geo);
    
    float l_dot_P = dot(geo.l_up.xyz, S.P.xyz) + geo.l_up.w * S.P.w;
    
    vec4 P_flat = vec4(S.P.xyz, -S.P.w); 
    deriv.X = P_flat - geo.f * l_dot_P * geo.l_up;
    
    vec3 grad_A = (-2.0 * geo.r * geo.inv_r2_a2) * geo.inv_r2_a2 * geo.grad_r;
    
    float dirSign = isOutgoing ? -1.0 : 1.0;
    
    // 引入 dirSign 进行导数修正
    float rx_az = dirSign * geo.r * S.X.x - PhysicalSpinA * S.X.z;
    float rz_ax = dirSign * geo.r * S.X.z + PhysicalSpinA * S.X.x;
    
    vec3 d_num_lx = dirSign * S.X.x * geo.grad_r; 
    d_num_lx.x += dirSign * geo.r; 
    d_num_lx.z -= PhysicalSpinA;
    vec3 grad_lx = geo.inv_r2_a2 * d_num_lx + rx_az * grad_A;
    
    vec3 grad_ly = dirSign * (geo.r * geo.inv_den_f) * vec3(-S.X.x * S.X.y, geo.r2 - S.X.y * S.X.y, -S.X.z * S.X.y);
    
    vec3 d_num_lz = dirSign * S.X.z * geo.grad_r;
    d_num_lz.z += dirSign * geo.r;
    d_num_lz.x += PhysicalSpinA;
    vec3 grad_lz = geo.inv_r2_a2 * d_num_lz + rz_ax * grad_A;
    
    vec3 P_dot_grad_l = S.P.x * grad_lx + S.P.y * grad_ly + S.P.z * grad_lz;
    
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
    bool isOutgoing,      // <--- 新增
    KerrGeometry geo0, 
    State k1
) {
    State s0; s0.X = X; s0.P = P;
    // k2 Step
    State s1; 
    s1.X = s0.X + 0.5 * dt * k1.X; 
    s1.P = s0.P + 0.5 * dt * k1.P;
    float sign1 = GetIntermediateSign(s0.X, s1.X, r_sign, PhysicalSpinA);
    KerrGeometry geo1;
    ComputeGeometryScalars(s1.X.xyz, PhysicalSpinA, PhysicalQ, fade, sign1, isOutgoing, geo1);
    State k2 = GetDerivativesAnalytic(s1, PhysicalSpinA, PhysicalQ, fade, isOutgoing, geo1);
    // k3 Step
    State s2; 
    s2.X = s0.X + 0.5 * dt * k2.X; 
    s2.P = s0.P + 0.5 * dt * k2.P;
    float sign2 = GetIntermediateSign(s0.X, s2.X, r_sign, PhysicalSpinA);
    KerrGeometry geo2;
    ComputeGeometryScalars(s2.X.xyz, PhysicalSpinA, PhysicalQ, fade, sign2, isOutgoing, geo2);
    State k3 = GetDerivativesAnalytic(s2, PhysicalSpinA, PhysicalQ, fade, isOutgoing, geo2);
    // k4 Step
    State s3; 
    s3.X = s0.X + dt * k3.X; 
    s3.P = s0.P + dt * k3.P;
    float sign3 = GetIntermediateSign(s0.X, s3.X, r_sign, PhysicalSpinA);
    KerrGeometry geo3;
    ComputeGeometryScalars(s3.X.xyz, PhysicalSpinA, PhysicalQ, fade, sign3, isOutgoing, geo3);
    State k4 = GetDerivativesAnalytic(s3, PhysicalSpinA, PhysicalQ, fade, isOutgoing, geo3);
    vec4 finalX = s0.X + (dt / 6.0) * (k1.X + 2.0 * k2.X + 2.0 * k3.X + k4.X);
    vec4 finalP = s0.P + (dt / 6.0) * (k1.P + 2.0 * k2.P + 2.0 * k3.P + k4.P);
    
    float finalSign = GetIntermediateSign(s0.X, finalX, r_sign, PhysicalSpinA);
    if(finalSign > 0) { 
        ApplyHamiltonianCorrection(finalP, finalX, E, PhysicalSpinA, PhysicalQ, fade, finalSign, isOutgoing);
    }
    X = finalX;
    P = finalP;
}
// =============================================================================
// SECTION 6: 热折射，吸积盘与喷流,经纬网
// =============================================================================

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

    //核心半径估计 (Jet Core)
    float coreRadiusLimit = sqrt(2.0 * InterRadius * InterRadius + 0.03 * 0.03 * y * y);
    
    //外壳半径估计 (Jet Shell)
    // 对应 JetColor: Rho < 1.3 * InterRadius + 0.25 * Wid
    float shellRadiusLimit = 1.3 * InterRadius + 0.25 * y;
    
    // 取两者较大的作为热浪边界，并稍微膨胀以覆盖辉光
    float maxJetRadius = max(coreRadiusLimit, shellRadiusLimit) * 1.2;
    
    //垂直长度限制
    // 喷流在 OuterRadius 附近开始衰减
    float jLen = OuterRadius * 0.8;
    
    float rMaskJet = 1.0 - smoothstep(maxJetRadius * 0.8, maxJetRadius * 1.1, r);
    float hMaskJet = 1.0 - smoothstep(jLen * 0.75, jLen * 1.0, y);
    
    float startYMask = smoothstep(InterRadius * 0.5, InterRadius * 1.5, y);
    
    return rMaskJet * hMaskJet * startYMask;
}

// 包围盒检测优化
bool IsInHazeBoundingVolume(vec3 pos, float probeDist, float OuterRadius) {
    float maxR = OuterRadius * 1.2;
    float maxY = maxR; 
    float r = length(pos);
    if (r > maxR + probeDist) return false;
    return true;
}

// 计算热浪偏移力
vec3 GetHazeForce(vec3 pos_Rg, float time, float PhysicalSpinA, float PhysicalQ, 
                  float InterRadius, float OuterRadius, float Thin, float Hopper,
                  float AccretionRate)
{
    //吸积盘热浪强度计算
    float dDens = HAZE_DISK_DENSITY_REF;
    float dLimitAbs = 20.0;
    float dFactorAbs = clamp((log(dDens/dLimitAbs)) / 2.302585, 0.0, 1.0);
    //喷流密度仅作参考
    float jDensRef = HAZE_JET_DENSITY_REF; 
    float dFactorRel = 1.0;
    if (jDensRef > 1e-20) dFactorRel = clamp((log(dDens/jDensRef)) / 2.302585, 0.0, 1.0);
    float diskHazeStrength = dFactorAbs * dFactorRel;

    //喷流热浪强度计算
    float jetHazeStrength = 0.0;
    float JetThreshold = 1e-2;
    
    if (AccretionRate >= JetThreshold)
    {
        float logRate = log(AccretionRate);
        float logMin  = log(JetThreshold);
        float logMax  = log(1.0);
        
        float intensity = clamp((logRate - logMin) / (logMax - logMin), 0.0, 1.0);
        jetHazeStrength = intensity;
    }

    //退出优化
    if (diskHazeStrength <= 0.001 && jetHazeStrength <= 0.001) return vec3(0.0);

    vec3 totalForce = vec3(0.0);
    float eps = 0.1;

    //循环周期计算
    float rotSpeedBase = 100.0 * HAZE_ROT_SPEED; 
    float jetSpeedBase = 50.0 * HAZE_FLOW_SPEED;
    
    // 计算内边缘角速度
    float ReferenceOmega = GetKeplerianAngularVelocity(6.0, 1.0, PhysicalSpinA, PhysicalQ);
    
    //内圈每旋转一定圈数，噪声完成一次淡入淡出循环
    float AdaptiveFrequency = abs(ReferenceOmega * rotSpeedBase) / (2.0 * kPi * 5.14);
    
    AdaptiveFrequency = max(AdaptiveFrequency, 0.1);

    float flowTime = time * AdaptiveFrequency;
    
    float phase1 = fract(flowTime);
    float phase2 = fract(flowTime + 0.5);
    
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

    // 垂直漂移
    float VerticalDrift1 = t_offset1 * 1.0; 
    float VerticalDrift2 = t_offset2 * 1.0;
    
    //吸积盘热浪
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

    //喷流热浪
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



vec4 DiskColor(vec4 BaseColor, vec4 RayPos, vec4 LastRayPos,
               vec4 iP_cov, vec4 lastiP_cov, float iE_obs,
               float InterRadius, float OuterRadius, float Thin, float Hopper, float Brightmut, float Darkmut, float Reddening, float Saturation, float DiskTemperatureArgument,
               float BlackbodyIntensityExponent, float RedShiftColorExponent, float RedShiftIntensityExponent,
               float PeakTemperature, float ShiftMax, 
               float PhysicalSpinA, 
               float PhysicalQ, bool isoutgoing,
               float ThetaInShell,
               inout float RayMarchPhase,
               vec2 WP_CamX, vec2 WP_CamY, inout vec2 StokesQU
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
    vec3 EndPos   = RayPos.xyz;
    vec3 ChordDelta = EndPos - StartPos;
    vec3 ChordDir = length(ChordDelta) > 1e-8 ? normalize(ChordDelta) : vec3(0.0, 1.0, 0.0);

    // --- 【新增：利用局域度规计算空间固有距离（Proper Distance）】 ---
    vec3 MidPos = 0.5 * (StartPos + EndPos);
    KerrGeometry geo_mid;
    // 使用原始输入系下的 Geometry 评估中点度规项
    ComputeGeometryScalars(MidPos, PhysicalSpinA, PhysicalQ, 1.0, 1.0, isoutgoing, geo_mid);
    float l_dot_dx = dot(geo_mid.l_down.xyz, ChordDelta);
    // dl = sqrt( |dx|^2 + f * (l . dx)^2 )
    float proper_dist = sqrt(max(1e-9, dot(ChordDelta, ChordDelta) + geo_mid.f * l_dot_dx * l_dot_dx));

    float StartTimeLag = LastRayPos.w;
    float EndTimeLag   = RayPos.w;

    float R_Start = KerrSchildRadius(StartPos, PhysicalSpinA, 1.0);
    float R_End   = KerrSchildRadius(RayPos.xyz, PhysicalSpinA, 1.0);
    if (max(R_Start, R_End) < InterRadius * 0.9) return BaseColor;

    // 将原本的 StepLength 依赖彻底替换为计算出的物理固有距离
    float TotalDist = proper_dist; 
    float TraveledDist = 0.0;
    
    int SafetyLoopCount = 0;
    const int MaxLoops = 114514; 

    while (TraveledDist < TotalDist && SafetyLoopCount < MaxLoops)
    {
        if (CurrentResult.a > 0.99) break;
        SafetyLoopCount++;

        vec3 CurrentPos = mix(StartPos, EndPos, clamp(TraveledDist / max(1e-9, TotalDist), 0.0, 1.0));
        float DistanceToBlackHole = length(CurrentPos); 
        
        float SmallStepBoundary = max(OuterRadius, 12.0);
        float StepSize = 1.0; 
        
        StepSize *= 0.15 + 0.25 * min(max(0.0, 0.5 * (0.5 * DistanceToBlackHole / max(10.0 , SmallStepBoundary) - 1.0)), 1.0);
        if ((DistanceToBlackHole) >= 2.0 * SmallStepBoundary) StepSize *= DistanceToBlackHole;
        else if ((DistanceToBlackHole) >= 1.0 * SmallStepBoundary) StepSize *= ((1.0 + 0.25 * max(DistanceToBlackHole - 12.0, 0.0)) * (2.0 * SmallStepBoundary - DistanceToBlackHole) + DistanceToBlackHole * (DistanceToBlackHole - SmallStepBoundary)) / SmallStepBoundary;
        else StepSize *= min(1.0 + 0.25 * max(DistanceToBlackHole - 12.0, 0.0), DistanceToBlackHole);
        
        StepSize = max(0.01, StepSize); 

        float DistToNextSample = RayMarchPhase * StepSize;
        float NextTarget = min(TotalDist, TraveledDist + DistToNextSample);

        vec3 PosPrev = mix(StartPos, EndPos, clamp(TraveledDist / max(1e-9, TotalDist), 0.0, 1.0));
        vec3 PosNext = mix(StartPos, EndPos, clamp(NextTarget / max(1e-9, TotalDist), 0.0, 1.0));

        bool crossed = (PosPrev.y * PosNext.y < 0.0);
        bool shouldSample = false;
        vec3 SamplePos = PosNext;
        crossed = false;

        if (crossed)
        {
            float t_cross = abs(PosPrev.y) / max(1e-9, abs(PosPrev.y) + abs(PosNext.y));
            vec3 CPoint = mix(PosPrev, PosNext, t_cross);
            
            SamplePos = CPoint + min(Thin, length(CPoint - PosPrev)) * ChordDir * (-1.0 + 2.0 * RandomStep(10000.0 * (CPoint.zx / OuterRadius), fract(iTime * 1.0 + 0.5)));
            shouldSample = true;
            
            RayMarchPhase = 1.0;
            TraveledDist = NextTarget; 
        }
        else
        {
            if (NextTarget < TotalDist)
            {
                SamplePos = PosNext;
                shouldSample = true;
                RayMarchPhase = 1.0;
                TraveledDist = NextTarget;
            }
            else
            {
                float DistanceTraveled = TotalDist - TraveledDist;
                RayMarchPhase -= DistanceTraveled / StepSize;
                if (RayMarchPhase < 0.0) RayMarchPhase = 0.0;
                TraveledDist = TotalDist;
            }
        }

        if (shouldSample)
        {
            float TimeInterpolant = min(1.0, TraveledDist / max(1e-9, TotalDist));
            float CurrentRayTimeLag = mix(StartTimeLag, EndTimeLag, TimeInterpolant);
            
            vec4 Sample_X = vec4(SamplePos, CurrentRayTimeLag);
            vec4 Sample_P_cov = mix(lastiP_cov, iP_cov, TimeInterpolant);
            
            if (isoutgoing) {
                transformKerrSchild_YSpin(Sample_X, 1.0, Sample_P_cov, 0.5, PhysicalSpinA, PhysicalQ, true);
            }
            
            SamplePos = Sample_X.xyz;
            float EmissionTime = iBlackHoleTime + Sample_X.w;

            float PosR = KerrSchildRadius(SamplePos, PhysicalSpinA, 1.0);
            float PosY = SamplePos.y;
            
            float GeometricThin = Thin + max(0.0, (length(SamplePos.xz) - 3.0) * Hopper);
            float InterCloudEffectiveRadius = (PosR - InterRadius) / min(OuterRadius - InterRadius, 12.0);
            float InnerCloudBound = max(GeometricThin, Thin * 1.0) * max(0.0, 1.0 - 5.0 * pow(InterCloudEffectiveRadius, 2.0));
            float UnionBound = max(GeometricThin * 1.5, max(0.0, InnerCloudBound));

            if (abs(PosY) < UnionBound && PosR < OuterRadius && PosR > InterRadius)
            {
                 KerrGeometry geo_emit;
                 ComputeGeometryScalars(SamplePos, PhysicalSpinA, PhysicalQ, 1.0, 1.0, false, geo_emit);
                 vec4 Sample_P_up = RaiseIndex(Sample_P_cov, geo_emit);
                 vec3 local_Dir = normalize(Sample_P_up.xyz);

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
                     
                     float inv_r = 1.0 / max(1e-6, PosR);
                     float inv_r2 = inv_r * inv_r;
                     float V_pot = inv_r - (PhysicalQ * PhysicalQ) * inv_r2;
                     
                     float g_tt = -(1.0 - V_pot);
                     float g_tphi = -PhysicalSpinA * V_pot; 
                     float g_phiphi = PosR * PosR + PhysicalSpinA * PhysicalSpinA + PhysicalSpinA * PhysicalSpinA * V_pot;
                     float norm_metric = g_tt + 2.0 * AngularVelocity * g_tphi + AngularVelocity * AngularVelocity * g_phiphi;
                     
                     float min_norm = -0.01; 
                     float u_t = inversesqrt(max(abs(min_norm), -norm_metric));
                     
                     float P_phi = - SamplePos.x * Sample_P_cov.z + SamplePos.z * Sample_P_cov.x;
                     float E_emit = u_t * (iE_obs - AngularVelocity * P_phi);
                     float FreqRatio = 1.0 / max(1e-6, E_emit);

                     float DiskTemperature = pow(DiskTemperatureArgument * pow(1.0 / max(1e-6, PosR), 3.0) * max(1.0 - sqrt(InterRadius / max(1e-6, PosR)), 0.000001), 0.25);
                     float VisionTemperature = DiskTemperature * pow(FreqRatio, RedShiftColorExponent); 
                     float BrightWithoutRedshift = 0.05 * min(OuterRadius / (1000.0), 1000.0 / OuterRadius) + 0.55 / exp(5.0 * EffectiveRadius) * mix(0.2 + 0.8 * abs(local_Dir.y), 1.0, clamp(GeometricThin - 0.8, 0.2, 1.0)); 
                     BrightWithoutRedshift *= pow(DiskTemperature / PeakTemperature, BlackbodyIntensityExponent); 
                     
                     float RotPosR = PosR + 0.25 / 3.0 * EmissionTime;
                     float Density = DenAndThiFactor;
                     vec4 SampleColor = vec4(0.0);

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
        
                     SampleColor.xyz *= 1.0 + clamp(iPhotonRingBoost, 0.0, 10.0) * clamp(0.3 * ThetaInShell - 0.1, 0.0, 1.0);
                     VisionTemperature *= 1.0 + clamp(iPhotonRingColorTempBoost, 0.0, 10.0) * clamp(0.3 * ThetaInShell - 0.1, 0.0, 1.0);
                     
                     float InnerAngVel = GetKeplerianAngularVelocity(max(3.0, InterRadius), 1.0, PhysicalSpinA, PhysicalQ);
                     float InnerCloudTimePhase = kPi / (kPi / max(1e-6, InnerAngVel)) * EmissionTime; 
                     float InnerRotArg = 0.666666 * InnerCloudTimePhase;
                     float PosThetaForInnerCloud = Vec2ToTheta(SamplePos.zx, vec2(cos(InnerRotArg), sin(InnerRotArg)));

                     if (abs(PosY) < InnerCloudBound) 
                     {
                         float DustIntensity = max(1.0 - pow(PosY / (GeometricThin * max(1.0 - 5.0 * pow(InterCloudEffectiveRadius, 2.0), 0.0001)), 2.0), 0.0);
                         if (DustIntensity > 0.0) {
                            float DustNoise = GenerateAccretionDiskNoise(vec3(1.5 * fract((1.5 * PosThetaForInnerCloud + InnerCloudTimePhase) / 2.0 / kPi) * 2.0 * kPi, PosR, PosY), 0.0, 6.0, 80.0);
                            float DustVal = DustIntensity * DustNoise;
                            SampleColor += 0.02 * vec4(vec3(DustVal), 0.2 * DustVal) * sqrt(max(0.0, 1.0001 - local_Dir.y * local_Dir.y));
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
                     SampleColor.xyz *= mix(1.0, max(1.0, abs(local_Dir.y) / 0.2), clamp(0.3 - 0.6 * (PerturbedThickness / max(1e-6, Density) - 1.0), 0.0, 0.3));
                     SampleColor.xyz *= 1.0 + 1.2 * max(0.0, max(0.0, min(1.0, 3.0 - 2.0 * Thin)) * min(0.5, 1.0 - 5.0 * Hopper));
                     SampleColor.xyz *= Brightmut * clamp(4.0 - 18.0 * (PosR - InterRadius) / (OuterRadius - InterRadius), 1.0, 4.0);
                     SampleColor.a   *= Darkmut * clamp(5.0 - 24.0 * (PosR - InterRadius) / (OuterRadius - InterRadius), 1.0, 5.0);
                     
                     if (E_emit < 0.0) 
                     {
                         float cMax = max(max(SampleColor.r, SampleColor.g), SampleColor.b);
                         float cMin = min(min(SampleColor.r, SampleColor.g), SampleColor.b);
                         SampleColor.rgb = vec3(cMax + cMin) - SampleColor.rgb;
                         if(iWhitehole==0) SampleColor.rgba=vec4(0.0);
                     }

                     vec4 StepColor = SampleColor * StepSize;

                     if (iPolarization != 0) {
                         float chi = -0.7; 
                         float cosChi = cos(chi);
                         float sinChi = sin(chi);
                         
                         vec4 B_tor = vec4(-SamplePos.z, 0.0, SamplePos.x, 0.0);
                         vec4 B_rad = vec4(SamplePos.x, SamplePos.y, SamplePos.z, 0.0);
                         vec4 B_up = normalize(B_tor) * cosChi + normalize(B_rad) * sinChi;
                         B_up.w = 0.0;
                     
                         vec4 u_up_fluid = vec4(AngularVelocity * (-SamplePos.z), 0.0, AngularVelocity * SamplePos.x, 1.0) * u_t;
                     
                         vec4 p_up = Sample_P_up;
                     
                         vec4 f_down;
                         f_down.x =  det3(u_up_fluid.yzw, p_up.yzw, B_up.yzw);
                         f_down.y = -det3(u_up_fluid.xzw, p_up.xzw, B_up.xzw);
                         f_down.z =  det3(u_up_fluid.xyw, p_up.xyw, B_up.xyw);
                         f_down.w = -det3(u_up_fluid.xyz, p_up.xyz, B_up.xyz);
                     
                         float f_norm = sqrt(max(1e-12, abs(dot(RaiseIndex(f_down, geo_emit), f_down))));
                         f_down /= f_norm;
                     
                         vec4 Emit_X = vec4(SamplePos, EmissionTime);
                         vec2 WP_emit = GetWalkerPenrose(Emit_X, Sample_P_cov, f_down, PhysicalSpinA, PhysicalQ, PosR);
                         
                         vec2 ScreenAmps = SolvePolarization(WP_emit, WP_CamX, WP_CamY);
                         
                         float weight = (SampleColor.r + SampleColor.g + SampleColor.b) * StepSize * pow(1.0 - CurrentResult.a, 1.0);
                         StokesQU.x += (ScreenAmps.x * ScreenAmps.x - ScreenAmps.y * ScreenAmps.y) * weight;
                         StokesQU.y += (2.0 * ScreenAmps.x * ScreenAmps.y) * weight;
                     }

                     float aR = 1.0 + Reddening * (1.0 - 1.0);
                     float aG = 1.0 + Reddening * (3.0 - 1.0);
                     float aB = 1.0 + Reddening * (6.0 - 1.0);
                     
                     float Sum_rgb = (StepColor.r + StepColor.g + StepColor.b) * pow(1.0 - CurrentResult.a, aG);
                     float Denominator = StepColor.r * pow(1.0 - CurrentResult.a, aR) + StepColor.g * pow(1.0 - CurrentResult.a, aG) + StepColor.b * pow(1.0 - CurrentResult.a, aB);
                     
                     float r001 = 0.0; float g001 = 0.0; float b001 = 0.0;
                     if (Denominator > 0.000001)
                     {
                         r001 = Sum_rgb * StepColor.r * pow(1.0 - CurrentResult.a, aR) / Denominator;
                         g001 = Sum_rgb * StepColor.g * pow(1.0 - CurrentResult.a, aG) / Denominator;
                         b001 = Sum_rgb * StepColor.b * pow(1.0 - CurrentResult.a, aB) / Denominator;
                         
                         r001 *= pow(3.0 * r001 / (r001 + g001 + b001), Saturation);
                         g001 *= pow(3.0 * g001 / (r001 + g001 + b001), Saturation);
                         b001 *= pow(3.0 * b001 / (r001 + g001 + b001), Saturation);
                     }
                     
                     CurrentResult.r += r001;
                     CurrentResult.g += g001;
                     CurrentResult.b += b001;
                     CurrentResult.a += StepColor.a * (1.0 - CurrentResult.a);
                 }
            }
        }
    }
    return CurrentResult;
}
vec4 DiskColortoRed(vec4 BaseColor, vec4 RayPos, vec4 LastRayPos,
               vec4 iP_cov, vec4 lastiP_cov, float iE_obs,
               float InterRadius, float OuterRadius, float Thin, float Hopper, float Brightmut, float Darkmut, float Reddening, float Saturation, float DiskTemperatureArgument,
               float BlackbodyIntensityExponent, float RedShiftColorExponent, float RedShiftIntensityExponent,
               float PeakTemperature, float ShiftMax, 
               float PhysicalSpinA, 
               float PhysicalQ, bool isoutgoing,
               float ThetaInShell,
               inout float RayMarchPhase,
               vec2 WP_CamX, vec2 WP_CamY, inout vec2 StokesQU
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
    vec3 EndPos   = RayPos.xyz;
    vec3 ChordDelta = EndPos - StartPos;
    vec3 ChordDir = length(ChordDelta) > 1e-8 ? normalize(ChordDelta) : vec3(0.0, 1.0, 0.0);

    // --- 【同步：利用局域度规计算空间固有距离（Proper Distance）】 ---
    vec3 MidPos = 0.5 * (StartPos + EndPos);
    KerrGeometry geo_mid;
    ComputeGeometryScalars(MidPos, PhysicalSpinA, PhysicalQ, 1.0, 1.0, isoutgoing, geo_mid);
    float l_dot_dx = dot(geo_mid.l_down.xyz, ChordDelta);
    float proper_dist = sqrt(max(1e-9, dot(ChordDelta, ChordDelta) + geo_mid.f * l_dot_dx * l_dot_dx));

    float StartTimeLag = LastRayPos.w;
    float EndTimeLag   = RayPos.w;

    float R_Start = KerrSchildRadius(StartPos, PhysicalSpinA, 1.0);
    float R_End   = KerrSchildRadius(RayPos.xyz, PhysicalSpinA, 1.0);
    if (max(R_Start, R_End) < InterRadius * 0.9) return BaseColor;

    // 替换 StepLength
    float TotalDist = proper_dist; 
    float TraveledDist = 0.0;
    
    int SafetyLoopCount = 0;
    const int MaxLoops = 114514; 

    while (TraveledDist < TotalDist && SafetyLoopCount < MaxLoops)
    {
        if (CurrentResult.a > 0.99) break;
        SafetyLoopCount++;

        vec3 CurrentPos = mix(StartPos, EndPos, clamp(TraveledDist / max(1e-9, TotalDist), 0.0, 1.0));
        float DistanceToBlackHole = length(CurrentPos); 
        
        float SmallStepBoundary = max(OuterRadius, 12.0);
        float StepSize = 1.0; 
        
        StepSize *= 0.15 + 0.25 * min(max(0.0, 0.5 * (0.5 * DistanceToBlackHole / max(10.0 , SmallStepBoundary) - 1.0)), 1.0);
        if ((DistanceToBlackHole) >= 2.0 * SmallStepBoundary) StepSize *= DistanceToBlackHole;
        else if ((DistanceToBlackHole) >= 1.0 * SmallStepBoundary) StepSize *= ((1.0 + 0.25 * max(DistanceToBlackHole - 12.0, 0.0)) * (2.0 * SmallStepBoundary - DistanceToBlackHole) + DistanceToBlackHole * (DistanceToBlackHole - SmallStepBoundary)) / SmallStepBoundary;
        else StepSize *= min(1.0 + 0.25 * max(DistanceToBlackHole - 12.0, 0.0), DistanceToBlackHole);
        
        StepSize = max(0.01, StepSize); 

        // 同步相位的步进逻辑
        float DistToNextSample = RayMarchPhase * StepSize;
        float NextTarget = min(TotalDist, TraveledDist + DistToNextSample);

        vec3 PosPrev = mix(StartPos, EndPos, clamp(TraveledDist / max(1e-9, TotalDist), 0.0, 1.0));
        vec3 PosNext = mix(StartPos, EndPos, clamp(NextTarget / max(1e-9, TotalDist), 0.0, 1.0));

        bool crossed = (PosPrev.y * PosNext.y < 0.0);
        bool shouldSample = false;
        vec3 SamplePos = PosNext;
        crossed = false;

        if (crossed)
        {
            float t_cross = abs(PosPrev.y) / max(1e-9, abs(PosPrev.y) + abs(PosNext.y));
            vec3 CPoint = mix(PosPrev, PosNext, t_cross);
            
            SamplePos = CPoint + min(Thin, length(CPoint - PosPrev)) * ChordDir * (-1.0 + 2.0 * RandomStep(10000.0 * (CPoint.zx / OuterRadius), fract(iTime * 1.0 + 0.5)));
            shouldSample = true;
            
            RayMarchPhase = 1.0;
            TraveledDist = NextTarget; 
        }
        else
        {
            if (NextTarget < TotalDist)
            {
                SamplePos = PosNext;
                shouldSample = true;
                RayMarchPhase = 1.0;
                TraveledDist = NextTarget;
            }
            else
            {
                float DistanceTraveled = TotalDist - TraveledDist;
                RayMarchPhase -= DistanceTraveled / StepSize;
                if (RayMarchPhase < 0.0) RayMarchPhase = 0.0;
                TraveledDist = TotalDist;
            }
        }

        if (shouldSample)
        {
            float TimeInterpolant = min(1.0, TraveledDist / max(1e-9, TotalDist));
            float CurrentRayTimeLag = mix(StartTimeLag, EndTimeLag, TimeInterpolant);
            
            // --- 【同步：动量插值与坐标变换】 ---
            vec4 Sample_X = vec4(SamplePos, CurrentRayTimeLag);
            vec4 Sample_P_cov = mix(lastiP_cov, iP_cov, TimeInterpolant);
            
            if (isoutgoing) {
                transformKerrSchild_YSpin(Sample_X, 1.0, Sample_P_cov, 0.5, PhysicalSpinA, PhysicalQ, true);
            }
            
            SamplePos = Sample_X.xyz;
            float EmissionTime = iBlackHoleTime + Sample_X.w;

            float PosR = KerrSchildRadius(SamplePos, PhysicalSpinA, 1.0);
            float PosY = SamplePos.y;
            
            float GeometricThin = length(SamplePos.xz) * Hopper + Thin;
            float UnionBound = (GeometricThin * 1.0);

            if (abs(PosY) < UnionBound && PosR < OuterRadius && PosR > InterRadius)
            {
                 // --- 【同步：局域度规与逆变动量提升】 ---
                 KerrGeometry geo_emit;
                 ComputeGeometryScalars(SamplePos, PhysicalSpinA, PhysicalQ, 1.0, 1.0, false, geo_emit);
                 vec4 Sample_P_up = RaiseIndex(Sample_P_cov, geo_emit);

                 float AngularVelocity = GetKeplerianAngularVelocity(max(InterRadius, PosR), 1.0, PhysicalSpinA, PhysicalQ);
                 
                 float inv_r = 1.0 / max(1e-6, PosR);
                 float inv_r2 = inv_r * inv_r;
                 
                 float V_pot = inv_r - (PhysicalQ * PhysicalQ) * inv_r2;
                 
                 float g_tt = -(1.0 - V_pot);
                 float g_tphi = -PhysicalSpinA * V_pot; 
                 float g_phiphi = PosR * PosR + PhysicalSpinA * PhysicalSpinA + PhysicalSpinA * PhysicalSpinA * V_pot;
                 
                 float norm_metric = g_tt + 2.0 * AngularVelocity * g_tphi + AngularVelocity * AngularVelocity * g_phiphi;
                 
                 float min_norm = -0.01; 
                 float u_t = inversesqrt(max(abs(min_norm), -norm_metric));
                 
                 // --- 【同步：角动量计算使用插值后的 Sample_P_cov】 ---
                 float P_phi = - SamplePos.x * Sample_P_cov.z + SamplePos.z * Sample_P_cov.x;
                 
                 float E_emit = u_t * (iE_obs - AngularVelocity * P_phi);
                 float FreqRatio = 1.0 / max(1e-6, E_emit);
                 if(E_emit < 0.0) FreqRatio = 0.0;

                 float DiskTemperature = pow(DiskTemperatureArgument * pow(1.0 / max(1e-6, PosR), 3.0) * max(1.0 - sqrt(InterRadius / max(1e-6, PosR)), 0.000001), 0.25);
                 float VisionTemperature = DiskTemperature * pow(FreqRatio, RedShiftColorExponent); 
                 float BrightWithoutRedshift = 1.0;
                 BrightWithoutRedshift *= pow(DiskTemperature / PeakTemperature, BlackbodyIntensityExponent); 
                 
                 float Density = 1.0;
                 vec4 SampleColor = vec4(0.0);

                 SampleColor.xyz = vec3(BrightWithoutRedshift);
                 SampleColor.a = Density;

                 // 纯红盘特征
                 SampleColor.xyz *= vec3(1.0, 0.0, 0.0); 
                 SampleColor.xyz *= pow(FreqRatio, RedShiftIntensityExponent); 

                 SampleColor.xyz *= Brightmut;
                 SampleColor.a   *= Darkmut;

                 SampleColor.xyz *= pow(InterRadius / 1.0, 4.0);

                 bool IsPositiveEnergy = iE_obs > 0.0;
                 if (!IsPositiveEnergy) 
                 {
                     float cMax = max(max(SampleColor.r, SampleColor.g), SampleColor.b);
                     float cMin = min(min(SampleColor.r, SampleColor.g), SampleColor.b);
                     SampleColor.rgb = vec3(cMax + cMin) - SampleColor.rgb;
                 }

                 vec4 StepColor = SampleColor * StepSize;

                 // =========================================================
                 // [更新] 偏振计算
                 // =========================================================
                 if (iPolarization != 0) {
                     float chi = -0.7; 
                     float cosChi = cos(chi);
                     float sinChi = sin(chi);
                     
                     vec4 B_tor = vec4(-SamplePos.z, 0.0, SamplePos.x, 0.0);
                     vec4 B_rad = vec4(SamplePos.x, SamplePos.y, SamplePos.z, 0.0);
                     vec4 B_up = normalize(B_tor) * cosChi + normalize(B_rad) * sinChi;
                     B_up.w = 0.0;
                 
                     vec4 u_up_fluid = vec4(AngularVelocity * (-SamplePos.z), 0.0, AngularVelocity * SamplePos.x, 1.0) * u_t;
                 
                     // --- 【同步：使用插值并计算出的 Sample_P_up】 ---
                     vec4 p_up = Sample_P_up;
                 
                     vec4 f_down;
                     f_down.x =  det3(u_up_fluid.yzw, p_up.yzw, B_up.yzw);
                     f_down.y = -det3(u_up_fluid.xzw, p_up.xzw, B_up.xzw);
                     f_down.z =  det3(u_up_fluid.xyw, p_up.xyw, B_up.xyw);
                     f_down.w = -det3(u_up_fluid.xyz, p_up.xyz, B_up.xyz);
                 
                     float f_norm = sqrt(max(1e-12, abs(dot(RaiseIndex(f_down, geo_emit), f_down))));
                     f_down /= f_norm;
                 
                     // --- 【同步：传入插值后的 Sample_P_cov】 ---
                     vec4 Emit_X = vec4(SamplePos, EmissionTime);
                     vec2 WP_emit = GetWalkerPenrose(Emit_X, Sample_P_cov, f_down, PhysicalSpinA, PhysicalQ, PosR);
                     
                     vec2 ScreenAmps = SolvePolarization(WP_emit, WP_CamX, WP_CamY);
                     
                     float weight = (SampleColor.r + SampleColor.g + SampleColor.b) * StepSize * pow(1.0 - CurrentResult.a, 1.0);
                     StokesQU.x += (ScreenAmps.x * ScreenAmps.x - ScreenAmps.y * ScreenAmps.y) * weight;
                     StokesQU.y += (2.0 * ScreenAmps.x * ScreenAmps.y) * weight;
                 }
                 // =========================================================

                 // 简化的颜色叠加逻辑
                 CurrentResult = CurrentResult + StepColor * pow((1.0 - CurrentResult.a), 1.0);
            }
        }
    }
    
    return CurrentResult;
}

vec4 DiskColortoBlue(vec4 BaseColor, float StepLength, vec4 RayPos, vec4 LastRayPos,
               vec3 RayDir, vec3 LastRayDir,vec4 iP_cov, float iE_obs,
               float InterRadius, float OuterRadius, float Thin, float Hopper, float Brightmut, float Darkmut, float Reddening, float Saturation, float DiskTemperatureArgument,
               float BlackbodyIntensityExponent, float RedShiftColorExponent, float RedShiftIntensityExponent,
               float PeakTemperature, float ShiftMax, 
               float PhysicalSpinA, 
               float PhysicalQ, bool isoutgoing,
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
        
        float GeometricThin = length(SamplePos.xz) * Hopper+Thin;
        
        // 计算内侧云参数与包围盒
        float InterCloudEffectiveRadius = (PosR - InterRadius) / min(OuterRadius - InterRadius, 12.0);
        float InnerCloudBound = max(GeometricThin, Thin * 1.0) * max(0.0,1.0 - 5.0 * pow(InterCloudEffectiveRadius, 2.0));
        
        // 外层包围盒取主盘与内侧云的并集
        // GeometricThin * 1.5 是主盘噪声的包围盒，InnerCloudBound 是内侧云的包围盒
        float UnionBound = (GeometricThin * 1.0);

        if (abs(PosY) < UnionBound && PosR < OuterRadius && PosR > InterRadius)
        {


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
                 if(E_emit<0.0) FreqRatio =  0.0;




                 float DiskTemperature = pow(DiskTemperatureArgument * pow(1.0 / max(1e-6, PosR), 3.0) * max(1.0 - sqrt(InterRadius / max(1e-6, PosR)), 0.000001), 0.25);
                 float VisionTemperature = DiskTemperature * pow(FreqRatio, RedShiftColorExponent); 
                 float BrightWithoutRedshift = 1.0;
                 BrightWithoutRedshift *= pow(DiskTemperature / PeakTemperature, BlackbodyIntensityExponent); 
                 
                 float RotPosR = PosR + 0.25 / 3.0 * EmissionTime;
                 float Density = 1.0;
                 
                 vec4 SampleColor = vec4(0.0);

                 

                     SampleColor.xyz =vec3(BrightWithoutRedshift) ;
                     SampleColor.a=Density;
    
                 SampleColor.xyz *= vec3(0.0,0.0,1.0); 
                 SampleColor.xyz *= pow(FreqRatio, RedShiftIntensityExponent); 
                 

                 

                  SampleColor.xyz *= Brightmut;
                  SampleColor.a   *= Darkmut;

                  SampleColor.xyz*=pow(InterRadius/1.0,4.0);

                 bool IsPositiveEnergy =iE_obs>0.0;
                 if (!IsPositiveEnergy) 
                 {
                     float cMax = max(max(SampleColor.r, SampleColor.g), SampleColor.b);
                     float cMin = min(min(SampleColor.r, SampleColor.g), SampleColor.b);
                     SampleColor.rgb = vec3(cMax + cMin) - SampleColor.rgb;
                 }





                 vec4 StepColor = SampleColor * StepSize;
                 

                 CurrentResult = CurrentResult + StepColor * pow((1.0 - CurrentResult.a), 1.0);

            }
        }
        RayMarchPhase = 1.0;
    }
    
    return CurrentResult;
}

vec4 JetColor(vec4 BaseColor, vec4 RayPos, vec4 LastRayPos,
              vec4 iP_cov, vec4 lastiP_cov, float iE_obs,
              float InterRadius, float OuterRadius, float JetRedShiftIntensityExponent, float JetBrightmut, float JetReddening, float JetSaturation, float AccretionRate, float JetShiftMax, 
              float PhysicalSpinA, 
              float PhysicalQ, bool isoutgoing,
              inout float RayMarchPhase 
              ) 
{
    vec4 CurrentResult = BaseColor;
    vec3 StartPos = LastRayPos.xyz; 
    vec3 EndPos   = RayPos.xyz;
    
    if (any(isnan(StartPos)) || any(isinf(StartPos))) return BaseColor;

    // --- 1. 计算局域度规并提取固有空间距离 ---
    vec3 ChordDelta = EndPos - StartPos;
    vec3 MidPos = 0.5 * (StartPos + EndPos);
    KerrGeometry geo_mid;
    ComputeGeometryScalars(MidPos, PhysicalSpinA, PhysicalQ, 1.0, 1.0, isoutgoing, geo_mid);
    
    float l_dot_dx = dot(geo_mid.l_down.xyz, ChordDelta);
    float proper_dist = sqrt(max(1e-9, dot(ChordDelta, ChordDelta) + geo_mid.f * l_dot_dx * l_dot_dx));

    float StartTimeLag = LastRayPos.w;
    float EndTimeLag   = RayPos.w;

    float TotalDist = proper_dist; // 彻底替换旧的 StepLength
    float TraveledDist = 0.0;
    
    float R_Start = length(StartPos.xz);
    float R_End   = length(RayPos.xz); 
    float MaxR_XZ = max(R_Start, R_End);
    float MaxY    = max(abs(StartPos.y), abs(RayPos.y));
    
    if (MaxR_XZ > OuterRadius * 1.5 && MaxY < OuterRadius) return BaseColor;

    int SafetyLoopCount = 0;
    const int MaxLoops = 114514; 
    
    while (TraveledDist < TotalDist && SafetyLoopCount < MaxLoops)
    {
        if (CurrentResult.a > 0.99) break;
        SafetyLoopCount++;

        // 利用插值比例计算当前检查点位置
        vec3 CurrentPos = mix(StartPos, EndPos, clamp(TraveledDist / max(1e-9, TotalDist), 0.0, 1.0));
        float DistanceToBlackHole = length(CurrentPos); 
        
        float SmallStepBoundary = max(OuterRadius, 12.0);
        float StepSize = 1.0; 
        
        StepSize *= 0.15 + 0.25 * min(max(0.0, 0.5 * (0.5 * DistanceToBlackHole / max(10.0 , SmallStepBoundary) - 1.0)), 1.0);
        if ((DistanceToBlackHole) >= 2.0 * SmallStepBoundary) StepSize *= DistanceToBlackHole;
        else if ((DistanceToBlackHole) >= 1.0 * SmallStepBoundary) StepSize *= ((1.0 + 0.25 * max(DistanceToBlackHole - 12.0, 0.0)) * (2.0 * SmallStepBoundary - DistanceToBlackHole) + DistanceToBlackHole * (DistanceToBlackHole - SmallStepBoundary)) / SmallStepBoundary;
        else StepSize *= min(1.0 + 0.25 * max(DistanceToBlackHole - 12.0, 0.0), DistanceToBlackHole);
        
        StepSize = max(0.01, StepSize); 

        float DistToNextSample = RayMarchPhase * StepSize;
        float NextTarget = min(TotalDist, TraveledDist + DistToNextSample);

        bool shouldSample = false;
        if (NextTarget < TotalDist)
        {
            shouldSample = true;
            RayMarchPhase = 1.0;
            TraveledDist = NextTarget;
        }
        else
        {
            float DistanceTraveled = TotalDist - TraveledDist;
            RayMarchPhase -= DistanceTraveled / StepSize;
            if(RayMarchPhase < 0.0) RayMarchPhase = 0.0;
            TraveledDist = TotalDist;
        }

        if (shouldSample)
        {
            // --- 2. 在原始坐标系下进行物理量插值 ---
            float TimeInterpolant = min(1.0, TraveledDist / max(1e-9, TotalDist));
            vec3 OrigSamplePos = mix(StartPos, EndPos, TimeInterpolant);
            float CurrentRayTimeLag = mix(StartTimeLag, EndTimeLag, TimeInterpolant);
            
            vec4 Sample_X = vec4(OrigSamplePos, CurrentRayTimeLag);
            vec4 Sample_P_cov = mix(lastiP_cov, iP_cov, TimeInterpolant);

            // --- 3. 统一转换到 Ingoing 坐标系 ---
            if (isoutgoing) {
                transformKerrSchild_YSpin(Sample_X, 1.0, Sample_P_cov, 0.5, PhysicalSpinA, PhysicalQ, true);
            }
            
            vec3 SamplePos = Sample_X.xyz;
            float EmissionTime = iBlackHoleTime + Sample_X.w;

            float PosR = KerrSchildRadius(SamplePos, PhysicalSpinA, 1.0);
            float PosY = SamplePos.y;
            float RhoSq = dot(SamplePos.xz, SamplePos.xz);
            float Rho = sqrt(RhoSq);
            
            vec4 AccumColor = vec4(0.0);
            bool InJet = false;

            // 内部喷流核心
            if (RhoSq < 2.0 * InterRadius * InterRadius + 0.03 * 0.03 * PosY * PosY && PosR < sqrt(2.0) * OuterRadius)
            {
                InJet = true;
                float ShapeVal = 1.0 / sqrt(max(1e-9, InterRadius * InterRadius + 0.02 * 0.02 * PosY * PosY));
                float noiseInput = 0.3 * (EmissionTime - 1.0 / 0.8 * abs(abs(PosY) + 100.0 * (RhoSq / max(0.1, PosR)))) / max(1e-6, (OuterRadius / 100.0)) / (1.0 / 0.8);
                float a = mix(0.7 + 0.3 * PerlinNoise1D(noiseInput), 1.0, exp(-0.01 * 0.01 * PosY * PosY));
                
                vec4 Col = vec4(1.0, 1.0, 1.0, 0.5) * max(0.0, 1.0 - 5.0 * ShapeVal * abs(1.0 - pow(Rho * ShapeVal, 2.0))) * ShapeVal;
                Col *= a;
                Col *= max(0.0, 1.0 - 1.0 * exp(-0.0001 * PosY / max(1e-6, InterRadius) * PosY / max(1e-6, InterRadius)));
                Col *= exp(-4.0 / (2.0) * PosR / max(1e-6, OuterRadius) * PosR / max(1e-6, OuterRadius));
                Col *= 0.5;
                
                AccumColor += Col * StepSize; 
            }

            // 外围扭转喷流壳
            float Wid = abs(PosY);
            if (Rho < 1.3 * InterRadius + 0.25 * Wid && Rho > 0.7 * InterRadius + 0.15 * Wid && PosR < 30.0 * InterRadius)
            {
                InJet = true;
                float InnerTheta = 2.0 * GetKeplerianAngularVelocity(InterRadius, 1.0, PhysicalSpinA, PhysicalQ) * (EmissionTime - 1.0 / 0.8 * abs(PosY));
                float ShapeVal = 1.0 / max(1e-9, (InterRadius + 0.2 * Wid));
                
                float Twist = 0.2 * (1.1 - exp(-0.1 * 0.1 * PosY * PosY)) * (PerlinNoise1D(0.35 * (EmissionTime - 1.0 / 0.8 * abs(PosY)) / (1.0 / 0.8)) - 0.5);
                vec2 TwistedPos = SamplePos.xz + Twist * vec2(cos(0.666666 * InnerTheta), -sin(0.666666 * InnerTheta));
                
                vec4 Col = vec4(1.0, 1.0, 1.0, 0.5) * max(0.0, 1.0 - 2.0 * abs(1.0 - pow(length(TwistedPos) * ShapeVal, 2.0))) * ShapeVal;
                Col *= 1.0 - exp(-PosY / max(1e-6, InterRadius) * PosY / max(1e-6, InterRadius));
                Col *= exp(-0.005 * PosY / max(1e-6, InterRadius) * PosY / max(1e-6, InterRadius));
                Col *= 0.5;
                
                AccumColor += Col * StepSize; 
            }

            if (InJet)
            {
                // --- 4. 使用转换到 Ingoing 系后的物理计算 ---
                KerrGeometry geo_sample;
                // 注意：isoutgoing 必须填 false，因为我们已经在上文将其统一转换为了 Ingoing 坐标
                ComputeGeometryScalars(SamplePos, PhysicalSpinA, PhysicalQ, 1.0, 1.0, false, geo_sample);
                
                // 物理喷流速度 0.8c, 洛伦兹因子 Gamma = 1/sqrt(1 - 0.8^2) = 1.6666667
                float v_jet = 0.8;
                float Gamma = 1.6666667; 
                vec3 U_spatial = vec3(0.0, sign(PosY) * Gamma * v_jet, 0.0);
                
                // 根据归一化条件 g_uv U^u U^v = -1 求解时间分量 U^t
                float l_dot_u_sp = dot(geo_sample.l_down.xyz, U_spatial);
                float U_sp_sq = dot(U_spatial, U_spatial);
                
                float A = -1.0 + geo_sample.f;
                float B = 2.0 * geo_sample.f * l_dot_u_sp;
                float C = U_sp_sq + geo_sample.f * l_dot_u_sp * l_dot_u_sp + 1.0; 
                
                float Det = B * B - 4.0 * A * C;
                
                // 光锥约束保护
                if (Det < 0.0) {
                    if (A < 0.0) {
                        U_spatial *= 0.5; 
                    } else {
                        U_spatial = -1.5 * geo_sample.grad_r; 
                    }
                    l_dot_u_sp = dot(geo_sample.l_down.xyz, U_spatial);
                    U_sp_sq = dot(U_spatial, U_spatial);
                    C = U_sp_sq + geo_sample.f * l_dot_u_sp * l_dot_u_sp + 1.0;
                    B = 2.0 * geo_sample.f * l_dot_u_sp;
                    Det = max(0.0, B * B - 4.0 * A * C);
                }
                
                float sqrtDet = sqrt(Det);
                float Ut;
                if (abs(A) < 1e-7) {
                    Ut = -C / max(1e-19, B); 
                } else {
                    if (B < 0.0) {
                         Ut = 2.0 * C / (-B + sqrtDet);
                    } else {
                         Ut = (-B - sqrtDet) / (2.0 * A);
                    }
                }
                
                vec4 U_jet = vec4(U_spatial, Ut);
                
                // 严谨计算红移/蓝移 E = - p_u U^u
                // 使用刚刚插值并经过了换系处理的局域四动量 Sample_P_cov
                float E_emit = -dot(Sample_P_cov, U_jet);
                float FreqRatio = 1.0 / max(1e-6, E_emit);

                float JetTemperature = min(100000.0 * FreqRatio,100000.0); 
                AccumColor.xyz *= KelvinToRgb(JetTemperature);
                AccumColor.xyz *= min(pow(FreqRatio, JetRedShiftIntensityExponent), JetShiftMax);
                AccumColor *= JetBrightmut * (0.5 + 0.5 * tanh(log(max(1e-6, AccretionRate)) + 1.0));
                AccumColor.a *= 0.0; 
                
                bool IsPositiveEnergy = E_emit > 0.0;
                if (!IsPositiveEnergy) 
                {
                    float cMax = max(max(AccumColor.r, AccumColor.g), AccumColor.b);
                    float cMin = min(min(AccumColor.r, AccumColor.g), AccumColor.b);
                    AccumColor.rgb = vec3(cMax + cMin) - AccumColor.rgb;
                }

                // ... (颜色累加不变)
                float aR = 1.0 + JetReddening * (1.0 - 1.0);
                float aG = 1.0 + JetReddening * (3.0 - 1.0);
                float aB = 1.0 + JetReddening * (6.0 - 1.0);
                float Sum_rgb = (AccumColor.r + AccumColor.g + AccumColor.b) * pow(1.0 - CurrentResult.a, aG);
                
                float Denominator = AccumColor.r * pow(1.0 - CurrentResult.a, aR) + AccumColor.g * pow(1.0 - CurrentResult.a, aG) + AccumColor.b * pow(1.0 - CurrentResult.a, aB);
                float r001 = 0.0; float g001 = 0.0; float b001 = 0.0;
                if (Denominator > 0.000001)
                {
                    r001 = Sum_rgb * AccumColor.r * pow(1.0 - CurrentResult.a, aR) / Denominator;
                    g001 = Sum_rgb * AccumColor.g * pow(1.0 - CurrentResult.a, aG) / Denominator;
                    b001 = Sum_rgb * AccumColor.b * pow(1.0 - CurrentResult.a, aB) / Denominator;
                    
                    r001 *= pow(3.0 * r001 / (r001 + g001 + b001), JetSaturation);
                    g001 *= pow(3.0 * g001 / (r001 + g001 + b001), JetSaturation);
                    b001 *= pow(3.0 * b001 / (r001 + g001 + b001), JetSaturation);
                }
                
                CurrentResult.r += r001;
                CurrentResult.g += g001;
                CurrentResult.b += b001;
                CurrentResult.a += AccumColor.a * (1.0 - CurrentResult.a);
            }
        }
    }
    return CurrentResult;
}

vec4 ImageDiskColor(vec4 BaseColor, vec4 RayPos, vec4 LastRayPos,
                    vec4 P_cov, vec4 LastP_cov, 
                    float PhysicalSpinA, float PhysicalQ, bool isoutgoing,
                    float EndStepSign, float dlambda,
                    float InterRadius, float OuterRadius,
                    float RedShiftColorExponent, float RedShiftIntensityExponent)
{
    vec4 CurrentResult = BaseColor;
    // 如果不透明度已满，直接返回
    if (CurrentResult.a > 0.99) return CurrentResult;

    // 仅当光线穿过赤道面 (y=0) 时触发运算
    if (LastRayPos.y * RayPos.y >= 0.0) return CurrentResult;

    float StartStepSign = EndStepSign;
    float t_cross = -1.0;
    vec4 DiskHitX = vec4(0.0);
    vec3 DiskHitPos = vec3(0.0);

    // 获取起点和终点的几何信息并升指标，求出坐标对仿射参量的导数 dX/dlambda
    KerrGeometry geo_last;
    ComputeGeometryScalars(LastRayPos.xyz, PhysicalSpinA, PhysicalQ, 1.0, StartStepSign, isoutgoing, geo_last);
    vec4 V0 = RaiseIndex(LastP_cov, geo_last); 
    vec4 T0 = V0 * dlambda; 

    KerrGeometry geo_curr;
    ComputeGeometryScalars(RayPos.xyz, PhysicalSpinA, PhysicalQ, 1.0, EndStepSign, isoutgoing, geo_curr);
    vec4 V1 = RaiseIndex(P_cov, geo_curr);     
    vec4 T1 = V1 * dlambda; 

    // --- 赤道盘相交：三次 Hermite 曲线求根 ---
    float denom = (LastRayPos.y - RayPos.y);
    if(abs(denom) > 1e-9) {
        t_cross = LastRayPos.y / denom; // 线性初猜
        
        // 牛顿迭代求精确零点
        for(int iter = 0; iter < 3; iter++) {
            float t2 = t_cross * t_cross;
            float t3 = t2 * t_cross;
            
            float h00 = 2.0*t3 - 3.0*t2 + 1.0;
            float h10 = t3 - 2.0*t2 + t_cross;
            float h01 = -2.0*t3 + 3.0*t2;
            float h11 = t3 - t2;
            float yt = h00*LastRayPos.y + h10*T0.y + h01*RayPos.y + h11*T1.y;
            
            float dh00 = 6.0*t2 - 6.0*t_cross;
            float dh10 = 3.0*t2 - 4.0*t_cross + 1.0;
            float dh01 = -6.0*t2 + 6.0*t_cross;
            float dh11 = 3.0*t2 - 2.0*t_cross;
            float dyt = dh00*LastRayPos.y + dh10*T0.y + dh01*RayPos.y + dh11*T1.y;
            
            t_cross -= yt / (dyt + 1e-12);
        }
        t_cross = clamp(t_cross, 0.0, 1.0);
        
        // 依据精确 t 计算交点四维坐标
        float t2 = t_cross * t_cross;
        float t3 = t2 * t_cross;
        vec4 H = vec4(2.0*t3 - 3.0*t2 + 1.0, t3 - 2.0*t2 + t_cross, -2.0*t3 + 3.0*t2, t3 - t2);
        DiskHitX = H.x*LastRayPos + H.y*T0 + H.z*RayPos + H.w*T1;
        DiskHitPos = DiskHitX.xyz;
        
        if (length(DiskHitPos.xz) < abs(PhysicalSpinA)) {
            StartStepSign = -EndStepSign;
        }
    } else {
        return CurrentResult;
    }

    // --- 将坐标映射回统一的 Ingoing 参考系 ---
    float HitTime_disk = DiskHitX.w;
    vec3 PatternPosDisk = DiskHitPos;
    if (isoutgoing) {
        vec4 tempX = vec4(DiskHitPos, HitTime_disk);
        vec4 dummyP = vec4(0.0);
        float diskSign = (length(DiskHitPos.xz) < abs(PhysicalSpinA)) ? -StartStepSign : StartStepSign;
        transformKerrSchild_YSpin(tempX, diskSign, dummyP, 0.5, PhysicalSpinA, PhysicalQ, true);
        PatternPosDisk = tempX.xyz;
    }

    // --- 几何与纹理映射逻辑 ---
    float r_xz = length(PatternPosDisk.xz);
    // 挖去内部孔洞
    if (r_xz < InterRadius) return CurrentResult;

    // 根据对角线为 OuterRadius 计算正方形边长
    // 对角线 D = OuterRadius，正方形边长 S = D / sqrt(2)
    float ImageWidth = OuterRadius * 0.70710678; 
    float HalfWidth = ImageWidth * 0.5;

    // 剔除正方形边界之外的区域
    if (abs(PatternPosDisk.x) > HalfWidth || abs(PatternPosDisk.z) > HalfWidth) return CurrentResult;

    // 映射到 [0, 1] 的 UV 坐标
    float U = (PatternPosDisk.x + HalfWidth) / ImageWidth;
    float V = (PatternPosDisk.z + HalfWidth) / ImageWidth;

    // 使用 textureLod 避免由于控制流分支导致计算 mipmap 梯度报错
    vec4 TexColor = textureLod(iImageTexture, vec2(U, V), 0.0);
    TexColor.xyz*=iBrightmut; 
    TexColor.a*=iDarkmut;   
    if (TexColor.a < 0.01) return CurrentResult; // 纯透明部分跳过

    // --- 提取四维动量用于频移计算 ---
    vec4 HitP_cov = mix(LastP_cov, P_cov, t_cross);

    // 计算静止物体 (U_spatial = 0) 局部能量 E_emit
    vec4 U_zero = vec4(0.0, 0.0, 0.0, 1.0); 
    KerrGeometry geo_hit;
    float hitSign = (length(DiskHitPos.xz) < abs(PhysicalSpinA)) ? -StartStepSign : StartStepSign;
    ComputeGeometryScalars(DiskHitPos, PhysicalSpinA, PhysicalQ, 1.0, hitSign, isoutgoing, geo_hit);

    vec4 U_lower = LowerIndex(U_zero, geo_hit);
    float norm_sq = dot(U_zero, U_lower);
    float norm = sqrt(max(1e-9, abs(norm_sq)));
    vec4 U_static = U_zero / norm;

    // 发射能量 = - P_mu * U^mu
    float E_emit = -dot(HitP_cov, U_static); 
    float Shift = 1.0 / max(1e-6, abs(E_emit)); 

    // --- 非黑体天空盒风格频移逻辑 ---
    float EffectiveColorShift = pow(Shift, RedShiftColorExponent);

    vec3 Rcolor = TexColor.r * 1.0 * WavelengthToRgb(max(453.0, 645.0 / EffectiveColorShift));
    vec3 Gcolor = TexColor.g * 1.5 * WavelengthToRgb(max(416.0, 510.0 / EffectiveColorShift));
    vec3 Bcolor = TexColor.b * 0.6 * WavelengthToRgb(max(380.0, 440.0 / EffectiveColorShift));
    vec3 Scolor = Rcolor + Gcolor + Bcolor;

    float OStrength = 0.3 * TexColor.r + 0.6 * TexColor.g + 0.1 * TexColor.b;
    float RStrength = 0.3 * Scolor.r + 0.6 * Scolor.g + 0.1 * Scolor.b;
    Scolor *= OStrength / max(RStrength, 0.001);

    // 亮度频移乘数
    Scolor *= pow(Shift, RedShiftIntensityExponent);

    if (E_emit < 0.0) {
        float cMax = max(max(Scolor.r, Scolor.g), Scolor.b);
        float cMin = min(min(Scolor.r, Scolor.g), Scolor.b);
        Scolor.rgb = vec3(cMax + cMin) - Scolor.rgb;
        if (iWhitehole == 0) {
            Scolor.rgb = vec3(0.0);
            TexColor.a = 0.0;
        }
    }

    // 混合到当前累计颜色
    CurrentResult.rgb += Scolor * TexColor.a * (1.0 - CurrentResult.a);
    CurrentResult.a   += TexColor.a * (1.0 - CurrentResult.a);

    return CurrentResult;
}
// 完美复刻原版逻辑的 3阶分形布朗运动 (3 Octaves FBM + Domain Warping)
float Fbm_Standalone(vec3 x) {
    vec2 sum = vec2(0.0);
    float ampl = 1.0;
    for (int i = 0; i < 3; ++i) {
        sum += ampl * PerlinNoise(x);
        // 原版的域扭曲(Domain Warping)逻辑，增加流体感
        x += 1.4 * sum.xyx * vec3(0.7, 0.6, 1.3);
        ampl *= 0.74;
        x *= vec3(4.0, 4.0, 4.0);
    }
    return sum.x;
}
vec4 DensestarColor(vec4 BaseColor, vec4 RayPos, vec4 LastRayPos,
                    vec4 P_cov, vec4 LastP_cov, 
                    float PhysicalSpinA, float PhysicalQ, bool isoutgoing,
                    float EndStepSign, float dlambda)
{
    vec4 CurrentResult = BaseColor;
    // 如果不透明度已满，或者未启用致密星渲染，则直接返回
    if (iDensestarsurfaceR == 0.0) return CurrentResult;

    // 获取起点和终点的几何信息并升指标，求出坐标对仿射参量的导数 dX/dlambda
    KerrGeometry geo_last;
    ComputeGeometryScalars(LastRayPos.xyz, PhysicalSpinA, PhysicalQ, 1.0, EndStepSign, isoutgoing, geo_last);
    vec4 V0 = RaiseIndex(LastP_cov, geo_last); 
    vec4 T0 = V0 * dlambda; 

    KerrGeometry geo_curr;
    ComputeGeometryScalars(RayPos.xyz, PhysicalSpinA, PhysicalQ, 1.0, EndStepSign, isoutgoing, geo_curr);
    vec4 V1 = RaiseIndex(P_cov, geo_curr);     
    vec4 T1 = V1 * dlambda; 

    float TargetSignedR = iDensestarsurfaceR; 
    float TargetGeoR = abs(TargetSignedR); 
    
    vec3 O = LastRayPos.xyz;
    vec3 D_vec = RayPos.xyz - LastRayPos.xyz;

    // 椭球面求交
    vec2 roots = IntersectKerrEllipsoid(O, D_vec, TargetGeoR, PhysicalSpinA);
    
    float t_hits[2];
    t_hits[0] = roots.x;
    t_hits[1] = roots.y;
    // 确保按射线前进方向排序
    if (t_hits[0] > t_hits[1]) {
        float temp = t_hits[0]; t_hits[0] = t_hits[1]; t_hits[1] = temp;
    }
    
    for (int j = 0; j < 2; j++) {
        float t = t_hits[j];
        
        if (t >= 0.0 && t <= 1.0) {
            float HitPointSign = EndStepSign;
            if (HitPointSign * TargetSignedR < 0.0) continue;

            // 曲线与椭球面相交的牛顿迭代修整
            for(int iter = 0; iter < 2; iter++) {
                float t2 = t*t; float t3 = t2*t;
                vec4 H = vec4(2.0*t3 - 3.0*t2 + 1.0, t3 - 2.0*t2 + t, -2.0*t3 + 3.0*t2, t3 - t2);
                vec3 pos = (H.x*LastRayPos + H.y*T0 + H.z*RayPos + H.w*T1).xyz;
                float curR = KerrSchildRadius(pos, PhysicalSpinA, HitPointSign);
                
                float dt = 0.001;
                float nt = t + dt;
                float nt2 = nt*nt; float nt3 = nt2*nt;
                vec4 nH = vec4(2.0*nt3 - 3.0*nt2 + 1.0, nt3 - 2.0*nt2 + nt, -2.0*nt3 + 3.0*nt2, nt3 - nt2);
                vec3 npos = (nH.x*LastRayPos + nH.y*T0 + nH.z*RayPos + nH.w*T1).xyz;
                float nextR = KerrSchildRadius(npos, PhysicalSpinA, HitPointSign);
                
                float dr_dt = (nextR - curR) / dt;
                t -= (curR - TargetSignedR) / (dr_dt + 1e-12);
            }
            
            if (t < 0.0 || t > 1.0) continue; 
            
            // 提取高精度四维坐标与动量
            float t2 = t*t; float t3 = t2*t;
            vec4 H = vec4(2.0*t3 - 3.0*t2 + 1.0, t3 - 2.0*t2 + t, -2.0*t3 + 3.0*t2, t3 - t2);
            vec4 HitX = H.x*LastRayPos + H.y*T0 + H.z*RayPos + H.w*T1;
            
            vec3 HitPos = HitX.xyz;
            float HitTime = HitX.w;
            
            float CheckR = KerrSchildRadius(HitPos, PhysicalSpinA, HitPointSign);
            if (abs(CheckR - TargetSignedR) > 0.1 * TargetGeoR + 0.1) continue; 

            vec4 HitP_cov = mix(LastP_cov, P_cov, t);

            // 映射参考系以提取局部坐标
            vec3 PatternPos = HitPos;
            float PatternTime = HitTime;
            if (isoutgoing) {
                vec4 tempX = vec4(HitPos, HitTime);
                vec4 dummyP = vec4(0.0);
                transformKerrSchild_YSpin(tempX, HitPointSign, dummyP, 0.5, PhysicalSpinA, PhysicalQ, true);
                PatternPos = tempX.xyz;
                PatternTime = tempX.w;
            }

            // --- 【核心修改：刚体角速度与四维速度】 ---
            float R2 = TargetSignedR * TargetSignedR;
            float a2 = PhysicalSpinA * PhysicalSpinA;
            float Omega_star = PhysicalSpinA / (R2 + a2); // 星体指定等角速度刚体旋转

            float EmissionTime = iBlackHoleTime + PatternTime;
            
            // =================================================================
            // [MOD移植] 物理模式下的中子星 3D 噪声表面纹理生成
            // =================================================================
            
            // 1. 获取归一化的表面坐标
            vec3 pos_tex = normalize(PatternPos);
            
            // 2. 将 3D 坐标随刚体自旋绕 Y 轴旋转
            float rotAngle = Omega_star * EmissionTime;
            float c_rot = cos(rotAngle);
            float s_rot = sin(rotAngle);
            pos_tex.xz = mat2(c_rot, s_rot, -s_rot, c_rot) * pos_tex.xz;
            
            // 3. 应用中子星的纹理缩放倍数
            pos_tex *= 4.0;
            
            // 4. 表面等离子体沸腾/流动动画 (利用 EmissionTime 替代原版的 starTime)
            // 原版: sin(starTime / 128.0 * 3.14) * 50.0
            // 这里我们调整一下时间缩放比例，保证视觉流动速度适当
            float animSpeed = EmissionTime * 0.01; 
            
            vec3 noisePos = vec3(
                pos_tex.x + sin(animSpeed) * 2.0,
                pos_tex.y + cos(animSpeed) * 2.0, 
                pos_tex.z
            );
            
            // 5. 采样独立实现的 3D FBM 噪声
            float noiseVal = Fbm_Standalone(noisePos);
            
            // 6. 将噪声映射为温度调制系数 (0.8 ~ 1.0 波动区间，完美复刻 MOD)
            float tempMod = clamp(noiseVal * 0.2 + 0.8,0.5,1.5);
            
            // 7. 映射到发光体的物理静止系温度 (基准温度乘以调制系数)
            // 你可以把 6000.0 替换为你的恒星基础温度变量（如果外部有传如 iDensestarTemp 等）
            float BaseTemp_Kelvin = 6000.0 * tempMod; 
            
            // =================================================================

            // --- 【核心修改：频移系数 g 计算】 ---
            // 星体刚体旋转的局部四维速度 U_star
            vec3 VelSpatial = Omega_star * vec3(HitPos.z, 0.0, -HitPos.x);
            vec4 U_star_unnorm = vec4(VelSpatial, 1.0); 
            
            KerrGeometry geo_hit;
            ComputeGeometryScalars(HitPos, PhysicalSpinA, PhysicalQ, 1.0, HitPointSign, isoutgoing, geo_hit);
            vec4 U_star_lower = LowerIndex(U_star_unnorm, geo_hit);
            float norm_sq = dot(U_star_unnorm, U_star_lower);
            vec4 U_star = U_star_unnorm / sqrt(max(1e-9, abs(norm_sq)));

            // 发射能量与无穷远观测能量 (P_cov 的 w 分量为 -E_obs 守恒量)
            float E_emit_raw = -dot(HitP_cov, U_star);
            float g = 1.0 / max(1e-9, abs(E_emit_raw)); 


            // --- 【核心修改：计算频移后的温度与亮度】 ---
            // 观测温度 = 静止温度 * (g ^ 频移温度指数)
            float ObsTemp_Kelvin = BaseTemp_Kelvin * pow(g, iDensestarRedShiftColorExponent);
            
            // 获取对应色温的黑体颜色
            vec3 BlackBodyColor = KelvinToRgb(ObsTemp_Kelvin);

            // 观测亮度 = (自身静止温度导致的热辐射亮度基数) * (g ^ 频移亮度指数)
            float BaseIntensity = pow(BaseTemp_Kelvin / 6000.0, iDensestarBlackbodyIntensityExponent); 
            float RedshiftIntensity = pow(g, iDensestarRedShiftIntensityExponent);
            float FinalIntensity = BaseIntensity * RedshiftIntensity;

            vec4 StarCol = vec4(iDensestarBrightmut * BlackBodyColor * FinalIntensity, 1.0);
            
            // 处理负能量光线 / 反宇宙反色
            if (E_emit_raw < 0.0) {
                float cMax = max(max(StarCol.r, StarCol.g), StarCol.b);
                float cMin = min(min(StarCol.r, StarCol.g), StarCol.b);
                StarCol.rgb = vec3(cMax + cMin) - StarCol.rgb;
                if (iWhitehole == 0) StarCol.rgba = vec4(0.0);
            }
            
            // 将实体完全覆盖上去并退出循环
            CurrentResult.rgb += StarCol.rgb * StarCol.a * (1.0 - CurrentResult.a);
            CurrentResult.a   += StarCol.a * (1.0 - CurrentResult.a);
            
            if(CurrentResult.a > 0.99) break; 
        }
    }

    return CurrentResult;
}
// =====================================================================
// 辅助函数 1：获取在 Ingoing 坐标下，沿零矢量内落的粒子空间坐标
// M = 0.5 (rs = 1.0)，Y轴为自旋轴，沿赤道面内落
// =====================================================================
vec3 GetIngoingNullParticlePos(float time, float a) {
    // 设定周期，让光点不断生成并下落，方便持续观察
    float period = 15.0; 
    float t = mod(time, period);
    
    // 初始半径 r0 设为 10.0 rs。沿 Ingoing 主要零矢量下落，满足 dr/dt = -1
    float r0 = 10.0;
    float r = r0 - t; 
    
    // 固定坐标角 (赤道面 theta = pi/2, phi = 0)
    float theta = 1.57079632679; 
    float phi = 0.0; 
    
    float sinTh = sin(theta);
    float cosTh = cos(theta);
    float sinPh = sin(phi);
    float cosPh = cos(phi);
    
    // Kerr-Schild 笛卡尔坐标系映射 (Y-Spin)
    vec3 pos;
    pos.x = (r * cosPh + a * sinPh) * sinTh;
    pos.y = r * cosTh;
    pos.z = (r * sinPh - a * cosPh) * sinTh;
    
    return pos;
}

// =====================================================================
// 辅助函数 2：对光线参数 tau 进行插值、换系，并返回与光点的距离平方
// =====================================================================
float GetDotDistSq(float tau, vec4 LastRayPos, vec4 RayPos, vec4 T0, vec4 T1, 
                   float signR, float a, float Q, bool isoutgoing, out vec4 exactX_in) 
{
    // 1. 在入参所在系（原生系）进行 Hermite 插值
    float t2 = tau * tau; 
    float t3 = t2 * tau;
    vec4 H = vec4(2.0*t3 - 3.0*t2 + 1.0, t3 - 2.0*t2 + tau, -2.0*t3 + 3.0*t2, t3 - t2);
    vec4 X_nat = H.x * LastRayPos + H.y * T0 + H.z * RayPos + H.w * T1;
    
    exactX_in = X_nat;
    
    // 2. 坐标系转换：如果当前是 Outgoing 系，则转换为 Ingoing 系
    // 参考你原代码的写法，传入 true 执行转换
    if (isoutgoing) {
        vec4 dummyP = vec4(0.0);
        transformKerrSchild_YSpin(exactX_in, signR, dummyP, 0.5, a, Q, true);
    }
    
    // 3. 提取 Ingoing 系的全局时间
    float exactTime = iBlackHoleTime + exactX_in.w;
    
    // 4. 获取对应的光点物理位置
    vec3 dotP = GetIngoingNullParticlePos(exactTime, a);
    
    // 5. 返回欧氏距离平方
    vec3 diff = exactX_in.xyz - dotP;
    return dot(diff, diff);
}

// =====================================================================
// 主函数：绘制沿着零矢量内落的白色光点
// =====================================================================
vec4 DrawFallingWhiteDot(vec4 BaseColor, vec4 RayPos, vec4 LastRayPos,
                         vec4 P_cov, vec4 LastP_cov, 
                         float PhysicalSpinA, float PhysicalQ, bool isoutgoing,
                         float EndStepSign, float dlambda)
{
    vec4 CurrentResult = BaseColor;
    if (CurrentResult.a > 0.99) return CurrentResult;

    // 1. 获取起点和终点的几何信息并升指标，求出 dX/dlambda
    KerrGeometry geo_last;
    ComputeGeometryScalars(LastRayPos.xyz, PhysicalSpinA, PhysicalQ, 1.0, EndStepSign, isoutgoing, geo_last);
    vec4 V0 = RaiseIndex(LastP_cov, geo_last); 
    vec4 T0 = V0 * dlambda; 

    KerrGeometry geo_curr;
    ComputeGeometryScalars(RayPos.xyz, PhysicalSpinA, PhysicalQ, 1.0, EndStepSign, isoutgoing, geo_curr);
    vec4 V1 = RaiseIndex(P_cov, geo_curr);     
    vec4 T1 = V1 * dlambda; 
    
    // 2. 为了防止大步长直接穿透 0.02 rs 的球体，取 tau = 0.0, 0.5, 1.0 采样
    vec4 dummyX;
    float d0  = GetDotDistSq(0.0, LastRayPos, RayPos, T0, T1, EndStepSign, PhysicalSpinA, PhysicalQ, isoutgoing, dummyX);
    float d05 = GetDotDistSq(0.5, LastRayPos, RayPos, T0, T1, EndStepSign, PhysicalSpinA, PhysicalQ, isoutgoing, dummyX);
    float d1  = GetDotDistSq(1.0, LastRayPos, RayPos, T0, T1, EndStepSign, PhysicalSpinA, PhysicalQ, isoutgoing, dummyX);
    
    // 3. 抛物线拟合 P(tau) = A*tau^2 + B*tau + C，寻找最近点参数 tau_min
    float A = 2.0 * d1 + 2.0 * d0 - 4.0 * d05;
    float B = 4.0 * d05 - d1 - 3.0 * d0;
    
    float tau_min = 0.5;
    if (abs(A) > 1e-9) {
        tau_min = clamp(-B / (2.0 * A), 0.0, 1.0);
    } else {
        tau_min = (d0 < d1) ? 0.0 : 1.0;
    }
    
    // 4. 精确计算最近点处的距离
    float distSq_min = GetDotDistSq(tau_min, LastRayPos, RayPos, T0, T1, EndStepSign, PhysicalSpinA, PhysicalQ, isoutgoing, dummyX);
    
    // 保险机制：防止拟合在极端扭曲下失效，确保拿到的确实是最小值
    float min_d = distSq_min;
    if(d0  < min_d) min_d = d0;
    if(d1  < min_d) min_d = d1;
    if(d05 < min_d) min_d = d05;
    
    float exactDist = sqrt(max(0.0, min_d));
    
    // 设定目标范围为 0.02 rs
    float threshold = 0.1;

    // 5. 如果光线确实掠过了该范围，则进行染色
    if (exactDist < threshold) {
        // 计算这一步跨越的空间真实长度
        float stepLength = length(RayPos.xyz - LastRayPos.xyz);
        
        // 计算光线在目标球体内截取的弦长
        float chordLength = 2.0 * sqrt(max(0.0, threshold * threshold - exactDist * exactDist));
        
        // 取步长与弦长的较小值，保证渲染不依赖于光追步长的大小 (防闪烁/过爆核心机制)
        float effectiveLength = min(stepLength, chordLength);
        
        // 外部柔和光晕
        float intensity = smoothstep(threshold, 0.0, exactDist);
        // 内部高亮核心
        float core = exp(-(exactDist * exactDist) / (0.005 * 0.005));
        
        // 亮度乘子，根据视觉反馈微调
        float density = intensity * 200.0 + core * 800.0;
        
        // 根据穿过的有效长度计算不透明度贡献
        float dotAlpha = clamp(density * effectiveLength, 0.0, 1.0);
        
        // 叠加白光
        vec3 whiteColor = vec3(1.0, 1.0, 1.0);
        CurrentResult.rgb += whiteColor * dotAlpha * (1.0 - CurrentResult.a);
        CurrentResult.a   += dotAlpha * (1.0 - CurrentResult.a);
    }

    return CurrentResult;
}
vec4 GridColorSimple(vec4 BaseColor, vec4 RayPos, vec4 LastRayPos,
               vec4 P_cov, vec4 LastP_cov, 
               float PhysicalSpinA, float PhysicalQ, bool isoutgoing,
               float EndStepSign, float dlambda,bool showInnerGrid)
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
    vec4 DiskHitX = vec4(0.0); // 新增：用于记录赤道盘相交时的四维坐标信息

    // --- 【修改 2：计算端点坐标速度与 Hermite 曲线切线】 ---
    // 获取起点和终点的几何信息并升指标，求出坐标对仿射参量的导数 dX/dlambda
    KerrGeometry geo_last;
    ComputeGeometryScalars(LastRayPos.xyz, PhysicalSpinA, PhysicalQ, 1.0, StartStepSign, isoutgoing, geo_last);
    vec4 V0 = RaiseIndex(LastP_cov, geo_last); 
    vec4 T0 = V0 * dlambda; // 转换为对插值参数 t(0~1) 的导数

    KerrGeometry geo_curr;
    ComputeGeometryScalars(RayPos.xyz, PhysicalSpinA, PhysicalQ, 1.0, EndStepSign, isoutgoing, geo_curr);
    vec4 V1 = RaiseIndex(P_cov, geo_curr);     
    vec4 T1 = V1 * dlambda; 

// --- 【修改 3：赤道盘相交改为三次曲线求根】 ---
    if (LastRayPos.y * RayPos.y < 0.0) {
        float denom = (LastRayPos.y - RayPos.y);
        if(abs(denom) > 1e-9) {
            t_cross = LastRayPos.y / denom; // 线性初猜
            
            // 牛顿迭代求三次 Hermite 曲线在 y 轴的精确零点
            for(int iter = 0; iter < 3; iter++) {
                float t2 = t_cross * t_cross;
                float t3 = t2 * t_cross;
                
                // 基函数
                float h00 = 2.0*t3 - 3.0*t2 + 1.0;
                float h10 = t3 - 2.0*t2 + t_cross;
                float h01 = -2.0*t3 + 3.0*t2;
                float h11 = t3 - t2;
                float yt = h00*LastRayPos.y + h10*T0.y + h01*RayPos.y + h11*T1.y;
                
                // 导数
                float dh00 = 6.0*t2 - 6.0*t_cross;
                float dh10 = 3.0*t2 - 4.0*t_cross + 1.0;
                float dh01 = -6.0*t2 + 6.0*t_cross;
                float dh11 = 3.0*t2 - 2.0*t_cross;
                float dyt = dh00*LastRayPos.y + dh10*T0.y + dh01*RayPos.y + dh11*T1.y;
                
                t_cross -= yt / (dyt + 1e-12);
            }
            t_cross = clamp(t_cross, 0.0, 1.0);
            
            // 依据精确 t 计算交点四维坐标
            float t2 = t_cross * t_cross;
            float t3 = t2 * t_cross;
            vec4 H = vec4(2.0*t3 - 3.0*t2 + 1.0, t3 - 2.0*t2 + t_cross, -2.0*t3 + 3.0*t2, t3 - t2);
            DiskHitX = H.x*LastRayPos + H.y*T0 + H.z*RayPos + H.w*T1;
            DiskHitPos = DiskHitX.xyz;
            
            if (length(DiskHitPos.xz) < abs(PhysicalSpinA)) {
                StartStepSign = -EndStepSign;
                bHasCrossed = true;
            }
        }
    }
    // ---------------------------------------------

    bool CheckPositive = (StartStepSign > 0.0) || (EndStepSign > 0.0);
    bool CheckNegative = (StartStepSign < 0.0) || (EndStepSign < 0.0);

    float HorizonDiscrim = 0.25 - PhysicalSpinA * PhysicalSpinA - PhysicalQ * PhysicalQ;
    float RH_Outer = 0.5 + sqrt(max(0.0, HorizonDiscrim));
    float RH_Inner = 0.5 - sqrt(max(0.0, HorizonDiscrim));
    bool HasHorizon = HorizonDiscrim >= 0.0;

    if (CheckPositive) {
        SignedGridRadii[GridCount] = 70.0;
        GridColors[GridCount] = 0.3*vec3(0.0, 1.0, 1.0); 
        GridCount++;

        if (HasHorizon) {
            SignedGridRadii[GridCount] = RH_Outer * 1.06; 
            GridColors[GridCount] = 0.3*vec3(0.0, 1.0, 0.0); 
            GridCount++;
            if(showInnerGrid)
            {
                SignedGridRadii[GridCount] = RH_Inner * 0.94; 
                GridColors[GridCount] =0.3* vec3(1.0, 0.0, 0.0); 
                GridCount++;
            }
        }
    }
    
    if (CheckNegative) {
        SignedGridRadii[GridCount] = -70.0;  
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

                // --- 【修改 4：曲线与椭球网格面相交的牛顿迭代修整】 ---
                for(int iter = 0; iter < 2; iter++) {
                    float t2 = t*t; float t3 = t2*t;
                    vec4 H = vec4(2.0*t3 - 3.0*t2 + 1.0, t3 - 2.0*t2 + t, -2.0*t3 + 3.0*t2, t3 - t2);
                    vec3 pos = (H.x*LastRayPos + H.y*T0 + H.z*RayPos + H.w*T1).xyz;
                    float curR = KerrSchildRadius(pos, PhysicalSpinA, HitPointSign);
                    
                    // 数值导数
                    float dt = 0.001;
                    float nt = t + dt;
                    float nt2 = nt*nt; float nt3 = nt2*nt;
                    vec4 nH = vec4(2.0*nt3 - 3.0*nt2 + 1.0, nt3 - 2.0*nt2 + nt, -2.0*nt3 + 3.0*nt2, nt3 - nt2);
                    vec3 npos = (nH.x*LastRayPos + nH.y*T0 + nH.z*RayPos + nH.w*T1).xyz;
                    float nextR = KerrSchildRadius(npos, PhysicalSpinA, HitPointSign);
                    
                    float dr_dt = (nextR - curR) / dt;
                    t -= (curR - TargetSignedR) / (dr_dt + 1e-12);
                }
                
                if (t < 0.0 || t > 1.0) continue; 
                
                // 依据精确 t 提取四维坐标
                float t2 = t*t; float t3 = t2*t;
                vec4 H = vec4(2.0*t3 - 3.0*t2 + 1.0, t3 - 2.0*t2 + t, -2.0*t3 + 3.0*t2, t3 - t2);
                vec4 HitX = H.x*LastRayPos + H.y*T0 + H.z*RayPos + H.w*T1;
                
                vec3 HitPos = HitX.xyz;
                float HitTime = HitX.w;
                // ----------------------------------------------------
                
                float CheckR = KerrSchildRadius(HitPos, PhysicalSpinA, HitPointSign);
                if (abs(CheckR - TargetSignedR) > 0.1 * TargetGeoR + 0.1) continue; 

                // 动量在此保持线性插值即可（已足够准确用于后续投影运算）
                vec4 HitP_cov = mix(LastP_cov, P_cov, t);
                // --- 几何图案部分：将坐标映射回统一的 Ingoing 参考系 ---
                vec3 PatternPos = HitPos;
                float PatternTime = HitTime;
                if (isoutgoing) {
                    vec4 tempX = vec4(HitPos, HitTime);
                    vec4 dummyP = vec4(0.0);
                    transformKerrSchild_YSpin(tempX, HitPointSign, dummyP, 0.5, PhysicalSpinA, PhysicalQ, true);
                    PatternPos = tempX.xyz;
                    PatternTime = tempX.w;
                }

                float Omega = GetZamoOmega(TargetSignedR, PhysicalSpinA, PhysicalQ, PatternPos.y);

                float Phi_raw = Vec2ToTheta(normalize(PatternPos.zx), vec2(0.0, 1.0));
                float Phi = Phi_raw + Omega * PatternTime + iBlackHoleTime*GetZamoOmega(TargetSignedR, PhysicalSpinA, PhysicalQ, 0.0);
                
                float CosTheta = clamp(PatternPos.y / TargetGeoR, -1.0, 1.0);
                float Theta = acos(CosTheta);
                float SinTheta = sqrt(max(0.0, 1.0 - CosTheta * CosTheta));

                float DensityPhi = 24.0;
                float DensityTheta = 13.0;
                float DistFactor = min(20.0,length(PatternPos));
                float LineWidth = 0.002 * DistFactor; 
                LineWidth = clamp(LineWidth, 0.01, 0.15); 

                float PatternPhi = abs(fract(Phi / (2.0 * kPi) * DensityPhi) - 0.5);
                float GridPhi = smoothstep(LineWidth / max(0.005, SinTheta), 0.0, PatternPhi);

                float PatternTheta = abs(fract(Theta / kPi * DensityTheta) - 0.5);
                float GridTheta = smoothstep(LineWidth, 0.0, PatternTheta);
                
                float GridIntensity = max(GridPhi, GridTheta);

                // --- 【修改：新增计算网格点局部能量】 ---
                float Omega_zamo = GetZamoOmega(TargetSignedR, PhysicalSpinA, PhysicalQ, HitPos.y);
                vec3 VelSpatial = Omega_zamo * vec3(HitPos.z, 0.0, -HitPos.x);
                vec4 U_zamo_unnorm = vec4(VelSpatial, 1.0); 
                KerrGeometry geo_hit;
                ComputeGeometryScalars(HitPos, PhysicalSpinA, PhysicalQ, 1.0, HitPointSign, isoutgoing, geo_hit);
                vec4 U_zamo_lower = LowerIndex(U_zamo_unnorm, geo_hit);
                float norm_sq = dot(U_zamo_unnorm, U_zamo_lower);
                float norm = sqrt(max(1e-9, abs(norm_sq)));
                vec4 U_zamo = U_zamo_unnorm / norm;
                float E_emit = -dot(HitP_cov, U_zamo);
                // ------------------------------------

                if (GridIntensity > 0.01) {
                    vec4 GridCol = vec4(TargetColor * 2.0, 1.0);
                    
                    // --- 【修改：新增负能量判断与反色/剔除逻辑】 ---
                    if (E_emit < 0.0) {
                        float cMax = max(max(GridCol.r, GridCol.g), GridCol.b);
                        float cMin = min(min(GridCol.r, GridCol.g), GridCol.b);
                        GridCol.rgb = vec3(cMax + cMin) - GridCol.rgb;
                        if (iWhitehole == 0) GridCol.rgba = vec4(0.0);
                    }
                    // -----------------------------------------
                    
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
        float HitTime_disk = DiskHitX.w;
        vec4 HitP_cov = mix(LastP_cov, P_cov, t_cross);
        // --- 提取统一网格相位的偏移 ---
        vec3 PatternPosDisk = DiskHitPos;
        if (isoutgoing) {
            vec4 tempX = vec4(DiskHitPos, HitTime_disk);
            vec4 dummyP = vec4(0.0);
            float diskSign = (length(DiskHitPos.xz) < abs(PhysicalSpinA)) ? -StartStepSign : StartStepSign;
            transformKerrSchild_YSpin(tempX, diskSign, dummyP, 0.5, PhysicalSpinA, PhysicalQ, true);
            PatternPosDisk = tempX.xyz;
        }

        float Phi_raw = Vec2ToTheta(normalize(PatternPosDisk.zx), vec2(0.0, 1.0));
        float Phi = Phi_raw;
        
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

        // --- 【修改：新增计算静态能量投影】 ---
        vec4 U_zero = vec4(0.0, 0.0, 0.0, 1.0); 
        float E_emit_disk = -dot(HitP_cov, U_zero); 
        // ---------------------------------

        if (GridIntensity > 0.01) {
            vec3 RingColor = 0.3*vec3(1.0, 1.0, 1.0);
            vec4 GridCol = vec4(RingColor * 5.0, 1.0);
            
            // --- 【修改：新增负能量判断与反色/剔除逻辑】 ---
            if (E_emit_disk < 0.0) {
                float cMax = max(max(GridCol.r, GridCol.g), GridCol.b);
                float cMin = min(min(GridCol.r, GridCol.g), GridCol.b);
                GridCol.rgb = vec3(cMax + cMin) - GridCol.rgb;
                if (iWhitehole == 0) GridCol.rgba = vec4(0.0);
            }
            // -----------------------------------------
            
            float Alpha = GridIntensity * 0.8;
            CurrentResult.rgb += GridCol.rgb * Alpha * (1.0 - CurrentResult.a);
            CurrentResult.a   += Alpha * (1.0 - CurrentResult.a);
        }
    }

    return CurrentResult;
}

vec4 GridColor(vec4 BaseColor, vec4 RayPos, vec4 LastRayPos,
               vec4 iP_cov, float iE_obs,
               float PhysicalSpinA, float PhysicalQ, bool isoutgoing,
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
        SignedGridRadii[GridCount++] = RH_Outer * 1.06; 
        SignedGridRadii[GridCount++] = 20.0;
        
        if (HorizonDiscrim >= 0.0) {
           SignedGridRadii[GridCount++] = RH_Inner * 0.94; 
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

                float HitTime = mix(LastRayPos.w, RayPos.w, t);

                // --- 物理计算部分：维持在当前的平滑坐标系（ HitPos 和 isoutgoing ）---
                float Omega = GetZamoOmega(TargetSignedR, PhysicalSpinA, PhysicalQ, HitPos.y);
                vec3 VelSpatial = Omega * vec3(HitPos.z, 0.0, -HitPos.x);
                vec4 U_zamo_unnorm = vec4(VelSpatial, 1.0); 
                
                KerrGeometry geo_hit;
                ComputeGeometryScalars(HitPos, PhysicalSpinA, PhysicalQ, 1.0, HitPointSign, isoutgoing, geo_hit);
                
                vec4 U_zamo_lower = LowerIndex(U_zamo_unnorm, geo_hit);
                float norm_sq = dot(U_zamo_unnorm, U_zamo_lower);
                float norm = sqrt(max(1e-9, abs(norm_sq)));
                vec4 U_zamo = U_zamo_unnorm / norm;

                float E_emit = -dot(iP_cov, U_zamo);
                float Shift = 1.0/ max(1e-6, abs(E_emit)); 

                // --- 几何图案部分：将坐标映射回统一的 Ingoing 参考系 ---
                vec3 PatternPos = HitPos;
                float PatternTime = HitTime;
                if (isoutgoing) {
                    vec4 tempX = vec4(HitPos, HitTime);
                    vec4 dummyP = vec4(0.0);
                    // out_to_in = true 转换为 Ingoing
                    transformKerrSchild_YSpin(tempX, HitPointSign, dummyP, 0.5, PhysicalSpinA, PhysicalQ, true);
                    PatternPos = tempX.xyz;
                    PatternTime = tempX.w;
                }

                float Phi_raw = Vec2ToTheta(normalize(PatternPos.zx), vec2(0.0, 1.0));
                float Phi = Phi_raw + Omega * PatternTime + iBlackHoleTime*GetZamoOmega(TargetSignedR, PhysicalSpinA, PhysicalQ, 1.0);
                
                float CosTheta = clamp(PatternPos.y / TargetGeoR, -1.0, 1.0);
                float Theta = acos(CosTheta);
                float SinTheta = sqrt(max(0.0, 1.0 - CosTheta * CosTheta));

                float DensityPhi = 24.0;
                float DensityTheta = 12.0;
                float DistFactor = length(PatternPos);
                float LineWidth = 0.001 * DistFactor;
                LineWidth = clamp(LineWidth, 0.01, 0.1); 

                float PatternPhi = abs(fract(Phi / (2.0 * kPi) * DensityPhi) - 0.5);
                float GridPhi = smoothstep(LineWidth / max(0.005, SinTheta), 0.0, PatternPhi);

                float PatternTheta = abs(fract(Theta / kPi * DensityTheta) - 0.5);
                float GridTheta = smoothstep(LineWidth, 0.0, PatternTheta);
                
                float GridIntensity = max(GridPhi, GridTheta);

                if (GridIntensity > 0.01) {
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

    // --- 赤道面的网格处理同样映射为统一相空间 ---
    if (bHasCrossed && CurrentResult.a < 0.99) {
        
        float HitRho = length(DiskHitPos.xz);
        float a_abs = abs(PhysicalSpinA);
        float HitTime_disk = mix(LastRayPos.w, RayPos.w, t_cross);
        
        vec3 PatternPosDisk = DiskHitPos;
        if (isoutgoing) {
            vec4 tempX = vec4(DiskHitPos, HitTime_disk);
            vec4 dummyP = vec4(0.0);
            float diskSign = (length(DiskHitPos.xz) < abs(PhysicalSpinA)) ? -StartStepSign : StartStepSign;
            transformKerrSchild_YSpin(tempX, diskSign, dummyP, 0.5, PhysicalSpinA, PhysicalQ, true);
            PatternPosDisk = tempX.xyz;
        }

        float Phi_raw = Vec2ToTheta(normalize(PatternPosDisk.zx), vec2(0.0, 1.0));
        float Phi = Phi_raw;
        
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
            // (频移由于和空间无关依然使用原始数据)
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

// =============================================================================
// SECTION7: KN阴影计算
// =============================================================================

bool IsAccretionDiskVisible(float InterR, float OuterR, float Thin, float Hopper, float Bright, float Dark)
{
    if (InterR >= OuterR) return false;
    if (Thin <= 0.0 && Hopper == 0.0) return false;
    if (Bright <= 0.0 && Dark < 0.0) return false;
    
    return true;
}

bool IsJetVisible(float AccretionRate, float JetBright)
{
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
    float c1 = sign_term * (-2.0 * a * M2);
    float c0 = Q2 * Q2 - M2 * Q2;
    
    // 初始猜测：
    // 顺行(A, 小根)，u 较小 (r 接近 M 或 2M)
    // 逆行(B, 大根)，u 较大 (r 接近 3M 或 4M)
    float u = is_max_root ? 2.2 * M : 0.8 * M; 
    
    // 牛顿迭代求解
    for(int i=0; i<8; i++) {
        float u2 = u * u;
        float u3 = u2 * u;
        
        float f  = u2 * u2 + c2 * u2 + c1 * u + c0;
        float df = 4.0 * u3 + 2.0 * c2 * u + c1;
        
        if (abs(df) < 1e-6) break;
        u = u - f / df;
    }
    return abs(u); 
}

// 将静态观测者的正弦值转换为落体观测者
float GetDropFrameAngle(float SinThetaStat, float CosThetaStat, float r, float M, float Q, float a, int ObserverMode) {
    // 静态观者 (ObserverMode == 0)
    if (ObserverMode == 0) {
        return atan(SinThetaStat, CosThetaStat);
    }
    
    // 落体观者 (ObserverMode == 1)
    float a2 = a * a;
    float r2 = r * r;
    float MassChargeTerm = 2.0 * M * r - Q * Q;
    
    float numerator_v = MassChargeTerm * (r2 + a2);
    float denominator_v = r2 * (r2 + a2) + a2 * MassChargeTerm;
    
    float v_sq = numerator_v / max(1e-9, denominator_v);
    v_sq = (1.0+0.05*a)*min(0.9999, max(0.0, v_sq)); //略微加速、增强收缩，作为冗余
    float v = sqrt(v_sq);
    
    // 应用相对论光行差
    // sin(θ') = sin(θ) * sqrt(1-v^2) / (1 + v*cos(θ))
    // cos(θ') = (cos(θ) + v) / (1 + v*cos(θ))
    
    float denom = 1.0 + v * CosThetaStat;
    float sin_fall = SinThetaStat * sqrt(max(0.0, 1.0 - v_sq));
    float cos_fall = CosThetaStat + v;
    
    return atan(sin_fall, cos_fall);
}

// 计算 R-N (a=0) 黑洞的阴影半张角 
float GetShadowHalfAngleRN(float r, float M, float Q, int ObserverMode)
{
    float M2 = M * M;
    float Q2 = Q * Q;
    float r2 = r * r;
    
    // 光子球半径 r_ps
    float term_root = sqrt(max(0.0, 9.0 * M2 - 8.0 * Q2));
    float r_ps = 0.5 * (3.0 * M + term_root);
    
    // 临界碰撞参数 b_c
    float metric_factor_ps = 1.0 - 2.0 * M / r_ps + Q2 / (r_ps * r_ps);
    float b_c = r_ps / sqrt(max(1e-6, metric_factor_ps));
    
    // 计算静态观者的 Sin 和 Cos
    // f(r) = 1 - 2M/r + Q^2/r^2
    float f_r = 1.0 - 2.0 * M / r + Q2 / r2;
    float sqrt_f = sqrt(max(0.0, f_r));
    
    // Sin = (b_c / r) * sqrt(f)
    float sin_theta_stat = (b_c / r) * sqrt_f;
    
    // 判断光子球内外来决定 Cos 的符号
    // r < r_ps 时，阴影遮挡超过半个天空，为钝角 (Cos < 0)
    // 增加一个微小的 epsilon 防止 r == r_ps 时闪烁
    float cos_sign = (r >= r_ps - 1e-4) ? 1.0 : -1.0;
    
    // 计算 Cos
    float cos_theta_stat = cos_sign * sqrt(max(0.0, 1.0 - sin_theta_stat * sin_theta_stat));
    
    // 换坐标系
    return GetDropFrameAngle(sin_theta_stat, cos_theta_stat, r, M, Q, 0.0, ObserverMode);
}


// =============================================================================
// SECTION8: main
// =============================================================================

struct TraceResult {
    vec3  EscapeDir;      // 最终逸出方向 (World Space)
    float FreqShift;      // 频移 (E_emit / E_obs)
    float Status;         // 0=Stop, 1=Sky, 2=Antiverse,3=不透明体积,4=光来自上个宇宙（出白洞）,5=光来自上个宇宙的antiverse
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
    // 吸积盘相关参量
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

    const float kPhysicsFactor = 1.52491e30; 
    float DiskArgument = kPhysicsFactor / iBlackHoleMassSol * (iMu / AccretionEffective) * (iAccretionRate);
    float PeakTemperature = pow(DiskArgument * 0.05665278, 0.25);

    // 自旋/电荷 量纲化
    float PhysicalSpinA = iSpin * CONST_M;  
    float PhysicalQ     = iQ * CONST_M; 
    
    // 视界位置
    float HorizonDiscrim = 0.25 - PhysicalSpinA * PhysicalSpinA - PhysicalQ * PhysicalQ;
    float EventHorizonR = 0.5 + sqrt(max(0.0, HorizonDiscrim));
    float InnerHorizonR = 0.5 - sqrt(max(0.0, HorizonDiscrim));
    bool  bIsNakedSingularity = HorizonDiscrim < 0.0;

    // 包围球
    float RaymarchingBoundary = max(max(iOuterRadiusRs + 1.0, 501.0),iSpin*2.0);
    float BackgroundShiftMax = 2.0;
    float ShiftMax = 1.0; //吸积盘亮度最大值
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

    vec4 P_cov = vec4(0.0,0.0,0.0,-1.0);

    float E_conserved = 1.0;
    vec3 RayDir = RayDirWorld_Geo;
    vec3 LastDir = RayDir;
    vec3 LastPos = RayPosLocal;
    float GravityFade = CubicInterpolate(max(min(1.0 - (length(RayPosLocal) - 100.0) / (RaymarchingBoundary - 100.0), 1.0), 0.0));
    
    
    
    if (iEnableHeatHaze == 1 &&  iInWhichUniverse==0 && CurrentUniverseSign>0.0)//热折射
    {
        // 1. 坐标与参数准备 (Rg 空间)
        vec3 pos_Rg_Start = X.xyz; 
        vec3 rayDirNorm = normalize(RayDir);

        float totalProbeDist = float(HAZE_PROBE_STEPS) * HAZE_STEP_SIZE;
        
        // [适配] 使用 iTime，取模防止噪声溢出
        float hazeTime = mod(iBlackHoleTime, 1000.0); 

        if(iDEBUG==1)
        {
            float debugAccum = 0.0;
            float debugStep = 1.0; 
            vec3 debugPos = pos_Rg_Start;
            
            float rotSpeedBase = 100.0 * HAZE_ROT_SPEED;
            float jetSpeedBase = 50.0 * HAZE_FLOW_SPEED;
            
            float ReferenceOmega = GetKeplerianAngularVelocity(6.0, 1.0, PhysicalSpinA, PhysicalQ);
            float AdaptiveFrequency = abs(ReferenceOmega * rotSpeedBase) / (2.0 * kPi * 5.14);
            AdaptiveFrequency = max(AdaptiveFrequency, 0.1);
            float flowTime = hazeTime * AdaptiveFrequency;

            float phase1 = fract(flowTime); float phase2 = fract(flowTime + 0.5);
            float weight1 = 1.0 - abs(2.0 * phase1 - 1.0); float weight2 = 1.0 - abs(2.0 * phase2 - 1.0);
            float t_offset1 = phase1 - 0.5; float t_offset2 = phase2 - 0.5;
            
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
            
           Result += 0.1*vec4(vec3(min(1.0, debugAccum)), 1.0);
        }
        

        //几何剔除优化
        if (IsInHazeBoundingVolume(pos_Rg_Start, totalProbeDist, iOuterRadiusRs)) 
        {
            vec3 accumulatedForce = vec3(0.0);
            float totalWeight = 0.0;

            // 累积探测
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

            // 偏转应用
            float forceMagSq = dot(avgHazeForce, avgHazeForce);
            if (forceMagSq > 1e-10)
            {
                vec3 forcePerp = avgHazeForce - dot(avgHazeForce, rayDirNorm) * rayDirNorm;
                // 计算总偏转量
                vec3 deflection = forcePerp * HAZE_STRENGTH * 25.0; 
                RayDir = normalize(RayDir + deflection * 0.1); 
                LastDir = RayDir;
            }
            if(iDEBUG==1)
            {
                if (length(avgHazeForce) > 1e-4) {
                    vec3 debugVec = normalize(avgHazeForce) * 0.5 + 0.5;
                    debugVec *= (0.5 + 10.0 * length(avgHazeForce)); 
                    Result += 0.1*vec4(debugVec, 1.0);
                }
            }
        }

    }
    
    bool isoutgoing = false; 
    //if(iWhitehole==1) isoutgoing=true;//test
    //if (bShouldContinueMarchRay) {
    //    // --- 测试 Ingoing 系 ---
    //    vec4 P_in = GetInitialMomentum(RayDir, X, iObserverMode, iUniverseSign, PhysicalSpinA, PhysicalQ, GravityFade, false);
    //    float score_in = 1e38; // 评分标准：|P^t|，越小越好
    //    if (P_in != vec4(114514.0)) {
    //        KerrGeometry geo_in;
    //        ComputeGeometryScalars(X.xyz, PhysicalSpinA, PhysicalQ, GravityFade, iUniverseSign, false, geo_in);
    //        score_in = abs(RaiseIndex(P_in, geo_in).w); 
    //    }
    //
    //    // --- 测试 Outgoing 系 (仅在允许最大延拓时进行，常规黑洞无需测试) ---
    //    vec4 P_out = vec4(114514.0);
    //    float score_out = 1e38;
    //    if (iWhitehole == 1) {
    //        P_out = GetInitialMomentum(RayDir, X, iObserverMode, iUniverseSign, PhysicalSpinA, PhysicalQ, GravityFade, true);
    //        if (P_out != vec4(114514.0)) {
    //            KerrGeometry geo_out;
    //            ComputeGeometryScalars(X.xyz, PhysicalSpinA, PhysicalQ, GravityFade, iUniverseSign, true, geo_out);
    //            score_out = abs(RaiseIndex(P_out, geo_out).w);
    //        }
    //    }
    //
    //    // --- 竞态选择最优坐标系 ---
    //    if (score_out < score_in && P_out != vec4(114514.0)) {
    //        isoutgoing = true;
    //        P_cov = P_out;
    //    } else {
    //        isoutgoing = false;
    //        P_cov = P_in;
    //    }
    //}
    
    //// 两边都崩溃了，或者不符合物理规范，直接黑屏
    //if (P_cov == vec4(114514.0)) {
    //    bShouldContinueMarchRay = false;
    //    bWaitCalBack = false;
    //    Result = vec4(0.0, 0.0, 0.0, 1.0);
    //}
    if (bShouldContinueMarchRay) {
       P_cov = GetInitialMomentum(RayDir, X, iObserverMode, iUniverseSign, PhysicalSpinA, PhysicalQ, GravityFade, isoutgoing);
       
       // 如果被拦截（观者在该系下变成类空），且允许最大延拓，说明这是向外运动(如出白洞)的观者
       if (P_cov == vec4(114514.0) && iWhitehole == 1) {    //注意，此处有未定位的bug，导致角度变化
       //    // 1. 记录旧的坐标用于反解旋转矩阵
       //    vec4 X_old = X;
       //    vec4 dummyP = vec4(0.0); // 传入一个假动量，因为我们这步只需要变 X
       //    
       //    // 2. 将坐标变换到 Outgoing 系 (out_to_in = false)
       //    // transformKerrSchild_YSpin(X, iUniverseSign, dummyP, CONST_M, PhysicalSpinA, PhysicalQ, false);
       //    
       //    // 3. 反解旋转矩阵的 sin 和 cos，并同步旋转 RayDir
       //    float rho2 = X_old.x * X_old.x + X_old.z * X_old.z;
       //    if (rho2 > 1e-12) {
       //        float cos_a = (X_old.x * X.x + X_old.z * X.z) / rho2;
       //        float sin_a = (X_old.z * X.x - X_old.x * X.z) / rho2;
       //        
       //        vec3 oldDir = RayDir;
       //        RayDir.x = oldDir.z * sin_a + oldDir.x * cos_a;
       //        RayDir.z = oldDir.z * cos_a - oldDir.x * sin_a;
       //    }
       //
       //    // 4. 切换坐标系标识
           isoutgoing = true; 
       //    
       //    // 5. 在正确的 Outgoing 坐标系下，使用变换后的 X 和 RayDir 重新计算合法的初始动量
           P_cov = GetInitialMomentum(RayDir, X, iObserverMode, iUniverseSign, PhysicalSpinA, PhysicalQ, GravityFade, isoutgoing);
       }
    }
    if (P_cov == vec4(114514.0))
    {
        bShouldContinueMarchRay = false;
        bWaitCalBack = false;
        Result = vec4(0.0,0.0,0.0,1.0);
    }




    // -------------------------------------------------------------------------
    // 初始化偏振基底 (严格绑定到相机真实的屏幕轴)
    // -------------------------------------------------------------------------
// =========================================================================
    // 初始化偏振基底 (绑定相机屏幕，并执行严格的四维广相正交化与洛伦兹变换)
    // =========================================================================
    vec2 WP_CamX = vec2(0.0);
    vec2 WP_CamY = vec2(0.0);
    vec2 StokesQU = vec2(0.0);

    if (bShouldContinueMarchRay && iPolarization != 0) {
        // 1. 坐标系对齐：Walker-Penrose 常数必须在 Ingoing 系下计算
        vec4 X_wp = X;
        vec4 P_cov_wp = P_cov;
        if (isoutgoing) {
            // 如果光线被判定在白洞外流区，临时将其逆变换回 Ingoing 系
            transformKerrSchild_YSpin(X_wp, CurrentUniverseSign, P_cov_wp, CONST_M, PhysicalSpinA, PhysicalQ, true);
        }

        KerrGeometry geo_wp;
        ComputeGeometryScalars(X_wp.xyz, PhysicalSpinA, PhysicalQ, GravityFade, CurrentUniverseSign, false, geo_wp);
        vec4 P_up_wp = RaiseIndex(P_cov_wp, geo_wp);

        // 2. 提取观测者四维速度 U^mu (处理狭义相对论光行差与洛伦兹变换的基础)
        vec4 U_up;
        float g_tt = -1.0 + geo_wp.f;
        float time_comp = 1.0 / sqrt(max(1e-9, -g_tt));
        U_up = vec4(0.0, 0.0, 0.0, time_comp);
        
        if (iObserverMode == 1) { // 自由落体观者
            float r = geo_wp.r; float r2 = geo_wp.r2; float a = PhysicalSpinA; float a2 = geo_wp.a2;
            float y_phys = X_wp.y; 
            float rho2 = r2 + a2 * (y_phys * y_phys) / (r2 + 1e-9);
            float Q2 = PhysicalQ * PhysicalQ;
            float MassChargeTerm = 2.0 * CONST_M * r - Q2;
            float Xi = sqrt(max(0.0, MassChargeTerm * (r2 + a2)));
            float DenomPhi = rho2 * (MassChargeTerm + Xi);
            float U_phi_KS = (abs(DenomPhi) > 1e-9) ? (-MassChargeTerm * a / DenomPhi) : 0.0;
            float U_r_KS = -Xi / max(1e-9, rho2);
            float inv_r2_a2 = 1.0 / (r2 + a2);
            float Ux_rad = (r * X_wp.x - a * X_wp.z) * inv_r2_a2 * U_r_KS;
            float Uz_rad = (r * X_wp.z + a * X_wp.x) * inv_r2_a2 * U_r_KS;
            float Uy_rad = (X_wp.y / r) * U_r_KS;
            float Ux_tan =  X_wp.z * U_phi_KS;
            float Uz_tan = -X_wp.x * U_phi_KS;
            vec3 U_spatial = vec3(Ux_rad + Ux_tan, Uy_rad, Uz_rad + Uz_tan);
            float l_dot_u_spatial = dot(geo_wp.l_down.xyz, U_spatial);
            float U_spatial_sq = dot(U_spatial, U_spatial);
            float A = -1.0 + geo_wp.f;
            float B = 2.0 * geo_wp.f * l_dot_u_spatial;
            float C = U_spatial_sq + geo_wp.f * (l_dot_u_spatial * l_dot_u_spatial) + 1.0; 
            float Det = max(0.0, B*B - 4.0 * A * C);
            float Ut = (abs(A) < 1e-7) ? (-C / max(1e-19, B)) : ((B < 0.0) ? (2.0 * C / (-B + sqrt(Det))) : ((-B - sqrt(Det)) / (2.0 * A)));
            U_up = mix(vec4(0.0, 0.0, 0.0, time_comp), vec4(U_spatial, Ut), GravityFade);
        } else if (iObserverMode == 2 || iObserverMode == 3) { // 自定义速度观者
            vec3 v_in = (iObserverMode == 2) ? iCameraVelocity.xyz : -iCameraVelocity.xyz;
            if (any(isnan(v_in)) || any(isinf(v_in))) v_in = vec3(0.0);
            vec4 V_up = vec4(v_in, 1.0);
            vec4 V_down = LowerIndex(V_up, geo_wp);
            float V_sq = dot(V_up, V_down);
            if (V_sq < 0.0) U_up = V_up * inversesqrt(-V_sq);
        }
        vec4 U_down = LowerIndex(U_up, geo_wp);
        float E_obs = -dot(P_up_wp, U_down); // 光子在观者局部系测量的相对论能量

        // 3. 提取相机系下纯粹的屏幕 Right(X轴) 和 Up(Y轴) 初始三维矢量
        vec3 rawCamRight = vec3(1.0, 0.0, 0.0);
        vec3 rawCamUp    = vec3(0.0, 1.0, 0.0);
        vec3 Right_BHLocal = WorldToLocalRot * (iInverseCamRot * vec4(rawCamRight, 0.0)).xyz;
        vec3 Up_BHLocal    = WorldToLocalRot * (iInverseCamRot * vec4(rawCamUp, 0.0)).xyz;

        // 强行升维 (初始时间分量设为0，相当于无穷远平直时空的坐标轴)
        vec4 R_up = vec4(Right_BHLocal, 0.0);
        vec4 Y_up = vec4(Up_BHLocal, 0.0);

        // 4. 执行严谨的 4维 Gram-Schmidt 广义相对论正交化
        
        // 步骤 4.1: 消除时间膨胀分量，确保基底在观者局部标架内是纯空间的 (F · U = 0)
        R_up += dot(R_up, U_down) * U_up;
        Y_up += dot(Y_up, U_down) * U_up;
        vec4 R_down = LowerIndex(R_up, geo_wp);
        vec4 Y_down = LowerIndex(Y_up, geo_wp);

        // 步骤 4.2: 消除径向分量，确保基底垂直于光子运动方向 (F · P = 0)
        // 构造光子在观者局部系下的纯空间方向 D^mu = P^mu - E * U^mu
        vec4 D_up = P_up_wp - E_obs * U_up;
        vec4 D_down = LowerIndex(D_up, geo_wp);
        float invE2 = 1.0 / max(1e-12, E_obs * E_obs); // D^mu 的模长平方恒等于 E^2

        // 在 4D 度规下剔除 D^mu 方向的投影分量
        vec4 FX_up = R_up - (dot(R_up, D_down) * invE2) * D_up;
        vec4 FY_up = Y_up - (dot(Y_up, D_down) * invE2) * D_up;

        vec4 FX_down = LowerIndex(FX_up, geo_wp);
        vec4 FY_down = LowerIndex(FY_up, geo_wp);

        // 步骤 4.3: 在弯曲时空度规下归一化 (使得 F · F = 1)
        float normX = sqrt(max(1e-12, dot(FX_up, FX_down)));
        float normY = sqrt(max(1e-12, dot(FY_up, FY_down)));
        FX_down /= normX;
        FY_down /= normY;

        // 5. 此时的 FX, FY 已经是包含广相弯曲+狭相光行差修正后的完美四维横向偏振矢量
        float r_start = KerrSchildRadius(X_wp.xyz, PhysicalSpinA, CurrentUniverseSign);
        WP_CamX = GetWalkerPenrose(X_wp, P_cov_wp, FX_down, PhysicalSpinA,PhysicalQ, r_start);
        WP_CamY = GetWalkerPenrose(X_wp, P_cov_wp, FY_down, PhysicalSpinA,PhysicalQ, r_start);
    }



    //    if (P_cov == vec4(114514.0))
    //{
    //    bShouldContinueMarchRay = false;
    //    bWaitCalBack = false;
    //    Result = vec4(0.0,0.0,0.0,1.0);
    //}

    // ==== 新增的调试接管分支 ====
    if (iDEBUG == 2 && bShouldContinueMarchRay)
    {
        vec3 dbgColor = DebugInitialMomentum(
            P_cov, X, iObserverMode, iUniverseSign, 
            PhysicalSpinA, PhysicalQ, GravityFade, isoutgoing, iCameraVelocity.xyz
        );
        res.AccumColor = vec4(dbgColor, 1.0);
        res.Status = 3.0; // 设置为不透明直接返回，绘制到屏幕
        return res;
    }
    // ========================

    E_conserved = -P_cov.w;
    //if(iGrid==0 && E_conserved<0.0){ res.FreqShift=0.0;res.AccumColor=vec4(0.0);  res.Status = 0.0; return res;}//剔除最大延拓解下II/IV区内从 平行宇宙 去往/来自 平行虫洞 的光。（视界内和观者相向而行）    开启网格时会误伤能层内部分从网格发出的光线，igrid非0时的剔除改为视界间进行
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
        if (!bIsNakedSingularity && CurrentUniverseSign > 0.0 && iWhitehole==0) 
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


#define SHADOW_SIZE_MULTIPLIER    0.995     // 阴影半径微调系数 (Multiplier for shadow radius)


    // -------------------------------------------------------------------------
    // 阴影剔除逻辑
    // -------------------------------------------------------------------------

    //非裸奇点、在universe侧、距离足够远(>RN视界或KN逆行光子轨道)、当前还需要继续步进
    float AbsSpinA = abs(CONST_M * iSpin);
    bool bIsRot = AbsSpinA > 1e-5;
    
    // 判定是否需要剔除：
    // 非裸奇点 (视界存在),在正宇宙 (CurrentUniverseSign > 0),当前光线还需要继续步进 (bShouldContinueMarchRay)
    if (!bIsNakedSingularity && CurrentUniverseSign > 0.0 && bShouldContinueMarchRay && iGrid==0 && iWhitehole==0 &&(iObserverMode==0 || iObserverMode==1 ) && iEnableShadowCulling==1)
    {
        // 预计算剔除启动的阈值半径
        float CullingStartRadius;
        
        if (!bIsRot) {
            // 纯RN/史瓦西黑洞：允许进入光子球内部，直到非常接近视界
            CullingStartRadius = 1.005 * EventHorizonR;
        } else {
            // 计算逆行光子轨道半径 r_B (凸出侧)
            // 使用 SolveQuarticU 计算 r_B (对应参数 +1.0)
            float u_B_calc = SolveQuarticU(CONST_M, PhysicalQ, AbsSpinA, 1.0, true);
            float r_B_calc = (u_B_calc * u_B_calc + PhysicalQ * PhysicalQ) / CONST_M;

            CullingStartRadius = r_B_calc + 0.05;
        }
        if (CameraStartR > CullingStartRadius)
        {
 // 计算视线与黑洞中心的夹角
            vec3 ToCenterDir = -normalize(RayPosLocal); // 局部系下黑洞在原点
            float CosAlpha = dot(normalize(RayDir), ToCenterDir);
            float RayAngle = acos(clamp(CosAlpha, -1.0, 1.0)); // 当前像素视线与黑洞中心的夹角

            // 估算阴影的大致可能张角，仅在这个区域内进一步计算
            float SafetyFactor = 2.5 + 1.1 * abs(iSpin) - 0.5*iQ;
            float MaxShadowAngleEstimate = SafetyFactor * (2.0 * CONST_M) / max(1e-6, CameraStartR);
            if (RayAngle < MaxShadowAngleEstimate || CameraStartR < 3.0*EventHorizonR) // 大致朝向黑洞或在光子球内
            {
                float RayAngle = acos(CosAlpha); // 当前像素视线与黑洞中心的夹角
                bool bHitShadow = false; 
                
                if (!bIsRot)
                {
                    //球对称
                    float ShadowHalfAngle = GetShadowHalfAngleRN(CameraStartR, CONST_M, PhysicalQ, iObserverMode);
                    ShadowHalfAngle *= SHADOW_SIZE_MULTIPLIER;
                    
                    if (RayAngle < ShadowHalfAngle) bHitShadow = true;
                }
                else
                {
                    float M = CONST_M;
                    float Q = PhysicalQ;
                    float a = PhysicalSpinA; 
                    float a_abs = AbsSpinA; 
                    float Q2 = Q*Q;
                    float a2 = a_abs*a_abs;
                    float r = CameraStartR;
                    
                    //极轴视角
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

                    if (LatFactor > 0.99999)
                    {
                        float effectiveMult = SHADOW_SIZE_MULTIPLIER ;
                        if (RayAngle < AngleOF * effectiveMult) bHitShadow = true;
                    }
                    else
                    {
                        
                        // 赤道视角
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
                        
                        // 混合
                        // 调试：如何选择混合函数。需要是一些凹函数。
                        float MixWA = clamp(tan(LatFactor*1.48)/10.98338,0.0,1.0);//指数函数在这里会前期太小、后期太大，所以用tan
                        float MixWB = pow(LatFactor, 2.5);//基本确定2.5
                        float MixWE = pow(LatFactor, 6.0);//基本确定6.0
                        float MixWCD = pow(LatFactor, 0.75);//基本确定0.75
                        
                        float AngleOA = mix(AngleOA0, AngleOF, MixWA);
                        float AngleOB = mix(AngleOB0, AngleOF, MixWB);
                        float AngleEC = mix(AngleEC0, AngleOF, MixWCD);
                        float AngleOE = mix(AngleOE0, 0.0,     MixWE);

                        float AberrationShift = 0.0;
                        if (iObserverMode == 1) {
                            // gtphi_stat是在赤道面上算出的，实际上切向速度随纬度变化(在两极处为0)
                            float SinTheta = sqrt(max(0.0, 1.0 - LatFactor * LatFactor));
                            float SinAberration = abs(gtphi_stat) * InvSqrtD * SinTheta; 
                            float CosAberration = sqrt(max(0.0, 1.0 - SinAberration * SinAberration));
                            // 算出横向光行差导致的视角偏移量
                            AberrationShift = 2.0*0.6666*a_abs*GetDropFrameAngle(SinAberration, CosAberration, r, M, Q, a_abs, iObserverMode); 
                        }
                        // 视平面判定
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
                        float CenterEx = SignChirality * (AngleOE + AberrationShift);
                        float dx = x_ang - CenterEx;
                        float dy = y_ang;
                        
                        float RadiusA_from_E = 0.99*(AngleOA + AngleOE);
                        float RadiusB_from_E = max(1e-5, AngleOB - AngleOE);
                        
                        float CurrentHRadius;
                        float CurrentVRadius = AngleEC;
                        
                        if (iDEBUG == 1)
                        {
                            vec2 currP = vec2(x_ang, y_ang);
                            float dotSize = 0.002; // 调试点大小（弧度）
                            
                            // O是白色
                            vec2 ptO = vec2(0.0, 0.0);
                            if (length(currP - ptO) < dotSize) {
                                Result += 0.3*vec4(1.0, 1.0, 1.0, 1.0);
                                res.Status = 3.0; return res;
                            }
                            
                            // C、D、E是蓝色
                            vec2 ptE = vec2(CenterEx, 0.0);
                            vec2 ptC = vec2(CenterEx,  AngleEC);
                            vec2 ptD = vec2(CenterEx, -AngleEC);
                            if (length(currP - ptE) < dotSize || length(currP - ptC) < dotSize || length(currP - ptD) < dotSize) {
                                Result += 0.3*vec4(0.0, 0.5, 1.0, 1.0);
                                res.Status = 3.0; return res;
                            }
                            
                            // A、B是红色
                            vec2 ptA = vec2(CenterEx - SignChirality * RadiusA_from_E, 0.0);
                            vec2 ptB = vec2(CenterEx + SignChirality * RadiusB_from_E, 0.0);
                            if (length(currP - ptA) < dotSize || length(currP - ptB) < dotSize) {
                                Result += 0.3*vec4(1.0, 0.0, 0.0, 1.0);
                                res.Status = 3.0; return res;
                            }
                        }
                        
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
                
                //执行剔除

                if (bHitShadow)
                {
                    bool bHasDisk = IsAccretionDiskVisible(iInterRadiusRs, iOuterRadiusRs, iThinRs, iHopper, iBrightmut, iDarkmut);
                    bool bHasJet  = IsJetVisible(iAccretionRate, iJetBrightmut);
                    
                    if (!bHasDisk && !bHasJet)
                    {
                        // 纯黑洞，无盘无喷流：立即返回黑色
                        if(iDEBUG==1)
                        {
                            res.AccumColor = vec4(0.0, 0.5, 0.0, 1.0); 
                            res.Status = 3.0; 
                        }else{
                            res.AccumColor = vec4(0.0, 0.0, 0.0, 1.0); 
                            res.Status = 3.0;
                        }
                        res.CurrentSign = CurrentUniverseSign;
                        res.EscapeDir = vec3(0.0);
                        res.FreqShift = 1.0;
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



    float MaxStep=150.0+300.0/(1.0+1000.0*(1.0-iSpin*iSpin-iQ*iQ)*(1.0-iSpin*iSpin-iQ*iQ));
    if(iWhitehole==1) MaxStep=1145;
    if(bIsNakedSingularity) MaxStep=450;//150.0+300.0/(1.0+10.0*(1.0-iSpin*iSpin-iQ*iQ)*(1.0-iSpin*iSpin-iQ*iQ));

    // -------------------------------------------------------------------------
    // 主循环
    // -------------------------------------------------------------------------
    int Count = 0;
    float lastR = 0.0;
    bool bIntoOutHorizon = false;
    bool bIntoInHorizon = false;
    bool bEscapeOutHorizon = false;
    bool bEscapeInHorizon = false;
    int universeoffset=0;
    if(iWhitehole == 1 && !bIsNakedSingularity && CameraStartR <InnerHorizonR) universeoffset++;
    float LastDr = 0.0;           
    int RadialTurningCounts = 0;  
    float RayMarchPhase = RandomStep(FragUv, iTime); 
    float ThetaInShell=0;
    bool shiftinout=false;
    bool fromwhitehole=false;
    vec4 LastX_ingoing;
    vec4 X_ingoing = X;
    vec4 P_cov_ingoing = P_cov;
    vec4 LastP_cov_ingoing = P_cov_ingoing; // [新增] 用于保存上一步的局域四动量
    if(P_cov== vec4(0.0,0.0,0.0,-1.0))	{
        P_cov_ingoing.xyz = -RayDir;
    }
    else{
        if (isoutgoing) {
            transformKerrSchild_YSpin(X_ingoing, CurrentUniverseSign, P_cov_ingoing, CONST_M, PhysicalSpinA, PhysicalQ, isoutgoing);
        }
        LastX_ingoing = X_ingoing;
        LastP_cov_ingoing = P_cov_ingoing; // [新增] 初始化 LastP_cov
    }
    // ----------------------------------------

    while (bShouldContinueMarchRay)
    {
        DistanceToBlackHole = length(X.xyz);
        if (DistanceToBlackHole > RaymarchingBoundary)
        { 
            bShouldContinueMarchRay = false; 
            bWaitCalBack = true; 
            break;  //离开足够远
        }
        
        KerrGeometry geo;
        ComputeGeometryScalars(X.xyz, PhysicalSpinA, PhysicalQ, GravityFade, CurrentUniverseSign, isoutgoing, geo);

        if (CurrentUniverseSign > 0.0 && geo.r < TerminationR && !bIsNakedSingularity && TerminationR != -1.0 && iWhitehole==0) 
        { 
            bShouldContinueMarchRay = false;
            bWaitCalBack = false;
            if(iDEBUG==1) Result += vec4(0.0, 0.3, 0.3, 0.0); 
            break;  //视界判定情况1，直接进入视界判定区
        }
        if (Count >int(float( MaxStep)*iQuality*(1.0+0.3*iQuality))) 
        { 
            bShouldContinueMarchRay = false; 
            bWaitCalBack = false;
            if(bIsNakedSingularity&&RadialTurningCounts <= 2) bWaitCalBack = true;
            if(iDEBUG==1) Result += vec4(0.0, 0.3, 0.0, 0.0);
            break; //耗尽步数
        }

        State s0; s0.X = X; s0.P = P_cov;
        State k1 = GetDerivativesAnalytic(s0, PhysicalSpinA, PhysicalQ, GravityFade, isoutgoing, geo);

        float CurrentDr = dot(geo.grad_r, k1.X.xyz);
        shiftinout = false;

        if (Count > 0 && CurrentDr * LastDr < 0.0) RadialTurningCounts++;
        if (Count==0) lastR = geo.r;

        //if ((iWhitehole == 1|| (CameraStartR>(EventHorizonR+0.1)&& iGrid==0)|| bIsNakedSingularity||iMu>2.0)&&(geo.r>0.3*InnerHorizonR || bIsNakedSingularity))
        {
            vec4 P_contra = RaiseIndex(P_cov, geo);
            float current_Sum = dot(abs(P_contra), vec4(1.0));
            // 试探换系
            vec4 test_X = X;
            vec4 test_P = P_cov;
            transformKerrSchild_YSpin(test_X, CurrentUniverseSign, test_P, CONST_M, PhysicalSpinA, PhysicalQ, isoutgoing);
            
            KerrGeometry test_geo;
            ComputeGeometryScalars(test_X.xyz, PhysicalSpinA, PhysicalQ, GravityFade, CurrentUniverseSign, !isoutgoing, test_geo);
            
            vec4 test_P_contra = RaiseIndex(test_P, test_geo);
            float test_Sum = dot(abs(test_P_contra), vec4(1.0));
            
            if (current_Sum > 2.0 * test_Sum) 
            {
                X = test_X;
                P_cov = test_P;
                isoutgoing = !isoutgoing;
                geo = test_geo;
                s0.X = X; 
                s0.P = P_cov;
                k1 = GetDerivativesAnalytic(s0, PhysicalSpinA, PhysicalQ, GravityFade, isoutgoing, geo);
                CurrentDr = dot(geo.grad_r, k1.X.xyz);
                shiftinout = true;
            
            
                //Result+=vec4(0.0,0.2,0.0,0.5)*(1.0-Result.a);
            }
        }
        LastDr = CurrentDr;

        if(geo.r < InnerHorizonR && lastR > InnerHorizonR) bEscapeInHorizon = true;    //检测穿进(追踪方向)内视界   
        if(geo.r < EventHorizonR && lastR > EventHorizonR) bEscapeOutHorizon = true;   //检测穿进(追踪方向)外视界   

        if(iWhitehole == 1 && !bIsNakedSingularity && geo.r < InnerHorizonR && lastR > InnerHorizonR) universeoffset++;    

        if (iWhitehole==0 && RadialTurningCounts > 2 ) 
        {
            bShouldContinueMarchRay = false; bWaitCalBack = false;
            if(iDEBUG==1) Result += vec4(0.3, 0.0, 0.3, 1.0); 
            break;//识别剔除束缚态光子轨道
        }else if((bIsNakedSingularity&&RadialTurningCounts > 4) || (!bIsNakedSingularity&&universeoffset>3))
        {
            bShouldContinueMarchRay = false; bWaitCalBack = false;
            if(iDEBUG==1) Result += vec4(0.3, 0.0, 0.3, 1.0); 
            break;
        }

        

        if(iGrid==0 && iWhitehole==0)
        {
            if(geo.r > InnerHorizonR && lastR < InnerHorizonR) bIntoInHorizon = true;   //检测穿出(追踪方向)内视界    
            if(geo.r > EventHorizonR && lastR < EventHorizonR) bIntoOutHorizon = true;  //检测穿出(追踪方向)外视界    

            if (CurrentUniverseSign > 0.0 && !bIsNakedSingularity && iWhitehole==0)
            {
                float SafetyGap = 0.001;
                float ToHorizonGap = 0.2;
                float PhotonShellLimit = ProgradePhotonRadius - SafetyGap; 
                float preCeiling = min(CameraStartR - SafetyGap, TerminationR + ToHorizonGap);
                if(bIntoInHorizon)  { preCeiling = InnerHorizonR + ToHorizonGap; } //处理 射线从相机出发 -> 向外运动 -> 调头 -> 向内运动 -> 撞击内视界 的光
                if(bIntoOutHorizon) { preCeiling = EventHorizonR + ToHorizonGap; } //处理 射线从相机出发 -> 向外运动 -> 调头 -> 向内运动 -> 撞击外视界 的光
                
                float PruningCeiling = min(iInterRadiusRs, preCeiling);
                PruningCeiling = min(PruningCeiling, PhotonShellLimit); 
                PruningCeiling = min(PruningCeiling, iDensestarsurfaceR); 
            
                if (geo.r < PruningCeiling)
                {
                    float DrDlambda = dot(geo.grad_r, k1.X.xyz);
                    if (DrDlambda > 1e-4) 
                    {
                        bShouldContinueMarchRay = false;
                        bWaitCalBack = false;
                        if(iDEBUG==1) Result += vec4(0.0, 0., 0.3, 0.0);
                        break;  //视界判定情况2，对凝结在视界前的光提前剔除
                    }
                }
            }
        }
        //if(iWhitehole==0 && !bIsNakedSingularity&&  ( (  (geo.r > InnerHorizonR && geo.r < EventHorizonR ) || (geo.r > EventHorizonR && lastR < InnerHorizonR) || (lastR > EventHorizonR && geo.r < InnerHorizonR) ) && E_conserved<0.0)){ bShouldContinueMarchRay = false;bWaitCalBack = false; }
        //对动量和位置及其导数做自适应步长。对电荷做自适应步长（Q贡献r^-2项
        float rho = length(X.xz);
        float DistRing = sqrt(X.y * X.y + pow(rho - abs(PhysicalSpinA), 2.0));
        float Vel_Mag = length(k1.X); 
        float Force_Mag = length(k1.P);
        float Mom_Mag = length(P_cov);
        
        float PotentialTerm = (PhysicalQ * PhysicalQ) / (geo.r2 + 0.01);
        float QDamping = 1.0 / (1.0 + 1.0 * PotentialTerm); 
        
        float ADamping =1.0;
        if(!bIsNakedSingularity) {ADamping = mix(max(abs(iSpin),0.1), 1.0, clamp((geo.r - InnerHorizonR) / (0.5 - InnerHorizonR), 0.0, 1.0));}
        float ErrorTolerance = 0.5 * ADamping*QDamping;
        float StepGeo =  DistRing / (Vel_Mag + 1e-9);
        float StepForce = Mom_Mag / (Force_Mag + 1e-15);
        
        float dLambda = ErrorTolerance*min(StepGeo, StepForce);
        dLambda = max(dLambda, 1e-7); 

        vec4 LastX = X;
        vec4 LastP_cov=P_cov;
        LastX_ingoing = X_ingoing;
        LastP_cov_ingoing = P_cov_ingoing; // [新增] 缓存步进前的协变四动量
        // --------------------------------------------------

        GravityFade = CubicInterpolate(max(min(1.0 - ( DistanceToBlackHole - 100.0) / (RaymarchingBoundary - 100.0), 1.0), 0.0));
        
        vec4 P_contra_step = RaiseIndex(P_cov, geo);
        if (P_contra_step.w > 10000.0*iQuality && !bIsNakedSingularity && CurrentUniverseSign > 0.0) 
        { 
            bShouldContinueMarchRay = false; 
            bWaitCalBack = false;
            if(iDEBUG==1) Result += vec4(0.3, 0.3, 0.2, 0.0);  
            break; //视界判定情况3，凝结在视界
        }

        StepGeodesicRK4_Optimized(X, P_cov, E_conserved, -dLambda/iQuality, PhysicalSpinA, PhysicalQ, GravityFade, CurrentUniverseSign, isoutgoing, geo, k1);
        float deltar = geo.r - lastR;


        // 大概在这里分界，上面的内容基本上对 inout 不敏感，比如半径。

        // 将步进结果转换为统一的 Ingoing 系供后续渲染

        //获取当前步在 Ingoing 系下的状态
        X_ingoing = X;
        P_cov_ingoing = P_cov;
        if (isoutgoing) {
            transformKerrSchild_YSpin(X_ingoing, CurrentUniverseSign, P_cov_ingoing, CONST_M, PhysicalSpinA, PhysicalQ, isoutgoing);
        }

        
        vec3 StepVec_ingoing = X_ingoing.xyz - LastX_ingoing.xyz; // [修改] 直接用 LastX_ingoing
        float ActualStepLength_ingoing = length(StepVec_ingoing);


        float drdl = deltar / max(ActualStepLength_ingoing, 1e-9);

        float rotfact = clamp(1.0 + iBoostRot * dot(-StepVec_ingoing, vec3(X_ingoing.z, 0.0, -X_ingoing.x)) / ActualStepLength_ingoing / length(X_ingoing.xz) * clamp(iSpin, -1.0, 1.0), 0.0, 2.0);
        if( geo.r < 1.6 + pow(abs(iSpin), 0.666666)){
            ThetaInShell += ActualStepLength_ingoing / (0.5*lastR + 0.5*geo.r) / (1.0 + 1000.0*drdl*drdl) * rotfact * clamp(11.0 - 10.0*(iSpin*iSpin + iQ*iQ), 0.0, 1.0);
        }

        lastR = geo.r;
        
        if (LastX.y * X.y < 0.0) { 
            float t_cross = LastX.y / (LastX.y - X.y);
            float rho_cross = length(mix(LastX.xz, X.xz, t_cross));
            if (rho_cross < abs(PhysicalSpinA)) CurrentUniverseSign *= -1.0;
        }

       
        if (CurrentUniverseSign > 0.0 && iBlackHoleMassSol > 0.0 &&   int(33+iInWhichUniverse-universeoffset)%3==0       )
        {
           if(IsAccretionDiskVisible(iInterRadiusRs, iOuterRadiusRs, iThinRs, iHopper, iBrightmut, iDarkmut))
           {
               //Result = DiskColor(Result, X, LastX,  P_cov,LastP_cov, E_conserved,
               //              iInterRadiusRs, iOuterRadiusRs, iThinRs, iHopper, iBrightmut, iDarkmut, iReddening, iSaturation, DiskArgument, 
               //              iBlackbodyIntensityExponent, iRedShiftColorExponent, iRedShiftIntensityExponent, PeakTemperature, ShiftMax, 
               //              PhysicalSpinA, 
               //              PhysicalQ, isoutgoing, 
               //              ThetaInShell,
               //              RayMarchPhase,WP_CamX, WP_CamY, StokesQU 
               //              ); 

               Result = ImageDiskColor(Result, X, LastX, P_cov, LastP_cov,
                                       PhysicalSpinA, PhysicalQ, isoutgoing,
                                       CurrentUniverseSign, -dLambda/iQuality,
                                       iInterRadiusRs, iOuterRadiusRs,
                                       iRedShiftColorExponent, iRedShiftIntensityExponent);
           }
           //if(IsJetVisible(iAccretionRate, iJetBrightmut)){
           //    Result = JetColor(Result, ActualStepLength_ingoing, X_ingoing, LastX_ingoing, RayDir_ingoing, LastDir_ingoing, P_cov_ingoing, E_conserved,
           //                  iInterRadiusRs, iOuterRadiusRs, iJetRedShiftIntensityExponent, iJetBrightmut, iReddening, iJetSaturation, iAccretionRate, iJetShiftMax, 
           //                  clamp(PhysicalSpinA, -0.049, 0.049), 
           //                  PhysicalQ , false,
           //                  RayMarchPhase // <----- 补上被漏掉的这一项
           //                  ); 
           //}
        }
        // if (CurrentUniverseSign > 0.0 && iBlackHoleMassSol > 0.0 &&   int(33+iInWhichUniverse-universeoffset)%3==1       )
        //{
        //   if(IsAccretionDiskVisible(iInterRadiusRs, iOuterRadiusRs, iThinRs, iHopper, iBrightmut, iDarkmut))
        //   {
        //       Result = DiskColortoBlue(Result, ActualStepLength_ingoing, X_ingoing, LastX_ingoing, RayDir_ingoing, LastDir_ingoing, P_cov_ingoing, E_conserved,
        //                     iInterRadiusRs, iOuterRadiusRs, iThinRs, iHopper, iBrightmut, iDarkmut, iReddening, iSaturation, DiskArgument, 
        //                     iBlackbodyIntensityExponent, iRedShiftColorExponent, iRedShiftIntensityExponent, PeakTemperature, ShiftMax, 
        //                     clamp(PhysicalSpinA, -0.49, 0.49), 
        //                     PhysicalQ, false, 
        //                     ThetaInShell,
        //                     RayMarchPhase 
        //                     ); 
        //   }
        //   if(IsJetVisible(iAccretionRate, iJetBrightmut)){
        //       Result = JetColor(Result, ActualStepLength_ingoing, X_ingoing, LastX_ingoing, RayDir_ingoing, LastDir_ingoing, P_cov_ingoing, E_conserved,
        //                     iInterRadiusRs, iOuterRadiusRs, iJetRedShiftIntensityExponent, iJetBrightmut, iReddening, iJetSaturation, iAccretionRate, iJetShiftMax, 
        //                     clamp(PhysicalSpinA, -0.049, 0.049), 
        //                     PhysicalQ , false,
        //                     RayMarchPhase // <----- 补上被漏掉的这一项
        //                     ); 
        //   }
        //}
        if(iDensestarsurfaceR!=0.0&&(bIsNakedSingularity||iDensestarsurfaceR>EventHorizonR))
        {
            Result = DensestarColor(Result, X, LastX, P_cov,LastP_cov,
                        PhysicalSpinA, 
                        PhysicalQ, isoutgoing,
                        CurrentUniverseSign,-dLambda/iQuality);
        }
        if(iGrid==1)
        {
            // 传入真实的当前步坐标 X, LastX 以及当前的 isoutgoing 状态
            Result = GridColor(Result, X, LastX, 
                        P_cov, E_conserved,
                        PhysicalSpinA, 
                        PhysicalQ, isoutgoing,
                        CurrentUniverseSign);
        }
        else if(iGrid==2)
        {
            bool showInnerGrid=true;
            if(!bIsNakedSingularity&&iWhitehole==0&&CameraStartR>InnerHorizonR) showInnerGrid=false;
            Result = GridColorSimple(Result, X, LastX, P_cov,LastP_cov,
                        PhysicalSpinA, 
                        PhysicalQ, isoutgoing, // 新增 isoutgoing 传参
                        CurrentUniverseSign,-dLambda/iQuality,showInnerGrid);
        }
        //Result = DrawFallingWhiteDot(Result, X, LastX, P_cov,LastP_cov,
        //                PhysicalSpinA, 
        //                PhysicalQ, isoutgoing,
        //                CurrentUniverseSign,-dLambda/iQuality);
        if (Result.a > 0.99) { bShouldContinueMarchRay = false; bWaitCalBack = false; break; }
        

        Count++;
    }

    if (iDEBUG == 3)
    {
        // 将步数归一化（0.0 到 1.0），MaxStep 是循环的最大限制
        float stepHeat = float(Count) / MaxStep;
        
        // 也可以使用简单的热力图颜色：蓝色(少) -> 红色(多)
        res.AccumColor = vec4(mix(vec3(0.0, 0.0, 1.0), vec3(1.0, 0.0, 0.0), stepHeat), 1.0);
        
        // 或者简单的灰度显示：
        // res.AccumColor = vec4(vec3(stepHeat), 1.0);
        
        res.Status = 3.0; // 强制标记为不透明，直接输出颜色
        return res;
    }
    // 结果打包
    res.CurrentSign = CurrentUniverseSign;
    res.AccumColor  = Result;
    


    float EVPA = 0.5 * atan(StokesQU.y, StokesQU.x); 
    float PolIntensity = length(StokesQU);

    if (iPolarization==1) {
        float hue = (EVPA + kPi/2.0) / kPi;
        // 色相映射显示偏振参数
        vec3 polColor = clamp(abs(fract(hue + vec3(3.0, 2.0, 1.0)/3.0) * 6.0 - 3.0) - 1.0, 0.0, 1.0);
        res.AccumColor = vec4(polColor * PolIntensity, Result.a);

    } 
    else if (iPolarization==2) {
        // 马吕斯定律 (滤光片): I_out = 0.5 * (I_in + Q*cos(2θ) + U*sin(2θ))
        // 实际上这等价于计算光的主偏振方向和滤光片的偏振夹角 Δθ, 其透过率为 cos^2(Δθ)
        // 1. 获取像素内极化部分的光对应的占比（如果所有光都是完全偏振的，这里的除法则抵消）
        float Q = StokesQU.x;
        float U = StokesQU.y;
        
        float maxPolIntensity = max(PolIntensity, 1e-12);
        
        // 对于完全偏振光，滤光片将其强度在 [0, 1] 间变动
        float filterTransmission = 0.5 + 0.5 * (Q * cos(2.0 * iPolarizationAngle) + U * sin(2.0 * iPolarizationAngle)) / maxPolIntensity;
        
        // 保留原吸积盘上色，将偏振透射衰减系数相乘
        // 当光经过完美的偏振片时，如果相位正对则为亮度最高点，正交则变暗甚至接近黑。
        // 你可以通过降低 0.5 增加对暗部的限制或者乘某增亮系数平衡总体视觉亮度
        res.AccumColor.rgb *= filterTransmission;
    }





    //阴影剔除的 Debug 颜色
    if (bDeferredShadowCulling && !bIsNakedSingularity)
    {// 检查是否是因为撞到了我们设定的 TerminationR 而退出的
        float FinalR = length(X.xyz);
        // 在截断半径内，或不再继续步进）容差 +0.1防闪烁
        if (FinalR <= TerminationR + 0.1 || !bShouldContinueMarchRay)
        {
            if(iDEBUG==1) 
            {
                float RemainingAlpha = max(0.0, 1.0 - res.AccumColor.a);
                res.AccumColor.rgb += vec3(0.0, 0.5, 0.0) * RemainingAlpha;
                res.AccumColor.a = 1.0;
                res.Status = 3.0; 
            } else {
                res.AccumColor.a = 1.0;
                res.Status = 3.0;
            }
            return res;
        }
    }
    // 状态位定义:
    // 0.0 = Absorbed/Lost (视界/超时，且未被体积光完全遮挡)
    // 1.0 = Sky (Universe +1)
    // 2.0 = Antiverse (Universe -1)
    // 3.0 = Opaque (Result.a > 0.99，体积光完全遮挡，其边界与前三者交界不做检查)
    // 4.0 = last universe
    // 5.0 = last anti-universe

    //小数位.0 .2区分能量正负，用于上色平行宇宙
    if (Result.a > 0.99) {
        res.Status = 3.0; 
        res.EscapeDir = vec3(0.0); 
        res.FreqShift = 0.0;
    } 
    else if (bWaitCalBack) {
        KerrGeometry geo_sky;
        ComputeGeometryScalars(X_ingoing.xyz, PhysicalSpinA, PhysicalQ, GravityFade, CurrentUniverseSign, false, geo_sky); // 假定已经足够远，GravityFade取1即可，f会自然趋于0
        vec4 P_up_sky = RaiseIndex(P_cov_ingoing, geo_sky);
        
        // 天空盒方向使用Ingoing系局部动量方向
        res.EscapeDir = LocalToWorldRot * normalize(-P_up_sky.xyz);
        
        // 检测是否为 NaN 或 Inf
        bool isInvalid = any(isnan(res.EscapeDir)) || any(isinf(res.EscapeDir));
        
        if (DistanceToBlackHole > RaymarchingBoundary ) 
        {
            res.EscapeDir = LocalToWorldRot * normalize(-P_cov_ingoing.xyz);
        }
        if(isInvalid) 
        {
            res.EscapeDir = LocalToWorldRot * normalize(RayDir);
        }
        res.FreqShift = clamp(1.0 / max(1e-14, abs(E_conserved)), 1.0/iBackShiftMax, iBackShiftMax);
        if (iDEBUG == 4)
        {

            res.AccumColor = vec4(mix(vec3(0.0, 0.0, 1.0), vec3(1.0, 0.0, 0.0), 0.5*(log(iBackShiftMax)+log(res.FreqShift))/(log(iBackShiftMax))), 1.0);

        
            res.Status = 3.0; // 强制标记为不透明，直接输出颜色
            return res;
        }
        if (CurrentUniverseSign  > 0.0) res.Status = 1.0; 
        else res.Status = 2.0; 
        
    if (iWhitehole == 1 && !bIsNakedSingularity && universeoffset>0)
    {
        if (res.Status == 1.0)
            res.Status = 1.0+3.0*float(universeoffset);
        else if (res.Status == 2.0)
            res.Status = 2.0+3.0*float(universeoffset);
    }
    } 
    else {
        res.Status = 0.0; 
        res.EscapeDir = vec3(0.0);
        res.FreqShift = 0.0;
    }
    float energyFlag = (E_conserved >= 0.0) ? 0.0 : 0.2; 
    res.Status       +=  energyFlag;    

    return res;
}