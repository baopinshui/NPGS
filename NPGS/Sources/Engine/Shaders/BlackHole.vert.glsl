#version 450
#pragma shader_stage(vertex)

layout(location = 0) in  vec2 Position;
layout(location = 0) out vec3 WorldUpView;
layout(location = 1) out vec3 BlackHoleRelativePos;  
layout(location = 2) out vec3 BlackHoleRelativeDiskNormal;

layout(set = 0, binding = 0) uniform CameraBuffer 
{
    vec3 iWorldUpView;                  // 相机上方向
    vec3 iBlackHoleRelativePos;         // 黑洞位置
    vec3 iBlackHoleRelativeDiskNormal;  // 吸积盘法线
};

out gl_PerVertex
{
    vec4 gl_Position;
};

void main() 
{
    gl_Position = vec4(Position, 0.0, 1.0);

    WorldUpView                 = iWorldUpView;
    BlackHoleRelativePos        = iBlackHoleRelativePos;
    BlackHoleRelativeDiskNormal = iBlackHoleRelativeDiskNormal;
}