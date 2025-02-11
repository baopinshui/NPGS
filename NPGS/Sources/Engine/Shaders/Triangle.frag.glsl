#version 450
#pragma shader_stage(fragment)

layout(location = 0) in  vec2 TexCoordFromVert;
layout(location = 0) out vec4 FragColor;

layout(set = 0, binding = 1) uniform sampler2D iTextures[2];

layout(push_constant) uniform FragPushConstant
{
	layout(offset = 64) float iTime;
};

void main()
{
    FragColor = mix(texture(iTextures[0], TexCoordFromVert), texture(iTextures[1], TexCoordFromVert), clamp(sin(iTime), 0, 1));
}
