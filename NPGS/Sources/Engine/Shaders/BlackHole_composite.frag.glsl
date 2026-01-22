// --- START OF FILE BlackHole_composite.frag.glsl ---
#version 450
#pragma shader_stage(fragment)

layout(location = 0) out vec4 FragColor;

// 绑定低分辨率预计算结果 (必须在外部设置为 Nearest/Point Filter)
layout(set = 1, binding = 3) uniform sampler2D iPrepassDistortion; // Point Filter: XYZ=Dir*Shift, W=Flag
layout(set = 1, binding = 4) uniform sampler2D iPrepassVolumetric; // Point Filter: RGBA=Volumetric

#include "BlackHole_common.glsl"

// 边缘检测阈值
const float EDGE_NORMAL_THRESHOLD = 0.99; // 方向向量点积阈值
const float EDGE_STATUS_TOLERANCE = 0.1;  // 状态位必须完全一致

// 辅助函数：采样背景

// 敏感边界判定：忽略涉及 Opaque(3.0) 的边界，只关心 Sky/EventHorizon/Antiverse 之间的硬切换
bool IsSensitiveBoundary(float sA, float sB)
{
    // 如果任意一个像素是 Opaque (3.0)，则视为非敏感边界（进行插值平滑处理）
    if (sA > 2.5 || sB > 2.5) return false;

    // 在前三种状态中，如果状态不一致，则是硬边缘
    return abs(sA - sB) > 0.1;
}

