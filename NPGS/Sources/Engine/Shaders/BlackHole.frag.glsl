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
    float iGameTime;
    float iTimeDelta;                   // 时间间隔
    float iTimeRate;                    // 时间速率
 };

layout(set = 0, binding = 1) uniform BlackHoleArgs
{
    mat4x4 iInverseCamRot;
    vec4  iBlackHoleRelativePosRs;      // 相机系中黑洞位置
    vec4  iBlackHoleRelativeDiskNormal; // 相机系中吸积盘法向
    vec4  iBlackHoleRelativeDiskTangen; // 相机系中吸积盘切向

    float iBlackHoleTime;               // iGameTime*c/Rs
    float iBlackHoleMassSol;            // 黑洞质量，单位太阳质量
    float iSpin;                        // 无量纲自旋参数
    float iMu;                          // 吸积物比荷的倒数
    float iAccretionRate;               // 吸积率

    float iInterRadiusRs;                 // 盘内缘，单位倍Rs
    float iOuterRadiusRs;                 // 盘外缘，单位倍Rs
    float iThinRs;                        // 盘半厚，单位倍Rs
    float iHopper;                      // 盘形状漏斗系数
    float iBrightmut;                   // 总亮度倍率
    float iDarkmut;                     // 总不透明度倍率
    float iReddening;                   // 红化强度
    float iSaturation;                  // 盘饱和度
    float iBlackbodyIntensityExponent;  // 物理温度对亮度的影响
    float iRedShiftColorExponent;       // 视觉温度
    float iRedShiftIntensityExponent;   // 红蓝移对亮度的影响

    float iJetRedShiftIntensityExponent;// 红蓝移对喷流亮度的影响
    float iJetBrightmut;                // 喷流总亮度倍率
    float iJetSaturation;               // 喷流饱和度
    float iJetShiftMax;                 // 喷流蓝移限制
    
    float iBlendWeight;                 // TAA混合权重


};

layout(set = 1, binding = 0) uniform texture2D iHistoryTex;
layout(set = 1, binding = 1) uniform samplerCube iBackground;

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

    float BrightnessScale = 1.0 / max(max(1.5*RgbColor.r, RgbColor.g), RgbColor.b);
    
    if (Kelvin < 1000.0)
    {
        BrightnessScale *= (Kelvin - 400.0) / 600.0;
    }
    
    RgbColor *= BrightnessScale;
    return RgbColor;
}

