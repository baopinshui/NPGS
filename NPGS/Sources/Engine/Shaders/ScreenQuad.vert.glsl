#version 450
#pragma shader_stage(vertex)

layout(location = 0) in vec2 Position;
layout(location = 1) in vec2 TexCoord;
layout(location = 0) out vec2 TexCoordFromVert;

out gl_PerVertex
{
    vec4 gl_Position;
};

void main() 
{
    TexCoordFromVert = TexCoord;
    gl_Position = vec4(Position, 0.0, 1.0);
}
