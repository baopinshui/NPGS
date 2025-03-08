#version 450
#pragma shader_stage(fragment)

layout(location = 0) out vec4 FragColor;
layout(location = 0) in  vec3 TexCoord;

layout(set = 1, binding = 0) uniform samplerCube iSkybox;

void main()
{
	FragColor = texture(iSkybox, TexCoord);
}
