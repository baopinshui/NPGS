#version 450
#pragma shader_stage(vertex)

layout(location = 0) in vec3   Position;
layout(location = 1) in mat4x4 Model;

layout(set = 0, binding = 0) uniform UniformBuffer
{
	layout(offset = 128) mat4x4 iLightSpaceMatrix;
};

out gl_PerVertex
{
	vec4 gl_Position;
};

void main()
{
	gl_Position = iLightSpaceMatrix * Model * vec4(Position, 1.0);
}
