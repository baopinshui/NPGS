#pragma once

#include <glm/glm.hpp>

// Vertex structs
// --------------
struct FVertex
{
    glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec2 TexCoord;
};

struct FQuadVertex
{
    glm::vec2 Position;
    glm::vec2 TexCoord;
};

struct FQuadOnlyVertex
{
    glm::vec2 Position;
};

// Uniform buffer structs
// ----------------------
struct FGameArgs
{
    glm::aligned_vec2 Resolution;
    float FovRadians;
    float Time;
    float TimeDelta;
    float TimeRate;
} GameArgs{};

struct FBlackHoleArgs
{
    glm::mat4x4 InverseCamRot;
    glm::vec4 WorldUpView;
    glm::vec4 BlackHoleRelativePos;
    glm::vec4 BlackHoleRelativeDiskNormal;
    float BlackHoleMassSol;
    float Spin;
    float Mu;
    float AccretionRate;
    float InterRadiusLy;
    float OuterRadiusLy;
    float BlendWeight;
} BlackHoleArgs{};

struct FMatrices
{
    glm::aligned_mat4x4 View{ glm::mat4x4(1.0f) };
    glm::aligned_mat4x4 Projection{ glm::mat4x4(1.0f) };
    glm::aligned_mat3x3 NormalMatrix{ glm::mat3x3(1.0f) };
} Matrices;

struct FMaterial
{
    alignas(16) float Shininess;
};

struct FLight
{
    glm::aligned_vec3 Position;
    glm::aligned_vec3 Ambient;
    glm::aligned_vec3 Diffuse;
    glm::aligned_vec3 Specular;
};

struct FLightMaterial
{
    FMaterial         Material;
    FLight            Light;
    glm::aligned_vec3 ViewPos;
} LightMaterial;