// [新增] 手动双线性插值
// 输入 UV，输出插值后的 方向信息(RGB+Shift) 和 体积光(RGBA)
// 注意：Status(W) 不插值，直接返回最近邻
void ManualBilinearSample(vec2 uv, out vec3 outDistortion, out vec4 outVolumetric, out float outNearestStatus)
{
    ivec2 texSize = textureSize(iPrepassDistortion, 0);
    vec2 pixelPos = uv * vec2(texSize) - 0.5;
    ivec2 basePos = ivec2(floor(pixelPos));
    vec2 f = fract(pixelPos); // 插值权重

    // 获取 2x2 邻域 (使用 texelFetch 确保准确性)
    ivec2 p00 = basePos;
    ivec2 p10 = basePos + ivec2(1, 0);
    ivec2 p01 = basePos + ivec2(0, 1);
    ivec2 p11 = basePos + ivec2(1, 1);

    // 1. 采样 Distortion (RGB=Dir, W=Flag)
    vec4 d00 = texelFetch(iPrepassDistortion, p00, 0);
    vec4 d10 = texelFetch(iPrepassDistortion, p10, 0);
    vec4 d01 = texelFetch(iPrepassDistortion, p01, 0);
    vec4 d11 = texelFetch(iPrepassDistortion, p11, 0);

    // 2. 采样 Volumetric
    vec4 v00 = texelFetch(iPrepassVolumetric, p00, 0);
    vec4 v10 = texelFetch(iPrepassVolumetric, p10, 0);
    vec4 v01 = texelFetch(iPrepassVolumetric, p01, 0);
    vec4 v11 = texelFetch(iPrepassVolumetric, p11, 0);

    // 3. 执行双线性插值 (mix x, then mix y)
    outDistortion = mix(mix(d00.xyz, d10.xyz, f.x), mix(d01.xyz, d11.xyz, f.x), f.y);
    outVolumetric = mix(mix(v00, v10, f.x), mix(v01, v11, f.x), f.y);
    
    // 4. 状态位不插值，取最近邻 (通常是 d00 或根据 f > 0.5 判断)
    // 这里为了简单和边缘逻辑一致，取最接近中心的点
    // 如果 f.x > 0.5 且 f.y > 0.5 则取 d11，以此类推
    // 但作为 Fast Path，直接取 d00 或者直接传参进来的 FlagC 也是可以的
    // 为了更精确的背景选择，我们找权重最大的那个 Status
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
    // 1. 边缘检测 (Edge Detection)
    // 使用 texelFetch 确保与 Nearest 采样器行为一致
    // -------------------------------------------------------------------------
    ivec2 TexSize = textureSize(iPrepassDistortion, 0);
    // 计算当前像素对应的低分辩率纹理坐标 (中心对齐)
    vec2 RawPos = Uv * vec2(TexSize);
    ivec2 CenterCoord = ivec2(floor(RawPos)); // Nearest neighbor coordinate

    // 采样中心和十字邻域 (只读 Flag)
    // 注意：这里没有 -0.5 的偏移，因为我们要找的是当前片元"落在"的那个低分辩率像素
    vec4 CenterData = texelFetch(iPrepassDistortion, CenterCoord, 0);
    
    float FlagC = round(CenterData.w);
    float FlagL = round(texelFetch(iPrepassDistortion, CenterCoord + ivec2(-1, 0), 0).w);
    float FlagR = round(texelFetch(iPrepassDistortion, CenterCoord + ivec2( 1, 0), 0).w);
    float FlagU = round(texelFetch(iPrepassDistortion, CenterCoord + ivec2( 0,-1), 0).w);
    float FlagD = round(texelFetch(iPrepassDistortion, CenterCoord + ivec2( 0, 1), 0).w);

    // A. 状态位敏感边界检测
    bool IsEdgeStatus = IsSensitiveBoundary(FlagC, FlagL) ||
                        IsSensitiveBoundary(FlagC, FlagR) ||
                        IsSensitiveBoundary(FlagC, FlagU) ||
                        IsSensitiveBoundary(FlagC, FlagD);

    // B. 几何不连续性检测
    // 如果中心点是 Opaque (3.0)，跳过几何检测
    bool IsEdgeGeo = false;
    if (FlagC < 2.5) {
        vec3 DirC = normalize(CenterData.xyz + 1e-6);
        // 仅检查同为非 Opaque 的邻居
        if (FlagL < 2.5 && dot(normalize(texelFetch(iPrepassDistortion, CenterCoord + ivec2(-1, 0), 0).xyz + 1e-6), DirC) < EDGE_NORMAL_THRESHOLD) IsEdgeGeo = true;
        else if (FlagR < 2.5 && dot(normalize(texelFetch(iPrepassDistortion, CenterCoord + ivec2( 1, 0), 0).xyz + 1e-6), DirC) < EDGE_NORMAL_THRESHOLD) IsEdgeGeo = true;
        else if (FlagU < 2.5 && dot(normalize(texelFetch(iPrepassDistortion, CenterCoord + ivec2( 0,-1), 0).xyz + 1e-6), DirC) < EDGE_NORMAL_THRESHOLD) IsEdgeGeo = true;
        else if (FlagD < 2.5 && dot(normalize(texelFetch(iPrepassDistortion, CenterCoord + ivec2( 0, 1), 0).xyz + 1e-6), DirC) < EDGE_NORMAL_THRESHOLD) IsEdgeGeo = true;
    }

    // -------------------------------------------------------------------------
    // 2. 分支渲染
    // -------------------------------------------------------------------------
    float CurrentStatus;
    vec3  CurrentDir;
    float CurrentShift;

    if (IsEdgeStatus || IsEdgeGeo)
    {
        // === 边缘路径：High Quality (重算) ===
        TraceResult res = TraceRay(Uv, iResolution);
        
        FinalColor    = res.AccumColor;
        CurrentStatus = res.Status;
        CurrentDir    = res.EscapeDir;
        CurrentShift  = res.FreqShift;

        // Debug: 可视化重算区域
        // FinalColor = vec4(0.0, 0.0, 0.3, 0.0); 
    }
    else
    {
        // === 平滑路径：Fast Path (手动插值) ===
        vec3 InterpDistortion;
        vec4 InterpVolumetric;
        float NearestStatus;

        // 调用手动双线性插值
        ManualBilinearSample(Uv, InterpDistortion, InterpVolumetric, NearestStatus);

        FinalColor    = InterpVolumetric;
        CurrentStatus = NearestStatus;
        
        // 解析插值后的方向和频移
        vec3 RawVec  = InterpDistortion;
        CurrentShift = length(RawVec);
        CurrentDir   = RawVec / (CurrentShift + 1e-9);
    }

    // -------------------------------------------------------------------------
    // 统一背景采样 (Unified Background Sampling)
    // -------------------------------------------------------------------------
    // 只有状态 1 (Sky) 和 2 (Antiverse) 才采样背景
    // 状态 0 (Stop) 和 3 (Opaque) 不采样
    if (FinalColor.a < 0.99 && CurrentStatus > 0.5 && CurrentStatus < 2.5) 
    {
        vec4 Bg = SampleBackground(CurrentDir, CurrentShift, CurrentStatus);
        FinalColor += 0.9999 * Bg * (1.0 - FinalColor.a);
    }

    // -------------------------------------------------------------------------
    // 3. 后处理 (Tone Mapping & TAA Blend)
    // -------------------------------------------------------------------------
    FinalColor = ApplyToneMapping(FinalColor);


    // TAA 混合
    // 注意：如果 iHistoryTex 也是 Nearest，这里可能需要根据情况调整，但通常 History 是由系统处理的
    vec4 PrevColor = texelFetch(iHistoryTex, ivec2(gl_FragCoord.xy), 0);
    FragColor      = (iBlendWeight) * FinalColor + (1.0 - iBlendWeight) * PrevColor;
}