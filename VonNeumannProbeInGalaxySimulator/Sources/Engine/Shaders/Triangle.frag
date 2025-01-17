#version 460
#pragma shader_stage(fragment)

layout(location = 0) in  vec4 ColorFromVert;
layout(location = 1) in  vec2 TexCoordFromVert;
layout(location = 0) out vec4 FragColor;

layout(set = 0, binding = 1) uniform sampler2D iTexture;

void main()
{
    //FragColor = ColorFromVert;
    FragColor = texture(iTexture, TexCoordFromVert);
}
