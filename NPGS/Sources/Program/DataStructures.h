#pragma once

#define GLM_FORCE_ALIGNED_GENTYPES

#include <glm/glm.hpp>
#include <glm/gtc/type_aligned.hpp>


// Vertex structs
// --------------
struct FVertex
{
    glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec2 TexCoord;
};

struct FInstanceData
{
    glm::mat4x4 Model{ glm::mat4x4(1.0f) };
};

struct FSkyboxVertex
{
    glm::vec3 Position;
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
    float GameTime;
    float TimeDelta;
    float TimeRate;
};

struct FBlackHoleArgs
{
    glm::mat4x4 InverseCamRot;
    glm::vec4 BlackHoleRelativePosRs;
    glm::vec4 BlackHoleRelativeDiskNormal;
    glm::vec4 BlackHoleRelativeDiskTangen;
    int   Grid;
    int   ObserverMode;
    float UniverseSign;
    float BlackHoleTime;
    float BlackHoleMassSol;
    float Spin;
    float Q;
    float Mu;
    float AccretionRate;
    float InterRadiusRs;
    float OuterRadiusRs;
    float ThinRs;
    float Hopper;
    float Brightmut;
    float Darkmut;
    float Reddening;
    float Saturation;
    float BlackbodyIntensityExponent;
    float RedShiftColorExponent;
    float RedShiftIntensityExponent;
    float JetRedShiftIntensityExponent;
    float JetBrightmut;
    float JetSaturation;
    float JetShiftMax;
    float BlendWeight;
};

struct FMatrices
{
    glm::aligned_mat4x4 View{ glm::mat4x4(1.0f) };
    glm::aligned_mat4x4 Projection{ glm::mat4x4(1.0f) };
    glm::aligned_mat4x4 LightSpaceMatrix{ glm::mat4x4(1.0f) };
} ;

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
} ;
extern FGameArgs GameArgs;
extern FBlackHoleArgs BlackHoleArgs;
extern FMatrices Matrices;
extern FLightMaterial LightMaterial;
extern float cfov;