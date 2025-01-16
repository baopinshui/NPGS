#version 460
#pragma shader_stage(fragment)

layout(location = 0) in  vec4 ColorFromVert;
layout(location = 0) out vec4 FragColor;

void main()
{
    FragColor = vec4(ColorFromVert);
}
