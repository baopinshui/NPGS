#version 450
#ifdef GL_ARB_shader_draw_parameters
#extension GL_ARB_shader_draw_parameters : enable
#endif

layout(binding = 0, std140) uniform _26_28
{
    vec2 _m0[3];
} _28;

layout(location = 0) out vec2 _9;
layout(location = 1) in vec2 _11;
layout(location = 0) in vec2 _22;
#ifdef GL_ARB_shader_draw_parameters
#define SPIRV_Cross_BaseInstance gl_BaseInstanceARB
#else
uniform int SPIRV_Cross_BaseInstance;
#endif

void main()
{
    _9 = _11;
    gl_Position = vec4(_22 + _28._m0[(gl_InstanceID + SPIRV_Cross_BaseInstance)], 0.0, 1.0);
}

