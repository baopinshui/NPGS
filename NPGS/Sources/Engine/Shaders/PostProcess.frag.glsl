#version 450
#pragma shader_stage(fragment)

layout(location = 0) out vec4 FragColor;
layout(location = 0) in  vec2 TexCoordFromVert;

layout(set = 0, binding = 0) uniform sampler2D iTexture;

void main()
{
	FragColor = texture(iTexture, TexCoordFromVert);
}