vec3 WavelengthToRgb(float wavelength) {
    vec3 color = vec3(0.0);

    if (wavelength < 380.0 || wavelength > 750.0) {
        return color; 
    }

    if (wavelength >= 380.0 && wavelength < 440.0) {
        color.r = -(wavelength - 440.0) / (440.0 - 380.0);
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

    float factor = 0.0;
    if (wavelength >= 380.0 && wavelength < 420.0) {
        factor = 0.3 + 0.7 * (wavelength - 380.0) / (420.0 - 380.0);
    } else if (wavelength >= 420.0 && wavelength < 645.0) {
        factor = 1.0;
    } else if (wavelength >= 645.0 && wavelength <= 750.0) {
        factor = 0.3 + 0.7 * (750.0 - wavelength) / (750.0 - 645.0);
    }

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

vec4 DiskColor(vec4 BaseColor,  float StepLength, vec3 RayPos, vec3 LastRayPos,
               vec3 RayDir, vec3 LastRayDir, vec3 BlackHolePos, vec3 DiskNormal,vec3 DiskTangen,
               float InterRadius, float OuterRadius,float Thin,float Hopper , float Brightmut,float Darkmut,float Reddening,float Saturation,float DiskTemperatureArgument,
               float BlackbodyIntensityExponent,float RedShiftColorExponent,float RedShiftIntensityExponent,
               float PeakTemperature, float ShiftMax)
{
    vec3 CameraPos     = WorldToBlackHoleSpace(vec4(0.0, 0.0, 0.0, 1.0), BlackHolePos, DiskNormal, DiskTangen);
    vec3 PosOnDisk     = WorldToBlackHoleSpace(vec4(RayPos, 1.0),        BlackHolePos, DiskNormal, DiskTangen);
    vec3 LastPosOnDisk = WorldToBlackHoleSpace(vec4(LastRayPos, 1.0),    BlackHolePos, DiskNormal, DiskTangen);
    vec3 DirOnDisk     = WorldToBlackHoleSpace(vec4(RayDir, 0.0),        BlackHolePos, DiskNormal, DiskTangen);

    float PosR = length(PosOnDisk);
    float PosY = PosOnDisk.y;
    float LPosY = LastPosOnDisk.y;
    if( LPosY*PosY<0.0)//当光线穿过盘，让它停在盘上，以适应极薄的盘
    {
    
    vec3 CPoint=(-PosOnDisk*LPosY+LastPosOnDisk*PosY)/(PosY-LPosY);
    PosOnDisk=CPoint+min(Thin,length(CPoint-LastPosOnDisk))*DirOnDisk*(-1.0+2.0*RandomStep(10000.0*(PosOnDisk.zx/OuterRadius), fract(iTime * 1.0 + 0.5)));
    
    StepLength=length(PosOnDisk-LastPosOnDisk);
    PosR = length(PosOnDisk);
    PosY = PosOnDisk.y;
    }
    
    //Thin与DenAndThiFactor相乘构成轮廓
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
        if ((abs(PosY) < Thin * DenAndThiFactor) || (PosY < Thin * (1.0 - 5.0 * pow(InterCloudEffectiveRadius, 2.0))))//第一个条件是盘轮廓，第二个是薄盘内层团块云的轮廓
        {
            float AngularVelocity  = GetKeplerianAngularVelocity(PosR, 1.0);
            float HalfPiTimeInside = kPi / GetKeplerianAngularVelocity(3.0 , 1.0);

            float SpiralTheta=12.0*2.0/sqrt(3.0)*(atan(sqrt(0.6666666*(PosR)-1.0)));//盘以恒定径向速度向内收缩，同时走过轨迹使得切向速度是此处圆轨道速度
            float InnerTheta= kPi / HalfPiTimeInside *iBlackHoleTime ;
            float PosThetaForInnerCloud = Vec2ToTheta(PosOnDisk.zx, vec2(cos(0.666666*InnerTheta),sin(0.666666*InnerTheta)));
            float PosTheta            = Vec2ToTheta(PosOnDisk.zx, vec2(cos(-SpiralTheta), sin(-SpiralTheta)));
            float PosLogarithmicTheta            = Vec2ToTheta(PosOnDisk.zx, vec2(cos(-2.0*log(PosR)), sin(-2.0*log(PosR))));


            // 计算盘温度
            float DiskTemperature = pow(DiskTemperatureArgument * pow(1.0/PosR,3.0) * max(1.0 - sqrt(InterRadius / PosR), 0.000001), 0.25);
            // 计算云相对速度
            vec3 CloudVelocity = AngularVelocity * cross(vec3(0., 1., 0.), PosOnDisk);
            float RelativeVelocity = dot(-DirOnDisk, CloudVelocity);
            // 计算多普勒因子
            float Dopler = sqrt((1.0 + RelativeVelocity) / (1.0 - RelativeVelocity));
            // 总红移量，含多普勒因子和引力红移和
            float RedShift = Dopler * sqrt(max(1.0 - 1.0 / PosR, 0.000001)) / sqrt(max(1.0 - 1.0 / length(CameraPos), 0.000001));


            float RotPosR=PosR+0.25/3.0*iBlackHoleTime;
            
            float Density           =DenAndThiFactor;
            
            
            //厚度，后面的项在薄盘时制造起伏，但不会大于1
            float Thick             = Thin * Density* (0.4+0.6*clamp(Thin-0.5,0.0,2.5)/2.5 + (1.0-(0.4+0.6*clamp(Thin-0.5,0.0,2.5)/2.5))* SoftSaturate(GenerateAccretionDiskNoise(vec3(1.5 * PosTheta,RotPosR, 1.0), -0.7+NoiseLevel, 1.3+NoiseLevel, 80.0))); // 盘厚
            float ThickM = Thin * Density;
            float VerticalMixFactor = 0.0;
            float DustColor         = 0.0;
            
            vec4  Color0            = vec4(0.0);
            
            if (abs(PosY) <Thin*  Density)
            {
                float Levelmut=0.91*log(1.0+(0.06/0.91*max(0.0,min(1000.0,PosR)-10.0)));//越靠外，噪声空间频率越低
                float Conmut=   80.0*log(1.0+(0.1*0.06*max(0.0,min(1000000.0,PosR)-10.0)));
                //云以及一圈时平滑过渡
                Color0      =                                vec4(GenerateAccretionDiskNoise(vec3(0.1 * RotPosR, 0.1 * PosY , 0.02*pow(OuterRadius,0.7) * PosTheta), NoiseLevel+2.0-Levelmut, NoiseLevel+4.0-Levelmut, 80.0-Conmut)); // 云本体
                if(PosTheta+kPi<0.1*kPi)
                {
                    Color0*=(PosTheta+kPi)/(0.1*kPi);
                    Color0+=(1.0-((PosTheta+kPi)/(0.1*kPi)))*vec4(GenerateAccretionDiskNoise(vec3(0.1 * RotPosR, 0.1 * PosY , 0.02*pow(OuterRadius,0.7) * (PosTheta+2.0*kPi)), NoiseLevel+2.0-Levelmut, NoiseLevel+4.0-Levelmut, 80.0-Conmut));
                }
                //当前云的实现在大半径变成同心圆，所以在远处人为加上螺旋条纹
                if(PosR>max(0.15379*OuterRadius,0.15379*64.0))
                {
                    float Spir      =                                     (GenerateAccretionDiskNoise(vec3(0.1 * (PosR*(4.65114e-6)-0.1/3.0*iBlackHoleTime-0.08*OuterRadius*PosLogarithmicTheta), 0.1 * PosY , 0.02*pow(OuterRadius,0.7) * PosLogarithmicTheta), NoiseLevel+2.0-Levelmut, NoiseLevel+3.0-Levelmut, 80.0-Conmut)); // 云本体
                    if(PosLogarithmicTheta+kPi<0.1*kPi)
                    {
                        Spir*=(PosLogarithmicTheta+kPi)/(0.1*kPi);
                        Spir+=(1.0-((PosLogarithmicTheta+kPi)/(0.1*kPi)))*(GenerateAccretionDiskNoise(vec3(0.1 *(PosR*(4.65114e-6) -0.1/3.0*iBlackHoleTime-0.08*OuterRadius*(PosLogarithmicTheta+2.0*kPi)), 0.1 * PosY , 0.02*pow(OuterRadius,0.7) * (PosLogarithmicTheta+2.0*kPi)), NoiseLevel+2.0-Levelmut, NoiseLevel+3.0-Levelmut, 80.0-Conmut));
                    }
                    Color0*=(mix(1.0,clamp(0.7*Spir*1.5-0.5,0.0,3.0),0.5+0.5*max(-1.0,1.0-exp(-1.5*0.1*(100.0*PosR/max(OuterRadius,64.0)-20.0)))));
                }

                VerticalMixFactor = max(0.0, (1.0 - abs(PosY) / Thick));//密度离盘越远越低
                Density    *= 0.7 * VerticalMixFactor * Density;
                
                
                Color0.xyz *= Density * 1.4;
                Color0.a   *= (Density)*(Density)/0.3;
                Color0.xyz *=max(0.0, (0.2+2.0*sqrt(pow(PosY / Thick,2.0)+0.001)));//在中心加不透明度
            }
            if (abs(PosY) < Thin * (1.0 - 5.0 * pow( InterCloudEffectiveRadius, 2.0)))//薄盘内层团块云
            {
              DustColor = max(1.0 - pow(PosY / (Thin * max(1.0 - 5.0 * pow(InterCloudEffectiveRadius, 2.0), 0.0001)), 2.0), 0.0) * GenerateAccretionDiskNoise(vec3(1.5 * fract((1.5 *  PosThetaForInnerCloud + kPi / HalfPiTimeInside *iBlackHoleTime) / 2.0 / kPi) * 2.0 * kPi, PosR , PosY ), 0., 6., 80.0);
              Color0 += 0.02 * vec4(vec3(DustColor), 0.2 * DustColor) * sqrt(1.0001 - DirOnDisk.y * DirOnDisk.y) * min(1.0, Dopler * Dopler);
            }
           
           
           
            Color =  Color0;
//                                        超大盘增亮外侧                                           正常亮度衰减
            float BrightWithoutRedshift = 0.05*min(OuterRadius/(1000.0),1000.0/OuterRadius)+0.55 / exp(5.0*EffectiveRadius)*mix(0.2+0.8*abs(RayDir.y),1.0,clamp(Thick-0.8,0.2,1.0)); // 原亮度
            BrightWithoutRedshift *= pow(DiskTemperature/PeakTemperature, BlackbodyIntensityExponent); // 计算物理温度对亮度的影响。BlackbodyIntensityExponent的默认值为0.0，物理值为4.0。默认值较低是为了避免低温部分太暗。
            
            float VisionTemperature = DiskTemperature * pow(RedShift, RedShiftColorExponent); // 计算视觉温度。RedShiftColorExponent的默认值为3.0，物理值为1.0。默认值较高是为了增强红蓝对比。
            
            Color.xyz *= BrightWithoutRedshift * KelvinToRgb(VisionTemperature); 
            Color.xyz *= min(pow(RedShift, RedShiftIntensityExponent),ShiftMax); // 计算红蓝移对亮度的影响。RedShiftIntensityExponent的默认值为4.0，物理值为4.0。

            Color.xyz *= min(1.0, 1.8 * (OuterRadius - PosR) / (OuterRadius - InterRadius)); // 一个非物理的亮度更改，让外围更暗。1.0 + 0.5 * ((PosR - InterRadius) / InterRadius + InterRadius / (PosR - InterRadius)) - max(1.0, RedShift));
            
            
            Color.a*=0.125;
            
            //解耦总光深与外径
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
    //星际红化和饱和度控制

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
                                         abs(1.0-pow(length(PosOnDisk.xz
                                                          //+0.3*(1.1-exp(-0.01*0.01*PosY*PosY/Rs/Rs))*Rs*
                                                          //(PerlinNoise1D   (0.5*(iTime*TimeRate-kLightYearToMeter/0.8/kSpeedOfLight*abs(PosY))/Rs/(kLightYearToMeter/0.8/kSpeedOfLight)   )    -0.5)
                                                          //*vec2(cos(0.666666*InnerTheta),sin(0.666666*InnerTheta))
                                                          )*Shape,2.0)))*Shape;
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
                                                          )*Shape,2.0
                                                     )
                                             )
                                             )*Shape;
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
            float Dopler = sqrt((1.0 + RelativeVelocity) / (1.0 - RelativeVelocity));
            float RedShift = Dopler * sqrt(max(1.0 - 1.0 / PosR, 0.000001)) / sqrt(max(1.0 - 1.0 / length(CameraPos), 0.000001));
            Color.xyz*=min(pow(RedShift,JetRedShiftIntensityExponent),JetShiftMax);
            Color *= StepLength *JetBrightmut*(0.5+0.5*tanh(log(AccretionRate)+1.0));
            Color.a*=0.0;
    }
    if(NotInJet)
    {
        return BaseColor;
    }
        //星际红化和饱和度控制
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

