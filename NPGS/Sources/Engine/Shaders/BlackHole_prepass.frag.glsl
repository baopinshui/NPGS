// --- START OF FILE BlackHole_prepass.frag.glsl ---
#version 450
#pragma shader_stage(fragment)



// 输出定义
// Layout 0: xyz = Dir * Shift, w = Status Flag
layout(location = 0) out vec4 OutDistortionFlag; 
// Layout 1: 吸积盘预计算结果 (必须保留，否则平滑区域没有吸积盘)
layout(location = 1) out vec4 OutVolumetric;


// 引入 Common
#include "BlackHole_common.glsl"



void main()
{
    // 输入分辨率 (iResolution 应该是 Low Res 的尺寸)
    vec2 FragUv = gl_FragCoord.xy / iResolution.xy;
    
    // 执行核心追踪
    TraceResult res = TraceRay(FragUv, iResolution);

    // 打包输出 1: 几何信息
    // 将频移编码进方向向量的长度，或者直接乘进去
    OutDistortionFlag.xyz = res.EscapeDir * res.FreqShift;
    OutDistortionFlag.w   = res.Status;

    // 打包输出 2: 颜色信息
    OutVolumetric = res.AccumColor;
}