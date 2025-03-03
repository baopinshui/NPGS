#version 450
#pragma shader_stage(fragment)
#extension GL_EXT_samplerless_texture_functions : enable

#include "Common/CoordConverter.glsl"
#include "Common/NumericConstants.glsl"

layout(location = 0) out vec4 FragColor;
layout(location = 0) in  vec2 TexCoordFromVert;

 layout(set = 0, binding = 0) uniform GameArgs
 {
    vec2  iResolution;                  // 视口分辨率
    float iFovRadians;                  // 视场角（弧度）
    float iTime;                        // 时间
    float iTimeDelta;                   // 时间间隔
    float iTimeRate;                    // 时间速率
 };

layout(set = 0, binding = 1) uniform BlackHoleArgs
{
    vec3  iWorldUpView;                 // 相机上方向
    vec3  iBlackHoleRelativePos;        // 黑洞位置
    vec3  iBlackHoleRelativeDiskNormal; // 吸积盘法线
    float iBlackHoleMassSol;            // 黑洞质量，单位太阳质量
    float iSpin;                        // 无量纲自旋参数
    float iMu;                          // 吸积物比荷的倒数
    float iAccretionRate;               // 吸积率
    float iInterRadiusLy;               // 盘内缘，单位光年
    float iOuterRadiusLy;               // 盘外缘，单位光年
    float iBlendWeight;
};

layout(set = 1, binding = 0) uniform texture2D iHistoryTex;
layout(set = 1, binding = 1) uniform sampler2D iBackground;

const float kSigma            = 5.670373e-8;
const float kLightYearToMeter = 9460730472580800.0;

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
    float Sx1 = 1.0 - Sx;
    float Sy1 = 1.0 - Sy;
    float Sz1 = 1.0 - Sz;
    
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

float GenerateDiskNoise(vec3 Position, int NoiseStartLevel, int NoiseEndLevel, float ContrastLevel)
{
    float NoiseAccumulator = 10.0;
    float NoiseFrequency   = 1.0;
    
    for (int Level = NoiseStartLevel; Level < NoiseEndLevel; ++Level)
    {
        NoiseFrequency = pow(3.0, float(Level));
        vec3 ScaledPosition = vec3(NoiseFrequency * Position.x,
                                   NoiseFrequency * Position.y,
                                   NoiseFrequency * Position.z);

        NoiseAccumulator *= (1.0 + 0.1 * PerlinNoise(ScaledPosition));
    }
    
    return log(1.0 + pow(0.1 * NoiseAccumulator, ContrastLevel));
}

float Vec2ToTheta(vec2 v1, vec2 v2)
{
    float VecDot   = dot(v1, v2);
    float VecCross = v1.x * v2.y - v1.y * v2.x;
    float Angle    = asin(0.999999 * VecCross / (length(v1) * length(v2)));

    float Dx = step(0.0, VecDot);   // VecDot   > 0 ? 1 : 0
    float Cx = step(0.0, VecCross); // VecCross > 0 ? 1 : 0
    
    return mix(mix(-kPi - Angle, kPi - Angle, Cx), // VecDot < 0
               Angle,                              // VecDot > 0
               Dx);
}

