#version 450
#pragma shader_stage(vertex)

layout(location = 0) in  vec3 Position;
layout(location = 1) in  vec3 Normal;
layout(location = 2) in  vec2 TexCoord;
layout(location = 0) out vec2 TexCoordFromVert;
layout(location = 1) out vec3 NormalFromVert;
layout(location = 2) out vec3 FragPosFromVert;

layout(push_constant) uniform VertPushConstant
{
	mat4x4 iModel;
};

layout(set = 0, binding = 0) uniform Matrices
{
	mat4x4 iView;
	mat4x4 iProjection;
	mat3x3 iNormalMatrix;
};

out gl_PerVertex
{
	vec4 gl_Position;
};

void main()
{
	TexCoordFromVert = TexCoord;
	NormalFromVert   = iNormalMatrix * Normal;
	FragPosFromVert  = vec3(iModel * vec4(Position, 1.0));
	gl_Position      = iProjection * iView * iModel * vec4(Position, 1.0);
}