void main()
{
    vec4  Result = vec4(0.0);
    vec2  FragUv = gl_FragCoord.xy / iResolution.xy;
    float Fov    = tan(iFovRadians / 2.0);

    float Zx = 1.0 + pow(1.0 - pow(iSpin, 2), 0.333333333333333) * (pow(1.0 + pow(iSpin, 2), 0.333333333333333) + pow(1.0 - iSpin, 0.333333333333333)); // 辅助变量
    float RmsRatio = (3.0 + sqrt(3.0 * pow(iSpin, 2) + Zx * Zx) - sqrt((3.0 - Zx) * (3.0 + Zx + 2.0 * sqrt(3.0 * pow(iSpin, 2) + Zx * Zx)))) / 2.0;     // 赤道顺行最内稳定圆轨与视界半径之比
    float AccretionEffective = sqrt(1.0 - 1.0 / RmsRatio); // 吸积放能效率，以落到最内稳定圆轨为基准
    
    const float kPhysicsFactor = 1.52491e30; 
    float DiskArgument = kPhysicsFactor * (iMu / AccretionEffective) * (iAccretionRate / iBlackHoleMassSol);

    // 计算峰值温度的四次方，用于自适应亮度。峰值温度出现在 49 * InterRadiusLy / 36 处
    float PeakTemperature = pow(DiskArgument * 0.05665278,0.25);


    float SmallStepBoundary=max(iOuterRadiusRs,12.0);

    float ShiftMax                 = 1.5; // 设定一个蓝移的亮度增加上限，以免亮部太亮
    float BackgroundShiftMax       = 2.0;
    float BackgroundBlueShift= min(1.0 / sqrt(1.0 - 1.0 /max( length(iBlackHoleRelativePosRs.xyz),1.001)+0.005),BackgroundShiftMax);

    vec3  RayPos     = vec3(0.0);
    vec3  RayDir     = FragUvToDir(FragUv + 0.5 * vec2(RandomStep(FragUv, fract(iTime * 1.0 + 0.5)),
                                                       RandomStep(FragUv, fract(iTime * 1.0))) / iResolution, Fov, iResolution);

    vec3  PosToBlackHole           = RayPos - iBlackHoleRelativePosRs.xyz;
    float DistanceToBlackHole      = length(PosToBlackHole);
    vec3  NormalizedPosToBlackHole = PosToBlackHole / DistanceToBlackHole;
    
    float SingularR=0.9;
    float RaymarchingBoundary=max(iOuterRadiusRs+1.0,501.0);
    if(length(iBlackHoleRelativePosRs.xyz)> SingularR){
        if(length(iBlackHoleRelativePosRs.xyz)<RaymarchingBoundary){
           RayDir=normalize(RayDir-NormalizedPosToBlackHole*dot(NormalizedPosToBlackHole,RayDir)*(-sqrt(max(1.0-CubicInterpolate(max(min(1.0-(DistanceToBlackHole-100.0)/(RaymarchingBoundary-100.0),1.0),0.0))/DistanceToBlackHole,0.00000000000000001))+1.0));
        }
        
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
        bool bShouldCalBackgroundAndReturn = false;
        bool bWaitCalBack=false;
        if(length(iBlackHoleRelativePosRs.xyz)>RaymarchingBoundary){
            vec3 O = RayPos;
            vec3 D = RayDir;
            vec3 C = iBlackHoleRelativePosRs.xyz;
            float r = RaymarchingBoundary-1.0;
            
            vec3 OC = O - C;
            float a = dot(D, D);
            float b = 2.0 * dot(D, OC);
            float c = dot(OC, OC) - r*r;
            float delta = b*b - 4.0*a*c;
            
            if (delta < 0.0) bShouldCalBackgroundAndReturn=true; 
            
            float sqrtDelta = sqrt(delta);
            float t1 = (-b - sqrtDelta) / (2.0*a);
            float t2 = (-b + sqrtDelta) / (2.0*a);
            float tEnter = min(t1, t2);
            float tExit = max(t1, t2);
            
            if (tExit < 0.0) bShouldCalBackgroundAndReturn=true; 
            
            float tStart = max(tEnter, 0.0);
            RayPos = O + D * tStart;
        }
        while (bShouldContinueMarchRay)
        {
        
            PosToBlackHole           = RayPos - iBlackHoleRelativePosRs.xyz;
            DistanceToBlackHole      = length(PosToBlackHole);
            NormalizedPosToBlackHole = PosToBlackHole / DistanceToBlackHole;
        
            if ((DistanceToBlackHole > (RaymarchingBoundary)  )||bShouldCalBackgroundAndReturn == true)//&& (Count > 20))
            {
                bShouldContinueMarchRay = false;
                bWaitCalBack=true;
                break;
            }
            if (DistanceToBlackHole < SingularR)
            {
                bShouldContinueMarchRay = false;
                break;
            }
            if (bShouldContinueMarchRay)
            {   Result = DiskColor(Result, StepLength, RayPos, LastRayPos, RayDir, LastRayDir,
                                    iBlackHoleRelativePosRs.xyz, iBlackHoleRelativeDiskNormal.xyz,iBlackHoleRelativeDiskTangen.xyz,
                                    iInterRadiusRs, iOuterRadiusRs,iThinRs,iHopper, iBrightmut,iDarkmut,iReddening,iSaturation, DiskArgument,  iBlackbodyIntensityExponent,iRedShiftColorExponent,iRedShiftIntensityExponent, PeakTemperature, ShiftMax);  // 吸积盘颜色
                Result = JetColor(Result, StepLength, RayPos, LastRayPos, RayDir, LastRayDir,
                                   iBlackHoleRelativePosRs.xyz, iBlackHoleRelativeDiskNormal.xyz,iBlackHoleRelativeDiskTangen.xyz, 
                                    iInterRadiusRs, iOuterRadiusRs,iJetRedShiftIntensityExponent,iJetBrightmut,iReddening, iJetSaturation, iAccretionRate,iJetShiftMax);  // 喷流颜色   

            }
        
            if (Result.a > 0.99)
            {
                bShouldContinueMarchRay = false;
                break;
            }
        
            LastRayPos   = RayPos;
            LastRayDir   = RayDir;
            LastDistance = DistanceToBlackHole;
            CosTheta     = length(cross(NormalizedPosToBlackHole, RayDir)); // 前进方向与切向夹角
            DeltaPhiRate = -1.0 * pow(CosTheta, 3) * (1.5 / DistanceToBlackHole)*CubicInterpolate(max(min(1.0-(0.01*DistanceToBlackHole-1.0)/4.0,1.0),0.0)); // 单位长度光偏折角
        
            if (Count == 0)
            {
                RayStep = RandomStep(FragUv, fract(iTime * 1.0)); // 光起步步长抖动
            }
            else
            {
                RayStep = 1.0;
            }
        
            RayStep *= 0.15 + 0.25 * min(max(0.0, 0.5 * (0.5 * DistanceToBlackHole / max(10.0 , SmallStepBoundary) - 1.0)), 1.0);

            if ((DistanceToBlackHole) >= 2.0 * SmallStepBoundary)
            {
                RayStep *= DistanceToBlackHole;
            }
            else if ((DistanceToBlackHole) >= 1.0 * SmallStepBoundary)
            {
                RayStep *= ((1.0+0.25*max(DistanceToBlackHole-12.0,0.0)) * (2.0 * SmallStepBoundary - DistanceToBlackHole) +
                            DistanceToBlackHole * (DistanceToBlackHole - SmallStepBoundary)) / SmallStepBoundary;
            }
            else
            {
                RayStep *= min(1.0+0.25*max(DistanceToBlackHole-12.0,0.0),DistanceToBlackHole);
            }
        
            
            DeltaPhi   = RayStep / DistanceToBlackHole * DeltaPhiRate;
        
            RayPos    += RayDir * RayStep;
            RayDir     = normalize(RayDir + (DeltaPhi + DeltaPhi * DeltaPhi * DeltaPhi / 3.0) *
                         cross(cross(RayDir, NormalizedPosToBlackHole), RayDir) / CosTheta);  // 更新方向，里面的 (dthe + DeltaPhi ^ 3 / 3) 是 tan(dthe)
            StepLength = RayStep;
        //    lucheng+=RayStep;
            ++Count;
        }
        if(bWaitCalBack==true){
                //FragUv = DirToFragUv((iInverseCamRot*vec4(RayDir,1.0)).xyz, iResolution);
                vec4 Backcolor=textureLod(iBackground, (iInverseCamRot * vec4(RayDir, 1.0)).xyz,min(1.0, textureQueryLod(iBackground, (iInverseCamRot*vec4(RayDir,1.0)).xyz).x));
                vec4 TexColor=Backcolor;
                if( length(iBlackHoleRelativePosRs.xyz)<200.0){
                    vec3 Rcolor=Backcolor.r*1.0*WavelengthToRgb(max(453,645.0/BackgroundBlueShift));
                    vec3 Gcolor=Backcolor.g*1.5*WavelengthToRgb(max(416,510.0/BackgroundBlueShift));
                    vec3 Bcolor=Backcolor.b*0.6*WavelengthToRgb(max(380,440.0/BackgroundBlueShift));
                    vec3 Scolor=Rcolor+Gcolor+Bcolor;
                    float OStrength=0.3*Backcolor.r+0.6*Backcolor.g+0.1*Backcolor.b;
                    float RStrength=0.3*Scolor.r+0.6*Scolor.g+0.1*Scolor.b;
                    Scolor*=OStrength/max(RStrength,0.001);
                    TexColor = vec4(Scolor,Backcolor.a)*BackgroundBlueShift*BackgroundBlueShift*BackgroundBlueShift*BackgroundBlueShift;
                }
               //if(gl_FragCoord.x/ iResolution.x<0.5&&gl_FragCoord.y/ iResolution.y<0.5){
               // TexColor=vec4(0.0,log(1+Count)/10.0,0.0,1.0);
               //}else if(gl_FragCoord.x/ iResolution.x<0.5&&gl_FragCoord.y/ iResolution.y>=0.5){
               // TexColor=vec4(0.1*textureQueryLod(iBackground, (iInverseCamRot*vec4(RayDir,1.0)).xyz).x);}
                Result += 0.7 * TexColor * (1.0 - Result.a);
         }
         float RedFactor   = 3.0*Result.r / (Result.g+Result.b+Result.g);
         float BlueFactor  = 3.0*Result.b / (Result.g+Result.b+Result.g);
         float GreenFactor = 3.0*Result.g / (Result.g+Result.b+Result.g);
         float BloomMax   = 8.0;
         Result.r = min(-4.0 * log(1.0 - pow(Result.r, 2.2)), BloomMax * RedFactor);
         Result.g = min(-4.0 * log(1.0 - pow(Result.g, 2.2)), BloomMax * GreenFactor);
         Result.b = min(-4.0 * log(1.0 - pow(Result.b, 2.2)), BloomMax * BlueFactor);
         Result.a = min(-4.0 * log(1.0 - pow(Result.a, 2.2)), 4.0);
        
        // TAA
        

    }
    float BlendWeight = iBlendWeight;

    vec4 PrevColor = texelFetch(iHistoryTex, ivec2(gl_FragCoord.xy), 0);
    FragColor      = (BlendWeight) * Result + (1.0 - BlendWeight) * PrevColor;
}
