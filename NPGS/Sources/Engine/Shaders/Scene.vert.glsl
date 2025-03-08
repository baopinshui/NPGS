#version 450
#pragma shader_stage(vertex)

layout(location = 0) in vec3   Position;
layout(location = 1) in vec3   Normal;
layout(location = 2) in vec2   TexCoord;
layout(location = 3) in mat4x4 Model;

layout(location = 0) out VertexToFragment
{
	vec3 Normal;
	vec2 TexCoord;
	vec3 FragPos;
	vec4 FragPosLightSpace;
} OutputData;

layout(set = 0, binding = 0) uniform Matrices
{
	mat4x4 iView;
	mat4x4 iProjection;
	mat4x4 iLightSpaceMatrix;
};

out gl_PerVertex
{
	vec4 gl_Position;
};

void main()
{
	OutputData.TexCoord          = TexCoord;
	OutputData.Normal            = vec3(transpose(inverse(Model)) * vec4(Normal, 1.0));
	OutputData.FragPos           = vec3(Model * vec4(Position, 1.0));
	OutputData.FragPosLightSpace = iLightSpaceMatrix * vec4(OutputData.FragPos, 1.0);
	gl_Position                  = iProjection * iView * Model * vec4(Position, 1.0);
}
