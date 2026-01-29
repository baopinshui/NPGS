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

    vec2 Jitter = vec2(RandomStep(Uv, fract(iTime * 1.0 + 0.5)), 
                       RandomStep(Uv, fract(iTime * 1.0))) / iResolution;

    float CurrentStatus;
    vec3  CurrentDir;
    float CurrentShift;
    TraceResult res = TraceRay(Uv + 0.5 * Jitter, iResolution);

        vec4 FinalColor    = res.AccumColor;
        CurrentStatus = res.Status;
        CurrentDir    = res.EscapeDir;
        CurrentShift  = res.FreqShift;

    if ( CurrentStatus > 0.5 && CurrentStatus < 2.5) 
    {
        vec4 Bg = SampleBackground(CurrentDir, CurrentShift, CurrentStatus);
        FinalColor += 0.9999 * Bg * (1.0 - FinalColor.a);
    }


    FinalColor = ApplyToneMapping(FinalColor,CurrentShift);

    vec4 PrevColor = texelFetch(iHistoryTex, ivec2(gl_FragCoord.xy), 0);
    FragColor      = (iBlendWeight) * FinalColor + (1.0 - iBlendWeight) * PrevColor;
}