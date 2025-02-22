#pragma once

#include <glm/glm.hpp>

// Vertex structs
// --------------
struct FVertex
{
    glm::vec3 Postion;
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
    glm::aligned_vec3 WorldUpView;
    glm::aligned_vec3 BlackHoleRelativePos;
    glm::vec3 BlackHoleRelativeDiskNormal;
    float BlackHoleMassSol;
    float Spin;
    float Mu;
    float AccretionRate;
    float InterRadiusLy;
    float OuterRadiusLy;
} BlackHoleArgs{};
