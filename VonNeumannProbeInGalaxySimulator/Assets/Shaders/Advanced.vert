#version 460 core

layout(location = 0) in vec3 Position;
layout(location = 1) in vec2 TexCoord;

layout(location = 0) out vec2 TexCoordPassFromVert;

layout(std430, binding = 0) buffer Matrices
{
    mat4 iModel;
    mat4 iView;
    mat4 iProjection;
};

void main()
{
    TexCoordPassFromVert = TexCoord;
    gl_Position = iProjection * iView * iModel * vec4(Position, 1.0);
}