vec3 KelvinToRgb(float Kelvin)
{
    if (Kelvin < 400.01)
    {
        return vec3(0.0);
    }

    float Teff     = (Kelvin - 6500.0) / (6500.0 * Kelvin * 2.2);
    vec3  RgbColor = vec3(0.0);
    
    RgbColor.r = exp(2.05539304e4 * Teff);
    RgbColor.g = exp(2.63463675e4 * Teff);
    RgbColor.b = exp(3.30145739e4 * Teff);

    float BrightnessScale = 1.0 / max(max(RgbColor.r, RgbColor.g), RgbColor.b);
    
    if (Kelvin < 1000.0)
    {
        BrightnessScale *= (Kelvin - 400.0) / 600.0;
    }
    
    RgbColor *= BrightnessScale;
    return RgbColor;
}
vec3 WavelengthToRgb(float wavelength) {
    vec3 color = vec3(0.0);

    if (wavelength < 380.0 ) {
        return vec3(0.0,0.0,1.0);
    }
    if (wavelength > 750.0 ) {
        return vec3(1.0,0.0,0.0);
    }


    // 根据波长计算颜色
    if (wavelength >= 380.0 && wavelength < 440.0) {
        color.r =0.0;
        color.g = 0.0;
        color.b = 1.0;
    } else if (wavelength >= 440.0 && wavelength < 490.0) {
        color.r = 0.0;
        color.g = (wavelength - 440.0) / (490.0 - 440.0);
        color.b = 1.0;
    } else if (wavelength >= 490.0 && wavelength < 510.0) {
        color.r = 0.0;
        color.g = 1.0;
        color.b = -(wavelength - 510.0) / (510.0 - 490.0);
    } else if (wavelength >= 510.0 && wavelength < 580.0) {
        color.r = (wavelength - 510.0) / (580.0 - 510.0);
        color.g = 1.0;
        color.b = 0.0;
    } else if (wavelength >= 580.0 && wavelength < 645.0) {
        color.r = 1.0;
        color.g = -(wavelength - 645.0) / (645.0 - 580.0);
        color.b = 0.0;
    } else if (wavelength >= 645.0 && wavelength <= 750.0) {
        color.r = 1.0;
        color.g = 0.0;
        color.b = 0.0;
    }

    return color;
}
float GetKeplerianAngularVelocity(float Radius, float Rs)
{
    return sqrt(kSpeedOfLight / kLightYearToMeter * kSpeedOfLight * Rs / kLightYearToMeter /
                ((2.0 * Radius - 3.0 * Rs) * pow(Radius, 2)));
}

