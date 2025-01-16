#version 460 core

layout(location = 0) out vec4 FragColor;
layout(location = 0) in  vec3 ColorPassFromGeom;

void main()
{
	FragColor = vec4(ColorPassFromGeom, 1.0);
}
