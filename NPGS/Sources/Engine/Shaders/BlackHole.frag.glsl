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

#include "BlackHole_common.glsl"

void main()
{
    vec2 Uv = gl_FragCoord.xy / iResolution.xy;

    // 1. TAA Jitter 计算 (直接复用 Common 里的 RandomStep)
    vec2 Jitter = vec2(RandomStep(Uv, fract(iTime * 1.0 + 0.5)), 
                       RandomStep(Uv, fract(iTime * 1.0))) / iResolution;

    // 2. 核心光线追踪 (传入带 Jitter 的 UV)
    float CurrentStatus;
    vec3  CurrentDir;
    float CurrentShift;
    TraceResult res = TraceRay(Uv + 0.5 * Jitter, iResolution);

    // 3. 初始颜色 (体积光部分)
        vec4 FinalColor    = res.AccumColor;
        CurrentStatus = res.Status;
        CurrentDir    = res.EscapeDir;
        CurrentShift  = res.FreqShift;

    // 4. 背景合成
    if ( CurrentStatus > 0.5 && CurrentStatus < 2.5) 
    {
        vec4 Bg = SampleBackground(CurrentDir, CurrentShift, CurrentStatus);
        FinalColor += 0.9999 * Bg * (1.0 - FinalColor.a);
    }


    // 5. 色调映射
    FinalColor = ApplyToneMapping(FinalColor);

    // 6. TAA 历史混合
    vec4 PrevColor = texelFetch(iHistoryTex, ivec2(gl_FragCoord.xy), 0);
    FragColor      = (iBlendWeight) * FinalColor + (1.0 - iBlendWeight) * PrevColor;
}