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
// 工具函数 (噪声、插值、随机)
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
// 数学与颜色转换函数
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

vec3 WorldToBlackHoleSpace(vec4 Position, vec3 BlackHolePos, vec3 DiskNormal, vec3 DiskTangent)
{
    vec3 BlackHoleSpaceY = normalize(DiskNormal);  
    vec3 BlackHoleSpaceX = normalize(DiskTangent);
    BlackHoleSpaceX = normalize(BlackHoleSpaceX - dot(BlackHoleSpaceX, BlackHoleSpaceY) * BlackHoleSpaceY);
    vec3 BlackHoleSpaceZ = normalize(cross(BlackHoleSpaceX, BlackHoleSpaceY));

    mat4x4 Translate = mat4x4(1.0, 0.0, 0.0, -BlackHolePos.x,
                              0.0, 1.0, 0.0, -BlackHolePos.y,
                              0.0, 0.0, 1.0, -BlackHolePos.z,
                              0.0, 0.0, 0.0, 1.0);

    mat4x4 Rotate = mat4x4(BlackHoleSpaceX.x, BlackHoleSpaceX.y, BlackHoleSpaceX.z, 0.0,
                           BlackHoleSpaceY.x, BlackHoleSpaceY.y, BlackHoleSpaceY.z, 0.0,
                           BlackHoleSpaceZ.x, BlackHoleSpaceZ.y, BlackHoleSpaceZ.z, 0.0,
                           0.0,               0.0,               0.0,               1.0);

    Position = transpose(Rotate) * transpose(Translate) * Position;
    return Position.xyz;
}

