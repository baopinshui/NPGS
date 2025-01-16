#version 460 core

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;
layout(location = 0) in  vec2 TexCoordPassFromVert[];
layout(location = 0) out vec2 TexCoordPassFromGeom;

uniform float iTime;

vec3 GetNormal()
{
	vec3 x = vec3(gl_in[0].gl_Position - gl_in[1].gl_Position);
	vec3 y = vec3(gl_in[0].gl_Position - gl_in[2].gl_Position);
	return normalize(cross(x, y));
}
