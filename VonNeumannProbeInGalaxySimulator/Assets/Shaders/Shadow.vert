#version 460 core

layout(location = 0) in vec3 Position;

uniform mat4x4 iLightSpaceMatrix;
uniform mat4x4 iModel;

void main()
{
	gl_Position = iLightSpaceMatrix * iModel * vec4(Position, 1.0);
}
