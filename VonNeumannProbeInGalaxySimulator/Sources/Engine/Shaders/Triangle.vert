#version 460
#pragma shader_stage(vertex)

layout(location = 0) in vec2 Position;
layout(location = 1) in vec4 Color;
layout(location = 2) in vec2 TexCoord;

layout(location = 0) out vec4 ColorFromVert;
layout(location = 1) out vec2 TexCoordFromVert;
			
layout(set = 0, binding = 0) uniform UniformBuffer
{
	vec2 iOffsets[3];
};
				 
void main()
{
	ColorFromVert = Color;
	TexCoordFromVert = TexCoord;
	gl_Position = vec4(Position + iOffsets[gl_InstanceIndex], 0.0, 1.0);
}
