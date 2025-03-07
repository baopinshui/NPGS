#version 450
#pragma shader_stage(fragment)

layout(location = 0) out vec4 FragColor;
layout(location = 0) in  vec2 TexCoordFromVert;

layout(set = 0, binding = 0) uniform sampler2D iTextures[2];

void main()
{
    FragColor = texture(iTextures[0], TexCoordFromVert);
	// float DepthValue = texture(iTextures[1], TexCoordFromVert).r;
	// FragColor = vec4(vec3(DepthValue), 1.0);
}
