#version 460 core

layout(location = 0) in vec2 InputPosition;
layout(location = 1) in vec2 InputTexCoord;

layout(location = 0) out vec2 TexCoord;

void main()
{
	TexCoord    = InputTexCoord;
	gl_Position = vec4(InputPosition, 0.0, 1.0);
}
