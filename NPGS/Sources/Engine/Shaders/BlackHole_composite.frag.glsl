//BlackHole_composite.frag.glsl
#version 450
#pragma shader_stage(fragment)

layout(location = 0) out vec4 FragColor;

// 绑定低分辨率预计算结果 (必须在外部设置为 Nearest)
layout(set = 1, binding = 7) uniform sampler2D iPrepassDistortion; // Point Filter: XYZ=Dir*Shift, W=Flag，不要插值！不然会破坏标志位
layout(set = 1, binding = 8) uniform sampler2D iPrepassVolumetric; // Point Filter: RGBA=Volumetric

#include "BlackHole_common.glsl"

// 边缘检测阈值
const float EDGE_NORMAL_THRESHOLD = 0.99; // 方向向量点积阈值
const float EDGE_STATUS_TOLERANCE = 0.1;  // 状态位必须一致



// 敏感边界判定：忽略涉及 Opaque(3.0) 的边界，只关心 Sky/EventHorizon/Antiverse 之间的边界
bool IsSensitiveBoundary(float sA, float sB)
{
    if ((sA < 2.5|| sA > 3.5) ||(sB < 2.5|| sB > 3.5)) return false;

    return abs(sA - sB) > 0.1;
}

// 双线性插值
void ManualBilinearSample(vec2 uv, out vec3 outDistortion, out vec4 outVolumetric, out float outNearestStatus)
{
    ivec2 texSize = textureSize(iPrepassDistortion, 0);
    vec2 pixelPos = uv * vec2(texSize) - 0.5;
    ivec2 basePos = ivec2(floor(pixelPos));
    vec2 f = fract(pixelPos); // 插值权重

    ivec2 p00 = basePos;
    ivec2 p10 = basePos + ivec2(1, 0);
    ivec2 p01 = basePos + ivec2(0, 1);
    ivec2 p11 = basePos + ivec2(1, 1);

    vec4 d00 = texelFetch(iPrepassDistortion, p00, 0);
    vec4 d10 = texelFetch(iPrepassDistortion, p10, 0);
    vec4 d01 = texelFetch(iPrepassDistortion, p01, 0);
    vec4 d11 = texelFetch(iPrepassDistortion, p11, 0);

    vec4 v00 = texelFetch(iPrepassVolumetric, p00, 0);
    vec4 v10 = texelFetch(iPrepassVolumetric, p10, 0);
    vec4 v01 = texelFetch(iPrepassVolumetric, p01, 0);
    vec4 v11 = texelFetch(iPrepassVolumetric, p11, 0);

    outDistortion = mix(mix(d00.xyz, d10.xyz, f.x), mix(d01.xyz, d11.xyz, f.x), f.y);
    outVolumetric = mix(mix(v00, v10, f.x), mix(v01, v11, f.x), f.y);
    
    //状态位不插值
    float sBest = d00.w;
    float wMax = (1.0-f.x)*(1.0-f.y);
    
    if (f.x*(1.0-f.y) > wMax) { wMax = f.x*(1.0-f.y); sBest = d10.w; }
    if ((1.0-f.x)*f.y > wMax) { wMax = (1.0-f.x)*f.y; sBest = d01.w; }
    if (f.x*f.y > wMax)       { sBest = d11.w; }
    
    outNearestStatus = sBest;
}

