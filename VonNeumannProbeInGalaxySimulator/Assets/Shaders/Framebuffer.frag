#version 460 core

layout(location = 0) out vec4 FragColor;
layout(location = 0) in  vec2 TexCoord;

uniform sampler2D iTexColorBuffer;

void main()
{
	vec3 Result = texture(iTexColorBuffer, TexCoord).rgb;
	FragColor   = vec4(pow(Result, vec3(1.0 / 2.2)), 1.0);
}
