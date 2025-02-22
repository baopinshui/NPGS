#version 450
#pragma shader_stage(vertex)

layout(location = 0) in vec2 Position;

out gl_PerVertex
{
    vec4 gl_Position;
};

void main() 
{
    gl_Position = vec4(Position, 0.0, 1.0);
}
