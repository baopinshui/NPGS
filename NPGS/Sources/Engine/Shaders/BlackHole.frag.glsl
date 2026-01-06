#version 450
#pragma shader_stage(fragment)
#extension GL_EXT_samplerless_texture_functions : enable

#include "Common/CoordConverter.glsl"
#include "Common/NumericConstants.glsl"

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

// -----------------------------------------------------------------------------
// 工具函数 (保持原样)
// -----------------------------------------------------------------------------

float RandomStep(vec2 Input, float Seed)
{
    return fract(sin(dot(Input + fract(11.4514 * sin(Seed)), vec2(12.9898, 78.233))) * 43758.5453);
}

float CubicInterpolate(float x)
{
    return 3.0 * pow(x, 2) - 2.0 * pow(x, 3);
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
    float v0 = 2.0 * fract(sin(PosInt*12.9898) * 43758.5453) - 1.0;
    float v1 = 2.0 * fract(sin((PosInt+1.0)*12.9898) * 43758.5453) - 1.0;
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

vec3 KelvinToRgb(float Kelvin)
{
    if (Kelvin < 400.01) return vec3(0.0);
    float Teff     = (Kelvin - 6500.0) / (6500.0 * Kelvin * 2.2);
    vec3  RgbColor = vec3(0.0);
    RgbColor.r = exp(2.05539304e4 * Teff);
    RgbColor.g = exp(2.63463675e4 * Teff);
    RgbColor.b = exp(3.30145739e4 * Teff);
    float BrightnessScale = 1.0 / max(max(1.5*RgbColor.r, RgbColor.g), RgbColor.b);
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
    return color * factor/pow(color.r*color.r+2.25*color.g*color.g+0.36*color.b*color.b,0.5)*(0.1*(color.r+color.g+color.b)+0.9);
}

float GetKeplerianAngularVelocity(float Radius, float Rs) 
{
    return sqrt(Rs / (2.0 * (Radius - 1.5 * Rs) * Radius * Radius));
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

float Shape(float x, float Alpha, float Beta)
{
    float k = pow(Alpha + Beta, Alpha + Beta) / (pow(Alpha, Alpha) * pow(Beta, Beta));
    return k * pow(x, Alpha) * pow(1.0 - x, Beta);
}

// -----------------------------------------------------------------------------
// Kerr-Schild Geometry (Spin Axis = Y)
// -----------------------------------------------------------------------------

// 常量定义: Rs = 1.0 (Simulation Units) => M = 0.5
const float CONST_M = 0.5; 
const mat4  MINKOWSKI_METRIC = mat4(
    1, 0, 0, 0,
    0, 1, 0, 0,
    0, 0, 1, 0,
    0, 0, 0, -1
);

// 计算 Kerr-Schild 半径 r
// 针对 Spin Y: 方程为 (x^2+z^2)/(r^2+a^2) + y^2/r^2 = 1
float KerrSchildRadius(vec3 p, float a) {
    float a2 = a*a;
    float rho2 = p.x*p.x + p.z*p.z; // 轴向半径平方 (xz平面)
    float y2 = p.y*p.y;             // 轴方向分量
    
    // 求解双二次方程 r^4 - (rho^2 + y^2 - a^2)r^2 - a^2 y^2 = 0
    float b = rho2 + y2 - a2;
    float r2 = 0.5 * (b + sqrt(b*b + 4.0 * a2 * y2));
    return sqrt(r2);
}

// 获取 Kerr-Schild 度规的逆 g^uv
// g^uv = eta^uv - f l^u l^v
mat4 GetInverseKerrMetric(vec4 X, float a,float fade) {
    float r = KerrSchildRadius(X.xyz, a);
    float r2 = r*r;
    float a2 = a*a;
    
    // Y轴自旋下的零矢量 l^u (contravariant, in +++- signature)
    // l^u = (lx, ly, lz, -1) 
    // l_spatial component derivation for Y-spin:
    // x -> (rx + az)/(r^2+a^2)
    // y -> y/r
    // z -> (rz - ax)/(r^2+a^2)
    float denom = 1.0 / (r2 + a2);
    float lx = (r * X.x + a * X.z) * denom;
    float ly = X.y / r;
    float lz = (r * X.z - a * X.x) * denom;
    
    vec4 l_up = vec4(lx, ly, lz, -1.0); // Outgoing null vector convention? 
    // Usually standard KS form is written with l_mu = (1, ...) or similar. 
    // With metric (-+++) or (+---), standard is l is null.
    // In (+,+,+,-), eta(l,l) = lx^2+ly^2+lz^2 - (-1)^2 = 1 - 1 = 0. Correct.
    
    float f = (2.0 * CONST_M * r * r2) / (r2 * r2 + a2 * X.y * X.y)*fade;
    
    return MINKOWSKI_METRIC - f * outerProduct(l_up, l_up);
}

mat4 GetKerrMetric(vec4 X, float a,float fade) {
    float r = KerrSchildRadius(X.xyz, a);
    float r2 = r*r;
    float a2 = a*a;
    
    // Y轴自旋下的零矢量 l^u (contravariant, in +++- signature)
    // l^u = (lx, ly, lz, -1) 
    // l_spatial component derivation for Y-spin:
    // x -> (rx + az)/(r^2+a^2)
    // y -> y/r
    // z -> (rz - ax)/(r^2+a^2)
    float denom = 1.0 / (r2 + a2);
    float lx = (r * X.x + a * X.z) * denom;
    float ly = X.y / r;
    float lz = (r * X.z - a * X.x) * denom;
    
    vec4 l_down = vec4(lx, ly, lz, 1.0); // Outgoing null vector convention? 
    // Usually standard KS form is written with l_mu = (1, ...) or similar. 
    // With metric (-+++) or (+---), standard is l is null.
    // In (+,+,+,-), eta(l,l) = lx^2+ly^2+lz^2 - (-1)^2 = 1 - 1 = 0. Correct.
    
    float f = (2.0 * CONST_M * r * r2) / (r2 * r2 + a2 * X.y * X.y)*fade;
    
    return MINKOWSKI_METRIC + f * outerProduct(l_down, l_down);
}

// 哈密顿量 H = 0.5 * g^uv p_u p_v
float Hamiltonian(vec4 X, vec4 P, float a,float fade) {
    mat4 g_inv = GetInverseKerrMetric(X, a,fade);
    return 0.5 * dot(g_inv * P, P);
}

// 哈密顿量的梯度 (数值差分)
vec4 GradHamiltonian(vec4 X, vec4 P, float a, float fade) {
    float r = KerrSchildRadius(X.xyz, a);
    // 动态 epsilon：靠近黑洞时需要更小的采样间距
    float eps = max(0.005, r * 0.005); 
    
    vec4 G;
    float invTwoEps = 0.5 / eps; // 预计算 1/(2*eps)
    
    G.x = (Hamiltonian(X + vec4(eps, 0.0, 0.0, 0.0), P, a, fade) - Hamiltonian(X - vec4(eps, 0.0, 0.0, 0.0), P, a, fade)) * invTwoEps;
    G.y = (Hamiltonian(X + vec4(0.0, eps, 0.0, 0.0), P, a, fade) - Hamiltonian(X - vec4(0.0, eps, 0.0, 0.0), P, a, fade)) * invTwoEps;
    G.z = (Hamiltonian(X + vec4(0.0, 0.0, eps, 0.0), P, a, fade) - Hamiltonian(X - vec4(0.0, 0.0, eps, 0.0), P, a, fade)) * invTwoEps;
    G.w = 0.0;
    return G;
}

void StepGeodesic(inout vec4 X, inout vec4 P, float dt, float a, float fade) {
    // --- 第一步：动量更新 ---
    vec4 gradH = GradHamiltonian(X, P, a, fade);
    P = P - dt * gradH;

    // --- 第二步：第一次修正 (保证位移前的速度矢量是 null 的) ---
    // 使用旧位置 X 的度规进行修正
    mat4 g_inv_old = GetInverseKerrMetric(X, a, fade);
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

    // --- 第三步：位置更新 ---
    // 使用刚才修正过的动量 P 来移动位置，此时速度 v^u = g^uv * P_v 是严格 null 的
    X = X + dt * (g_inv_old * P);

    // --- 第四步：第二次修正 (保证落点物理状态) ---
    // 使用新位置 X_new 的度规进行最终修正
    mat4 g_inv_new = GetInverseKerrMetric(X, a, fade);
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
// -----------------------------------------------------------------------------
// 吸积盘与喷流 (保留原有逻辑，仅适配输入)
// -----------------------------------------------------------------------------

vec4 DiskColor(vec4 BaseColor, float StepLength, vec3 RayPos, vec3 LastRayPos,
               vec3 RayDir, vec3 LastRayDir, vec3 BlackHolePos, vec3 DiskNormal,vec3 DiskTangen,
               float InterRadius, float OuterRadius,float Thin,float Hopper , float Brightmut,float Darkmut,float Reddening,float Saturation,float DiskTemperatureArgument,
               float BlackbodyIntensityExponent,float RedShiftColorExponent,float RedShiftIntensityExponent,
               float PeakTemperature, float ShiftMax)
{
    // 这里传入的 RayPos 是 Kerr-Schild 坐标（以黑洞为中心），
    // 原函数需要它们相对于黑洞位置，所以 WorldToBlackHoleSpace 内部的平移是不需要的（如果直接传相对坐标）
    // 但原函数设计是可以处理任意位置黑洞。
    // 在本光路计算中，我们模拟是在“黑洞为原点”的系中进行的，所以传入 BlackHolePos = (0,0,0) 即可适配。
    
    vec3 CameraPos     = WorldToBlackHoleSpace(vec4(0.0, 0.0, 0.0, 1.0), BlackHolePos, DiskNormal, DiskTangen);
    vec3 PosOnDisk     = WorldToBlackHoleSpace(vec4(RayPos, 1.0),        BlackHolePos, DiskNormal, DiskTangen);
    vec3 LastPosOnDisk = WorldToBlackHoleSpace(vec4(LastRayPos, 1.0),    BlackHolePos, DiskNormal, DiskTangen);
    vec3 DirOnDisk     = WorldToBlackHoleSpace(vec4(RayDir, 0.0),        BlackHolePos, DiskNormal, DiskTangen);

    float PosR = length(PosOnDisk);
    float PosY = PosOnDisk.y;
    float LPosY = LastPosOnDisk.y;
    if( LPosY*PosY<0.0)
    {
        vec3 CPoint=(-PosOnDisk*LPosY+LastPosOnDisk*PosY)/(PosY-LPosY);
        PosOnDisk=CPoint+min(Thin,length(CPoint-LastPosOnDisk))*DirOnDisk*(-1.0+2.0*RandomStep(10000.0*(PosOnDisk.zx/OuterRadius), fract(iTime * 1.0 + 0.5)));
        StepLength=length(PosOnDisk-LastPosOnDisk);
        PosR = length(PosOnDisk);
        PosY = PosOnDisk.y;
    }
    
    Thin+=max(0.0,(length(PosOnDisk.xz)-3.0)*Hopper);
 
    float NoiseLevel=max(0.0,2.0-0.6*Thin);
    vec4 Color = vec4(0.0);
    vec4 Result=vec4(0.0);
    if (abs(PosY) < Thin && PosR < OuterRadius && PosR > InterRadius)
    {
        float x=(PosR-InterRadius)/(OuterRadius-InterRadius);
        float a=max(1.0,(OuterRadius-InterRadius)/(10.0));
        float EffectiveRadius=(-1.+sqrt(1.+4.*a*a*x-4.*x*a))/(2.*a-2.);
        float InterCloudEffectiveRadius=(PosR-InterRadius)/min(OuterRadius-InterRadius,12.0);
        if(a==1.0){EffectiveRadius=x;}
        
        float DenAndThiFactor=Shape(EffectiveRadius, 0.9, 1.5);
        if ((abs(PosY) < Thin * DenAndThiFactor) || (PosY < Thin * (1.0 - 5.0 * pow(InterCloudEffectiveRadius, 2.0))))
        {
            float AngularVelocity  = GetKeplerianAngularVelocity(PosR, 1.0);
            float HalfPiTimeInside = kPi / GetKeplerianAngularVelocity(3.0 , 1.0);

            float SpiralTheta=12.0*2.0/sqrt(3.0)*(atan(sqrt(0.6666666*(PosR)-1.0)));
            float InnerTheta= kPi / HalfPiTimeInside *iBlackHoleTime ;
            float PosThetaForInnerCloud = Vec2ToTheta(PosOnDisk.zx, vec2(cos(0.666666*InnerTheta),sin(0.666666*InnerTheta)));
            float PosTheta            = Vec2ToTheta(PosOnDisk.zx, vec2(cos(-SpiralTheta), sin(-SpiralTheta)));
            float PosLogarithmicTheta            = Vec2ToTheta(PosOnDisk.zx, vec2(cos(-2.0*log(PosR)), sin(-2.0*log(PosR))));

            float DiskTemperature = pow(DiskTemperatureArgument * pow(1.0/PosR,3.0) * max(1.0 - sqrt(InterRadius / PosR), 0.000001), 0.25);
            vec3 CloudVelocity = AngularVelocity * cross(vec3(0., 1., 0.), PosOnDisk);
            float RelativeVelocity = dot(-DirOnDisk, CloudVelocity);
            float Dopler = sqrt((1.0 + RelativeVelocity) / (1.0 - RelativeVelocity));
            float RedShift = Dopler ;//* sqrt(max(1.0 - 1.0 / PosR, 0.000001)) / sqrt(max(1.0 - 1.0 / length(CameraPos), 0.000001));

            float RotPosR=PosR+0.25/3.0*iBlackHoleTime;
            float Density           =DenAndThiFactor;
            float Thick             = Thin * Density* (0.4+0.6*clamp(Thin-0.5,0.0,2.5)/2.5 + (1.0-(0.4+0.6*clamp(Thin-0.5,0.0,2.5)/2.5))* SoftSaturate(GenerateAccretionDiskNoise(vec3(1.5 * PosTheta,RotPosR, 0.0), -0.7+NoiseLevel, 1.3+NoiseLevel, 80.0))); 
            float ThickM = Thin * Density;
            float VerticalMixFactor = 0.0;
            float DustColor         = 0.0;
            vec4  Color0            = vec4(0.0);
            
            if (abs(PosY) <Thin*  Density)
            {
                float Levelmut=0.91*log(1.0+(0.06/0.91*max(0.0,min(1000.0,PosR)-10.0)));
                float Conmut=   80.0*log(1.0+(0.1*0.06*max(0.0,min(1000000.0,PosR)-10.0)));
                Color0      =                                vec4(GenerateAccretionDiskNoise(vec3(0.1 * RotPosR, 0.1 * PosY , 0.02*pow(OuterRadius,0.7) * PosTheta), NoiseLevel+2.0-Levelmut, NoiseLevel+4.0-Levelmut, 80.0-Conmut)); 
                if(PosTheta+kPi<0.1*kPi)
                {
                    Color0*=(PosTheta+kPi)/(0.1*kPi);
                    Color0+=(1.0-((PosTheta+kPi)/(0.1*kPi)))*vec4(GenerateAccretionDiskNoise(vec3(0.1 * RotPosR, 0.1 * PosY , 0.02*pow(OuterRadius,0.7) * (PosTheta+2.0*kPi)), NoiseLevel+2.0-Levelmut, NoiseLevel+4.0-Levelmut, 80.0-Conmut));
                }
                if(PosR>max(0.15379*OuterRadius,0.15379*64.0))
                {
                    float Spir      =                                     (GenerateAccretionDiskNoise(vec3(0.1 * (PosR*(4.65114e-6)-0.1/3.0*iBlackHoleTime-0.08*OuterRadius*PosLogarithmicTheta), 0.1 * PosY , 0.02*pow(OuterRadius,0.7) * PosLogarithmicTheta), NoiseLevel+2.0-Levelmut, NoiseLevel+3.0-Levelmut, 80.0-Conmut)); 
                    if(PosLogarithmicTheta+kPi<0.1*kPi)
                    {
                        Spir*=(PosLogarithmicTheta+kPi)/(0.1*kPi);
                        Spir+=(1.0-((PosLogarithmicTheta+kPi)/(0.1*kPi)))*(GenerateAccretionDiskNoise(vec3(0.1 *(PosR*(4.65114e-6) -0.1/3.0*iBlackHoleTime-0.08*OuterRadius*(PosLogarithmicTheta+2.0*kPi)), 0.1 * PosY , 0.02*pow(OuterRadius,0.7) * (PosLogarithmicTheta+2.0*kPi)), NoiseLevel+2.0-Levelmut, NoiseLevel+3.0-Levelmut, 80.0-Conmut));
                    }
                    Color0*=(mix(1.0,clamp(0.7*Spir*1.5-0.5,0.0,3.0),0.5+0.5*max(-1.0,1.0-exp(-1.5*0.1*(100.0*PosR/max(OuterRadius,64.0)-20.0)))));
                }

                VerticalMixFactor = max(0.0, (1.0 - abs(PosY) / Thick));
                Density    *= 0.7 * VerticalMixFactor * Density;
                Color0.xyz *= Density * 1.4;
                Color0.a   *= (Density)*(Density)/0.3;
                Color0.xyz *=max(0.0, (0.2+2.0*sqrt(pow(PosY / Thick,2.0)+0.001)));
            }
            if (abs(PosY) < Thin * (1.0 - 5.0 * pow( InterCloudEffectiveRadius, 2.0)))
            {
              DustColor = max(1.0 - pow(PosY / (Thin * max(1.0 - 5.0 * pow(InterCloudEffectiveRadius, 2.0), 0.0001)), 2.0), 0.0) * GenerateAccretionDiskNoise(vec3(1.5 * fract((1.5 *  PosThetaForInnerCloud + kPi / HalfPiTimeInside *iBlackHoleTime) / 2.0 / kPi) * 2.0 * kPi, PosR , PosY ), 0., 6., 80.0);
              Color0 += 0.02 * vec4(vec3(DustColor), 0.2 * DustColor) * sqrt(1.0001 - DirOnDisk.y * DirOnDisk.y) * min(1.0, Dopler * Dopler);
            }
           
            Color =  Color0;
            float BrightWithoutRedshift = 0.05*min(OuterRadius/(1000.0),1000.0/OuterRadius)+0.55 / exp(5.0*EffectiveRadius)*mix(0.2+0.8*abs(RayDir.y),1.0,clamp(Thick-0.8,0.2,1.0)); 
            BrightWithoutRedshift *= pow(DiskTemperature/PeakTemperature, BlackbodyIntensityExponent); 
            float VisionTemperature = DiskTemperature * pow(RedShift, RedShiftColorExponent); 
            Color.xyz *= BrightWithoutRedshift * KelvinToRgb(VisionTemperature); 
            Color.xyz *= min(pow(RedShift, RedShiftIntensityExponent),ShiftMax); 
            Color.xyz *= min(1.0, 1.8 * (OuterRadius - PosR) / (OuterRadius - InterRadius)); 
            Color.a*=0.125;
            Color*=max(mix(vec4(5.0/(max(Thin,0.2)+(0.0+Hopper*0.5)*OuterRadius)),vec4(vec3(0.3+0.7*5.0/(Thin+(0.0+Hopper*0.5)*OuterRadius)),1.0),0.0*exp(-pow(20.0*PosR/OuterRadius,2.0)))
                  , mix(vec4(100.0/OuterRadius),vec4(vec3(0.3+0.7*100.0/OuterRadius),1.0),exp(-pow(20.0*PosR/OuterRadius,2.0))));
            Color.xyz*=mix(1.0,max(1.0,abs(DirOnDisk.y)/0.2),clamp(0.3-0.6*(ThickM-1.0),0.0,0.3));
            Color *= StepLength ;
        }
    }
    else
    {
        return BaseColor;
    }
    
    Color.xyz*=Brightmut;
    Color.a*=Darkmut;

    float aR = 1.0+ Reddening*(1.0-1.0);
    float aG = 1.0+ Reddening*(3.0-1.0);
    float aB = 1.0+ Reddening*(6.0-1.0);
    float Sum_rgb = (Color.r + Color.g + Color.b)*pow(1.0 - BaseColor.a, aG);
    Sum_rgb *= 1.0;
    
    float r001 = 0.0;
    float g001 = 0.0;
    float b001 = 0.0;
        
    float Denominator = Color.r*pow(1.0 - BaseColor.a, aR) + Color.g*pow(1.0 - BaseColor.a, aG) + Color.b*pow(1.0 - BaseColor.a, aB);
    if (Denominator > 0.000001)
    {
        r001 = Sum_rgb * Color.r * pow(1.0 - BaseColor.a, aR) / Denominator;
        g001 = Sum_rgb * Color.g * pow(1.0 - BaseColor.a, aG) / Denominator;
        b001 = Sum_rgb * Color.b * pow(1.0 - BaseColor.a, aB) / Denominator;
        
       r001 *= pow(3.0*r001/(r001+g001+b001),Saturation);
       g001 *= pow(3.0*g001/(r001+g001+b001),Saturation);
       b001 *= pow(3.0*b001/(r001+g001+b001),Saturation);
    }

    Result.r=BaseColor.r + r001;
    Result.g=BaseColor.g + g001;
    Result.b=BaseColor.b + b001;
    Result.a=BaseColor.a + Color.a * pow((1.0 - BaseColor.a),1.0);
    return Result;
}

vec4 JetColor(vec4 BaseColor,  float StepLength, vec3 RayPos, vec3 LastRayPos,
               vec3 RayDir, vec3 LastRayDir, vec3 BlackHolePos, vec3 DiskNormal,vec3 DiskTangen,
               float InterRadius, float OuterRadius,float JetRedShiftIntensityExponent, float JetBrightmut,float JetReddening,float JetSaturation,float AccretionRate,float JetShiftMax)
{
    vec3 CameraPos = WorldToBlackHoleSpace(vec4(0.0, 0.0, 0.0, 1.0), BlackHolePos, DiskNormal, DiskTangen);
    vec3 PosOnDisk = WorldToBlackHoleSpace(vec4(RayPos, 1.0),        BlackHolePos, DiskNormal, DiskTangen);
    vec3 DirOnDisk = WorldToBlackHoleSpace(vec4(RayDir, 0.0),        BlackHolePos, DiskNormal, DiskTangen);

    float PosR = length(PosOnDisk);
    float PosY = PosOnDisk.y;
    vec4  Color            = vec4(0.0);
            
    bool NotInJet=true;        
    vec4 Result=vec4(0.0);
    if(length(PosOnDisk.xz)*length(PosOnDisk.xz)<2.0*InterRadius*InterRadius+0.03*0.03*PosY*PosY&&PosR<sqrt(2.0)*OuterRadius){
            vec4 Color0;
            NotInJet=false;

            float InnerTheta= 3.0*GetKeplerianAngularVelocity(InterRadius,1.0) *(iBlackHoleTime-1.0/0.8*abs(PosY));
            float Shape=1.0/sqrt(InterRadius*InterRadius+0.02*0.02*PosY*PosY);
            float a=mix(0.7+0.3*PerlinNoise1D(0.3*(iBlackHoleTime-1.0/0.8*abs(abs(PosY)+100.0*(dot(PosOnDisk.xz,PosOnDisk.xz)/PosR)))/(OuterRadius/100.0)/(1.0/0.8)),1.0,exp(-0.01*0.01*PosY*PosY));
            Color0=vec4(1.0,1.0,1.0,0.5)*max(0.0,1.0-5.0*Shape*
                                         abs(1.0-pow(length(PosOnDisk.xz)*Shape,2.0)))*Shape;
            Color0*=a;
            Color0*=max(0.0,1.0-1.0*exp(-0.0001*PosY/InterRadius*PosY/InterRadius));
            Color0*=exp(-4.0/(2.0)*PosR/OuterRadius*PosR/OuterRadius);
            Color0*=0.5;
            Color+=Color0;
    }
    float Wid=40.0*(sqrt(2.0*abs(PosY)/40.0+1.0)-1.0);
    Wid=abs(PosY);
    if(length(PosOnDisk.xz)<1.3*InterRadius+0.25*Wid&&length(PosOnDisk.xz)>0.7*InterRadius+0.15*Wid&&PosR<30.0*InterRadius){
            vec4 Color1;
            NotInJet=false;
            float InnerTheta= 2.0*GetKeplerianAngularVelocity(InterRadius, 1.0) *(iBlackHoleTime-1.0/0.8*abs(PosY));
            float Shape=1.0/(InterRadius+0.2*Wid);
            
            Color1=vec4(1.0,1.0,1.0,0.5)*max(0.0,1.0-2.0*
                                         abs(1.0-pow(length(PosOnDisk.xz
                                                          +0.2*(1.1-exp(-0.1*0.1*PosY*PosY))*
                                                          (PerlinNoise1D   (0.35*(iBlackHoleTime-1.0/0.8*abs(PosY))/(1.0/0.8)   )    -0.5)
                                                          *vec2(cos(0.666666*InnerTheta),-sin(0.666666*InnerTheta))
                                                          )*Shape,2.0)));
            Color1*=1.0-exp(-PosY/InterRadius*PosY/InterRadius);
            Color1*=exp(-0.005*PosY/InterRadius*PosY/InterRadius);
            Color1*=0.5;
            Color+=Color1;
    }
    if(!NotInJet)
    {
            float JetTemperature = 100000.0;
            Color.xyz *= KelvinToRgb(JetTemperature );
            float RelativeVelocity =-(DirOnDisk.y)*sqrt(1.0/(PosR))*sign(PosY);
            float Dopler = 1.;//sqrt((1.0 + RelativeVelocity) / (1.0 - RelativeVelocity));
            float RedShift = Dopler * sqrt(max(1.0 - 1.0 / PosR, 0.000001)) / sqrt(max(1.0 - 1.0 / length(CameraPos), 0.000001));
            Color.xyz*=min(pow(RedShift,JetRedShiftIntensityExponent),JetShiftMax);
            Color *= StepLength *JetBrightmut*(0.5+0.5*tanh(log(AccretionRate)+1.0));
            Color.a*=0.0;
    }
    if(NotInJet) return BaseColor;

    float aR = 1.0+ JetReddening*(1.0-1.0);
    float aG = 1.0+ JetReddening*(2.5-1.0);
    float aB = 1.0+ JetReddening*(4.5-1.0);
    float Sum_rgb = (Color.r + Color.g + Color.b)*pow(1.0 - BaseColor.a, aG);
    Sum_rgb *= 1.0;
    float r001 = 0.0;
    float g001 = 0.0;
    float b001 = 0.0;
    float Denominator = Color.r*pow(1.0 - BaseColor.a, aR) + Color.g*pow(1.0 - BaseColor.a, aG) + Color.b*pow(1.0 - BaseColor.a, aB);
    if (Denominator > 0.000001)
    {
        r001 = Sum_rgb * Color.r * pow(1.0 - BaseColor.a, aR) / Denominator;
        g001 = Sum_rgb * Color.g * pow(1.0 - BaseColor.a, aG) / Denominator;
        b001 = Sum_rgb * Color.b * pow(1.0 - BaseColor.a, aB) / Denominator;
       r001 *= pow(3.0*r001/(r001+g001+b001),JetSaturation);
       g001 *= pow(3.0*g001/(r001+g001+b001),JetSaturation);
       b001 *= pow(3.0*b001/(r001+g001+b001),JetSaturation);
    }
    Result.r=BaseColor.r + r001;
    Result.g=BaseColor.g + g001;
    Result.b=BaseColor.b + b001;
    Result.a=BaseColor.a + Color.a * pow((1.0 - BaseColor.a),1.0);
    return Result;
}

// -----------------------------------------------------------------------------
// Main Loop
// -----------------------------------------------------------------------------

void main()
{
    vec4  Result = vec4(0.0);
    vec2  FragUv = gl_FragCoord.xy / iResolution.xy;
    float Fov    = tan(iFovRadians / 2.0);

    // --- 0. 物理参数准备 ---
    float Zx = 1.0 + pow(1.0 - pow(000, 2), 0.333333333333333) * (pow(1.0 + pow(000, 2), 0.333333333333333) + pow(1.0 - 000, 0.333333333333333)); 
    float RmsRatio = (3.0 + sqrt(3.0 * pow(000, 2) + Zx * Zx) - sqrt((3.0 - Zx) * (3.0 + Zx + 2.0 * sqrt(3.0 * pow(000, 2) + Zx * Zx)))) / 2.0;    
    float AccretionEffective = sqrt(1.0 - 1.0 / RmsRatio); 
    const float kPhysicsFactor = 1.52491e30; 
    float DiskArgument = kPhysicsFactor * (iMu / AccretionEffective) * (iAccretionRate / iBlackHoleMassSol);
    float PeakTemperature = pow(DiskArgument * 0.05665278, 0.25);
    
    float SmallStepBoundary = max(iOuterRadiusRs, 12.0);
    float ShiftMax          = 1.5; 
    float BackgroundShiftMax = 2.0;
    float BackgroundBlueShift = min(1.0 / sqrt(1.0 - 1.0 /max(length(iBlackHoleRelativePosRs.xyz),1.001)+0.005), BackgroundShiftMax);

    // 物理自旋参数 a = J/M. 单位 M=0.5 (Rs=1.0) -> a = iSpin * 0.5
    float PhysicalSpinA = iSpin * CONST_M;
    float EventHorizonR = 0.5 + sqrt(max(0.0, 0.25 - PhysicalSpinA * PhysicalSpinA));
    float TerminationR = EventHorizonR * 0.98;
    // --- 1. 相机光线生成 (World Space / Kerr-Schild Background) ---
    // 抖动 UV
    vec2 Jitter = vec2(RandomStep(FragUv, fract(iTime * 1.0 + 0.5)), RandomStep(FragUv, fract(iTime * 1.0))) / iResolution;
    vec3 ViewDirLocal = FragUvToDir(FragUv + 0.5 * Jitter, Fov, iResolution);
    
    // 坐标变换: Camera Local -> World (Kerr-Schild)
    vec3 RayDirWorld = normalize((iInverseCamRot * vec4(ViewDirLocal, 0.0)).xyz);
    
    // 相机在 World Space 的位置 (黑洞在 (0,0,0))
    // Uniform 输入的是 "黑洞相对于相机的位置"。取反即为相机相对于黑洞的位置。
    vec3 CamToBHVecWorld = (iInverseCamRot * vec4(iBlackHoleRelativePosRs.xyz, 0.0)).xyz;
    vec3 RayPosWorld = -CamToBHVecWorld;
    
    // 转换吸积盘参数到 World Space
    vec3 DiskNormalWorld = normalize((iInverseCamRot * vec4(iBlackHoleRelativeDiskNormal.xyz, 0.0)).xyz);
    vec3 DiskTangentWorld = normalize((iInverseCamRot * vec4(iBlackHoleRelativeDiskTangen.xyz, 0.0)).xyz);

    // --- 2. 优化: 包围球快进 (Fast-Forward to Bounding Sphere) ---
    // 恢复原有逻辑：如果相机在远处，且光线穿过包围球，直接跳跃到包围球表面。
    // 如果不穿过，直接计算背景。
    
    float RaymarchingBoundary = max(iOuterRadiusRs + 1.0, 501.0);
    float DistanceToBlackHole = length(RayPosWorld);
    
    bool bShouldContinueMarchRay = true;
    bool bWaitCalBack = false; // 标记是否需要计算背景
    
    float GravityFade = CubicInterpolate(max(min(1.0 - (0.01 * DistanceToBlackHole - 1.0) / 4.0, 1.0), 0.0));
    // 如果我们在包围球外部
    if (DistanceToBlackHole > RaymarchingBoundary) 
    {
        vec3 O = RayPosWorld;
        vec3 D = RayDirWorld;
        float r = RaymarchingBoundary - 1.0; // 稍微向内一点，避免浮点误差导致的反复进出
        
        // 射线-球体求交 (解 |O + tD|^2 = r^2)
        // t^2(D.D) + 2t(O.D) + O.O - r^2 = 0, D.D=1
        float b = dot(O, D);
        float c = dot(O, O) - r * r;
        float delta = b * b - c; // (b/2)^2 - ac, simplified quadratic
        
        if (delta < 0.0) 
        {
            // 光线完全错过包围球 -> 直接渲染背景
            bShouldContinueMarchRay = false;
            bWaitCalBack = true;
        } 
        else 
        {
            // 找到进入点
            float sqrtDelta = sqrt(delta);
            float t1 = -b - sqrtDelta; // 进入点
            float t2 = -b + sqrtDelta; // 离开点 (如果相机在内部则需要)
            
            float tEnter = t1;
            // 只有当进入点在前方时才跳转 (t > 0)
            if (tEnter > 0.0) {
                RayPosWorld = O + D * tEnter;
            } else if (t2 > 0.0) {
                 // 相机在球内? 理论上上面的 if (Distance > Boundary) 已经排除了这种情况，
                 // 但为了稳健性保留逻辑。
            } else {
                // 球在相机背后
                bShouldContinueMarchRay = false;
                bWaitCalBack = true;
            }
        }
    }

    // --- 3. 物理动量初始化 (Momentum Initialization) ---
    // 在确定了起始位置 RayPosWorld (可能是相机位置，也可能是快进后的位置) 后，初始化四维动量。
    // 求解 g_uv v^u v^v = 0 得到 dx/dt 的系数 k
    
    vec4 X = vec4(RayPosWorld, 0.0); // t=0
    vec4 P_cov = vec4(0.0); // 协变动量
    
    if (bShouldContinueMarchRay) 
    {
        // 获取正度规 g_uv
        mat4 g = GetKerrMetric(X, PhysicalSpinA,GravityFade); 
        
        vec3 D = RayDirWorld;
        
        // 解方程: g_00 + 2*k*g_0i*D^i + k^2*g_ij*D^i*D^j = 0
        // 注意 GLSL 矩阵是列主序，下标访问是 [col][row]
        // g_tt = g[3][3], g_ti = g[i][3] (或者 g[3][i] 对称), g_ij = g[j][i]
        
        // A = g_ij D^i D^j
        float A = dot(D, (mat3(g) * D)); 
        // B = 2 * g_ti D^i (注意 g 的第4列 g[3] 对应 t, 包含了 (gtx, gty, gtz, gtt))
        float B = 2.0 * dot(vec3(g[0][3], g[1][3], g[2][3]), D);
        // C = g_tt
        float C = g[3][3];
        
        float discrim = B*B - 4.0*A*C;
        // 如果判别式<0，说明此处无法形成零测地线（非常罕见，除非在能层内且方向极端），取max防止NaN
        float k = (-B + sqrt(max(0.0, discrim))) / (2.0 * A);
        
        vec4 V_contra = vec4(k * D, 1.0); // v^u = (k*dir, 1.0)
        P_cov = g * V_contra; // p_u = g_uv v^v
    }
    
    // --- 4. 循环变量准备 ---
    vec3 LastRayPos = RayPosWorld;
    vec3 LastRayDir = RayDirWorld; // 用于多普勒计算和背景采样
    int Count = 0;
    
    // --- 5. 光线追踪主循环 (Geodesic Integration) ---
    
    while (bShouldContinueMarchRay)
    {
        // 5.1 距离与边界检查
        vec3 CurrentPos = X.xyz;
        DistanceToBlackHole = length(CurrentPos);
        
        // 逃逸检查: 超过边界
        if (DistanceToBlackHole > RaymarchingBoundary) {
            bShouldContinueMarchRay = false;
            bWaitCalBack = true; // 逃逸到无穷远，需要画背景
            break;
        }
        
        // 落入检查: 进入视界
        float CurrentKerrR = KerrSchildRadius(CurrentPos, PhysicalSpinA);
        
        if (CurrentKerrR < 0.9*TerminationR && abs(iSpin)<=1.0) {
            bShouldContinueMarchRay = false;
            bWaitCalBack = false; // 确实落入黑洞
            break;
        }
        
        // 安全中断
        if (Count > 300) {
            if(abs(iSpin)<=1.0){
                bShouldContinueMarchRay = false;
                bWaitCalBack = true; 
                break;
            }
            else if(Count > 1000){
                bShouldContinueMarchRay = false;
                bWaitCalBack = true; 
                break;
            }
        }

        // 5.2 步长计算 (RayStep) - 保持原有逻辑以匹配吸积盘细节
         float RayStep = 1.0;
        
        // 1. 计算到奇环 (Ring Singularity) 的特征距离
        // 在你的代码中自旋轴为 Y，奇环位于 XZ 平面，半径为 PhysicalSpinA
        float rho = length(X.xz);
        float distToRing = sqrt(X.y * X.y + pow(rho - abs(PhysicalSpinA), 2.0));
        
        // 2. 基础步长缩放因子 (远场线性增长，近场高精度)
        // 使用 distToRing 代替 DistanceToBlackHole 以适配裸奇环几何
        float baseScaleNear = min(1.0 + 0.25 * max(distToRing - 12.0, 0.0), distToRing);
        float baseScaleFar  = distToRing;

        // 3. 构建 C2 连续的平滑过渡函数 (Quintic Smoothstep: 6t^5 - 15t^4 + 10t^3)
        // 避免在边界处 (SmallStepBoundary) 出现光线断层
        float t = clamp((distToRing - SmallStepBoundary) / SmallStepBoundary, 0.0, 1.0);
        float smoothT = t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
        
        // 混合近场逻辑与远场逻辑
        RayStep = mix(baseScaleNear, baseScaleFar, smoothT);

        // 4. 吸积盘细节权重 (自适应噪声频率)
        // 保证在吸积盘区域步长足够小以捕捉体积云噪声，同时保持平滑
        float diskWeight = 0.15 + 0.25 * clamp(0.5 * (0.5 * distToRing / max(10.0, SmallStepBoundary) - 1.0), 0.0, 1.0);
        RayStep *= diskWeight;

        // 5. 起步抖动 (抗锯齿)
        if (Count == 0) {
            RayStep *= RandomStep(FragUv, fract(iTime * 1.0));
        }
        
        // 5.3 物理积分 (Hamiltonian Step)
        // 将 RayStep 用作仿射参数步长 dLambda
        // 对于光子，在远场 |dx/dlambda| ≈ 1，所以 dLambda ≈ dLocalDistance
        GravityFade = CubicInterpolate(max(min(1.0 - (0.01 * DistanceToBlackHole - 1.0) / 4.0, 1.0), 0.0));
        mat4 g_inv = GetInverseKerrMetric(X, PhysicalSpinA, GravityFade);
        vec4 V_contra = g_inv * P_cov;
        
        // 【核心修复】
        // 原代码: float dLambda = RayStep / max(V_coord_mag, 1e-8);
        // 问题: 视界附近 V_contra.w (时间流速) 远大于空间速度。只除以空间速度会导致 dLambda 过大，
        // 进而导致 dt = dLambda * V_contra.w 爆炸。巨大的 dt 会导致动量更新 (P_new = P_old - dt * GradH) 产生巨大误差。
        
        // 修复: 使用 4D 速度分量的总和或长度来限制步长。
        // 这确保了 Δx 和 Δt 都不会超过 RayStep 的数量级。
        float V_4d_Sum = length(V_contra.xyz) + abs(V_contra.w);
        float dLambda = 2.0*RayStep / max(V_4d_Sum, 1e-8);

        // 记录积分前的位置
        vec3 PreStepPos = X.xyz;
        
        // 执行辛积分 (或原有的 Euler 积分，但现在 dLambda 安全了)
        StepGeodesic(X, P_cov, dLambda, PhysicalSpinA, GravityFade);
        
        vec3 PostStepPos = X.xyz;
        
        // 5.4 计算几何步长和瞬时方向 (用于吸积盘函数)
        vec3 StepVec = PostStepPos - PreStepPos;
        float ActualStepLength = length(StepVec);
        vec3 InstantDir = normalize(StepVec); // 瞬时空间运动方向
        
        // 5.5 吸积盘与喷流采样
        // 注意：DiskColor 和 JetColor 需要 World 坐标系下的参数
        // 我们传入的是 vec3(0.0) 作为 BlackHolePos，因为 RayPos 已经是相对黑洞的坐标了
        Result = DiskColor(Result, ActualStepLength, PostStepPos, PreStepPos, InstantDir, LastRayDir,
                           vec3(0.0), DiskNormalWorld, DiskTangentWorld,
                           iInterRadiusRs, iOuterRadiusRs, iThinRs, iHopper, iBrightmut, iDarkmut, iReddening, iSaturation, DiskArgument, 
                           iBlackbodyIntensityExponent, iRedShiftColorExponent, iRedShiftIntensityExponent, PeakTemperature, ShiftMax);

        Result = JetColor(Result, ActualStepLength, PostStepPos, PreStepPos, InstantDir, LastRayDir,
                          vec3(0.0), DiskNormalWorld, DiskTangentWorld,
                          iInterRadiusRs, iOuterRadiusRs, iJetRedShiftIntensityExponent, iJetBrightmut, iReddening, iJetSaturation, iAccretionRate, iJetShiftMax);

        // 5.6 不透明度检查
        if (Result.a > 0.99) {
            bShouldContinueMarchRay = false;
            bWaitCalBack = false; // 被物体遮挡，不需要计算背景
            break;
        }
        
        // 5.7 更新状态
        LastRayPos = PostStepPos;
        LastRayDir = InstantDir; // 更新为当前瞬时方向，如果是最后一步，这就是逃逸方向
        Count++;
    }
    
    // --- 6. 背景采样 (WaitCalBack Logic) ---
    if (bWaitCalBack)
    {
        // 此时 LastRayDir 是光线逃逸到无穷远处的方向（Kerr-Schild 坐标系）
        // 在远场，Kerr-Schild 笛卡尔坐标趋于平直，该方向即为天空盒采样方向。
        vec3 EscapeDir = normalize(LastRayDir);
        
        // 使用 textureLod 进行采样
        vec4 Backcolor = textureLod(iBackground, EscapeDir, min(1.0, textureQueryLod(iBackground, EscapeDir).x));
        vec4 TexColor = Backcolor;

        // 6.1 背景多普勒/蓝移处理 (保留原逻辑)
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
        
        // 6.2 混合背景
        Result += 0.7 * TexColor * (1.0 - Result.a);
    }

    // --- 7. 色调映射与 TAA (保留原逻辑) ---
    float RedFactor   = 3.0*Result.r / (Result.g+Result.b+Result.g);
    float BlueFactor  = 3.0*Result.b / (Result.g+Result.b+Result.g);
    float GreenFactor = 3.0*Result.g / (Result.g+Result.b+Result.g);
    float BloomMax   = 8.0;
    Result.r = min(-4.0 * log(1.0 - pow(Result.r, 2.2)), BloomMax * RedFactor);
    Result.g = min(-4.0 * log(1.0 - pow(Result.g, 2.2)), BloomMax * GreenFactor);
    Result.b = min(-4.0 * log(1.0 - pow(Result.b, 2.2)), BloomMax * BlueFactor);
    Result.a = min(-4.0 * log(1.0 - pow(Result.a, 2.2)), 4.0);
    
    // TAA 混合
    float BlendWeight = iBlendWeight;
    vec4 PrevColor = texelFetch(iHistoryTex, ivec2(gl_FragCoord.xy), 0);
    FragColor      = (BlendWeight) * Result + (1.0 - BlendWeight) * PrevColor;
}