void main()
{
    vec2 Uv = gl_FragCoord.xy / iResolution.xy;
    vec4 FinalColor = vec4(0.0);
    
    // -------------------------------------------------------------------------
    // 1. 边缘检测
    // -------------------------------------------------------------------------
    ivec2 TexSize = textureSize(iPrepassDistortion, 0);
    // 计算当前像素对应的低分辩率纹理坐标 (中心对齐)
    vec2 RawPos = Uv * vec2(TexSize);
    ivec2 CenterCoord = ivec2(floor(RawPos)); // Nearest neighbor

    // 采样中心和十字邻域
    vec4 CenterData = texelFetch(iPrepassDistortion, CenterCoord, 0);
    
    float FlagC = round(CenterData.w);
    float FlagL = round(texelFetch(iPrepassDistortion, CenterCoord + ivec2(-1, 0), 0).w);
    float FlagR = round(texelFetch(iPrepassDistortion, CenterCoord + ivec2( 1, 0), 0).w);
    float FlagU = round(texelFetch(iPrepassDistortion, CenterCoord + ivec2( 0,-1), 0).w);
    float FlagD = round(texelFetch(iPrepassDistortion, CenterCoord + ivec2( 0, 1), 0).w);

    // 状态位敏感边界检测
    bool IsEdgeStatus = IsSensitiveBoundary(FlagC, FlagL) ||
                        IsSensitiveBoundary(FlagC, FlagR) ||
                        IsSensitiveBoundary(FlagC, FlagU) ||
                        IsSensitiveBoundary(FlagC, FlagD);

    // 几何不连续性检测
    // 如果中心点是 Opaque (3.0)，跳过几何检测
    bool IsEdgeGeo = false;
    if (FlagC < 2.5|| FlagC > 3.5) {
        vec3 DirC = normalize(CenterData.xyz + 1e-6);
        if (     (FlagC < 2.5|| FlagC > 3.5) && dot(normalize(texelFetch(iPrepassDistortion, CenterCoord + ivec2(-1, 0), 0).xyz + 1e-6), DirC) < EDGE_NORMAL_THRESHOLD) IsEdgeGeo = true;
        else if ((FlagC < 2.5|| FlagC > 3.5) && dot(normalize(texelFetch(iPrepassDistortion, CenterCoord + ivec2( 1, 0), 0).xyz + 1e-6), DirC) < EDGE_NORMAL_THRESHOLD) IsEdgeGeo = true;
        else if ((FlagC < 2.5|| FlagC > 3.5) && dot(normalize(texelFetch(iPrepassDistortion, CenterCoord + ivec2( 0,-1), 0).xyz + 1e-6), DirC) < EDGE_NORMAL_THRESHOLD) IsEdgeGeo = true;
        else if ((FlagC < 2.5|| FlagC > 3.5) && dot(normalize(texelFetch(iPrepassDistortion, CenterCoord + ivec2( 0, 1), 0).xyz + 1e-6), DirC) < EDGE_NORMAL_THRESHOLD) IsEdgeGeo = true;
    }

    // -------------------------------------------------------------------------
    // 2. 分支渲染
    // -------------------------------------------------------------------------
    float CurrentStatus;
    vec3  CurrentDir;
    float CurrentShift;

    if ((IsEdgeStatus || IsEdgeGeo) || iPrepass==0)
    {
        // 边缘路径重算
        TraceResult res = TraceRay(Uv, iResolution);
        
        FinalColor    = res.AccumColor;
        CurrentStatus = res.Status;
        CurrentDir    = res.EscapeDir;
        CurrentShift  = res.FreqShift;

        // FinalColor = vec4(0.0, 0.0, 0.3, 0.0); 
    }
    else
    {
        // 平滑路径插值
        vec3 InterpDistortion;
        vec4 InterpVolumetric;
        float NearestStatus;

        ManualBilinearSample(Uv, InterpDistortion, InterpVolumetric, NearestStatus);

        FinalColor    = InterpVolumetric;
        CurrentStatus = NearestStatus;
        
        vec3 RawVec  = InterpDistortion;
        CurrentShift = length(RawVec);
        CurrentDir   = RawVec / (CurrentShift + 1e-9);
    }

    // -------------------------------------------------------------------------
    // 背景采样
    // -------------------------------------------------------------------------

    if (FinalColor.a < 0.99 && ((CurrentStatus > 0.5 && CurrentStatus < 2.5) || CurrentStatus > 3.5)) 
    {
        vec4 Bg = SampleBackground(CurrentDir, CurrentShift, CurrentStatus);
        float rawStatus = CurrentStatus;            // 备份带小数的原始状态位
        CurrentStatus   = round(rawStatus);         // 立即还原为纯整数，确保下方的判断不受影响
        bool IsPositiveEnergy = abs(rawStatus - CurrentStatus) < 0.1;
        if (!IsPositiveEnergy) 
        {
            float cMax = max(max(Bg.r, Bg.g), Bg.b);
            float cMin = min(min(Bg.r, Bg.g), Bg.b);
            Bg.rgb = vec3(cMax + cMin) - Bg.rgb;
        }
        float invA = 1.0 - FinalColor.a;
        vec4 colorFactor = vec4(pow(invA, 1.0), pow(invA, 1.6), pow(invA, 2.5), 1.0);
        
        FinalColor += 0.9999999 * Bg * colorFactor;
    }

    
    // 如果原始值和整数的差值极小 (< 0.1)，说明小数部分是 0.0，即正能量
    // 如果差值较大 (约为 0.2)，则为负能量


    // -------------------------------------------------------------------------
    // 3. 后处理
    // -------------------------------------------------------------------------
    FinalColor = ApplyToneMapping(FinalColor,CurrentShift);


    // TAA 混合
    vec4 PrevColor = texelFetch(iHistoryTex, ivec2(gl_FragCoord.xy), 0);
    FragColor      = (iBlendWeight) * FinalColor + (1.0 - iBlendWeight) * PrevColor;
}