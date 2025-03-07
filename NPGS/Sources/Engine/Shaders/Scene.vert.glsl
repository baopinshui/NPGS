#version 450
#pragma shader_stage(vertex)

layout(location = 0) in vec3 Position;
layout(location = 1) in vec3 Normal;
layout(location = 2) in vec2 TexCoord;

layout(location = 0) out VertexToFragment
{
	vec3 Normal;
	vec2 TexCoord;
	vec3 FragPos;
	vec4 FragPosLightSpace;
} OutputData;

layout(push_constant) uniform VertPushConstant
{
	mat4x4 iModel;
};

layout(set = 0, binding = 0) uniform Matrices
{
	mat4x4 iView;
	mat4x4 iProjection;
	mat4x4 iLightSpaceMatrix;
	mat3x3 iNormalMatrix;
};

out gl_PerVertex
{
	vec4 gl_Position;
};

void main()
{
	OutputData.TexCoord          = TexCoord;
	OutputData.Normal            = iNormalMatrix * Normal;
	OutputData.FragPos           = vec3(iModel * vec4(Position, 1.0));
	OutputData.FragPosLightSpace = iLightSpaceMatrix * vec4(OutputData.FragPos, 1.0);
	gl_Position                  = iProjection * iView * iModel * vec4(Position, 1.0);
}
