#version 450
#pragma shader_stage(fragment)

layout(location = 0) in  vec2 TexCoordFromVert;
layout(location = 0) out vec4 FragColor;

layout(set = 1, binding = 0) uniform sampler2D iTexture;

void main()
{
#ifdef LAMP_BOX
    FragColor = vec4(1.0, 1.0, 1.0, 1.0);
#else
    FragColor = texture(iTexture, TexCoordFromVert);
#endif
}
