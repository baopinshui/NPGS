#pragma once

#include <limits>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Engine/Core/Base/Base.h"
#include "Engine/Core/Types/Entries/Astro/CelestialObject.h"
#include "Engine/Core/Types/Properties/StellarClass.h"

_NPGS_BEGIN
_ASTRO_BEGIN

class AStar : public FCelestialBody
{
public:
    enum class EEvolutionPhase : int
    {
        kPrevMainSequence          = -1,
        kMainSequence              =  0,
        kRedGiant                  =  2,
        kCoreHeBurn                =  3,
        kEarlyAgb                  =  4,
        kThermalPulseAgb           =  5,
        kPostAgb                   =  6,
        kWolfRayet                 =  9,
        kHeliumWhiteDwarf          =  11,
        kCarbonOxygenWhiteDwarf    =  12,
        kOxygenNeonMagnWhiteDwarf  =  13,
        kNeutronStar               =  14,
        kStellarBlackHole          =  15,
        kMiddleBlackHole           =  114514,
        kSuperMassiveBlackHole     =  1919810,
        kNull                      =  std::numeric_limits<int>::max()
    };

    enum class EStarFrom : int
    {
        kNormalFrom                = 0,
        kWhiteDwarfMerge           = 1,
        kSlowColdingDown           = 2,
        kEnvelopeDisperse          = 3,
        kElectronCaptureSupernova  = 4,
        kIronCoreCollapseSupernova = 5,
        kRelativisticJetHypernova  = 6,
        kPairInstabilitySupernova  = 7,
        kPhotondisintegration      = 8,
    };

    struct FExtendedProperties
    {
        Astro::FStellarClass Class;

        double Mass{};                    // 质量，单位 kg
        double Luminosity{};              // 辐射光度，单位 W
        double Lifetime{};                // 寿命，单位 yr
        double EvolutionProgress{};       // 演化进度
        float  FeH{};                     // 金属丰度
        float  InitialMass{};             // 恒星诞生时的质量，单位 kg
        float  SurfaceH1{};               // 表面氕质量分数
        float  SurfaceZ{};                // 表面金属丰度
        float  SurfaceEnergeticNuclide{}; // 表面含能核素质量分数
        float  SurfaceVolatiles{};        // 表面挥发物质量分数
        float  Teff{};                    // 有效温度
        float  CoreTemp{};                // 核心温度
        float  CoreDensity{};             // 核心密度，单位 kg/m^3
        float  StellarWindSpeed{};        // 恒星风速度，单位 m/s
        float  StellarWindMassLossRate{}; // 恒星风质量损失率，单位 kg/s
        float  MinCoilMass{};             // 最小举星器赤道偏转线圈质量，单位 kg

        EEvolutionPhase Phase{ EEvolutionPhase::kPrevMainSequence }; // 演化阶段
        EStarFrom       From{ EStarFrom::kNormalFrom };              // 恒星形成方式

        bool bIsSingleStar{ true };
        bool bHasPlanets{ true };
    };

public:
    using Base = FCelestialBody;
    using Base::Base;

    AStar() = default;
    AStar(const FCelestialBody::FBasicProperties& BasicProperties, const FExtendedProperties& ExtraProperties);
    ~AStar() = default;

    AStar& SetExtendedProperties(const FExtendedProperties& ExtraProperties);
    const FExtendedProperties& GetExtendedProperties() const;

    // Setters
    // Setters for ExtendedProperties
    // ------------------------------
    AStar& SetMass(double Mass);
    AStar& SetLuminosity(double Luminosity);
    AStar& SetLifetime(double Lifetime);
    AStar& SetEvolutionProgress(double EvolutionProgress);
    AStar& SetFeH(float FeH);
    AStar& SetInitialMass(float InitialMass);
    AStar& SetSurfaceH1(float SurfaceH1);
    AStar& SetSurfaceZ(float SurfaceZ);
    AStar& SetSurfaceEnergeticNuclide(float SurfaceEnergeticNuclide);
    AStar& SetSurfaceVolatiles(float SurfaceVolatiles);
    AStar& SetTeff(float Teff);
    AStar& SetCoreTemp(float CoreTemp);
    AStar& SetCoreDensity(float CoreDensity);
    AStar& SetStellarWindSpeed(float StellarWindSpeed);
    AStar& SetStellarWindMassLossRate(float StellarWindMassLossRate);
    AStar& SetMinCoilMass(float MinCoilMass);
    AStar& SetIsSingleStar(bool bIsSingleStar);
    AStar& SetHasPlanets(bool bHasPlanets);
    AStar& SetStarFrom(EStarFrom From);
    AStar& SetEvolutionPhase(EEvolutionPhase Phase);
    AStar& SetStellarClass(const Astro::FStellarClass& Class);

    // Getters
    // Getters for ExtendedProperties
    // ------------------------------
    double GetMass() const;
    double GetLuminosity() const;
    double GetLifetime() const;
    double GetEvolutionProgress() const;
    float  GetFeH() const;
    float  GetInitialMass() const;
    float  GetSurfaceH1() const;
    float  GetSurfaceZ() const;
    float  GetSurfaceEnergeticNuclide() const;
    float  GetSurfaceVolatiles() const;
    float  GetTeff() const;
    float  GetCoreTemp() const;
    float  GetCoreDensity() const;
    float  GetStellarWindSpeed() const;
    float  GetStellarWindMassLossRate() const;
    float  GetMinCoilMass() const;
    bool   GetIsSingleStar() const;
    bool   GetHasPlanets() const;
    EStarFrom       GetStarFrom() const;
    EEvolutionPhase GetEvolutionPhase() const;
    const Astro::FStellarClass& GetStellarClass() const;

    static const std::vector<std::pair<int, int>> _kSpectralSubclassMap_O;
    static const std::vector<std::pair<int, int>> _kSpectralSubclassMap_B;
    static const std::vector<std::pair<int, int>> _kSpectralSubclassMap_A;
    static const std::vector<std::pair<int, int>> _kSpectralSubclassMap_F;
    static const std::vector<std::pair<int, int>> _kSpectralSubclassMap_G;
    static const std::vector<std::pair<int, int>> _kSpectralSubclassMap_K;
    static const std::vector<std::pair<int, int>> _kSpectralSubclassMap_M;
    static const std::vector<std::pair<int, int>> _kSpectralSubclassMap_L;
    static const std::vector<std::pair<int, int>> _kSpectralSubclassMap_T;
    static const std::vector<std::pair<int, int>> _kSpectralSubclassMap_Y;
    static const std::vector<std::pair<int, int>> _kSpectralSubclassMap_WC;
    static const std::vector<std::pair<int, int>> _kSpectralSubclassMap_WN;
    static const std::vector<std::pair<int, int>> _kSpectralSubclassMap_WO;
    static const std::vector<std::pair<int, int>> _kSpectralSubclassMap_WNxh;
    static const std::vector<std::pair<int, std::vector<std::pair<int, int>>>> _kInitialCommonMap;
    static const std::vector<std::pair<int, std::vector<std::pair<int, int>>>> _kInitialWolfRayetMap;
    static const std::unordered_map<EEvolutionPhase, Astro::FStellarClass::ELuminosityClass> _kLuminosityMap;
    static const std::unordered_map<float, float> _kFeHSurfaceH1Map;

private:
    FExtendedProperties _ExtraProperties{};
};

_ASTRO_END
_NPGS_END

#include "Star.inl"