vec3 TransformBLtoKS(vec3 p_bl, float a) {
    float r = length(p_bl);
    if (r < 1e-4) return p_bl;
    vec3 p_ks;
    p_ks.y = p_bl.y; 
    float factor = a / r;
    p_ks.x = p_bl.x + p_bl.z * factor;
    p_ks.z = p_bl.z - p_bl.x * factor; 
    return p_ks;
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

// 计算 Kerr-Schild 半径 r (Boyer-Lindquist r)
float KerrSchildRadius(vec3 p, float a, float r_sign) {
    float a2 = a * a;
    float rho2 = p.x * p.x + p.z * p.z; 
    float y2 = p.y * p.y;             
    
    float b = rho2 + y2 - a2;
    float r2 = 0.5 * (b + sqrt(b * b + 4.0 * a2 * y2));
    // 返回带符号的半径
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
    // 注意：如果是 Antiverse (r<0)，这里的 f 会变成负值 (排斥引力)
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
    float distToRing = sqrt(X.y * X.y + pow(rho - abs(a), 2.0));
    // 限制最小 eps 防止精度丢失，同时在奇环附近保持极小步长
    float eps = max(1e-4, 0.001 * distToRing); 
    
    vec4 G;
    float invTwoEps = 0.5 / eps; 
    
    vec4 dx = vec4(eps, 0.0, 0.0, 0.0);
    vec4 dy = vec4(0.0, eps, 0.0, 0.0);
    vec4 dz = vec4(0.0, 0.0, eps, 0.0);
    
    // X 和 Z 方向
    G.x = (Hamiltonian(X + dx, P, a, fade, current_r_sign) - Hamiltonian(X - dx, P, a, fade, current_r_sign));
    G.z = (Hamiltonian(X + dz, P, a, fade, current_r_sign) - Hamiltonian(X - dz, P, a, fade, current_r_sign));
    
    // Y 方向：检测穿越奇环
    vec4 X_plus = X + dy;
    float sign_plus = current_r_sign;
    if (length(X_plus.xz) < abs(a) && X.y * X_plus.y < 0.0) {
        sign_plus = -current_r_sign;
    }
    float H_plus = Hamiltonian(X_plus, P, a, fade, sign_plus);
    
    vec4 X_minus = X - dy;
    float sign_minus = current_r_sign;
    if (length(X_minus.xz) < abs(a) && X.y * X_minus.y < 0.0) {
        sign_minus = -current_r_sign;
    }
    float H_minus = Hamiltonian(X_minus, P, a, fade, sign_minus);
    
    G.y = H_plus - H_minus;
    G.w = 0.0;
    
    return G * invTwoEps;
}

void StepGeodesic(inout vec4 X, inout vec4 P, float dt, float a, float fade, float r_sign) {
    vec4 gradH = GradHamiltonian(X, P, a, fade, r_sign);
    P = P - dt * gradH;
    
    mat4 g_inv_old = GetInverseKerrMetric(X, a, fade, r_sign);
    {
        float A = g_inv_old[3][3];
        float B = 2.0 * dot(vec3(g_inv_old[0][3], g_inv_old[1][3], g_inv_old[2][3]), P.xyz);
        float C = dot(P.xyz, mat3(g_inv_old) * P.xyz);
        float disc = B * B - 4.0 * A * C;
        if (disc >= 0.0) {
            float sqrtDisc = sqrt(disc);
            float pt1 = (-B - sqrtDisc) / (2.0 * A);
            float pt2 = (-B + sqrtDisc) / (2.0 * A);
            P.w = (abs(pt1 - P.w) < abs(pt2 - P.w)) ? pt1 : pt2;
        }
    }
    
    X = X + dt * (g_inv_old * P);
    
    mat4 g_inv_new = GetInverseKerrMetric(X, a, fade, r_sign);
    {
        float A = g_inv_new[3][3];
        float B = 2.0 * dot(vec3(g_inv_new[0][3], g_inv_new[1][3], g_inv_new[2][3]), P.xyz);
        float C = dot(P.xyz, mat3(g_inv_new) * P.xyz);
        float disc = B * B - 4.0 * A * C;
        if (disc >= 0.0) {
            float sqrtDisc = sqrt(disc);
            float pt1 = (-B - sqrtDisc) / (2.0 * A);
            float pt2 = (-B + sqrtDisc) / (2.0 * A);
            P.w = (abs(pt1 - P.w) < abs(pt2 - P.w)) ? pt1 : pt2;
        }
    }
}

// =============================================================================
// 吸积盘与喷流 (保留原样注释)
// =============================================================================

//vec4 DiskColor(vec4 BaseColor, float StepLength, vec3 RayPos, vec3 LastRayPos,
//               vec3 RayDir, vec3 LastRayDir, vec3 BlackHolePos, vec3 DiskNormal,vec3 DiskTangen,
//               float InterRadius, float OuterRadius,float Thin,float Hopper , float Brightmut,float Darkmut,float Reddening,float Saturation,float DiskTemperatureArgument,
//               float BlackbodyIntensityExponent,float RedShiftColorExponent,float RedShiftIntensityExponent,
//               float PeakTemperature, float ShiftMax, float Spin) // <-- Added Spin
//{
//    vec3 CameraPos     = WorldToBlackHoleSpace(vec4(0.0, 0.0, 0.0, 1.0), BlackHolePos, DiskNormal, DiskTangen);
//    
//    // 将光线位置转到以黑洞为中心的坐标系 (假设 WorldToBlackHoleSpace 使得 Y 轴为自旋轴)
//    vec3 PosOnDisk     = WorldToBlackHoleSpace(vec4(RayPos, 1.0),        BlackHolePos, DiskNormal, DiskTangen);
//    vec3 LastPosOnDisk = WorldToBlackHoleSpace(vec4(LastRayPos, 1.0),    BlackHolePos, DiskNormal, DiskTangen);
//    vec3 DirOnDisk     = WorldToBlackHoleSpace(vec4(RayDir, 0.0),        BlackHolePos, DiskNormal, DiskTangen);
//
//    // [物理修正 1] 使用 Kerr-Schild 半径代替欧氏距离
//    // 在 KS 坐标系中，y 坐标未被混合，因此垂直距离 abs(pos.y) 依然代表偏离赤道面的距离
//    float PosR = KerrSchildRadius(PosOnDisk, Spin,1.0);
//    
//    float PosY = PosOnDisk.y;
//    float LPosY = LastPosOnDisk.y;
//    
//    // 过零点检测 (Sub-step intersection)
//    if( LPosY*PosY<0.0)
//    {
//        vec3 CPoint=(-PosOnDisk*LPosY+LastPosOnDisk*PosY)/(PosY-LPosY);
//        // 更新位置到精确的交点
//        PosOnDisk=CPoint+min(Thin,length(CPoint-LastPosOnDisk))*DirOnDisk*(-1.0+2.0*RandomStep(10000.0*(PosOnDisk.zx/OuterRadius), fract(iTime * 1.0 + 0.5)));
//        StepLength=length(PosOnDisk-LastPosOnDisk);
//        // 重新计算修正后的物理半径
//        PosR = KerrSchildRadius(PosOnDisk, Spin,1.0);
//        PosY = PosOnDisk.y;
//    }
//    
//    // 抖动厚度 (使用 KS 坐标的 xz 分量近似极坐标 theta 抖动，这在视觉上足够且不需要转换)
//    Thin+=max(0.0,(length(PosOnDisk.xz)-3.0)*Hopper);
// 
//    float NoiseLevel=max(0.0,2.0-0.6*Thin);
//    vec4 Color = vec4(0.0);
//    vec4 Result=vec4(0.0);
//    
//    // 使用物理半径 PosR (BL r) 进行边界判定，这保证了内缘 ISCO 的形状正确
//    if (abs(PosY) < Thin && PosR < OuterRadius && PosR > InterRadius)
//    {
//        float x=(PosR-InterRadius)/(OuterRadius-InterRadius);
//        float a_param=max(1.0,(OuterRadius-InterRadius)/(10.0));
//        float EffectiveRadius=(-1.+sqrt(1.+4.*a_param*a_param*x-4.*x*a_param))/(2.*a_param-2.);
//        float InterCloudEffectiveRadius=(PosR-InterRadius)/min(OuterRadius-InterRadius,12.0);
//        if(a_param==1.0){EffectiveRadius=x;}
//        
//        float DenAndThiFactor=Shape(EffectiveRadius, 0.9, 1.5);
//        if ((abs(PosY) < Thin * DenAndThiFactor) || (PosY < Thin * (1.0 - 5.0 * pow(InterCloudEffectiveRadius, 2.0))))
//        {
//            // [物理修正 2] 获取 Kerr 轨道角速度
//            float AngularVelocity  = GetKeplerianAngularVelocity(PosR, 1.0, Spin);
//            float HalfPiTimeInside = kPi / GetKeplerianAngularVelocity(3.0, 1.0, Spin);
//
//            float BH_M = 0.5; // Rs = 1.0, 所以 M = 0.5
//            
//// 1. 准备中间变量
//float u = sqrt(PosR);
//// 计算 k = (a*sqrt(M))^(1/3)。加入 1e-8 防止 Spin=0 时除零崩溃
//float s = Spin * 0.70710678; // 0.7071... 是 sqrt(0.5)
//float k = sign(s) * pow(max(abs(s), 1e-8), 1.0/3.0);
//float SpiralTheta = 0.0;
//            float InnerTheta= kPi / HalfPiTimeInside *iBlackHoleTime ;
//            float PosThetaForInnerCloud = Vec2ToTheta(PosOnDisk.zx, vec2(cos(0.666666*InnerTheta),sin(0.666666*InnerTheta)));
//            float PosTheta            = Vec2ToTheta(PosOnDisk.zx, vec2(cos(-SpiralTheta), sin(-SpiralTheta)));
//            float PosLogarithmicTheta            = Vec2ToTheta(PosOnDisk.zx, vec2(cos(-2.0*log(PosR)), sin(-2.0*log(PosR))));
//
//            // 温度分布：基于物理半径 r
//            float DiskTemperature = pow(DiskTemperatureArgument * pow(1.0/PosR,3.0) * max(1.0 - sqrt(InterRadius / PosR), 0.000001), 0.25);
//            
//            // [物理修正 3] 多普勒效应与红移
//            // Kerr 盘面速度场：v = Omega * r_cylindrical (近似)
//            // 实际上 KS 下切向矢量和 BL 下略有不同，但对于着色器计算 cross(Y, Pos) 是足够好的切向近似
//            vec3 CloudVelocity = AngularVelocity * cross(vec3(0., 1., 0.), PosOnDisk);
//            float RelativeVelocity = dot(-DirOnDisk, CloudVelocity);
//            
//            // 狭义相对论多普勒因子
//            float Dopler = sqrt((1.0 + RelativeVelocity) / (1.0 - RelativeVelocity));
//            
//            // 广义相对论引力红移因子 (Equatorial Plane g_tt component)
//            // Kerr metric on equator g_tt = -(1 - 2M/r). So gravitational redshift is ~ sqrt(1 - 1/r)
//            // 这一点和 Schwarzschild 一样，区别在于粒子靠得更近了 (r 更小)
//            float GravitationalRedshift = sqrt(max(1.0 - 1.0 / PosR, 0.000001)); 
//            // 假设相机在无穷远，忽略相机引力势
//            
//            float RedShift = Dopler * GravitationalRedshift;
//
//            float RotPosR=PosR+0.25/3.0*iBlackHoleTime;
//            float Density           =DenAndThiFactor;
//            float Thick             = Thin * Density* (0.4+0.6*clamp(Thin-0.5,0.0,2.5)/2.5 + (1.0-(0.4+0.6*clamp(Thin-0.5,0.0,2.5)/2.5))* SoftSaturate(GenerateAccretionDiskNoise(vec3(1.5 * PosTheta,RotPosR, 0.0), -0.7+NoiseLevel, 1.3+NoiseLevel, 80.0))); 
//            float ThickM = Thin * Density;
//            float VerticalMixFactor = 0.0;
//            float DustColor         = 0.0;
//            vec4  Color0            = vec4(0.0);
//            
//            if (abs(PosY) <Thin*  Density)
//            {
//                float Levelmut=0.91*log(1.0+(0.06/0.91*max(0.0,min(1000.0,PosR)-10.0)));
//                float Conmut=   80.0*log(1.0+(0.1*0.06*max(0.0,min(1000000.0,PosR)-10.0)));
//                Color0      =                                vec4(GenerateAccretionDiskNoise(vec3(0.1 * RotPosR, 0.1 * PosY , 0.02*pow(OuterRadius,0.7) * PosTheta), NoiseLevel+2.0-Levelmut, NoiseLevel+4.0-Levelmut, 80.0-Conmut)); 
//                if(PosTheta+kPi<0.1*kPi)
//                {
//                    Color0*=(PosTheta+kPi)/(0.1*kPi);
//                    Color0+=(1.0-((PosTheta+kPi)/(0.1*kPi)))*vec4(GenerateAccretionDiskNoise(vec3(0.1 * RotPosR, 0.1 * PosY , 0.02*pow(OuterRadius,0.7) * (PosTheta+2.0*kPi)), NoiseLevel+2.0-Levelmut, NoiseLevel+4.0-Levelmut, 80.0-Conmut));
//                }
//                if(PosR>max(0.15379*OuterRadius,0.15379*64.0))
//                {
//                    float Spir      =                                     (GenerateAccretionDiskNoise(vec3(0.1 * (PosR*(4.65114e-6)-0.1/3.0*iBlackHoleTime-0.08*OuterRadius*PosLogarithmicTheta), 0.1 * PosY , 0.02*pow(OuterRadius,0.7) * PosLogarithmicTheta), NoiseLevel+2.0-Levelmut, NoiseLevel+3.0-Levelmut, 80.0-Conmut)); 
//                    if(PosLogarithmicTheta+kPi<0.1*kPi)
//                    {
//                        Spir*=(PosLogarithmicTheta+kPi)/(0.1*kPi);
//                        Spir+=(1.0-((PosLogarithmicTheta+kPi)/(0.1*kPi)))*(GenerateAccretionDiskNoise(vec3(0.1 *(PosR*(4.65114e-6) -0.1/3.0*iBlackHoleTime-0.08*OuterRadius*(PosLogarithmicTheta+2.0*kPi)), 0.1 * PosY , 0.02*pow(OuterRadius,0.7) * (PosLogarithmicTheta+2.0*kPi)), NoiseLevel+2.0-Levelmut, NoiseLevel+3.0-Levelmut, 80.0-Conmut));
//                    }
//                    Color0*=(mix(1.0,clamp(0.7*Spir*1.5-0.5,0.0,3.0),0.5+0.5*max(-1.0,1.0-exp(-1.5*0.1*(100.0*PosR/max(OuterRadius,64.0)-20.0)))));
//                }
//
//                VerticalMixFactor = max(0.0, (1.0 - abs(PosY) / Thick));
//                Density    *= 0.7 * VerticalMixFactor * Density;
//                Color0.xyz *= Density * 1.4;
//                Color0.a   *= (Density)*(Density)/0.3;
//                Color0.xyz *=max(0.0, (0.2+2.0*sqrt(pow(PosY / Thick,2.0)+0.001)));
//            }
//            if (abs(PosY) < Thin * (1.0 - 5.0 * pow( InterCloudEffectiveRadius, 2.0)))
//            {
//              DustColor = max(1.0 - pow(PosY / (Thin * max(1.0 - 5.0 * pow(InterCloudEffectiveRadius, 2.0), 0.0001)), 2.0), 0.0) * GenerateAccretionDiskNoise(vec3(1.5 * fract((1.5 *  PosThetaForInnerCloud + kPi / HalfPiTimeInside *iBlackHoleTime) / 2.0 / kPi) * 2.0 * kPi, PosR , PosY ), 0., 6., 80.0);
//              Color0 += 0.02 * vec4(vec3(DustColor), 0.2 * DustColor) * sqrt(1.0001 - DirOnDisk.y * DirOnDisk.y) * min(1.0, Dopler * Dopler);
//            }
//           
//            Color =  Color0;
//            float BrightWithoutRedshift = 0.05*min(OuterRadius/(1000.0),1000.0/OuterRadius)+0.55 / exp(5.0*EffectiveRadius)*mix(0.2+0.8*abs(RayDir.y),1.0,clamp(Thick-0.8,0.2,1.0)); 
//            BrightWithoutRedshift *= pow(DiskTemperature/PeakTemperature, BlackbodyIntensityExponent); 
//            float VisionTemperature = DiskTemperature * pow(RedShift, RedShiftColorExponent); 
//            Color.xyz *= BrightWithoutRedshift * KelvinToRgb(VisionTemperature); 
//            Color.xyz *= min(pow(RedShift, RedShiftIntensityExponent),ShiftMax); 
//            Color.xyz *= min(1.0, 1.8 * (OuterRadius - PosR) / (OuterRadius - InterRadius)); 
//            Color.a*=0.125;
//            Color*=max(mix(vec4(5.0/(max(Thin,0.2)+(0.0+Hopper*0.5)*OuterRadius)),vec4(vec3(0.3+0.7*5.0/(Thin+(0.0+Hopper*0.5)*OuterRadius)),1.0),0.0*exp(-pow(20.0*PosR/OuterRadius,2.0)))
//                  , mix(vec4(100.0/OuterRadius),vec4(vec3(0.3+0.7*100.0/OuterRadius),1.0),exp(-pow(20.0*PosR/OuterRadius,2.0))));
//            Color.xyz*=mix(1.0,max(1.0,abs(DirOnDisk.y)/0.2),clamp(0.3-0.6*(ThickM-1.0),0.0,0.3));
//            Color *= StepLength ;
//        }
//    }
//    else
//    {
//        return BaseColor;
//    }
//    
//    Color.xyz*=Brightmut;
//    Color.a*=Darkmut;
//
//    float aR = 1.0+ Reddening*(1.0-1.0);
//    float aG = 1.0+ Reddening*(3.0-1.0);
//    float aB = 1.0+ Reddening*(6.0-1.0);
//    float Sum_rgb = (Color.r + Color.g + Color.b)*pow(1.0 - BaseColor.a, aG);
//    Sum_rgb *= 1.0;
//    
//    float r001 = 0.0;
//    float g001 = 0.0;
//    float b001 = 0.0;
//        
//    float Denominator = Color.r*pow(1.0 - BaseColor.a, aR) + Color.g*pow(1.0 - BaseColor.a, aG) + Color.b*pow(1.0 - BaseColor.a, aB);
//    if (Denominator > 0.000001)
//    {
//        r001 = Sum_rgb * Color.r * pow(1.0 - BaseColor.a, aR) / Denominator;
//        g001 = Sum_rgb * Color.g * pow(1.0 - BaseColor.a, aG) / Denominator;
//        b001 = Sum_rgb * Color.b * pow(1.0 - BaseColor.a, aB) / Denominator;
//        
//       r001 *= pow(3.0*r001/(r001+g001+b001),Saturation);
//       g001 *= pow(3.0*g001/(r001+g001+b001),Saturation);
//       b001 *= pow(3.0*b001/(r001+g001+b001),Saturation);
//    }
//
//    Result.r=BaseColor.r + r001;
//    Result.g=BaseColor.g + g001;
//    Result.b=BaseColor.b + b001;
//    Result.a=BaseColor.a + Color.a * pow((1.0 - BaseColor.a),1.0);
//    return Result;
//}
//
//// [注意] 增加了 Spin 参数
//vec4 JetColor(vec4 BaseColor,  float StepLength, vec3 RayPos, vec3 LastRayPos,
//               vec3 RayDir, vec3 LastRayDir, vec3 BlackHolePos, vec3 DiskNormal,vec3 DiskTangen,
//               float InterRadius, float OuterRadius,float JetRedShiftIntensityExponent, float JetBrightmut,float JetReddening,float JetSaturation,float AccretionRate,float JetShiftMax, float Spin) // <-- Added Spin
//{
//    vec3 CameraPos = WorldToBlackHoleSpace(vec4(0.0, 0.0, 0.0, 1.0), BlackHolePos, DiskNormal, DiskTangen);
//    vec3 PosOnDisk = WorldToBlackHoleSpace(vec4(RayPos, 1.0),        BlackHolePos, DiskNormal, DiskTangen);
//    vec3 DirOnDisk = WorldToBlackHoleSpace(vec4(RayDir, 0.0),        BlackHolePos, DiskNormal, DiskTangen);
//
//    // [物理修正] 使用 Kerr-Schild 半径
//    float PosR = KerrSchildRadius(PosOnDisk, Spin,1.0);
//    
//    // 喷流高度依然使用 Y (在 KS 下，Y 方向是自旋轴，未被混合)
//    float PosY = PosOnDisk.y;
//    vec4  Color            = vec4(0.0);
//            
//    bool NotInJet=true;        
//    vec4 Result=vec4(0.0);
//    
//    // 使用 PosR 进行距离判断，length(PosOnDisk.xz) 近似为圆柱半径
//    // 注意：在强自旋下，圆柱半径在 KS 空间会略微扭曲，但用 Euclidean distance for jet width 依然是很好的近似
//    if(length(PosOnDisk.xz)*length(PosOnDisk.xz)<2.0*InterRadius*InterRadius+0.03*0.03*PosY*PosY&&PosR<sqrt(2.0)*OuterRadius){
//            vec4 Color0;
//            NotInJet=false;
//
//            // 角速度也必须更新为 Kerr 版本，尽管喷流是垂直的，但内部湍流可能受拖曳影响
//            float InnerTheta= 3.0*GetKeplerianAngularVelocity(InterRadius,1.0, Spin) *(iBlackHoleTime-1.0/0.8*abs(PosY));
//            float Shape=1.0/sqrt(InterRadius*InterRadius+0.02*0.02*PosY*PosY);
//            float a=mix(0.7+0.3*PerlinNoise1D(0.3*(iBlackHoleTime-1.0/0.8*abs(abs(PosY)+100.0*(dot(PosOnDisk.xz,PosOnDisk.xz)/PosR)))/(OuterRadius/100.0)/(1.0/0.8)),1.0,exp(-0.01*0.01*PosY*PosY));
//            Color0=vec4(1.0,1.0,1.0,0.5)*max(0.0,1.0-5.0*Shape*
//                                         abs(1.0-pow(length(PosOnDisk.xz)*Shape,2.0)))*Shape;
//            Color0*=a;
//            Color0*=max(0.0,1.0-1.0*exp(-0.0001*PosY/InterRadius*PosY/InterRadius));
//            Color0*=exp(-4.0/(2.0)*PosR/OuterRadius*PosR/OuterRadius);
//            Color0*=0.5;
//            Color+=Color0;
//    }
//    float Wid=40.0*(sqrt(2.0*abs(PosY)/40.0+1.0)-1.0);
//    Wid=abs(PosY);
//    if(length(PosOnDisk.xz)<1.3*InterRadius+0.25*Wid&&length(PosOnDisk.xz)>0.7*InterRadius+0.15*Wid&&PosR<30.0*InterRadius){
//            vec4 Color1;
//            NotInJet=false;
//            float InnerTheta= 2.0*GetKeplerianAngularVelocity(InterRadius, 1.0, Spin) *(iBlackHoleTime-1.0/0.8*abs(PosY));
//            float Shape=1.0/(InterRadius+0.2*Wid);
//            
//            Color1=vec4(1.0,1.0,1.0,0.5)*max(0.0,1.0-2.0*
//                                         abs(1.0-pow(length(PosOnDisk.xz
//                                                          +0.2*(1.1-exp(-0.1*0.1*PosY*PosY))*
//                                                          (PerlinNoise1D   (0.35*(iBlackHoleTime-1.0/0.8*abs(PosY))/(1.0/0.8)   )    -0.5)
//                                                          *vec2(cos(0.666666*InnerTheta),-sin(0.666666*InnerTheta))
//                                                          )*Shape,2.0)));
//            Color1*=1.0-exp(-PosY/InterRadius*PosY/InterRadius);
//            Color1*=exp(-0.005*PosY/InterRadius*PosY/InterRadius);
//            Color1*=0.5;
//            Color+=Color1;
//    }
//    if(!NotInJet)
//    {
//            float JetTemperature = 100000.0;
//            Color.xyz *= KelvinToRgb(JetTemperature );
//            // 喷流速度主要是径向/垂直的，受旋转影响较小
//            float RelativeVelocity =-(DirOnDisk.y)*sqrt(1.0/(PosR))*sign(PosY);
//            float Dopler =  sqrt((1.0 + RelativeVelocity) / (1.0 - RelativeVelocity));
//            float RedShift = Dopler * sqrt(max(1.0 - 1.0 / PosR, 0.000001)) / sqrt(max(1.0 - 1.0 / length(CameraPos), 0.000001));
//            Color.xyz*=min(pow(RedShift,JetRedShiftIntensityExponent),JetShiftMax);
//            Color *= StepLength *JetBrightmut*(0.5+0.5*tanh(log(AccretionRate)+1.0));
//            Color.a*=0.0;
//    }
//    if(NotInJet) return BaseColor;
//
//    float aR = 1.0+ JetReddening*(1.0-1.0);
//    float aG = 1.0+ JetReddening*(2.5-1.0);
//    float aB = 1.0+ JetReddening*(4.5-1.0);
//    float Sum_rgb = (Color.r + Color.g + Color.b)*pow(1.0 - BaseColor.a, aG);
//    Sum_rgb *= 1.0;
//    float r001 = 0.0;
//    float g001 = 0.0;
//    float b001 = 0.0;
//    float Denominator = Color.r*pow(1.0 - BaseColor.a, aR) + Color.g*pow(1.0 - BaseColor.a, aG) + Color.b*pow(1.0 - BaseColor.a, aB);
//    if (Denominator > 0.000001)
//    {
//        r001 = Sum_rgb * Color.r * pow(1.0 - BaseColor.a, aR) / Denominator;
//        g001 = Sum_rgb * Color.g * pow(1.0 - BaseColor.a, aG) / Denominator;
//        b001 = Sum_rgb * Color.b * pow(1.0 - BaseColor.a, aB) / Denominator;
//       r001 *= pow(3.0*r001/(r001+g001+b001),JetSaturation);
//       g001 *= pow(3.0*g001/(r001+g001+b001),JetSaturation);
//       b001 *= pow(3.0*b001/(r001+g001+b001),JetSaturation);
//    }
//    Result.r=BaseColor.r + r001;
//    Result.g=BaseColor.g + g001;
//    Result.b=BaseColor.b + b001;
//    Result.a=BaseColor.a + Color.a * pow((1.0 - BaseColor.a),1.0);
//    return Result;
//}

// =============================================================================
// 主函数
// =============================================================================

void main()
{
    vec4  Result = vec4(0.0);
    vec2  FragUv = gl_FragCoord.xy / iResolution.xy;
    float Fov    = tan(iFovRadians / 2.0);

    // --- 0. 物理参数准备 ---
    float iSpinclamp = clamp(iSpin, -0.095, 0.095);
    float Zx = 1.0 + pow(1.0 - pow(iSpinclamp, 2.0), 0.333333333333333) * (pow(1.0 + pow(iSpinclamp, 2.0), 0.333333333333333) + pow(1.0 - iSpinclamp, 0.333333333333333)); 
    float RmsRatio = (3.0 + sqrt(3.0 * pow(iSpinclamp, 2.0) + Zx * Zx) - sqrt((3.0 - Zx) * (3.0 + Zx + 2.0 * sqrt(3.0 * pow(iSpinclamp, 2.0) + Zx * Zx)))) / 2.0; 
    float AccretionEffective = sqrt(1.0 - 1.0 / RmsRatio); 
    const float kPhysicsFactor = 1.52491e30; 
    float DiskArgument = kPhysicsFactor / iBlackHoleMassSol* (iMu / AccretionEffective) * (iAccretionRate );
    float PeakTemperature = pow(DiskArgument * 0.05665278, 0.25);
    
    float SmallStepBoundary = max(iOuterRadiusRs, 12.0);
    float ShiftMax          = 1.5; 
    float BackgroundShiftMax = 2.0;
    float BackgroundBlueShift = min(1.0 / sqrt(1.0 - 1.0 /max(length(iBlackHoleRelativePosRs.xyz), 1.001) + 0.005), BackgroundShiftMax);
    
    // 物理自旋参数
    float PhysicalSpinA = iSpin * CONST_M;
    float EventHorizonR = 0.5 + sqrt(max(0.0, 0.25 - PhysicalSpinA * PhysicalSpinA));
    float TerminationR = EventHorizonR ;
    float CurrentUniverseSign = iUniverseSign;

    // --- 1. 相机光线生成 ---
    vec2 Jitter = vec2(RandomStep(FragUv, fract(iTime * 1.0 + 0.5)), RandomStep(FragUv, fract(iTime * 1.0))) / iResolution;
    vec3 ViewDirLocal = FragUvToDir(FragUv + 0.5 * Jitter, Fov, iResolution);
    
    // 转换到世界空间方向
    vec3 RayDirWorld = normalize((iInverseCamRot * vec4(ViewDirLocal, 0.0)).xyz);
    vec3 CamToBHVecVisual = (iInverseCamRot * vec4(iBlackHoleRelativePosRs.xyz, 0.0)).xyz;
    vec3 RayPosVisual = -CamToBHVecVisual;
    vec3 RayPosWorld = RayPosVisual; 
    
    // 吸积盘参数转换
    vec3 DiskNormalWorld = normalize((iInverseCamRot * vec4(iBlackHoleRelativeDiskNormal.xyz, 0.0)).xyz);
    vec3 DiskTangentWorld = normalize((iInverseCamRot * vec4(iBlackHoleRelativeDiskTangen.xyz, 0.0)).xyz);

    // --- 2. 优化: 包围球快进 (Raymarching Skip) ---
    float RaymarchingBoundary = max(iOuterRadiusRs + 1.0, 501.0);
    float DistanceToBlackHole = length(RayPosWorld);
    
    bool bShouldContinueMarchRay = true;
    bool bWaitCalBack = false;
    
    float GravityFade = CubicInterpolate(max(min(1.0 - (0.01 * DistanceToBlackHole - 1.0) / 4.0, 1.0), 0.0));
    
    if (DistanceToBlackHole > RaymarchingBoundary) 
    {
        vec3 O = RayPosWorld;
        vec3 D = RayDirWorld;
        float r = RaymarchingBoundary - 1.0; 
        
        float b = dot(O, D);
        float c = dot(O, O) - r * r;
        float delta = b * b - c; 
        
        if (delta < 0.0) 
        {
            bShouldContinueMarchRay = false;
            bWaitCalBack = true;
        } 
        else 
        {
            float sqrtDelta = sqrt(delta);
            float t1 = -b - sqrtDelta; 
            float t2 = -b + sqrtDelta; 
            
            float tEnter = t1;
            if (tEnter > 0.0) {
                RayPosWorld = O + D * tEnter;
            } else if (t2 > 0.0) {
                 // 相机在内部
            } else {
                bShouldContinueMarchRay = false;
                bWaitCalBack = true;
            }
        }
    }

    // --- 3. 物理动量初始化 (Tetrad Basis Construction) ---
    vec4 X = vec4(RayPosWorld, 0.0);
    vec4 P_cov = vec4(0.0);

    if (bShouldContinueMarchRay) 
    {
        mat4 g_cov = GetKerrMetric(X, PhysicalSpinA, GravityFade, CurrentUniverseSign); 
        float g_tt = g_cov[3][3];

        if (g_tt < 0) 
        {
            vec3 R_flat = normalize(RayPosWorld);
            vec3 View_flat = normalize(RayDirWorld);
            
            float cosTheta = dot(View_flat, R_flat);
            float sinTheta = sqrt(max(1.0 - cosTheta * cosTheta, 0.0));
            
            // 获取初始切向 T (尚未正交化)
            vec3 T_flat;
            if (sinTheta < 1e-5) {
                vec3 arb = (abs(R_flat.y) > 0.9) ? vec3(1.0, 0.0, 0.0) : vec3(0.0, 1.0, 0.0);
                T_flat = normalize(cross(R_flat, arb));
            } else {
                T_flat = (View_flat - cosTheta * R_flat) / sinTheta;
            }

            // === 提取核心参数 ===
            float D = -g_tt; // 正数，极小
            vec3 g_ti = g_cov[3].xyz;
            mat3 g_sp = mat3(g_cov);

            // 投影量
            float K_r = dot(g_ti, R_flat);
            float K_t = dot(g_ti, T_flat);
            
            vec3 gS_r = g_sp * R_flat;
            vec3 gS_t = g_sp * T_flat;
            
            float S_rr = dot(R_flat, gS_r);
            float S_tt = dot(T_flat, gS_t);
            float S_rt = dot(R_flat, gS_t);

            // === 构建高精度正交基底 (Analytic Gram-Schmidt) ===
            // 径向基底
            vec3 Num_R = K_r * g_ti + D * gS_r;
            float NormSq_R = max(K_r * K_r + D * S_rr, 1e-20);
            float InvNorm_R = inversesqrt(NormSq_R);

            // 切向基底 (代数消除奇异性)
            float CrossTerm = K_t * S_rr - K_r * S_rt;
            float Overlap = K_r * K_t + D * S_rt;
            vec3 Res_T = CrossTerm * g_ti + (NormSq_R * gS_t - Overlap * gS_r);

            // 计算 Residual 对应的模长平方
            float Core_Norm = S_tt * K_r * K_r + S_rr * K_t * K_t - 2.0 * S_rt * K_r * K_t;
            float PolyNorm = max(Core_Norm + D * (S_tt * S_rr - S_rt * S_rt), 1e-20);

            // === 组合动量 ===
            float Scale_T = sqrt(D / (NormSq_R * PolyNorm));
            
            // 组合分子
            vec3 Total_Spatial = g_ti + 
                                 cosTheta * Num_R * InvNorm_R + 
                                 sinTheta * Res_T * Scale_T;

            // 时间分量固定，空间分量统一除以 sqrt(D)
            P_cov.w = -sqrt(D);
            P_cov.xyz = Total_Spatial * inversesqrt(D);
        }
        else 
        {
            bShouldContinueMarchRay = false;
            Result = vec4(0.0); 
        }
    }

    // --- 4. 循环变量准备 ---
    vec3 LastRayPos = RayPosWorld;
    vec3 LastRayDir = RayDirWorld; 
    int Count = 0;
    
    // --- 5. 光线追踪主循环 ---
    while (bShouldContinueMarchRay)
    {
        // 5.1 距离与边界检查
        vec3 CurrentPos = X.xyz;
        DistanceToBlackHole = length(CurrentPos);
        
        // 逃逸检查: 超过边界
        if (DistanceToBlackHole > RaymarchingBoundary) {
            bShouldContinueMarchRay = false;
            bWaitCalBack = true; 
            break;
        }
        
        // 落入检查: 进入视界
        float CurrentKerrR = KerrSchildRadius(CurrentPos, PhysicalSpinA, CurrentUniverseSign);
        
        if (CurrentUniverseSign > 0.0 && CurrentKerrR < TerminationR && abs(iSpin) <= 1.0) {
            bShouldContinueMarchRay = false;
            bWaitCalBack = false; // 确实落入黑洞
            break;
        }
        
        // 安全中断
        if (Count > 300) {
            if (abs(iSpin) <= 1.0) {
                bShouldContinueMarchRay = false;
                bWaitCalBack = true; 
                break;
            }
            else if (Count > 1000) {
                bShouldContinueMarchRay = false;
                bWaitCalBack = true; 
                break;
            }
        }

        // 5.2 步长计算 (Adaptive Step)
        float RayStep = 1.0;
        if (Count == 0) {
            RayStep *= RandomStep(FragUv, fract(iTime * 1.0));
        }

        // 奇环特征距离计算
        float rho = length(X.xz);
        float distToRing = mix(
            sqrt(X.y * X.y + pow(rho - abs(PhysicalSpinA), 2.0)), 
            KerrSchildRadius(X.xyz, PhysicalSpinA, 1.0), 
            clamp(smoothstep(0.45, 0.55, KerrSchildRadius(CurrentPos, PhysicalSpinA, 1.0)) - 10.0 * max(abs(iSpin) - 1.0, 0.0), 0.0, 1.0)
        );
        
        // 基础步长缩放
        float baseScaleNear = min(1.0 + 0.25 * max(distToRing - 12.0, 0.0), distToRing);
        float baseScaleFar  = distToRing;
        float t = clamp((distToRing - SmallStepBoundary) / SmallStepBoundary, 0.0, 1.0);
        float smoothT = t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
        RayStep *= mix(baseScaleNear, baseScaleFar, smoothT);

        // 吸积盘细节权重
        float diskWeight = 0.15 + 0.25 * clamp(0.5 * (0.5 * distToRing / max(10.0, SmallStepBoundary) - 1.0), 0.0, 1.0);
        RayStep *= diskWeight;

        // 5.3 物理积分 (Hamiltonian Step)
        GravityFade = CubicInterpolate(max(min(1.0 - (0.01 * DistanceToBlackHole - 1.0) / 4.0, 1.0), 0.0));
        mat4 g_inv = GetInverseKerrMetric(X, PhysicalSpinA, GravityFade, CurrentUniverseSign);
        vec4 V_contra = g_inv * P_cov;
        
        // 使用 4D 速度分量限制步长，防止视界附近时间爆炸
        float V_4d_Sum = length(V_contra.xyz) + abs(V_contra.w);
        float dLambda = 2.0 * RayStep / max(V_4d_Sum, 1e-8);
       
        vec3 PreStepPos = X.xyz;
        StepGeodesic(X, P_cov, dLambda, PhysicalSpinA, GravityFade, CurrentUniverseSign);
        vec3 PostStepPos = X.xyz;

        // 穿越检测与宇宙符号翻转
        if (PreStepPos.y * PostStepPos.y < 0.0) 
        {
            float t_cross = PreStepPos.y / (PreStepPos.y - PostStepPos.y);
            vec3 CrossPoint = mix(PreStepPos, PostStepPos, t_cross);
            float rho_cross = length(CrossPoint.xz);
            
            if (rho_cross < abs(PhysicalSpinA)) 
            {
                CurrentUniverseSign *= -1.0;
            }
        }

        // 5.4 计算几何信息
        vec3 StepVec = PostStepPos - PreStepPos;
        float ActualStepLength = length(StepVec);
        vec3 InstantDir = normalize(StepVec);
        
        // 5.5 吸积盘与喷流采样 (调用被注释的函数)
        if (CurrentUniverseSign > 0.0) {
            //  Result = DiskColor(Result, ActualStepLength, PostStepPos, PreStepPos, InstantDir, LastRayDir,
            //                     vec3(0.0), DiskNormalWorld, DiskTangentWorld,
            //                     iInterRadiusRs, iOuterRadiusRs, iThinRs, iHopper, iBrightmut, iDarkmut, iReddening, iSaturation, DiskArgument, 
            //                     iBlackbodyIntensityExponent, iRedShiftColorExponent, iRedShiftIntensityExponent, PeakTemperature, ShiftMax,clamp(PhysicalSpinA,-0.049,0.049));
            // 
            //  Result = JetColor(Result, ActualStepLength, PostStepPos, PreStepPos, InstantDir, LastRayDir,
            //                    vec3(0.0), DiskNormalWorld, DiskTangentWorld,
            //                    iInterRadiusRs, iOuterRadiusRs, iJetRedShiftIntensityExponent, iJetBrightmut, iReddening, iJetSaturation, iAccretionRate, iJetShiftMax,clamp(PhysicalSpinA,-0.049,0.049));
        }

        // 5.6 不透明度检查
        if (Result.a > 0.99) {
            bShouldContinueMarchRay = false;
            bWaitCalBack = false;
            break;
        }
        
        // 5.7 更新状态
        LastRayPos = PostStepPos;
        LastRayDir = InstantDir;
        Count++;
    }
    
    // --- 6. 背景采样 (Skybox) ---
    if (bWaitCalBack)
    {
        vec3 EscapeDir = normalize(LastRayDir);
        vec4 Backcolor = textureLod(iBackground, EscapeDir, min(1.0, textureQueryLod(iBackground, EscapeDir).x));
        
        if (CurrentUniverseSign < 0.0) {
            Backcolor = textureLod(iAntiground, EscapeDir, min(1.0, textureQueryLod(iBackground, EscapeDir).x));
        }
        
        vec4 TexColor = Backcolor;

        // 背景多普勒/蓝移处理
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

    // --- 7. 色调映射与 TAA ---
    float RedFactor   = 3.0 * Result.r / (Result.g + Result.b + Result.g);
    float BlueFactor  = 3.0 * Result.b / (Result.g + Result.b + Result.g);
    float GreenFactor = 3.0 * Result.g / (Result.g + Result.b + Result.g);
    float BloomMax    = 8.0;
    
    Result.r = min(-4.0 * log(1.0 - pow(Result.r, 2.2)), BloomMax * RedFactor);
    Result.g = min(-4.0 * log(1.0 - pow(Result.g, 2.2)), BloomMax * GreenFactor);
    Result.b = min(-4.0 * log(1.0 - pow(Result.b, 2.2)), BloomMax * BlueFactor);
    Result.a = min(-4.0 * log(1.0 - pow(Result.a, 2.2)), 4.0);
    
    // TAA 混合
    float BlendWeight = iBlendWeight;
    vec4 PrevColor = texelFetch(iHistoryTex, ivec2(gl_FragCoord.xy), 0);
    FragColor      = (BlendWeight) * Result + (1.0 - BlendWeight) * PrevColor;
}