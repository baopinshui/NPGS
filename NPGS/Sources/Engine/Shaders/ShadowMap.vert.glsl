#version 450
#pragma shader_stage(vertex)

layout(location = 0) in vec3 Position;
layout(push_constant) uniform PushConstant
{
	mat4x4 iModel;
	mat4x4 iLightSpaceMatrix;
};

out gl_PerVertex
{
	vec4 gl_Position;
};

void main()
{
	gl_Position = iLightSpaceMatrix * iModel * vec4(Position, 1.0);
}
