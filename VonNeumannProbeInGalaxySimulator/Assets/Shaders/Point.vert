#version 460 core

layout(location = 0) in  vec2 VertexPosition;
layout(location = 1) in  vec3 VertexColor;
layout(location = 0) out vec3 ColorPassFromVert;

void main()
{
	ColorPassFromVert = VertexColor;
	gl_Position = vec4(VertexPosition, 0.0, 1.0);
}