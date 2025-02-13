#version 450
#pragma shader_stage(vertex)

layout(location = 0) in  vec3 Position;
layout(location = 1) in  vec2 TexCoord;
layout(location = 0) out vec2 TexCoordFromVert;

layout(push_constant) uniform VertPushConstant
{
	mat4x4 iModel;
};

layout(set = 0, binding = 0) uniform UniformBuffer
{
	mat4x4 iView;
	mat4x4 iProjection;
};

out gl_PerVertex
{
	vec4 gl_Position;
};

void main()
{
	TexCoordFromVert = TexCoord;
	gl_Position = iProjection * iView * iModel * vec4(Position, 1.0);
}
