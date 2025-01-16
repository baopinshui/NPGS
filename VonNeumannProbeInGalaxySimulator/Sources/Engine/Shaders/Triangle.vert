#version 460
#pragma shader_stage(vertex)

layout(location = 0) in vec2 Position;
layout(location = 1) in vec4 Color;

layout(location = 0) out vec4 ColorFromVert;
			
layout(set = 0, binding = 0) uniform UniformBuffer
{
	vec2 iOffsets[3];
};
				 
void main()
{
	ColorFromVert = Color;
	gl_Position = vec4(Position + iOffsets[gl_InstanceIndex], 0.0, 1.0);
}