vec3 WorldToBlackHoleSpace(vec4 Position, vec3 BlackHolePos, vec3 DiskNormal,vec3 WorldUpView)
{
    if (DiskNormal == WorldUpView)
    {
        DiskNormal += 0.0001 * vec3(1.0, 0.0, 0.0);
    }

    vec3 BlackHoleSpaceY = normalize(DiskNormal);
    vec3 BlackHoleSpaceZ = normalize(cross(WorldUpView,     BlackHoleSpaceY));
    vec3 BlackHoleSpaceX = normalize(cross(BlackHoleSpaceY, BlackHoleSpaceZ));

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

vec3 ApplyBlackHoleRelativeotation(vec4 Position, vec3 BlackHolePos, vec3 DiskNormal,vec3 WorldUpView)
{
    if (DiskNormal == WorldUpView)
    {
        DiskNormal += 0.0001 * vec3(1.0, 0.0, 0.0);
    }

    vec3 BlackHoleSpaceY = normalize(DiskNormal);
    vec3 BlackHoleSpaceZ = normalize(cross(WorldUpView,     BlackHoleSpaceY));
    vec3 BlackHoleSpaceX = normalize(cross(BlackHoleSpaceY, BlackHoleSpaceZ));

    mat4x4 Rotate = mat4x4(BlackHoleSpaceX.x, BlackHoleSpaceX.y, BlackHoleSpaceX.z, 0.0,
                           BlackHoleSpaceY.x, BlackHoleSpaceY.y, BlackHoleSpaceY.z, 0.0,
                           BlackHoleSpaceZ.x, BlackHoleSpaceZ.y, BlackHoleSpaceZ.z, 0.0,
                           0.0,               0.0,               0.0,               1.0);

    Position = transpose(Rotate) * Position;
    return Position.xyz;
}

float Shape(float x, float Alpha, float Beta)
{
    float k = pow(Alpha + Beta, Alpha + Beta) / (pow(Alpha, Alpha) * pow(Beta, Beta));
    return k * pow(x, Alpha) * pow(1.0 - x, Beta);
}

vec4 DiskColor(vec4 BaseColor, float TimeRate, float StepLength, vec3 RayPos, vec3 LastRayPos,
               vec3 RayDir, vec3 LastRayDir, vec3 WorldUpView, vec3 BlackHolePos, vec3 DiskNormal,
               float Rs, float InterRadiusLy, float OuterRadiusLy, float DiskTemperatureArgument,
               float QuadraticedPeakTemperature, float ShiftMax)
{
    vec3 CameraPos = WorldToBlackHoleSpace(vec4(0.0, 0.0, 0.0, 1.0),  BlackHolePos, DiskNormal, WorldUpView);
    vec3 PosOnDisk = WorldToBlackHoleSpace(vec4(RayPos, 1.0),         BlackHolePos, DiskNormal, WorldUpView);
    vec3 DirOnDisk = ApplyBlackHoleRelativeotation(vec4(RayDir, 1.0), BlackHolePos, DiskNormal, WorldUpView);

    float PosR = length(PosOnDisk.zx);
    float PosY = PosOnDisk.y;

    vec4 Color = vec4(0.0);
    if (abs(PosY) < 0.5 * Rs && PosR < OuterRadiusLy && PosR > InterRadiusLy)
    {
        float EffectiveRadius = 1.0 - ((PosR - InterRadiusLy) / (OuterRadiusLy - InterRadiusLy) * 0.5);
        if ((OuterRadiusLy - InterRadiusLy) > 9.0 * Rs)
        {
            if (PosR < 5.0 * Rs + InterRadiusLy)
            {
                EffectiveRadius = 1.0 - ((PosR - InterRadiusLy) / (9.0 * Rs) * 0.5);
            }
            else
            {
                EffectiveRadius = 1.0 - (0.5 / 0.9 * 0.5 + ((PosR - InterRadiusLy) / (OuterRadiusLy - InterRadiusLy) -
                                  5.0 * Rs / (OuterRadiusLy - InterRadiusLy)) / (1.0 - 5.0 * Rs / (OuterRadiusLy - InterRadiusLy)) * 0.5);
            }
        }

        if ((abs(PosY) < 0.5 * Rs * Shape(EffectiveRadius, 4.0, 0.9)) ||
            (PosY < 0.5 * Rs * (1.0 - 5.0 * pow(2.0 * (1.0 - EffectiveRadius), 2.0))))
        {
            float AngularVelocity  = GetKeplerianAngularVelocity(PosR, Rs);
            float HalfPiTimeInside = kPi / GetKeplerianAngularVelocity(3.0 * Rs, Rs);
            float EffectiveTime0   = fract(iTime * TimeRate / (HalfPiTimeInside))       * HalfPiTimeInside + 0.0 * HalfPiTimeInside;
            float EffectiveTime1   = fract(iTime * TimeRate / (HalfPiTimeInside) + 0.5) * HalfPiTimeInside + 1.0 * HalfPiTimeInside;
            float PhaseTimeIndex0  = trunc(iTime * TimeRate / (HalfPiTimeInside));
            float PhaseTimeIndex1  = trunc(iTime * TimeRate / (HalfPiTimeInside) + 0.5);
            float Phase0           = k2Pi * fract(43758.5453 * sin(PhaseTimeIndex0));
            float Phase1           = k2Pi * fract(43758.5453 * sin(PhaseTimeIndex1));

            float PosThetaWithoutTime = Vec2ToTheta(PosOnDisk.zx, vec2(1.0, 0.0));
            float PosTheta            = fract((PosThetaWithoutTime + AngularVelocity * EffectiveTime0 + Phase0) / 2.0 / kPi) * k2Pi;

            float DiskTemperature  = pow(DiskTemperatureArgument * pow(Rs, 3) / pow(PosR, 3) * max(1.0 - sqrt(InterRadiusLy / PosR), 0.000001), 0.25);
            vec3  CloudVelocity    = kLightYearToMeter / kSpeedOfLight * AngularVelocity * cross(vec3(0.0, 1.0, 0.0), PosOnDisk);
            float RelativeVelocity = dot(-DirOnDisk, CloudVelocity);

            float Dopler   = sqrt((1.0 + RelativeVelocity) / (1.0 - RelativeVelocity));
            float RedShift = Dopler * sqrt(max(1.0 - Rs / PosR, 0.000001)) / sqrt(max(1.0 - Rs / length(CameraPos), 0.000001));

            float Density           = 0.0;
            float Thick             = 0.0;
            float VerticalMixFactor = 0.0;
            float DustColor         = 0.0;
            vec4  Color0            = vec4(0.0);
            vec4  Color1            = vec4(0.0);

            Density = Shape(EffectiveRadius, 4.0, 0.9);
            if (abs(PosY) < 0.5 * Rs * Density)
            {
                Thick = 0.5 * Rs * Density * (0.4 + 0.6 * SoftSaturate(GenerateDiskNoise(vec3(1.5 * PosTheta, PosR / Rs, 1.0), 1, 3, 80.0)));
                VerticalMixFactor = max(0.0, (1.0 - abs(PosY) / Thick));
                Density    *= 0.7 * VerticalMixFactor * Density;
                Color0      = vec4(GenerateDiskNoise(vec3(1.0 * PosR / Rs, 1.0 * PosY / Rs, 0.5 * PosTheta), 3, 6, 80.0));
                Color0.xyz *= Density * 1.4 * (0.2 + 0.8 * VerticalMixFactor + (0.8 - 0.8 * VerticalMixFactor) *
                              GenerateDiskNoise(vec3(PosR / Rs, 1.5 * PosTheta, PosY / Rs), 1, 3, 80.0));
                Color0.a   *= Density; // * (1.0 + VerticalMixFactor);
            }
            if (abs(PosY) < 0.5 * Rs * (1.0 - 5.0 * pow(2.0 * (1.0 - EffectiveRadius), 2.0)))
            {
                DustColor = max(1.0 - pow(PosY / (0.5 * Rs * max(1.0 - 5.0 * pow(2.0 * (1.0 - EffectiveRadius), 2.0), 0.0001)), 2.0), 0.0) *
                            GenerateDiskNoise(vec3(1.5 * fract((1.5 * PosThetaWithoutTime + kPi / HalfPiTimeInside * EffectiveTime0 + Phase0) / 2.0 / kPi) * k2Pi, PosR / Rs, PosY / Rs), 0, 6, 80.0);
                Color0 += 0.02 * vec4(vec3(DustColor), 0.2 * DustColor) * sqrt(1.0001 - DirOnDisk.y * DirOnDisk.y) * min(1.0, Dopler * Dopler);
            }
            Color0 *= 0.5 - 0.5 * cos(k2Pi * fract(iTime * TimeRate / (HalfPiTimeInside)));

            PosTheta = fract((PosThetaWithoutTime + AngularVelocity * EffectiveTime1 + Phase1) / (k2Pi)) * k2Pi;

            Density = Shape(EffectiveRadius, 4.0, 0.9);
            if (abs(PosY) < 0.5 * Rs * Density)
            {
                Thick = 0.5 * Rs * Density * (0.4 + 0.6 * SoftSaturate(GenerateDiskNoise(vec3(1.5 * PosTheta, PosR / Rs, 1.0), 1, 3, 80.0)));
                VerticalMixFactor = max(0.0, (1.0 - abs(PosY) / Thick));
                Density    *= 0.7 * VerticalMixFactor * Density;
                Color1      = vec4(GenerateDiskNoise(vec3(1.0 * PosR / Rs, 1.0 * PosY / Rs, 0.5 * PosTheta), 3, 6, 80.0));
                Color1.xyz *= Density * 1.4 * (0.2 + 0.8 * VerticalMixFactor + (0.8 - 0.8 * VerticalMixFactor) * GenerateDiskNoise(vec3(PosR / Rs, 1.5 * PosTheta, PosY / Rs), 1, 3, 80.0));
                Color1.a   *= Density; // * (1.0 + VerticalMixFactor);
            }
            if (abs(PosY) < 0.5 * Rs * (1.0 - 5.0 * pow(2.0 * (1.0 - EffectiveRadius), 2.0)))
            {
                DustColor = max(1.0 - pow(PosY / (0.5 * Rs * max(1.0 - 5.0 * pow(2.0 * (1.0 - EffectiveRadius), 2.0), 0.0001)), 2.0), 0.0) *
                            GenerateDiskNoise(vec3(1.5 * fract((1.5 * PosThetaWithoutTime + kPi / HalfPiTimeInside * EffectiveTime1 + Phase1) / 2.0 / kPi) * k2Pi, PosR / Rs, PosY / Rs), 0, 6, 80.0);
                Color1 += 0.02 * vec4(vec3(DustColor), 0.2 * DustColor) * sqrt(1.0001 - DirOnDisk.y * DirOnDisk.y) * min(1.0, Dopler * Dopler);
            }
            Color1 *= 0.5 - 0.5 * cos(k2Pi * fract(iTime * TimeRate / (HalfPiTimeInside) + 0.5));

            Color = Color1 + Color0;
            Color *= 1.0 + 20.0 * exp(-10.0 * (PosR - InterRadiusLy) / (OuterRadiusLy - InterRadiusLy));

            float BrightWithoutRedShift = 4.5 * pow(DiskTemperature, 4) / QuadraticedPeakTemperature;
            if (DiskTemperature > 1000.0)
            {
                DiskTemperature = max(1000.0, DiskTemperature * RedShift * Dopler * Dopler);
            }

            DiskTemperature = min(100000.0, DiskTemperature);

            Color.xyz *= BrightWithoutRedShift * min(1.0, 1.8 * (OuterRadiusLy - PosR) / (OuterRadiusLy - InterRadiusLy)) *
                         KelvinToRgb(DiskTemperature / exp((PosR - InterRadiusLy) / (0.6 * (OuterRadiusLy - InterRadiusLy))));
            Color.xyz *= min(ShiftMax, RedShift) * min(ShiftMax, Dopler);
            RedShift=min(RedShift, ShiftMax);
            Color.xyz *= pow((1.0 - (1.0 - min(1.0, RedShift)) * (PosR - InterRadiusLy) / (OuterRadiusLy - InterRadiusLy)), 9.0);
            Color.xyz *= min(1.0, 1.0 + 0.5 * ((PosR - InterRadiusLy) / InterRadiusLy + InterRadiusLy / (PosR - InterRadiusLy)) - max(1.0, RedShift));

            Color *= StepLength / Rs;
        }
    }

    return BaseColor + Color * (1.0 - BaseColor.a);
}

void main()
{
    vec4  Result = vec4(0.0);
    vec2  FragUv = gl_FragCoord.xy / iResolution.xy;
    float Fov    = tan(iFovRadians / 2.0);

    float Rs = 2.0 * iBlackHoleMassSol * kGravityConstant / pow(kSpeedOfLight, 2) * kSolarMass;
    float Zx = 1.0 + pow(1.0 - pow(iSpin, 2), 0.333333333333333) * (pow(1.0 + pow(iSpin, 2), 0.333333333333333) + pow(1.0 - iSpin, 0.333333333333333)); // 辅助变量
    float RmsRatio = (3.0 + sqrt(3.0 * pow(iSpin, 2) + Zx * Zx) - sqrt((3.0 - Zx) * (3.0 + Zx + 2.0 * sqrt(3.0 * pow(iSpin, 2) + Zx * Zx)))) / 2.0;     // 赤道顺行最内稳定圆轨与视界半径之比
    float AccretionEffective = sqrt(1.0 - 1.0 / RmsRatio); // 吸积放能效率，以落到最内稳定圆轨为基准

    float EddingtonAccRate = 6.327 * iMu / pow(kSpeedOfLight, 2) * iBlackHoleMassSol * kSolarMass / AccretionEffective;         // 爱丁顿吸积率
    float DiskArgument = 3.0 * kGravityConstant * kSolarMass / pow(Rs, 3) * iBlackHoleMassSol * iAccretionRate / (8.0 * kPi * kSigma);  // 吸积盘温度系数

    // 计算峰值温度的四次方，用于自适应亮度。峰值温度出现在 49 * iInterRadiusLy / 36 处
    float QuadraticedPeakTemperature = DiskArgument * 0.05665278;

    Rs = Rs / kLightYearToMeter;

    vec3  PosToBlackHole           = vec3(0.0);
    vec3  NormalizedPosToBlackHole = vec3(0.0);
    float DistanceToBlackHole      = 0.0;
    float ShiftMax                 = 1.25; // 设定一个蓝移的亮度增加上限，以免亮部太亮
    float BackgroundBlueShift= min(1.0 / sqrt(max(1.0 - Rs / length(iBlackHoleRelativePos), 0.000001)),ShiftMax);

    vec3  RayDir     = FragUvToDir(FragUv + 0.5 * vec2(RandomStep(FragUv, fract(iTime * 1.0 + 0.5)),
                                                       RandomStep(FragUv, fract(iTime * 1.0))) / iResolution, Fov, iResolution);
    vec3  RayPos     = vec3(0.0);
    float RayStep    = 0.0;
    vec3  LastRayPos = vec3(0.0);
    vec3  LastRayDir = vec3(0.0);
    float StepLength = 0.0;

    float LastDistance = length(PosToBlackHole);
    float CosTheta     = 0.0;
    float DeltaPhi     = 0.0;
    float DeltaPhiRate = 0.0;
    int   Count        = 0;

    bool bShouldContinueMarchRay = true;
    while (bShouldContinueMarchRay)
    {

        PosToBlackHole           = RayPos - iBlackHoleRelativePos;
        DistanceToBlackHole      = length(PosToBlackHole);
        NormalizedPosToBlackHole = PosToBlackHole / DistanceToBlackHole;

        if (DistanceToBlackHole > (2.5 * iOuterRadiusLy) && DistanceToBlackHole > LastDistance && (Count > 20))
        {
            bShouldContinueMarchRay = false;
            FragUv = DirToFragUv(vec3(RayDir.xy,RayDir.z), iResolution);
            vec4 Backcolor=texture(iBackground, vec2(FragUv.x,1.0-FragUv.y));
            vec3 Rcolor=Backcolor.r*WavelengthToRgb(680.0/BackgroundBlueShift);
            vec3 Gcolor=Backcolor.g*WavelengthToRgb(510.0/BackgroundBlueShift);
            vec3 Bcolor=Backcolor.b*WavelengthToRgb(440.0/BackgroundBlueShift);
            vec4 TexColor = vec4(Rcolor+Gcolor+Bcolor,Backcolor.a);
            Result += 0.7 * TexColor * (1.0 - Result.a);
        }
        if (DistanceToBlackHole < 0.1 * Rs)
        {
            bShouldContinueMarchRay = false;
        }
        if (bShouldContinueMarchRay)
        {
            Result = DiskColor(Result, iTimeRate, StepLength, RayPos, LastRayPos, RayDir, LastRayDir,
                               iWorldUpView, iBlackHoleRelativePos, iBlackHoleRelativeDiskNormal, Rs,
                               iInterRadiusLy, iOuterRadiusLy, DiskArgument, QuadraticedPeakTemperature, ShiftMax); // 吸积盘颜色
        }

        if (Result.a > 0.99)
        {
            bShouldContinueMarchRay = false;
        }

        LastRayPos   = RayPos;
        LastRayDir   = RayDir;
        LastDistance = DistanceToBlackHole;
        CosTheta     = length(cross(NormalizedPosToBlackHole, RayDir)); // 前进方向与切向夹角
        DeltaPhiRate = -1.0 * pow(CosTheta, 3) * (1.5 * Rs / DistanceToBlackHole); // 单位长度光偏折角

        if (Count == 0)
        {
            RayStep = RandomStep(FragUv, fract(iTime * 1.0)); // 光起步步长抖动
        }
        else
        {
            RayStep = 1.0;
        }

        RayStep *= 0.15 + 0.25 * min(max(0.0, 0.5 * (0.5 * DistanceToBlackHole / max(10.0 * Rs, iOuterRadiusLy) - 1.0)), 1.0);

        if ((DistanceToBlackHole) >= 2.0 * iOuterRadiusLy)
        {
            RayStep *= DistanceToBlackHole;
        }
        else if ((DistanceToBlackHole) >= 1.0 * iOuterRadiusLy)
        {
            RayStep *= ((Rs) * (2.0 * iOuterRadiusLy - DistanceToBlackHole) +
                        DistanceToBlackHole * (DistanceToBlackHole - iOuterRadiusLy)) / iOuterRadiusLy;
        }
        else
        {
            RayStep *= min(Rs,DistanceToBlackHole);
        }

        RayPos    += RayDir * RayStep;
        DeltaPhi   = RayStep / DistanceToBlackHole * DeltaPhiRate;
        RayDir     = normalize(RayDir + (DeltaPhi + DeltaPhi * DeltaPhi * DeltaPhi / 3.0) *
                     cross(cross(RayDir, NormalizedPosToBlackHole), RayDir) / CosTheta);  // 更新方向，里面的 (dthe + DeltaPhi ^ 3 / 3) 是 tan(dthe)
        StepLength = RayStep;

        ++Count;
    }

     float RedFactor  = Result.r / Result.g;
     float BlueFactor = Result.b / Result.g;
     float BloomMax   = 12.0;
     Result.r = min(-4.0 * log(1.0 - pow(Result.r, 2.2)), BloomMax * RedFactor);
     Result.g = min(-4.0 * log(1.0 - pow(Result.g, 2.2)), BloomMax);
     Result.b = min(-4.0 * log(1.0 - pow(Result.b, 2.2)), BloomMax * BlueFactor);
     Result.a = min(-4.0 * log(1.0 - pow(Result.a, 2.2)), 4.0);

    // TAA

    float BlendWeight = iBlendWeight;//1.0 - pow(0.5, (iTimeDelta) / max(min((0.131 * 36.0 / (iTimeRate) * (GetKeplerianAngularVelocity(3.0 * 0.00000465, 0.00000465)) / (GetKeplerianAngularVelocity(3.0 * Rs, Rs))), 0.3), 0.02));

    vec4 PrevColor = texelFetch(iHistoryTex, ivec2(gl_FragCoord.xy), 0);
    FragColor      = (BlendWeight) * Result + (1.0 - BlendWeight) * PrevColor;
}
