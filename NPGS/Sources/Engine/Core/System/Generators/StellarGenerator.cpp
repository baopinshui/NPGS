#include "StellarGenerator.h"

#include <cmath>
#include <cstdint>
#include <algorithm>
#include <charconv>
#include <filesystem>
#include <format>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>

#include <glm/glm.hpp>

#include "Engine/Core/Base/Base.h"
#include "Engine/Core/Math/NumericConstants.h"
#include "Engine/Core/Runtime/AssetLoaders/AssetManager.h"
#include "Engine/Core/Runtime/AssetLoaders/CommaSeparatedValues.hpp"
#include "Engine/Core/Runtime/AssetLoaders/GetAssetFullPath.h"
#include "Engine/Utils/Logger.h"
#include "Engine/Utils/Utils.h"

_NPGS_BEGIN
_SYSTEM_BEGIN
_GENERATOR_BEGIN

// Tool macros
// -----------
#define GenerateDeathStarPlaceholder(Lifetime)                                                            \
{                                                                                                         \
    Astro::FStellarClass::FSpectralType DeathStarClass                                                    \
	{                                                                                                     \
        .HSpectralClass  = Astro::FStellarClass::ESpectralClass::kSpectral_Unknown,                       \
        .MSpectralClass  = Astro::FStellarClass::ESpectralClass::kSpectral_Unknown,                       \
        .LuminosityClass = Astro::FStellarClass::ELuminosityClass::kLuminosity_Unknown,                   \
        .SpecialMark     = std::to_underlying(Astro::FStellarClass::ESpecialMark::kCode_Null),            \
        .Subclass        = 0.0f,                                                                          \
        .AmSubclass      = 0.0f,                                                                          \
        .bIsAmStar       = false                                                                          \
    };                                                                                                    \
                                                                                                          \
    Astro::AStar DeathStar;                                                                               \
    DeathStar.SetStellarClass(                                                                            \
        Astro::FStellarClass(Astro::FStellarClass::EStellarType::kDeathStarPlaceholder, DeathStarClass)); \
    DeathStar.SetLifetime(Lifetime);                                                                      \
    throw DeathStar;                                                                                      \
}

// Tool functions
// --------------
namespace
{
    float DefaultAgePdf(const glm::vec3&, float Age, float UniverseAge)
    {
        float Probability = 0.0f;
        if (Age - (UniverseAge - 13.8f) < 8.0f)
        {
            Probability = std::exp((Age - (UniverseAge - 13.8f) / 8.4f));
        }
        else
        {
            Probability = 2.6f * std::exp((-0.5f * std::pow((Age - (UniverseAge - 13.8f)) - 8.0f, 2.0f)) / (std::pow(1.5f, 2.0f)));
        }

        return static_cast<float>(Probability);
    }

    float DefaultLogMassPdfSingleStar(float LogMassSol)
    {
        float Probability = 0.0f;

        if (std::pow(10.0f, LogMassSol) <= 1.0f)
        {
            Probability = 0.158f * std::exp(-1.0f * std::pow(LogMassSol + 1.0f, 2.0f) / 1.101128f);
        }
        else
        {
            Probability = 0.06371598f * std::pow(std::pow(10.0f, LogMassSol), -0.8f);
        }

        return Probability;
    }

    float DefaultLogMassPdfBinaryStar(float LogMassSol)
    {
        float Probability = 0.0f;

        if (std::pow(10.0, LogMassSol) <= 1.0f)
        {
            Probability = 0.086f * std::exp(-1.0f * std::pow(LogMassSol + 0.65757734f, 2.0f) / 1.101128f);
        }
        else
        {
            Probability = 0.058070157f * std::pow(std::pow(10.0f, LogMassSol), -0.65f);
        }

        return Probability;
    }
}

// FStellarGenerator implementations
// --------------------------------
FStellarGenerator::FStellarGenerator(const FStellarGenerationInfo& GenerationInfo)
    :
    _RandomEngine(*GenerationInfo.SeedSequence),
    _MagneticGenerators
    {
        Util::TUniformRealDistribution<>(std::log10(500.0f), std::log10(3000.0f)),
        Util::TUniformRealDistribution<>(1.0f, 3.0f),
        Util::TUniformRealDistribution<>(0.0f, 1.0f),
        Util::TUniformRealDistribution<>(3.0f, 4.0f),
        Util::TUniformRealDistribution<>(-1.0f, 0.0f),
        Util::TUniformRealDistribution<>(2.0f, 3.0f),
        Util::TUniformRealDistribution<>(0.5f, 4.5f),
        Util::TUniformRealDistribution<>(1e9f, 1e11f)
    },

    _FeHGenerators
    {
        std::make_unique<Util::TLogNormalDistribution<>>(-0.3f, 0.5f),
        std::make_unique<Util::TNormalDistribution<>>(-0.3f, 0.15f),
        std::make_unique<Util::TNormalDistribution<>>(-0.08f, 0.12f),
        std::make_unique<Util::TNormalDistribution<>>(0.05f, 0.16f)
    },

    _SpinGenerators
    {
        Util::TUniformRealDistribution<>(3.0f, 5.0f),
        Util::TUniformRealDistribution<>(0.001f, 0.998f)
    },

    _AgeGenerator(GenerationInfo.AgeLowerLimit, GenerationInfo.AgeUpperLimit),
    _CommonGenerator(0.0f, 1.0f),

    _LogMassGenerator(GenerationInfo.StellarTypeOption == FStellarGenerator::EStellarTypeGenerationOption::kMergeStar
                      ? std::make_unique<Util::TUniformRealDistribution<>>(0.0f, 1.0f)
                      : std::make_unique<Util::TUniformRealDistribution<>>(
                            std::log10(GenerationInfo.MassLowerLimit), std::log10(GenerationInfo.MassUpperLimit))),

    _MassPdfs(GenerationInfo.MassPdfs),
    _MassMaxPdfs(GenerationInfo.MassMaxPdfs),
    _AgeMaxPdf(GenerationInfo.AgeMaxPdf),
    _AgePdf(GenerationInfo.AgePdf),
    _UniverseAge(GenerationInfo.UniverseAge),
    _AgeLowerLimit(GenerationInfo.AgeLowerLimit),
    _AgeUpperLimit(GenerationInfo.AgeUpperLimit),
    _FeHLowerLimit(GenerationInfo.FeHLowerLimit),
    _FeHUpperLimit(GenerationInfo.FeHUpperLimit),
    _MassLowerLimit(GenerationInfo.MassLowerLimit),
    _MassUpperLimit(GenerationInfo.MassUpperLimit),
    _CoilTemperatureLimit(GenerationInfo.CoilTemperatureLimit),
    _dEpdM(GenerationInfo.dEpdM),

    _AgeDistribution(GenerationInfo.AgeDistribution),
    _FeHDistribution(GenerationInfo.FeHDistribution),
    _MassDistribution(GenerationInfo.MassDistribution),
    _StellarTypeOption(GenerationInfo.StellarTypeOption),
    _MultiplicityOption(GenerationInfo.MultiplicityOption)
{
    InitializeMistData();
    InitializePdfs();
}

FStellarGenerator::FStellarGenerator(const std::seed_seq&          SeedSequence,
                                     EStellarTypeGenerationOption  StellarTypeOption,
                                     EMultiplicityGenerationOption MultiplicityOption,
                                     float UniverseAge,
                                     float MassLowerLimit, float MassUpperLimit, EGenerationDistribution MassDistribution,
                                     float AgeLowerLimit,  float AgeUpperLimit,  EGenerationDistribution AgeDistribution,
                                     float FeHLowerLimit,  float FeHUpperLimit,  EGenerationDistribution FeHDistribution,
                                     float CoilTemperatureLimit, float dEpdM,
                                     const std::function<float(const glm::vec3&, float, float)>& AgePdf,
                                     const glm::vec2&                                            AgeMaxPdf,
                                     const std::array<std::function<float(float)>, 2>&           MassPdfs,
                                     const std::array<glm::vec2, 2>&                             MassMaxPdfs)
    :
    FStellarGenerator(FStellarGenerationInfo{
          .SeedSequence         = &SeedSequence,
          .StellarTypeOption    = StellarTypeOption,
          .MultiplicityOption   = MultiplicityOption,
          .UniverseAge          = UniverseAge,
          .MassLowerLimit       = MassLowerLimit,
          .MassUpperLimit       = MassUpperLimit,
          .MassDistribution     = MassDistribution,
          .AgeLowerLimit        = AgeLowerLimit,
          .AgeUpperLimit        = AgeUpperLimit,
          .AgeDistribution      = AgeDistribution,
          .FeHLowerLimit        = FeHLowerLimit,
          .FeHUpperLimit        = FeHUpperLimit,
          .FeHDistribution      = FeHDistribution,
          .CoilTemperatureLimit = CoilTemperatureLimit,
          .dEpdM                = dEpdM,
          .AgePdf               = AgePdf,
          .AgeMaxPdf            = AgeMaxPdf,
          .MassPdfs             = MassPdfs,
          .MassMaxPdfs          = MassMaxPdfs
    })
{
}

FStellarGenerator::FStellarGenerator(const FStellarGenerator& Other)
    :
    _RandomEngine(Other._RandomEngine),
    _MagneticGenerators(Other._MagneticGenerators),
    _SpinGenerators(Other._SpinGenerators),
    _AgeGenerator(Other._AgeGenerator),
    _CommonGenerator(Other._CommonGenerator),
    _MassPdfs(Other._MassPdfs),
    _MassMaxPdfs(Other._MassMaxPdfs),
    _AgeMaxPdf(Other._AgeMaxPdf),
    _AgePdf(Other._AgePdf),
    _UniverseAge(Other._UniverseAge),
    _AgeLowerLimit(Other._AgeLowerLimit),
    _AgeUpperLimit(Other._AgeUpperLimit),
    _FeHLowerLimit(Other._FeHLowerLimit),
    _FeHUpperLimit(Other._FeHUpperLimit),
    _MassLowerLimit(Other._MassLowerLimit),
    _MassUpperLimit(Other._MassUpperLimit),
    _CoilTemperatureLimit(Other._CoilTemperatureLimit),
    _dEpdM(Other._dEpdM),
    _AgeDistribution(Other._AgeDistribution),
    _FeHDistribution(Other._FeHDistribution),
    _MassDistribution(Other._MassDistribution),
    _StellarTypeOption(Other._StellarTypeOption),
    _MultiplicityOption(Other._MultiplicityOption)
{
    if (Other._LogMassGenerator != nullptr)
    {
        _LogMassGenerator = std::make_unique<Util::TUniformRealDistribution<>>(
            std::log10(Other._MassLowerLimit), std::log10(Other._MassUpperLimit));
    }

    if (Other._FeHGenerators[0] != nullptr)
    {
        auto* LogNormal = dynamic_cast<Util::TLogNormalDistribution<>*>(Other._FeHGenerators[0].get());
        _FeHGenerators[0] = std::make_unique<Util::TLogNormalDistribution<>>(*LogNormal);
    }

    if (Other._FeHGenerators[1] != nullptr)
    {
        auto* Normal = dynamic_cast<Util::TNormalDistribution<>*>(Other._FeHGenerators[1].get());
        _FeHGenerators[1] = std::make_unique<Util::TNormalDistribution<>>(*Normal);
    }

    if (Other._FeHGenerators[2] != nullptr)
    {
        auto* Normal = dynamic_cast<Util::TNormalDistribution<>*>(Other._FeHGenerators[2].get());
        _FeHGenerators[2] = std::make_unique<Util::TNormalDistribution<>>(*Normal);
    }

    if (Other._FeHGenerators[3] != nullptr)
    {
        auto* Normal = dynamic_cast<Util::TNormalDistribution<>*>(Other._FeHGenerators[3].get());
        _FeHGenerators[3] = std::make_unique<Util::TNormalDistribution<>>(*Normal);
    }
}

FStellarGenerator& FStellarGenerator::operator=(const FStellarGenerator& Other)
{
    if (this != &Other)
    {
        _RandomEngine         = Other._RandomEngine;
        _MagneticGenerators   = Other._MagneticGenerators;
        _SpinGenerators       = Other._SpinGenerators;
        _AgeGenerator         = Other._AgeGenerator;
        _CommonGenerator      = Other._CommonGenerator;
        _MassPdfs             = Other._MassPdfs;
        _MassMaxPdfs          = Other._MassMaxPdfs;
        _AgeMaxPdf            = Other._AgeMaxPdf;
        _AgePdf               = Other._AgePdf;
        _UniverseAge          = Other._UniverseAge;
        _AgeLowerLimit        = Other._AgeLowerLimit;
        _AgeUpperLimit        = Other._AgeUpperLimit;
        _FeHLowerLimit        = Other._FeHLowerLimit;
        _FeHUpperLimit        = Other._FeHUpperLimit;
        _MassLowerLimit       = Other._MassLowerLimit;
        _MassUpperLimit       = Other._MassUpperLimit;
        _CoilTemperatureLimit = Other._CoilTemperatureLimit;
        _dEpdM                = Other._dEpdM;
        _AgeDistribution      = Other._AgeDistribution;
        _FeHDistribution      = Other._FeHDistribution;
        _MassDistribution     = Other._MassDistribution;
        _StellarTypeOption    = Other._StellarTypeOption;
        _MultiplicityOption   = Other._MultiplicityOption;
        _LogMassGenerator     = Other._LogMassGenerator
                              ? std::make_unique<Util::TUniformRealDistribution<>>(
                                  std::log10(Other._MassLowerLimit), std::log10(Other._MassUpperLimit))
                              : nullptr;

        if (Other._FeHGenerators[0] != nullptr)
        {
            auto* LogNormal = dynamic_cast<Util::TLogNormalDistribution<>*>(Other._FeHGenerators[0].get());
            _FeHGenerators[0] = std::make_unique<Util::TLogNormalDistribution<>>(*LogNormal);
        }

        if (Other._FeHGenerators[1] != nullptr)
        {
            auto* Normal = dynamic_cast<Util::TNormalDistribution<>*>(Other._FeHGenerators[1].get());
            _FeHGenerators[1] = std::make_unique<Util::TNormalDistribution<>>(*Normal);
        }

        if (Other._FeHGenerators[2] != nullptr)
        {
            auto* Normal = dynamic_cast<Util::TNormalDistribution<>*>(Other._FeHGenerators[2].get());
            _FeHGenerators[2] = std::make_unique<Util::TNormalDistribution<>>(*Normal);
        }

        if (Other._FeHGenerators[3] != nullptr)
        {
            auto* Normal = dynamic_cast<Util::TNormalDistribution<>*>(Other._FeHGenerators[3].get());
            _FeHGenerators[3] = std::make_unique<Util::TNormalDistribution<>>(*Normal);
        }
    }

    return *this;
}

FStellarGenerator::FBasicProperties FStellarGenerator::GenerateBasicProperties(float Age, float FeH)
{
    FBasicProperties Properties{};
    Properties.StellarTypeOption = _StellarTypeOption;

    // 生成 3 个基本参数
    if (std::isnan(Age)) // 非有效数值，使用分布生成随机值
    {
        switch (_AgeDistribution)
        {
        case EGenerationDistribution::kFromPdf:
        {
            glm::vec2 MaxPdf = _AgeMaxPdf;
            if (!(_AgeLowerLimit < _UniverseAge - 1.38e10f + _AgeMaxPdf.x &&
                  _AgeUpperLimit > _UniverseAge - 1.38e10f + _AgeMaxPdf.x))
            {
                if (_AgeLowerLimit > _UniverseAge - 1.38e10f + _AgeMaxPdf.x)
                {
                    MaxPdf.y = _AgePdf(glm::vec3(), _AgeLowerLimit, _UniverseAge / 1e9f);
                }
                else if (_AgeUpperLimit < _UniverseAge - 1.38e10f + _AgeMaxPdf.x)
                {
                    MaxPdf.y = _AgePdf(glm::vec3(), _AgeUpperLimit, _UniverseAge / 1e9f);
                }
            }
            Properties.Age = GenerateAge(MaxPdf.y);
            break;
        }
        case EGenerationDistribution::kUniform:
        {
            Properties.Age = _AgeLowerLimit + _CommonGenerator(_RandomEngine) * (_AgeUpperLimit - _AgeLowerLimit);
            break;
        }
        case EGenerationDistribution::kUniformByExponent:
        {
            float Random      = _CommonGenerator(_RandomEngine);
            float LogAgeLower = std::log10(_AgeLowerLimit);
            float LogAgeUpper = std::log10(_AgeUpperLimit);
            Properties.Age    = std::pow(10.0f, LogAgeLower + Random * (LogAgeUpper - LogAgeLower));
            break;
        }
        default:
            break;
        }
    }
    else
    {
        Properties.Age = Age;
    }

    if (std::isnan(FeH)) // 非有效数值，使用分布生成随机值
    {
        Util::TDistribution<>* FeHGenerator = nullptr;

        float FeHLowerLimit = _FeHLowerLimit;
        float FeHUpperLimit = _FeHUpperLimit;

        // 不同的年龄使用不同的分布
        if (Properties.Age > _UniverseAge - 1.38e10f + 8e9f)
        {
            FeHGenerator  = _FeHGenerators[0].get();
            FeHLowerLimit = -_FeHUpperLimit; // 对数分布，但是是反的
            FeHUpperLimit = -_FeHLowerLimit;
        }
        else if (Properties.Age > _UniverseAge - 1.38e10f + 6e9f)
        {
            FeHGenerator = _FeHGenerators[1].get();
        }
        else if (Properties.Age > _UniverseAge - 1.38e10f + 4e9f)
        {
            FeHGenerator = _FeHGenerators[2].get();
        }
        else
        {
            FeHGenerator = _FeHGenerators[3].get();
        }

        do {
            FeH = (*FeHGenerator)(_RandomEngine);
        } while (FeH > FeHUpperLimit || FeH < FeHLowerLimit);

        if (Properties.Age > _UniverseAge - 1.38e10 + 8e9)
        {
            FeH *= -1.0f; // 把对数分布反过来
        }
    }

    Properties.FeH = FeH;

    if (_MultiplicityOption != EMultiplicityGenerationOption::kBinarySecondStar)
    {
        Util::TBernoulliDistribution<> BinaryProbability(0.45 - 0.07 * std::pow(10, FeH));
        if (BinaryProbability(_RandomEngine))
        {
            Properties.MultiplicityOption = EMultiplicityGenerationOption::kBinaryFirstStar;
            Properties.bIsSingleStar      = false;
        }
    }
    else
    {
        Properties.MultiplicityOption = EMultiplicityGenerationOption::kBinarySecondStar;
        Properties.bIsSingleStar      = false;
    }

    if (_MassLowerLimit == 0.0f && _MassUpperLimit == 0.0f)
    {
        Properties.InitialMassSol = 0.0f;
    }
    else
    {
        switch (_MassDistribution)
        {
        case EGenerationDistribution::kFromPdf: {
            glm::vec2 MaxPdf{};
            float LogMassLower = std::log10(_MassLowerLimit);
            float LogMassUpper = std::log10(_MassUpperLimit);
            std::function<float(float)> LogMassPdf = nullptr;

            if (Properties.MultiplicityOption != EMultiplicityGenerationOption::kBinarySecondStar)
            {
                if (Properties.MultiplicityOption == EMultiplicityGenerationOption::kBinaryFirstStar)
                {
                    LogMassPdf = _MassPdfs[1];
                    MaxPdf     = _MassMaxPdfs[1];
                }
                else
                {
                    LogMassPdf = _MassPdfs[0];
                    MaxPdf     = _MassMaxPdfs[0];
                }
            }
            else
            {
                LogMassPdf = _MassPdfs[1];
                MaxPdf     = _MassMaxPdfs[1];
            }

            if (!(LogMassLower < MaxPdf.x && LogMassUpper > MaxPdf.x))
            {
                // 调整最大值，防止接受率过低
                if (LogMassLower > MaxPdf.x)
                {
                    MaxPdf.y = LogMassPdf(LogMassLower);
                }
                else if (LogMassUpper < MaxPdf.x)
                {
                    MaxPdf.y = LogMassPdf(LogMassUpper);
                }
            }

            Properties.InitialMassSol = GenerateMass(MaxPdf.y, LogMassPdf);
            break;
        }
        case EGenerationDistribution::kUniform: {
            Properties.InitialMassSol = _MassLowerLimit + _CommonGenerator(_RandomEngine) * (_MassUpperLimit - _MassLowerLimit);
            break;
        }
        default:
            break;
        }
    }

    return Properties;
}

Astro::AStar FStellarGenerator::GenerateStar()
{
    auto Properties = GenerateBasicProperties();
    return GenerateStar(Properties);
}

Astro::AStar FStellarGenerator::GenerateStar(FBasicProperties& Properties)
{
    if (Util::Equal(Properties.InitialMassSol, -1.0f))
    {
        Properties = GenerateBasicProperties(Properties.Age, Properties.FeH);
    }

    Astro::AStar Star(Properties);
    std::vector<double> StarData;

    switch (Properties.StellarTypeOption)
    {
    case EStellarTypeGenerationOption::kRandom:
    {
        try
        {
            StarData = GetFullMistData(Properties, false, true);
        }
        catch (Astro::AStar& DeathStar)
        {
            DeathStar = static_cast<Astro::AStar>(Properties);
            ProcessDeathStar(EStellarTypeGenerationOption::kRandom, DeathStar);
            if (DeathStar.GetEvolutionPhase() == Astro::AStar::EEvolutionPhase::kNull)
            {
                // 如果爆了，削一半质量
                Properties.InitialMassSol /= 2;
                DeathStar = GenerateStar(Properties);
            }

            return DeathStar;
        }

        break;
    }
    case EStellarTypeGenerationOption::kGiant:
    {
        Properties.Age = std::numeric_limits<float>::quiet_NaN(); // 使用 NaN，在计算年龄的时候根据寿命赋值一个濒死年龄
        StarData = GetFullMistData(Properties, false, true);
        break;
    }
    case EStellarTypeGenerationOption::kDeathStar:
    {
        ProcessDeathStar(EStellarTypeGenerationOption::kDeathStar, Star);
        Properties.Age            = static_cast<float>(Star.GetAge());
        Properties.FeH            = Star.GetFeH();
        Properties.InitialMassSol = Star.GetInitialMass() / kSolarMass;

        if (Star.GetEvolutionPhase() == Astro::AStar::EEvolutionPhase::kNull)
        {
            // 如果爆了，削一半质量
            Properties.InitialMassSol /= 2;
            Star = GenerateStar(Properties);
        }

        return Star;

        break;
    }
    case EStellarTypeGenerationOption::kMergeStar:
    {
        ProcessDeathStar(EStellarTypeGenerationOption::kMergeStar, Star);
        return Star;

        break;
    }
    default:
        break;
    }

    if (StarData.empty())
    {
        return {};
    }

    double Lifetime          = StarData[_kLifetimeIndex];
    double EvolutionProgress = StarData[_kXIndex];
    float  Age               = static_cast<float>(StarData[_kStarAgeIndex]);
    float  RadiusSol         = static_cast<float>(std::pow(10.0, StarData[_kLogRIndex]));
    float  MassSol           = static_cast<float>(StarData[_kStarMassIndex]);
    float  Teff              = static_cast<float>(std::pow(10.0, StarData[_kLogTeffIndex]));
    float  SurfaceZ          = static_cast<float>(std::pow(10.0, StarData[_kLogSurfZIndex]));
    float  SurfaceH1         = static_cast<float>(StarData[_kSurfaceH1Index]);
    float  SurfaceHe3        = static_cast<float>(StarData[_kSurfaceHe3Index]);
    float  CoreTemp          = static_cast<float>(std::pow(10.0, StarData[_kLogCenterTIndex]));
    float  CoreDensity       = static_cast<float>(std::pow(10.0, StarData[_kLogCenterRhoIndex]));
    float  MassLossRate      = static_cast<float>(StarData[_kStarMdotIndex]);

    float LuminositySol  = std::pow(RadiusSol, 2.0f) * std::pow((Teff / kSolarTeff), 4.0f);
    float EscapeVelocity = std::sqrt((2.0f * kGravityConstant * MassSol * kSolarMass) / (RadiusSol * kSolarRadius));

    float LifeProgress         = static_cast<float>(Age / Lifetime);
    float WindSpeedCoefficient = 3.0f - LifeProgress;
    float StellarWindSpeed     = WindSpeedCoefficient * EscapeVelocity;

    float SurfaceEnergeticNuclide = (SurfaceH1 * 0.00002f + SurfaceHe3);
    float SurfaceVolatiles = 1.0f - SurfaceZ - SurfaceEnergeticNuclide;

    float Theta = _CommonGenerator(_RandomEngine) * 2.0f * Math::kPi;
    float Phi   = _CommonGenerator(_RandomEngine) * Math::kPi;

    Astro::AStar::EEvolutionPhase EvolutionPhase = static_cast<Astro::AStar::EEvolutionPhase>(StarData[_kPhaseIndex]);

    Star.SetInitialMass(Star.GetInitialMass() * kSolarMass);
    Star.SetIsSingleStar(Properties.bIsSingleStar);
    Star.SetAge(Age);
    Star.SetMass(MassSol * kSolarMass);
    Star.SetLifetime(Lifetime);
    Star.SetRadius(RadiusSol * kSolarRadius);
    Star.SetEscapeVelocity(EscapeVelocity);
    Star.SetLuminosity(LuminositySol * kSolarLuminosity);
    Star.SetTeff(Teff);
    Star.SetSurfaceH1(SurfaceH1);
    Star.SetSurfaceZ(SurfaceZ);
    Star.SetSurfaceEnergeticNuclide(SurfaceEnergeticNuclide);
    Star.SetSurfaceVolatiles(SurfaceVolatiles);
    Star.SetCoreTemp(CoreTemp);
    Star.SetCoreDensity(CoreDensity * 1000);
    Star.SetStellarWindSpeed(StellarWindSpeed);
    Star.SetStellarWindMassLossRate(-(MassLossRate * kSolarMass / kYearToSecond));
    Star.SetEvolutionProgress(EvolutionProgress);
    Star.SetEvolutionPhase(EvolutionPhase);
    Star.SetNormal(glm::vec2(Theta, Phi));

    CalculateSpectralType(static_cast<float>(StarData.back()), Star);
    GenerateMagnetic(Star);
    GenerateSpin(Star);

    double Mass          = Star.GetMass();
    double Luminosity    = Star.GetLuminosity();
    float  Radius        = Star.GetRadius();
    float  MagneticField = Star.GetMagneticField();

    float MinCoilMass = static_cast<float>(std::max(
        6.6156e14  * std::pow(MagneticField, 2.0f) * std::pow(Luminosity, 1.5) * std::pow(_CoilTemperatureLimit, -6.0f) * std::pow(_dEpdM, -1.0f),
        2.34865e29 * std::pow(MagneticField, 2.0f) * std::pow(Luminosity, 2.0) * std::pow(_CoilTemperatureLimit, -8.0f) * std::pow(Mass, -1.0)
    ));

    Star.SetMinCoilMass(MinCoilMass);

    return Star;
}

template <typename CsvType>
requires std::is_class_v<CsvType>
CsvType* FStellarGenerator::LoadCsvAsset(const std::string& Filename, const std::vector<std::string>& Headers)
{
    auto* AssetManager = Runtime::Asset::FAssetManager::GetInstance();
    {
        std::shared_lock Lock(_kCacheMutex);
        auto* Asset = AssetManager->GetAsset<CsvType>(Filename);
        if (Asset != nullptr)
        {
            return Asset;
        }
    }

    std::unique_lock Lock(_kCacheMutex);
    AssetManager->AddAsset<CsvType>(Filename, CsvType(Filename, Headers));

    return AssetManager->GetAsset<CsvType>(Filename);
}

void FStellarGenerator::InitializeMistData()
{
    if (_kbMistDataInitiated)
    {
        return;
    }

    const std::array kPresetPrefix
    {
        Runtime::Asset::GetAssetFullPath(Runtime::Asset::EAssetType::kDataTable, "StellarParameters/MIST/[Fe_H]=-4.0"),
        Runtime::Asset::GetAssetFullPath(Runtime::Asset::EAssetType::kDataTable, "StellarParameters/MIST/[Fe_H]=-3.0"),
        Runtime::Asset::GetAssetFullPath(Runtime::Asset::EAssetType::kDataTable, "StellarParameters/MIST/[Fe_H]=-2.0"),
        Runtime::Asset::GetAssetFullPath(Runtime::Asset::EAssetType::kDataTable, "StellarParameters/MIST/[Fe_H]=-1.5"),
        Runtime::Asset::GetAssetFullPath(Runtime::Asset::EAssetType::kDataTable, "StellarParameters/MIST/[Fe_H]=-1.0"),
        Runtime::Asset::GetAssetFullPath(Runtime::Asset::EAssetType::kDataTable, "StellarParameters/MIST/[Fe_H]=-0.5"),
        Runtime::Asset::GetAssetFullPath(Runtime::Asset::EAssetType::kDataTable, "StellarParameters/MIST/[Fe_H]=+0.0"),
        Runtime::Asset::GetAssetFullPath(Runtime::Asset::EAssetType::kDataTable, "StellarParameters/MIST/[Fe_H]=+0.5"),
        Runtime::Asset::GetAssetFullPath(Runtime::Asset::EAssetType::kDataTable, "StellarParameters/MIST/WhiteDwarfs/Thin"),
        Runtime::Asset::GetAssetFullPath(Runtime::Asset::EAssetType::kDataTable, "StellarParameters/MIST/WhiteDwarfs/Thick")
    };

    std::vector<float> Masses;

    for (const auto& PrefixDirectory : kPresetPrefix)
    {
        for (const auto& Entry : std::filesystem::directory_iterator(PrefixDirectory))
        {
            std::string Filename = Entry.path().filename().string();

            float Mass = 0.0f;
            std::from_chars(Filename.data(), Filename.data() + Filename.find("Ms_track.csv"), Mass);

            Masses.push_back(Mass);

            if (PrefixDirectory.find("WhiteDwarfs") != std::string::npos)
            {
                LoadCsvAsset<FWdMistData>(PrefixDirectory + "/" + Filename, _kWdMistHeaders);
            }
            else
            {
                LoadCsvAsset<FMistData>(PrefixDirectory + "/" + Filename, _kMistHeaders);
            }
        }

        _kMassFilesCache.emplace(PrefixDirectory, Masses);
        Masses.clear();
    }

    _kbMistDataInitiated = true;
}

void FStellarGenerator::InitializePdfs()
{
    if (_AgePdf == nullptr)
    {
        _AgePdf = DefaultAgePdf;
        _AgeMaxPdf = glm::vec2(8e9, 2.7f);
    }

    if (_MassPdfs[0] == nullptr)
    {
        _MassPdfs[0] = DefaultLogMassPdfSingleStar;
        _MassMaxPdfs[0] = glm::vec2(std::log10(0.1f), 0.158f);
    }

    if (_MassPdfs[1] == nullptr)
    {
        _MassPdfs[1] = DefaultLogMassPdfBinaryStar;
        _MassMaxPdfs[1] = glm::vec2(std::log10(0.22f), 0.086);
    }
}

float FStellarGenerator::GenerateAge(float MaxPdf)
{
    float Age = 0.0f;
    float Probability = 0.0f;
    do {
        Age = _AgeGenerator(_RandomEngine);
        Probability = DefaultAgePdf(glm::vec3(), Age / 1e9f, _UniverseAge / 1e9f);
    } while (_CommonGenerator(_RandomEngine) * MaxPdf > Probability);

    return Age;
}

float FStellarGenerator::GenerateMass(float MaxPdf, auto& LogMassPdf)
{
    float LogMass = 0.0f;
    float Probability = 0.0f;

    float LogMassLower = std::log10(_MassLowerLimit);
    float LogMassUpper = std::log10(_MassUpperLimit);

    if (LogMassUpper >= std::log10(300.0f))
    {
        LogMassUpper = std::log10(299.9f);
    }

    do {
        LogMass = (*_LogMassGenerator)(_RandomEngine);
        Probability = LogMassPdf(LogMass);
    } while ((LogMass < LogMassLower || LogMass > LogMassUpper) || _CommonGenerator(_RandomEngine) * MaxPdf > Probability);

    return std::pow(10.0f, LogMass);
}

std::vector<double>
FStellarGenerator::GetFullMistData(const FBasicProperties& Properties, bool bIsWhiteDwarf, bool bIsSingleWhiteDwarf)
{
    float TargetAge  = Properties.Age;
    float TargetFeH  = Properties.FeH;
    float TargetMass = Properties.InitialMassSol;

    std::string PrefixDirectory;
    std::string MassString;
    std::stringstream MassStream;
    std::pair<std::string, std::string> Files;

    if (!bIsWhiteDwarf)
    {
        const std::array kPresetFeH{ -4.0f, -3.0f, -2.0f, -1.5f, -1.0f, -0.5f, 0.0f, 0.5f };

        float ClosestFeH = *std::min_element(kPresetFeH.begin(), kPresetFeH.end(), [TargetFeH](float Lhs, float Rhs) -> bool
        {
            return std::abs(Lhs - TargetFeH) < std::abs(Rhs - TargetFeH);
        });

        TargetFeH = ClosestFeH;

        MassStream << std::fixed << std::setfill('0') << std::setw(6) << std::setprecision(2) << TargetMass;
        MassString = MassStream.str() + "0";

        std::stringstream FeHStream;
        FeHStream << std::fixed << std::setprecision(1) << TargetFeH;
        PrefixDirectory = FeHStream.str();
        if (TargetFeH >= 0.0f)
        {
            PrefixDirectory.insert(PrefixDirectory.begin(), '+');
        }
        PrefixDirectory.insert(0, Runtime::Asset::GetAssetFullPath(Runtime::Asset::EAssetType::kDataTable,
                                                                   "StellarParameters/MIST/[Fe_H]="));
    }
    else
    {
        if (bIsSingleWhiteDwarf)
        {
            PrefixDirectory = Runtime::Asset::GetAssetFullPath(Runtime::Asset::EAssetType::kDataTable,
                                                               "StellarParameters/MIST/WhiteDwarfs/Thin");
        }
        else
        {
            PrefixDirectory = Runtime::Asset::GetAssetFullPath(Runtime::Asset::EAssetType::kDataTable,
                                                               "StellarParameters/MIST/WhiteDwarfs/Thick");
        }
    }

    std::vector<float> Masses;
    {
        std::shared_lock Lock(_kCacheMutex);
        Masses = _kMassFilesCache[PrefixDirectory];
    }

    auto it = std::lower_bound(Masses.begin(), Masses.end(), TargetMass);
    if (it == Masses.end())
    {
        if (!bIsWhiteDwarf)
        {
            throw std::out_of_range("Mass value out of range.");
        }
        else
        {
            it = std::prev(Masses.end(), 1);
        }
    }

    float LowerMass = 0.0f;
    float UpperMass = 0.0f;

    if (*it == TargetMass)
    {
        LowerMass = UpperMass = *it;
    }
    else
    {
        LowerMass = it == Masses.begin() ? *it : *(it - 1);
        UpperMass = *it;
    }

    float MassCoefficient = (TargetMass - LowerMass) / (UpperMass - LowerMass);

    MassString.clear();
    MassStream.str("");
    MassStream.clear();

    MassStream << std::fixed << std::setfill('0') << std::setw(6) << std::setprecision(2) << LowerMass;
    MassString = MassStream.str() + "0";
    std::string LowerMassFile = PrefixDirectory + "/" + MassString + "Ms_track.csv";

    MassString.clear();
    MassStream.str("");
    MassStream.clear();

    MassStream << std::fixed << std::setfill('0') << std::setw(6) << std::setprecision(2) << UpperMass;
    MassString = MassStream.str() + "0";
    std::string UpperMassFile = PrefixDirectory + "/" + MassString + "Ms_track.csv";

    Files.first  = LowerMassFile;
    Files.second = UpperMassFile;

    std::vector<double> Result = InterpolateMistData(Files, TargetAge, TargetMass, MassCoefficient);
    Result.push_back(TargetFeH); // 加入插值使用的金属丰度，用于计算光谱类型

    return Result;
}

std::vector<double> FStellarGenerator::InterpolateMistData(const std::pair<std::string, std::string>& Files,
                                                           double TargetAge, double TargetMass, double MassCoefficient)
{
    std::vector<double> Result;

    if (Files.first.find("WhiteDwarfs") == std::string::npos)
    {
        if (Files.first != Files.second) [[likely]]
        {
            FMistData* LowerData = LoadCsvAsset<FMistData>(Files.first, _kMistHeaders);
            FMistData* UpperData = LoadCsvAsset<FMistData>(Files.second, _kMistHeaders);

            auto LowerPhaseChanges = FindPhaseChanges(LowerData);
            auto UpperPhaseChanges = FindPhaseChanges(UpperData);

            if (std::isnan(TargetAge)) // 年龄为 NaN 在这里代表要生成濒死恒星
            {
                double LowerLifetime = LowerPhaseChanges.back()[_kStarAgeIndex];
                double UpperLifetime = UpperPhaseChanges.back()[_kStarAgeIndex];
                double Lifetime = LowerLifetime + (UpperLifetime - LowerLifetime) * MassCoefficient;
                TargetAge = Lifetime - 500000;
            }

            std::pair<std::vector<std::vector<double>>, std::vector<std::vector<double>>> PhaseChangePair
            {
                LowerPhaseChanges,
                UpperPhaseChanges
            };

            double EvolutionProgress = CalculateEvolutionProgress(PhaseChangePair, TargetAge, MassCoefficient);

            double LowerLifetime = PhaseChangePair.first.back()[_kStarAgeIndex];
            double UpperLifetime = PhaseChangePair.second.back()[_kStarAgeIndex];

            std::vector<double> LowerRows = InterpolateStarData(LowerData, EvolutionProgress);
            std::vector<double> UpperRows = InterpolateStarData(UpperData, EvolutionProgress);

            LowerRows.push_back(LowerLifetime);
            UpperRows.push_back(UpperLifetime);

            Result = InterpolateFinalData({ LowerRows, UpperRows }, MassCoefficient, false);
        }
        else [[unlikely]]
        {
            FMistData* StarData = LoadCsvAsset<FMistData>(Files.first, _kMistHeaders);
            auto PhaseChanges = FindPhaseChanges(StarData);

            if (std::isnan(TargetAge))
            {
                double Lifetime = PhaseChanges.back()[_kStarAgeIndex];
                TargetAge = Lifetime - 500000;
            }

            double EvolutionProgress = 0.0;
            double Lifetime = 0.0;
            if (TargetMass >= 0.1)
            {
                std::pair<std::vector<std::vector<double>>, std::vector<std::vector<double>>> PhaseChangePair{ PhaseChanges, {} };
                EvolutionProgress = CalculateEvolutionProgress(PhaseChangePair, TargetAge, MassCoefficient);
                Lifetime          = PhaseChanges.back()[_kStarAgeIndex];
                Result            = InterpolateStarData(StarData, EvolutionProgress);
                Result.push_back(Lifetime);
            }
            else
            {
                // 外推小质量恒星的数据
                double OriginalLowerPhaseChangePoint = PhaseChanges[1][_kStarAgeIndex];
                double OriginalUpperPhaseChangePoint = PhaseChanges[2][_kStarAgeIndex];
                double LowerPhaseChangePoint = OriginalLowerPhaseChangePoint * std::pow(TargetMass / 0.1, -1.3);
                double UpperPhaseChangePoint = OriginalUpperPhaseChangePoint * std::pow(TargetMass / 0.1, -1.3);
                Lifetime = UpperPhaseChangePoint;
                if (TargetAge < LowerPhaseChangePoint)
                {
                    EvolutionProgress = TargetAge / LowerPhaseChangePoint - 1;
                }
                else if (LowerPhaseChangePoint <= TargetAge && TargetAge <= UpperPhaseChangePoint)
                {
                    EvolutionProgress = (TargetAge - LowerPhaseChangePoint) / (UpperPhaseChangePoint - LowerPhaseChangePoint);
                }
                else if (TargetAge > UpperPhaseChangePoint)
                {
                    GenerateDeathStarPlaceholder(Lifetime);
                }

                Result = InterpolateStarData(StarData, EvolutionProgress);
                Result.push_back(Lifetime);
                ExpandMistData(TargetMass, Result);
            }
        }
    }
    else
    {
        if (Files.first != Files.second) [[likely]]
        {
            FWdMistData* LowerData = LoadCsvAsset<FWdMistData>(Files.first,  _kWdMistHeaders);
            FWdMistData* UpperData = LoadCsvAsset<FWdMistData>(Files.second, _kWdMistHeaders);

            std::vector<double> LowerRows = InterpolateStarData(LowerData, TargetAge);
            std::vector<double> UpperRows = InterpolateStarData(UpperData, TargetAge);

            Result = InterpolateFinalData({ LowerRows, UpperRows }, MassCoefficient, true);
        }
        else [[unlikely]]
        {
            FWdMistData* StarData = LoadCsvAsset<FWdMistData>(Files.first, _kWdMistHeaders);
            Result = InterpolateStarData(StarData, TargetAge);
        }
    }

    return Result;
}

std::vector<std::vector<double>> FStellarGenerator::FindPhaseChanges(const FMistData* DataCsv)
{
    std::vector<std::vector<double>> Result;

    {
        std::shared_lock Lock(_kCacheMutex);
        if (_kPhaseChangesCache.contains(DataCsv))
        {
            return _kPhaseChangesCache[DataCsv];
        }
    }

    const auto* const DataArray = DataCsv->Data();
    int CurrentPhase = -2;
    for (const auto& Row : *DataArray)
    {
        if (Row[_kPhaseIndex] != CurrentPhase || Row[_kXIndex] == 10.0)
        {
            CurrentPhase = static_cast<int>(Row[_kPhaseIndex]);
            Result.push_back(Row);
        }
    }

    {
        std::unique_lock Lock(_kCacheMutex);
        if (!_kPhaseChangesCache.contains(DataCsv))
        {
            _kPhaseChangesCache.emplace(DataCsv, Result);
        }
        else
        {
            Result = _kPhaseChangesCache[DataCsv];
        }
    }

    return Result;
}

double FStellarGenerator::CalculateEvolutionProgress(std::pair<std::vector<std::vector<double>>,
                                                               std::vector<std::vector<double>>>& PhaseChanges,
                                                     double TargetAge, double MassCoefficient)
{
    double Result = 0.0;
    double Phase  = 0.0;

    if (PhaseChanges.second.empty()) [[unlikely]]
    {
        const auto& TimePointResults = FindSurroundingTimePoints(PhaseChanges.first, TargetAge);
        Phase = TimePointResults.first;
        const auto& TimePoints = TimePointResults.second;
        if (TargetAge > TimePoints.second)
        {
            GenerateDeathStarPlaceholder(TimePoints.second);
        }

        Result = (TargetAge - TimePoints.first) / (TimePoints.second - TimePoints.first) + Phase;
    }
    else [[likely]]
    {
        if (PhaseChanges.first.size() == PhaseChanges.second.size() &&
            (*std::prev(PhaseChanges.first.end(), 2))[_kPhaseIndex] == (*std::prev(PhaseChanges.second.end(), 2))[_kPhaseIndex])
        {
            const auto& TimePointResults = FindSurroundingTimePoints(PhaseChanges, TargetAge, MassCoefficient);

            Phase = TimePointResults.first;
            std::size_t Index = TimePointResults.second;

            if (Index + 1 != PhaseChanges.first.size())
            {
                std::pair<double, double> LowerTimePoints
                {
                    PhaseChanges.first[Index][_kStarAgeIndex],
                    PhaseChanges.first[Index + 1][_kStarAgeIndex]
                };

                std::pair<double, double> UpperTimePoints
                {
                    PhaseChanges.second[Index][_kStarAgeIndex],
                    PhaseChanges.second[Index + 1][_kStarAgeIndex]
                };

                const auto& [LowerLowerTimePoint, LowerUpperTimePoint] = LowerTimePoints;
                const auto& [UpperLowerTimePoint, UpperUpperTimePoint] = UpperTimePoints;

                double LowerTimePoint = LowerLowerTimePoint + (UpperLowerTimePoint - LowerLowerTimePoint) * MassCoefficient;
                double UpperTimePoint = LowerUpperTimePoint + (UpperUpperTimePoint - LowerUpperTimePoint) * MassCoefficient;

                Result = (TargetAge - LowerTimePoint) / (UpperTimePoint - LowerTimePoint) + Phase;

                if (Result > PhaseChanges.first.back()[_kPhaseIndex] + 1)
                {
                    return 0.0;
                }
            }
            else
            {
                Result = 0.0;
            }
        }
        else
        {
            if (PhaseChanges.first.back()[_kPhaseIndex] == PhaseChanges.second.back()[_kPhaseIndex])
            {
                double FirstDiscardTimePoint = 0.0;
                double FirstCommonTimePoint = (*std::prev(PhaseChanges.first.end(), 2))[_kStarAgeIndex];

                std::size_t MinSize = std::min(PhaseChanges.first.size(), PhaseChanges.second.size());
                for (std::size_t i = 0; i != MinSize - 1; ++i)
                {
                    if (PhaseChanges.first[i][_kPhaseIndex] != PhaseChanges.second[i][_kPhaseIndex])
                    {
                        FirstDiscardTimePoint = PhaseChanges.first[i][_kStarAgeIndex];
                        break;
                    }
                }

                double DeltaTimePoint = FirstCommonTimePoint - FirstDiscardTimePoint;
                (*std::prev(PhaseChanges.first.end(), 2))[_kStarAgeIndex] -= DeltaTimePoint;
                PhaseChanges.first.back()[_kStarAgeIndex] -= DeltaTimePoint;
            }

            AlignArrays(PhaseChanges);

            Result = CalculateEvolutionProgress(PhaseChanges, TargetAge, MassCoefficient);
            double IntegerPart = 0.0;
            double FractionalPart = std::modf(Result, &IntegerPart);
            if (PhaseChanges.second.back()[_kPhaseIndex] == 9 && FractionalPart > 0.99 && Result < 9.0 &&
                IntegerPart >= (*std::prev(PhaseChanges.first.end(), 3))[_kPhaseIndex])
            {
                Result = 9.0;
            }
        }
    }

    return Result;
}

std::pair<double, std::pair<double, double>>
FStellarGenerator::FindSurroundingTimePoints(const std::vector<std::vector<double>>& PhaseChanges, double TargetAge)
{
    std::vector<std::vector<double>>::const_iterator LowerTimePoint;
    std::vector<std::vector<double>>::const_iterator UpperTimePoint;

    if (PhaseChanges.size() != 2 || PhaseChanges.front()[_kPhaseIndex] != PhaseChanges.back()[_kPhaseIndex])
    {
        LowerTimePoint = std::lower_bound(PhaseChanges.begin(), PhaseChanges.end(), TargetAge,
        [](const std::vector<double>& Lhs, double Rhs) -> bool
        {
            return Lhs[0] < Rhs;
        });

        UpperTimePoint = std::upper_bound(PhaseChanges.begin(), PhaseChanges.end(), TargetAge,
        [](double Lhs, const std::vector<double>& Rhs) -> bool
        {
            return Lhs < Rhs[0];
        });

        if (LowerTimePoint == UpperTimePoint)
        {
            if (LowerTimePoint != PhaseChanges.begin())
            {
                --LowerTimePoint;
            }
        }

        if (UpperTimePoint == PhaseChanges.end())
        {
            --LowerTimePoint;
            --UpperTimePoint;
        }
    }
    else
    {
        LowerTimePoint = PhaseChanges.begin();
        UpperTimePoint = std::prev(PhaseChanges.end(), 1);
    }

    return { (*LowerTimePoint)[_kXIndex], { (*LowerTimePoint)[_kStarAgeIndex], (*UpperTimePoint)[_kStarAgeIndex] } };
}

std::pair<double, std::size_t>
FStellarGenerator::FindSurroundingTimePoints(const std::pair<std::vector<std::vector<double>>,
                                                             std::vector<std::vector<double>>>& PhaseChanges,
                                             double TargetAge, double MassCoefficient)
{
    std::vector<double> LowerPhaseChangeTimePoints;
    std::vector<double> UpperPhaseChangeTimePoints;
    for (std::size_t i = 0; i != PhaseChanges.first.size(); ++i)
    {
        LowerPhaseChangeTimePoints.push_back(PhaseChanges.first[i][_kStarAgeIndex]);
        UpperPhaseChangeTimePoints.push_back(PhaseChanges.second[i][_kStarAgeIndex]);
    }

    std::vector<double> PhaseChangeTimePoints =
        InterpolateArray({ LowerPhaseChangeTimePoints, UpperPhaseChangeTimePoints }, MassCoefficient);

    if (TargetAge > PhaseChangeTimePoints.back())
    {
        double Lifetime = LowerPhaseChangeTimePoints.back() +
            (UpperPhaseChangeTimePoints.back() - LowerPhaseChangeTimePoints.back()) * MassCoefficient;
        GenerateDeathStarPlaceholder(Lifetime);
    }

    std::vector<std::pair<double, double>> TimePointPairs;
    for (std::size_t i = 0; i != PhaseChanges.first.size(); ++i)
    {
        TimePointPairs.emplace_back(PhaseChanges.first[i][_kPhaseIndex], PhaseChangeTimePoints[i]);
    }

    std::pair<double, std::size_t> Result;
    for (std::size_t i = 0; i != TimePointPairs.size(); ++i)
    {
        if (TimePointPairs[i].second >= TargetAge)
        {
            Result.first = TimePointPairs[i == 0 ? 0 : i - 1].first;
            Result.second = i == 0 ? 0 : i - 1;
            break;
        }
    }

    return Result;
}

void FStellarGenerator::AlignArrays(std::pair<std::vector<std::vector<double>>, std::vector<std::vector<double>>>& Arrays)
{
    if (Arrays.first.back()[_kPhaseIndex] != 9 && Arrays.second.back()[_kPhaseIndex] != 9)
    {
        std::size_t MinSize = std::min(Arrays.first.size(), Arrays.second.size());
        Arrays.first.resize(MinSize);
        Arrays.second.resize(MinSize);
    }
    else if (Arrays.first.back()[_kPhaseIndex] != 9 && Arrays.second.back()[_kPhaseIndex] == 9)
    {
        if (Arrays.first.size() + 1 == Arrays.second.size())
        {
            Arrays.second.pop_back();
            Arrays.second.back()[_kPhaseIndex] = Arrays.first.back()[_kPhaseIndex];
            Arrays.second.back()[_kXIndex] = Arrays.first.back()[_kXIndex];
        }
        else
        {
            std::size_t MinSize = std::min(Arrays.first.size(), Arrays.second.size());
            Arrays.first.resize(MinSize - 1);
            Arrays.second.resize(MinSize - 1);
            Arrays.second.back()[_kPhaseIndex] = Arrays.first.back()[_kPhaseIndex];
            Arrays.second.back()[_kXIndex] = Arrays.first.back()[_kXIndex];
        }
    }
    else if (Arrays.first.back()[_kPhaseIndex] == 9 && Arrays.second.back()[_kPhaseIndex] == 9)
    {
        std::vector<double> LastArray1 = Arrays.first.back();
        std::vector<double> LastArray2 = Arrays.second.back();
        std::vector<double> SubLastArray1 = *std::prev(Arrays.first.end(), 2);
        std::vector<double> SubLastArray2 = *std::prev(Arrays.second.end(), 2);

        std::size_t MinSize = std::min(Arrays.first.size(), Arrays.second.size());

        Arrays.first.resize(MinSize - 2);
        Arrays.second.resize(MinSize - 2);
        Arrays.first.push_back(SubLastArray1);
        Arrays.first.push_back(LastArray1);
        Arrays.second.push_back(SubLastArray2);
        Arrays.second.push_back(LastArray2);
    }
    else
    {
        std::vector<double> LastArray1 = Arrays.first.back();
        std::vector<double> LastArray2 = Arrays.second.back();
        std::size_t MinSize = std::min(Arrays.first.size(), Arrays.second.size());
        Arrays.first.resize(MinSize - 1);
        Arrays.second.resize(MinSize - 1);
        Arrays.first.push_back(LastArray1);
        Arrays.second.push_back(LastArray2);
    }
}

std::vector<double> FStellarGenerator::InterpolateHrDiagram(FStellarGenerator::FHrDiagram* Data, double BvColorIndex)
{
    std::vector<double> Result;

    std::pair<std::vector<double>, std::vector<double>> SurroundingRows;
    try
    {
        SurroundingRows = Data->FindSurroundingValues("B-V", BvColorIndex);
    }
    catch (std::out_of_range& e)
    {
        NpgsCoreError("H-R Diagram interpolation capture exception: " + std::string(e.what()));
    }

    double Coefficient = (BvColorIndex - SurroundingRows.first[0]) / (SurroundingRows.second[0] - SurroundingRows.first[0]);

    auto& Array1 = SurroundingRows.first;
    auto& Array2 = SurroundingRows.second;

    while (!Array1.empty() && !Array2.empty() && (Array1.back() == -1 || Array2.back() == -1))
    {
        Array1.pop_back();
        Array2.pop_back();
    }

    Result = InterpolateArray(SurroundingRows, Coefficient);

    return Result;
}

std::vector<double> FStellarGenerator::InterpolateStarData(FStellarGenerator::FMistData* Data, double EvolutionProgress)
{
    return InterpolateStarData(Data, EvolutionProgress, "x", FStellarGenerator::_kXIndex, false);
}

std::vector<double> FStellarGenerator::InterpolateStarData(FStellarGenerator::FWdMistData* Data, double TargetAge)
{
    return InterpolateStarData(Data, TargetAge, "star_age", FStellarGenerator::_kWdStarAgeIndex, true);
}

std::vector<double> FStellarGenerator::InterpolateStarData(auto* Data, double Target, const std::string& Header,
                                                           int Index, bool bIsWhiteDwarf)
{
    std::vector<double> Result;

    std::pair<std::vector<double>, std::vector<double>> SurroundingRows;
    try
    {
        SurroundingRows = Data->FindSurroundingValues(Header, Target);
    }
    catch (std::out_of_range& e)
    {
        if (!bIsWhiteDwarf)
        {
            NpgsCoreError("Stellar data interpolation capture exception: " + std::string(e.what()));
            NpgsCoreError("Header: {}, Target: {}", Header, Target);
        }
        else
        {
            SurroundingRows.first  = Data->Data()->back();
            SurroundingRows.second = Data->Data()->back();
        }
    }

    if (SurroundingRows.first != SurroundingRows.second)
    {
        if (!bIsWhiteDwarf)
        {
            int LowerPhase = static_cast<int>(SurroundingRows.first[Index]);
            int UpperPhase = static_cast<int>(SurroundingRows.second[Index]);
            if (LowerPhase != UpperPhase)
            {
                SurroundingRows.second[Index] = LowerPhase + 1;
            }
        }

        double Coefficient = (Target - SurroundingRows.first[Index]) /
            (SurroundingRows.second[Index] - SurroundingRows.first[Index]);
        Result = InterpolateFinalData(SurroundingRows, Coefficient, bIsWhiteDwarf);
    }
    else
    {
        Result = SurroundingRows.first;
    }

    return Result;
}

std::vector<double>
FStellarGenerator::InterpolateArray(const std::pair<std::vector<double>, std::vector<double>>& DataArrays, double Coefficient)
{
    if (DataArrays.first.size() != DataArrays.second.size())
    {
        throw std::runtime_error("Data arrays size mismatch.");
    }

    std::size_t Size = DataArrays.first.size();
    std::vector<double> Result(Size);
    for (std::size_t i = 0; i != Size; ++i)
    {
        Result[i] = DataArrays.first[i] + (DataArrays.second[i] - DataArrays.first[i]) * Coefficient;
    }

    return Result;
}

std::vector<double> FStellarGenerator::InterpolateFinalData(const std::pair<std::vector<double>,
                                                                            std::vector<double>>& DataArrays,
                                                            double Coefficient, bool bIsWhiteDwarf)
{
    if (DataArrays.first.size() != DataArrays.second.size())
    {
        throw std::runtime_error("Data arrays size mismatch.");
    }

    std::vector<double> Result = InterpolateArray(DataArrays, Coefficient);

    if (!bIsWhiteDwarf)
    {
        Result[FStellarGenerator::_kPhaseIndex] = DataArrays.first[FStellarGenerator::_kPhaseIndex];
    }

    return Result;
}

void FStellarGenerator::CalculateSpectralType(float FeH, Astro::AStar& StarData)
{
    float Teff = StarData.GetTeff();
    auto EvolutionPhase = StarData.GetEvolutionPhase();

    Astro::FStellarClass::EStellarType StellarType = StarData.GetStellarClass().GetStellarType();
    Astro::FStellarClass::FSpectralType SpectralType;
    SpectralType.bIsAmStar = false;

    std::vector<std::pair<int, int>> SpectralSubclassMap;
    float Subclass = 0.0f;

    float SurfaceH1 = StarData.GetSurfaceH1();
    float MinSurfaceH1 = Astro::AStar::_kFeHSurfaceH1Map.at(FeH) - 0.01f;

    std::function<void(Astro::AStar::EEvolutionPhase)> CalculateSpectralSubclass =
    [&](Astro::AStar::EEvolutionPhase BasePhase) -> void
    {
        std::uint32_t SpectralClass = BasePhase == Astro::AStar::EEvolutionPhase::kWolfRayet ? 11 : 0;

        if (BasePhase != Astro::AStar::EEvolutionPhase::kWolfRayet)
        {
            if (BasePhase == Astro::AStar::EEvolutionPhase::kMainSequence)
            {
                // 如果表面氢质量分数低于 0.5 并且还是主序星阶段，转为 WR 星
                // 该情况只有 O 型星会出现
                if (SurfaceH1 < 0.5f)
                {
                    EvolutionPhase = Astro::AStar::EEvolutionPhase::kWolfRayet;
                    StarData.SetEvolutionPhase(EvolutionPhase);
                    CalculateSpectralSubclass(EvolutionPhase);
                    return;
                }
            }

            const auto& InitialMap = Astro::AStar::_kInitialCommonMap;
            for (auto it = InitialMap.begin(); it != InitialMap.end() - 1; ++it)
            {
                ++SpectralClass;
                if (it->first >= Teff && (it + 1)->first < Teff)
                {
                    SpectralSubclassMap = it->second;
                    break;
                }
            }
        }
        else
        {
            if (Teff >= 200000)
            {
                // 温度超过 20 万 K，直接赋值为 WO2
                SpectralType.HSpectralClass = Astro::FStellarClass::ESpectralClass::kSpectral_WO;
                SpectralType.Subclass = 2.0f;
                return;
            }
            else
            {
                if (SurfaceH1 >= 0.2f)
                {
                    // 根据表面氢质量分数来判断处于的 WR 阶段
                    SpectralSubclassMap = Astro::AStar::_kSpectralSubclassMap_WNxh;
                    SpectralClass = 13;
                    SpectralType.SpecialMark = std::to_underlying(Astro::FStellarClass::ESpecialMark::kCode_h);
                }
                else if (SurfaceH1 >= 0.1f)
                {
                    SpectralSubclassMap = Astro::AStar::_kSpectralSubclassMap_WN;
                    SpectralClass = 13;
                }
                else if (SurfaceH1 < 0.1f && SurfaceH1 > 0.05f)
                {
                    SpectralSubclassMap = Astro::AStar::_kSpectralSubclassMap_WC;
                    SpectralClass = 12;
                }
                else
                {
                    SpectralSubclassMap = Astro::AStar::_kSpectralSubclassMap_WO;
                    SpectralClass = 14;
                }
            }
        }

        SpectralType.HSpectralClass = static_cast<Astro::FStellarClass::ESpectralClass>(SpectralClass);

        if (SpectralSubclassMap.empty())
        {
            NpgsCoreError("Failed to find match subclass map of Age: {}, FeH: {}, Mass: {}, Teff: {}",
                          StarData.GetAge(), StarData.GetFeH(), StarData.GetMass() / kSolarMass, StarData.GetTeff());
        }

        for (auto it = SpectralSubclassMap.begin(); it != SpectralSubclassMap.end() - 1; ++it)
        {
            if (it->first >= Teff && (it + 1)->first < Teff)
            {
                Subclass = static_cast<float>(it->second);
                break;
            }
        }

        if (SpectralType.HSpectralClass == Astro::FStellarClass::ESpectralClass::kSpectral_WN &&
            SpectralType.SpecialMark & std::to_underlying(Astro::FStellarClass::ESpecialMark::kCode_h))
        {
            if (Subclass < 5)
            {
                Subclass = 5.0f;
            }
        }

        if (SpectralType.HSpectralClass == Astro::FStellarClass::ESpectralClass::kSpectral_WO)
        {
            if (Subclass > 4)
            {
                Subclass = 4.0f;
            }
        }

        SpectralType.Subclass = Subclass;
    };

    if (EvolutionPhase != Astro::AStar::EEvolutionPhase::kWolfRayet)
    {
        switch (StellarType)
        {
        case Astro::FStellarClass::EStellarType::kNormalStar:
        {
            if (Teff < 54000)
            {
                CalculateSpectralSubclass(EvolutionPhase);

                if (EvolutionPhase != Astro::AStar::EEvolutionPhase::kWolfRayet)
                {
                    if (EvolutionPhase == Astro::AStar::EEvolutionPhase::kPrevMainSequence)
                    {
                        SpectralType.LuminosityClass = CalculateLuminosityClass(StarData);
                    }
                    else if (EvolutionPhase == Astro::AStar::EEvolutionPhase::kMainSequence)
                    {
                        if (SpectralType.HSpectralClass == Astro::FStellarClass::ESpectralClass::kSpectral_O &&
                            SurfaceH1 < MinSurfaceH1)
                        {
                            SpectralType.LuminosityClass = CalculateLuminosityClass(StarData);
                        }
                        else
                        {
                            SpectralType.LuminosityClass = Astro::FStellarClass::ELuminosityClass::kLuminosity_V;
                        }
                    }
                    else
                    {
                        SpectralType.LuminosityClass = CalculateLuminosityClass(StarData);
                    }
                }
                else
                {
                    SpectralType.LuminosityClass = Astro::FStellarClass::ELuminosityClass::kLuminosity_Unknown;
                }
            }
            else
            {
                // 高于 O2 上限，转到通过表面氢质量分数判断阶段
                if (SurfaceH1 > MinSurfaceH1)
                {
                    SpectralType.HSpectralClass  = Astro::FStellarClass::ESpectralClass::kSpectral_O;
                    SpectralType.Subclass        = 2.0f;
                    SpectralType.LuminosityClass = Astro::FStellarClass::ELuminosityClass::kLuminosity_V;
                }
                else if (SurfaceH1 > 0.5f)
                {
                    SpectralType.HSpectralClass  = Astro::FStellarClass::ESpectralClass::kSpectral_O;
                    SpectralType.Subclass        = 2.0f;
                    SpectralType.LuminosityClass = CalculateLuminosityClass(StarData);
                }
                else
                {
                    CalculateSpectralSubclass(Astro::AStar::EEvolutionPhase::kWolfRayet);
                }
            }

            break;
        }
        case Astro::FStellarClass::EStellarType::kWhiteDwarf:
        {
            double MassSol = StarData.GetMass() / kSolarMass;

            if (Teff >= 12000)
            {
                if (MassSol <= 0.5)
                {
                    SpectralType.HSpectralClass = Astro::FStellarClass::ESpectralClass::kSpectral_DA;
                }
                else
                {
                    if (Teff > 45000)
                    {
                        SpectralType.HSpectralClass = Astro::FStellarClass::ESpectralClass::kSpectral_DO;
                    }
                    else
                    {
                        SpectralType.HSpectralClass = Astro::FStellarClass::ESpectralClass::kSpectral_DB;
                    }
                }
            }
            else
            {
                SpectralType.HSpectralClass = Astro::FStellarClass::ESpectralClass::kSpectral_DC;
            }

            Subclass = 50400.0f / Teff;
            if (Subclass > 9.5f)
            {
                Subclass = 9.5f;
            }

            SpectralType.Subclass = std::round(Subclass * 2.0f) / 2.0f;

            break;
        }
        case Astro::FStellarClass::EStellarType::kNeutronStar:
        {
            SpectralType.HSpectralClass = Astro::FStellarClass::ESpectralClass::kSpectral_Q;
            break;
        }
        case Astro::FStellarClass::EStellarType::kBlackHole:
        {
            SpectralType.HSpectralClass = Astro::FStellarClass::ESpectralClass::kSpectral_X;
            break;
        }
        case Astro::FStellarClass::EStellarType::kDeathStarPlaceholder:
        {
            SpectralType.HSpectralClass = Astro::FStellarClass::ESpectralClass::kSpectral_Unknown;
            break;
        }
        default:
        {
            SpectralType.HSpectralClass = Astro::FStellarClass::ESpectralClass::kSpectral_Unknown;
            break;
        }
        }
    }
    else
    {
        CalculateSpectralSubclass(Astro::AStar::EEvolutionPhase::kWolfRayet);
        SpectralType.LuminosityClass = Astro::FStellarClass::ELuminosityClass::kLuminosity_Unknown;
    }

    Astro::FStellarClass StellarClass(StellarType, SpectralType);
    StarData.SetStellarClass(StellarClass);
}

Astro::FStellarClass::ELuminosityClass FStellarGenerator::CalculateLuminosityClass(const Astro::AStar& StarData)
{
    float MassLossRateSolPerYear = StarData.GetStellarWindMassLossRate() * kYearToSecond / kSolarMass;
    double MassSol = StarData.GetMass() / kSolarMass;
    Astro::FStellarClass::ELuminosityClass LuminosityClass = Astro::FStellarClass::ELuminosityClass::kLuminosity_Unknown;

    double LuminositySol = StarData.GetLuminosity() / kSolarLuminosity;
    if (LuminositySol > 650000) // 光度高于 650000 Lsun
    {
        LuminosityClass = Astro::FStellarClass::ELuminosityClass::kLuminosity_0;
    }

    if (MassLossRateSolPerYear > 1e-4f && MassSol >= 15) // 表面物质流失率大于 1e-4 Msun/yr 并且质量大于等于 15 Msun
    {
        LuminosityClass = Astro::FStellarClass::ELuminosityClass::kLuminosity_IaPlus;
    }

    if (LuminosityClass != Astro::FStellarClass::ELuminosityClass::kLuminosity_Unknown) // 如果判断为特超巨星，直接返回
    {
        return LuminosityClass;
    }

    std::string HrDiagramDataFilePath =
        Runtime::Asset::GetAssetFullPath(Runtime::Asset::EAssetType::kDataTable, "StellarParameters/H-R Diagram/H-R Diagram.csv");
    FHrDiagram* HrDiagramData = LoadCsvAsset<FHrDiagram>(HrDiagramDataFilePath, _kHrDiagramHeaders);

    float Teff = StarData.GetTeff();
    float BvColorIndex = 0.0f;
    if (std::log10(Teff) < 3.691f)
    {
        BvColorIndex = -3.684f * std::log10(Teff) + 14.551f;
    }
    else
    {
        BvColorIndex = 0.344f * std::pow(std::log10(Teff), 2.0f) - 3.402f * std::log10(Teff) + 8.037f;
    }

    if (BvColorIndex < -0.3f || BvColorIndex > 1.9727273f)
    {
        // 超过 HR 表的范围，使用光度判断
        if (LuminositySol > 100000)
        {
            return Astro::FStellarClass::ELuminosityClass::kLuminosity_Ia;
        }
        else if (LuminositySol > 50000)
        {
            return Astro::FStellarClass::ELuminosityClass::kLuminosity_Iab;
        }
        else if (LuminositySol > 10000)
        {
            return Astro::FStellarClass::ELuminosityClass::kLuminosity_Ib;
        }
        else if (LuminositySol > 1000)
        {
            return Astro::FStellarClass::ELuminosityClass::kLuminosity_II;
        }
        else if (LuminositySol > 100)
        {
            return Astro::FStellarClass::ELuminosityClass::kLuminosity_III;
        }
        else if (LuminositySol > 10)
        {
            return Astro::FStellarClass::ELuminosityClass::kLuminosity_IV;
        }
        else if (LuminositySol > 0.05)
        {
            return Astro::FStellarClass::ELuminosityClass::kLuminosity_V;
        }
        else
        {
            return Astro::FStellarClass::ELuminosityClass::kLuminosity_VI;
        }
    }

    std::vector<double> LuminosityData = InterpolateHrDiagram(HrDiagramData, BvColorIndex);
    if (LuminositySol > LuminosityData[1])
    {
        return Astro::FStellarClass::ELuminosityClass::kLuminosity_Ia;
    }

    double ClosestValue = *std::min_element(LuminosityData.begin() + 1, LuminosityData.end(),
    [LuminositySol](double Lhs, double Rhs) -> bool
    {
        return std::abs(Lhs - LuminositySol) < std::abs(Rhs - LuminositySol);
    });

    while (LuminosityData.size() < 7)
    {
        LuminosityData.push_back(-1);
    }

    if (LuminositySol <= LuminosityData[1] && LuminositySol >= LuminosityData[2] &&
        (ClosestValue == LuminosityData[1] || ClosestValue == LuminosityData[2]))
    {
        LuminosityClass = Astro::FStellarClass::ELuminosityClass::kLuminosity_Iab;
    }
    else
    {
        if (ClosestValue == LuminosityData[2])
        {
            LuminosityClass = Astro::FStellarClass::ELuminosityClass::kLuminosity_Ib;
        }
        else if (ClosestValue == LuminosityData[3])
        {
            LuminosityClass = Astro::FStellarClass::ELuminosityClass::kLuminosity_II;
        }
        else if (ClosestValue == LuminosityData[4])
        {
            LuminosityClass = Astro::FStellarClass::ELuminosityClass::kLuminosity_III;
        }
        else if (ClosestValue == LuminosityData[5])
        {
            LuminosityClass = Astro::FStellarClass::ELuminosityClass::kLuminosity_IV;
        }
        else if (ClosestValue == LuminosityData[6])
        {
            LuminosityClass = Astro::FStellarClass::ELuminosityClass::kLuminosity_V;
        }
    }

    return LuminosityClass;
}

void FStellarGenerator::ProcessDeathStar(EStellarTypeGenerationOption DeathStarTypeOption, Astro::AStar& DeathStar)
{
    double InputAge     = DeathStar.GetAge();
    float  InputFeH     = DeathStar.GetFeH();
    float  InputMassSol = DeathStar.GetInitialMass();

    Astro::AStar::EEvolutionPhase       EvolutionPhase{};
    Astro::AStar::EStarFrom             DeathStarFrom{};
    Astro::FStellarClass::EStellarType  DeathStarType{};
    Astro::FStellarClass::FSpectralType DeathStarClass;

    double DeathStarAge = InputAge - static_cast<float>(DeathStar.GetLifetime());
    float DeathStarMassSol = 0.0f;

    auto CalculateBlackHoleMass = [&, this]() -> float
    {
        FBasicProperties Properties{};
        Properties.Age               = static_cast<float>(DeathStar.GetLifetime() - 100);
        Properties.FeH               = InputFeH;
        Properties.InitialMassSol    = InputMassSol;
        Properties.StellarTypeOption = EStellarTypeGenerationOption::kRandom;

        auto GiantStar = GenerateStar(Properties);

        return static_cast<float>(GiantStar.GetMass() / kSolarMass * 0.8);
    };

    if (InputFeH <= -2.0f && InputMassSol >= 140 && InputMassSol < 250)
    {
        EvolutionPhase = Astro::AStar::EEvolutionPhase::kNull;
        DeathStarFrom  = Astro::AStar::EStarFrom::kPairInstabilitySupernova;
        DeathStarType  = Astro::FStellarClass::EStellarType::kDeathStarPlaceholder;
        DeathStarClass =
        {
            .HSpectralClass  = Astro::FStellarClass::ESpectralClass::kSpectral_Unknown,
            .MSpectralClass  = Astro::FStellarClass::ESpectralClass::kSpectral_Unknown,
            .LuminosityClass = Astro::FStellarClass::ELuminosityClass::kLuminosity_Unknown,
            .SpecialMark     = std::to_underlying(Astro::FStellarClass::ESpecialMark::kCode_Null),
            .Subclass        = 0.0f,
            .AmSubclass      = 0.0f,
            .bIsAmStar       = false
        };
    }
    else if (InputFeH <= -2.0f && InputMassSol >= 250)
    {
        EvolutionPhase = Astro::AStar::EEvolutionPhase::kStellarBlackHole;
        DeathStarFrom  = Astro::AStar::EStarFrom::kPhotondisintegration;
        DeathStarType  = Astro::FStellarClass::EStellarType::kBlackHole;
        DeathStarClass =
        {
            .HSpectralClass  = Astro::FStellarClass::ESpectralClass::kSpectral_X,
            .MSpectralClass  = Astro::FStellarClass::ESpectralClass::kSpectral_Unknown,
            .LuminosityClass = Astro::FStellarClass::ELuminosityClass::kLuminosity_Unknown,
            .SpecialMark     = std::to_underlying(Astro::FStellarClass::ESpecialMark::kCode_Null),
            .Subclass        = 0.0f,
            .AmSubclass      = 0.0f,
            .bIsAmStar       = false
        };
        DeathStarMassSol = CalculateBlackHoleMass();
    }
    else
    {
        if (InputMassSol > -0.75f && InputMassSol < 0.8f)
        {
            DeathStarMassSol = (0.9795f - 0.393f * InputMassSol) * InputMassSol;
        }
        else if (InputMassSol >= 0.8f && InputMassSol < 7.9f)
        {
            DeathStarMassSol =
                -0.00012336f * std::pow(InputMassSol, 6.0f) + 0.003160f * std::pow(InputMassSol, 5.0f) -
                 0.02960f    * std::pow(InputMassSol, 4.0f) + 0.12350f  * std::pow(InputMassSol, 3.0f) -
                 0.21550f    * std::pow(InputMassSol, 2.0f) + 0.19022f  * InputMassSol + 0.46575f;
        }
        else if (InputMassSol >= 7.9f && InputMassSol < 10.0f)
        {
            DeathStarMassSol = 1.301f + 0.008095f * InputMassSol;
        }
        else if (InputMassSol >= 10.0f && InputMassSol < 21.0f)
        {
            DeathStarMassSol = 1.246f + 0.0136f * InputMassSol;
        }
        else if (InputMassSol >= 21.0f && InputMassSol < 23.3537f)
        {
            DeathStarMassSol = std::pow(10.0f, (1.334f - 0.009987f * InputMassSol));
        }
        else if (InputMassSol >= 23.3537f && InputMassSol < 33.75f)
        {
            DeathStarMassSol = 12.1f - 0.763f * InputMassSol + 0.0137f * std::pow(InputMassSol, 2.0f);
        }
        else
        {
            DeathStarMassSol = CalculateBlackHoleMass();
        }

        if (InputMassSol >= 0.075f && InputMassSol < 0.5f)
        {
            EvolutionPhase = Astro::AStar::EEvolutionPhase::kHeliumWhiteDwarf;
            DeathStarFrom  = Astro::AStar::EStarFrom::kSlowColdingDown;
            DeathStarType  = Astro::FStellarClass::EStellarType::kWhiteDwarf;
            DeathStarClass =
            {
                .HSpectralClass  = Astro::FStellarClass::ESpectralClass::kSpectral_Unknown,
                .MSpectralClass  = Astro::FStellarClass::ESpectralClass::kSpectral_Unknown,
                .LuminosityClass = Astro::FStellarClass::ELuminosityClass::kLuminosity_Unknown,
                .SpecialMark     = std::to_underlying(Astro::FStellarClass::ESpecialMark::kCode_Null),
                .Subclass        = 0.0f,
                .AmSubclass      = 0.0f,
                .bIsAmStar       = false
            };
        }
        else if (InputMassSol >= 0.5f && InputMassSol < 8.0f)
        {
            EvolutionPhase = Astro::AStar::EEvolutionPhase::kCarbonOxygenWhiteDwarf;
            DeathStarFrom  = Astro::AStar::EStarFrom::kEnvelopeDisperse;
            DeathStarType  = Astro::FStellarClass::EStellarType::kWhiteDwarf;
            DeathStarClass =
            {
                .HSpectralClass  = Astro::FStellarClass::ESpectralClass::kSpectral_Unknown,
                .MSpectralClass  = Astro::FStellarClass::ESpectralClass::kSpectral_Unknown,
                .LuminosityClass = Astro::FStellarClass::ELuminosityClass::kLuminosity_Unknown,
                .SpecialMark     = std::to_underlying(Astro::FStellarClass::ESpecialMark::kCode_Null),
                .Subclass        = 0.0f,
                .AmSubclass      = 0.0f,
                .bIsAmStar       = false
            };
        }
        else if (InputMassSol >= 8.0f && InputMassSol < 9.759f)
        {
            EvolutionPhase = Astro::AStar::EEvolutionPhase::kOxygenNeonMagnWhiteDwarf;
            DeathStarFrom  = Astro::AStar::EStarFrom::kEnvelopeDisperse;
            DeathStarType  = Astro::FStellarClass::EStellarType::kWhiteDwarf;
            DeathStarClass =
            {
                .HSpectralClass  = Astro::FStellarClass::ESpectralClass::kSpectral_Unknown,
                .MSpectralClass  = Astro::FStellarClass::ESpectralClass::kSpectral_Unknown,
                .LuminosityClass = Astro::FStellarClass::ELuminosityClass::kLuminosity_Unknown,
                .SpecialMark     = std::to_underlying(Astro::FStellarClass::ESpecialMark::kCode_Null),
                .Subclass        = 0.0f,
                .AmSubclass      = 0.0f,
                .bIsAmStar       = false
            };
        }
        else if (InputMassSol >= 9.759f && InputMassSol < 10.0f)
        {
            EvolutionPhase = Astro::AStar::EEvolutionPhase::kNeutronStar;
            DeathStarFrom  = Astro::AStar::EStarFrom::kElectronCaptureSupernova;
            DeathStarType  = Astro::FStellarClass::EStellarType::kNeutronStar;
            DeathStarClass =
            {
                .HSpectralClass  = Astro::FStellarClass::ESpectralClass::kSpectral_Q,
                .MSpectralClass  = Astro::FStellarClass::ESpectralClass::kSpectral_Unknown,
                .LuminosityClass = Astro::FStellarClass::ELuminosityClass::kLuminosity_Unknown,
                .SpecialMark     = std::to_underlying(Astro::FStellarClass::ESpecialMark::kCode_Null),
                .Subclass        = 0.0f,
                .AmSubclass      = 0.0f,
                .bIsAmStar       = false
            };
        }
        else if (InputMassSol >= 10.0f && InputMassSol < 21.0f)
        {
            EvolutionPhase = Astro::AStar::EEvolutionPhase::kNeutronStar;
            DeathStarFrom  = Astro::AStar::EStarFrom::kIronCoreCollapseSupernova;
            DeathStarType  = Astro::FStellarClass::EStellarType::kNeutronStar;
            DeathStarClass =
            {
                .HSpectralClass  = Astro::FStellarClass::ESpectralClass::kSpectral_Q,
                .MSpectralClass  = Astro::FStellarClass::ESpectralClass::kSpectral_Unknown,
                .LuminosityClass = Astro::FStellarClass::ELuminosityClass::kLuminosity_Unknown,
                .SpecialMark     = std::to_underlying(Astro::FStellarClass::ESpecialMark::kCode_Null),
                .Subclass        = 0.0f,
                .AmSubclass      = 0.0f,
                .bIsAmStar       = false
            };
        }
        else if (InputMassSol >= 21.0f && InputMassSol < 23.3537f)
        {
            EvolutionPhase = Astro::AStar::EEvolutionPhase::kStellarBlackHole;
            DeathStarFrom  = Astro::AStar::EStarFrom::kIronCoreCollapseSupernova;
            DeathStarType  = Astro::FStellarClass::EStellarType::kBlackHole;
            DeathStarClass =
            {
                .HSpectralClass  = Astro::FStellarClass::ESpectralClass::kSpectral_X,
                .MSpectralClass  = Astro::FStellarClass::ESpectralClass::kSpectral_Unknown,
                .LuminosityClass = Astro::FStellarClass::ELuminosityClass::kLuminosity_Unknown,
                .SpecialMark     = std::to_underlying(Astro::FStellarClass::ESpecialMark::kCode_Null),
                .Subclass        = 0.0f,
                .AmSubclass      = 0.0f,
                .bIsAmStar       = false
            };
        }
        else if (InputMassSol >= 23.3537f && InputMassSol < 33.75f)
        {
            EvolutionPhase = Astro::AStar::EEvolutionPhase::kNeutronStar;
            DeathStarFrom  = Astro::AStar::EStarFrom::kIronCoreCollapseSupernova;
            DeathStarType  = Astro::FStellarClass::EStellarType::kNeutronStar;
            DeathStarClass =
            {
                .HSpectralClass  = Astro::FStellarClass::ESpectralClass::kSpectral_Q,
                .MSpectralClass  = Astro::FStellarClass::ESpectralClass::kSpectral_Unknown,
                .LuminosityClass = Astro::FStellarClass::ELuminosityClass::kLuminosity_Unknown,
                .SpecialMark     = std::to_underlying(Astro::FStellarClass::ESpecialMark::kCode_Null),
                .Subclass        = 0.0f,
                .AmSubclass      = 0.0f,
                .bIsAmStar       = false
            };
        }
        else
        {
            EvolutionPhase = Astro::AStar::EEvolutionPhase::kStellarBlackHole;
            DeathStarFrom  = Astro::AStar::EStarFrom::kRelativisticJetHypernova;
            DeathStarType  = Astro::FStellarClass::EStellarType::kBlackHole;
            DeathStarClass =
            {
                .HSpectralClass  = Astro::FStellarClass::ESpectralClass::kSpectral_X,
                .MSpectralClass  = Astro::FStellarClass::ESpectralClass::kSpectral_Unknown,
                .LuminosityClass = Astro::FStellarClass::ELuminosityClass::kLuminosity_Unknown,
                .SpecialMark     = std::to_underlying(Astro::FStellarClass::ESpecialMark::kCode_Null),
                .Subclass        = 0.0f,
                .AmSubclass      = 0.0f,
                .bIsAmStar       = false
            };
        }
    }

    if (DeathStarTypeOption == EStellarTypeGenerationOption::kMergeStar ||
        DeathStarType == Astro::FStellarClass::EStellarType::kNeutronStar)
    {
        float MergeStarProbability = 0.1f * static_cast<int>(DeathStar.GetIsSingleStar());
        MergeStarProbability *= static_cast<int>(DeathStarTypeOption != EStellarTypeGenerationOption::kDeathStar);
        Util::TBernoulliDistribution MergeProbability(MergeStarProbability);
        if (DeathStarTypeOption == EStellarTypeGenerationOption::kMergeStar || MergeProbability(_RandomEngine))
        {
            DeathStar.SetIsSingleStar(true);
            DeathStarFrom = Astro::AStar::EStarFrom::kWhiteDwarfMerge;
            Util::TBernoulliDistribution BlackHoleProbability(0.114514);
            float MassSol = 0.0f;
            if (BlackHoleProbability(_RandomEngine))
            {
                Util::TUniformRealDistribution<> MassDistribution(2.6f, 2.76f);
                MassSol        = MassDistribution(_RandomEngine);
                EvolutionPhase = Astro::AStar::EEvolutionPhase::kStellarBlackHole;
                DeathStarType  = Astro::FStellarClass::EStellarType::kBlackHole;
                DeathStarClass =
                {
                    .HSpectralClass  = Astro::FStellarClass::ESpectralClass::kSpectral_X,
                    .MSpectralClass  = Astro::FStellarClass::ESpectralClass::kSpectral_Unknown,
                    .LuminosityClass = Astro::FStellarClass::ELuminosityClass::kLuminosity_Unknown,
                    .SpecialMark     = std::to_underlying(Astro::FStellarClass::ESpecialMark::kCode_Null),
                    .Subclass        = 0.0f,
                    .AmSubclass      = 0.0f,
                    .bIsAmStar       = false
                };
            }
            else
            {
                Util::TUniformRealDistribution<> MassDistribution(1.38f, 2.18072f);
                MassSol        = MassDistribution(_RandomEngine);
                EvolutionPhase = Astro::AStar::EEvolutionPhase::kNeutronStar;
                DeathStarType  = Astro::FStellarClass::EStellarType::kNeutronStar;
                DeathStarClass =
                {
                    .HSpectralClass  = Astro::FStellarClass::ESpectralClass::kSpectral_Q,
                    .MSpectralClass  = Astro::FStellarClass::ESpectralClass::kSpectral_Unknown,
                    .LuminosityClass = Astro::FStellarClass::ELuminosityClass::kLuminosity_Unknown,
                    .SpecialMark     = std::to_underlying(Astro::FStellarClass::ESpecialMark::kCode_Null),
                    .Subclass        = 0.0f,
                    .AmSubclass      = 0.0f,
                    .bIsAmStar       = false
                };
            }

            DeathStarMassSol = MassSol;
        }
    }

    float StarAge                 = 0.0f;
    float LogR                    = 0.0f;
    float LogTeff                 = 0.0f;
    float LogCenterT              = 0.0f;
    float LogCenterRho            = 0.0f;
    float SurfaceZ                = 0.0f;
    float SurfaceEnergeticNuclide = 0.0f;
    float SurfaceVolatiles        = 0.0f;

    switch (DeathStarType)
    {
    case Astro::FStellarClass::EStellarType::kWhiteDwarf:
    {
        FBasicProperties WhiteDwarfBasicProperties =
        {
            .Age            = static_cast<float>(DeathStarAge),
            .FeH            = 0.0f,
            .InitialMassSol = DeathStarMassSol
        };

        std::vector<double> WhiteDwarfData = GetFullMistData(WhiteDwarfBasicProperties, true, true);

        StarAge      = static_cast<float>(WhiteDwarfData[_kWdStarAgeIndex]);
        LogR         = static_cast<float>(WhiteDwarfData[_kWdLogRIndex]);
        LogTeff      = static_cast<float>(WhiteDwarfData[_kWdLogTeffIndex]);
        LogCenterT   = static_cast<float>(WhiteDwarfData[_kWdLogCenterTIndex]);
        LogCenterRho = static_cast<float>(WhiteDwarfData[_kWdLogCenterRhoIndex]);

        if (DeathStarMassSol < 0.2f || DeathStarMassSol > 1.3f)
        {
            LogR         = std::log10(0.0323f - 0.021384f * DeathStarMassSol);
            LogCenterT   = std::numeric_limits<float>::min();
            LogCenterRho = std::numeric_limits<float>::min();
        }

        if (DeathStarAge > StarAge)
        {
            LogTeff = static_cast<float>(
                std::log10(std::pow(10.0, LogTeff) * std::pow((20.0 * StarAge) / (DeathStarAge + 19.0 * StarAge), 7.0 / 4.0)));
            LogCenterT = std::numeric_limits<float>::min();
        }

        SurfaceZ                = 0.0f;
        SurfaceEnergeticNuclide = 0.0f;
        SurfaceVolatiles        = 1.0f;

        break;
    }
    case Astro::FStellarClass::EStellarType::kNeutronStar:
    {
        if (DeathStarAge < 1e5f)
        {
            DeathStarAge += 1e5f;
        }

        float Radius = 0.0f;
        if (DeathStarMassSol <= 0.77711f)
        {
            Radius = -4.783f + 2.565f / DeathStarMassSol + 42.0f * DeathStarMassSol - 55.4f * std::pow(DeathStarMassSol, 2.0f) +
                34.93f * std::pow(DeathStarMassSol, 3.0f) - 8.4f * std::pow(DeathStarMassSol, 4.0f);
        }
        else if (DeathStarMassSol <= 2.0181f)
        {
            Radius = 11.302f - 0.35184f * DeathStarMassSol;
        }
        else
        {
            Radius = -31951.1f + 63121.8f * DeathStarMassSol - 46717.8f * std::pow(DeathStarMassSol, 2.0f) +
                15358.4f * std::pow(DeathStarMassSol, 3.0f) - 1892.365f * std::pow(DeathStarMassSol, 4.0f);
        }

        LogR    = std::log10(Radius * 1000 / kSolarRadius);
        LogTeff = static_cast<float>(std::log10(1.5e8 * std::pow((DeathStarAge - 1e5) + 22000, -0.5)));

        SurfaceZ                = std::numeric_limits<float>::quiet_NaN();
        SurfaceEnergeticNuclide = std::numeric_limits<float>::quiet_NaN();
        SurfaceVolatiles        = std::numeric_limits<float>::quiet_NaN();

        break;
    }
    case Astro::FStellarClass::EStellarType::kBlackHole:
    {
        LogR                    = std::numeric_limits<float>::quiet_NaN();
        LogTeff                 = std::numeric_limits<float>::quiet_NaN();
        LogCenterT              = std::numeric_limits<float>::quiet_NaN();
        LogCenterRho            = std::numeric_limits<float>::quiet_NaN();
        SurfaceZ                = std::numeric_limits<float>::quiet_NaN();
        SurfaceEnergeticNuclide = std::numeric_limits<float>::quiet_NaN();
        SurfaceVolatiles        = std::numeric_limits<float>::quiet_NaN();

        break;
    }
    default:
        break;
    }

    double EvolutionProgress = static_cast<double>(EvolutionPhase);
    double Age               = DeathStarAge;
    float  MassSol           = DeathStarMassSol;
    float  RadiusSol         = std::pow(10.0f, LogR);
    float  Teff              = std::pow(10.0f, LogTeff);
    float  CoreTemp          = std::pow(10.0f, LogCenterT);
    float  CoreDensity       = std::pow(10.0f, LogCenterRho);

    float LuminositySol  = std::pow(RadiusSol, 2.0f) * std::pow((Teff / kSolarTeff), 4.0f);
    float EscapeVelocity = std::sqrt((2.0f * kGravityConstant * MassSol * kSolarMass) / (RadiusSol * kSolarRadius));

    float Theta = _CommonGenerator(_RandomEngine) * 2.0f * Math::kPi;
    float Phi   = _CommonGenerator(_RandomEngine) * Math::kPi;

    DeathStar.SetInitialMass(InputMassSol * kSolarMass);
    DeathStar.SetAge(Age);
    DeathStar.SetMass(MassSol * kSolarMass);
    DeathStar.SetLifetime(-DeathStar.GetLifetime());
    DeathStar.SetEvolutionProgress(EvolutionProgress);
    DeathStar.SetRadius(RadiusSol * kSolarRadius);
    DeathStar.SetEscapeVelocity(EscapeVelocity);
    DeathStar.SetLuminosity(LuminositySol * kSolarLuminosity);
    DeathStar.SetTeff(Teff);
    DeathStar.SetSurfaceZ(SurfaceZ);
    DeathStar.SetSurfaceEnergeticNuclide(SurfaceEnergeticNuclide);
    DeathStar.SetSurfaceVolatiles(SurfaceVolatiles);
    DeathStar.SetCoreTemp(CoreTemp);
    DeathStar.SetCoreDensity(CoreDensity * 1000);
    DeathStar.SetEvolutionPhase(EvolutionPhase);
    DeathStar.SetNormal(glm::vec2(Theta, Phi));
    DeathStar.SetStarFrom(DeathStarFrom);
    DeathStar.SetStellarClass(Astro::FStellarClass(DeathStarType, DeathStarClass));

    CalculateSpectralType(0.0, DeathStar);
    GenerateMagnetic(DeathStar);
    GenerateSpin(DeathStar);
}

void FStellarGenerator::GenerateMagnetic(Astro::AStar& StarData)
{
    Util::TDistribution<>* MagneticGenerator = nullptr;

    Astro::FStellarClass::EStellarType StellarType = StarData.GetStellarClass().GetStellarType();
    float MassSol = static_cast<float>(StarData.GetMass() / kSolarMass);
    Astro::AStar::EEvolutionPhase EvolutionPhase = StarData.GetEvolutionPhase();

    float MagneticField = 0.0f;

    switch (StellarType)
    {
    case Astro::FStellarClass::EStellarType::kNormalStar:
    {
        if (MassSol >= 0.075f && MassSol < 0.33f)
        {
            MagneticGenerator = &_MagneticGenerators[0];
        }
        else if (MassSol >= 0.33f && MassSol < 0.6f)
        {
            MagneticGenerator = &_MagneticGenerators[1];
        }
        else if (MassSol >= 0.6f && MassSol < 1.5f)
        {
            MagneticGenerator = &_MagneticGenerators[2];
        }
        else if (MassSol >= 1.5f && MassSol < 20.0f)
        {
            auto SpectralType = StarData.GetStellarClass().Data();
            if (EvolutionPhase == Astro::AStar::EEvolutionPhase::kMainSequence &&
                (SpectralType.HSpectralClass == Astro::FStellarClass::ESpectralClass::kSpectral_A ||
                 SpectralType.HSpectralClass == Astro::FStellarClass::ESpectralClass::kSpectral_B))
            {
                Util::TBernoulliDistribution ProbabilityGenerator(0.15); //  p 星的概率
                if (ProbabilityGenerator(_RandomEngine))
                {
                    MagneticGenerator = &_MagneticGenerators[3];
                    SpectralType.SpecialMark |= std::to_underlying(Astro::FStellarClass::ESpecialMark::kCode_p);
                    StarData.SetStellarClass(Astro::FStellarClass(Astro::FStellarClass::EStellarType::kNormalStar, SpectralType));
                }
                else
                {
                    MagneticGenerator = &_MagneticGenerators[4];
                }
            }
            else
            {
                MagneticGenerator = &_MagneticGenerators[4];
            }
        }
        else
        {
            MagneticGenerator = &_MagneticGenerators[5];
        }

        MagneticField = std::pow(10.0f, (*MagneticGenerator)(_RandomEngine)) / 10000;

        break;
    }
    case Astro::FStellarClass::EStellarType::kWhiteDwarf:
    {
        MagneticGenerator = &_MagneticGenerators[6];
        MagneticField = std::pow(10.0f, (*MagneticGenerator)(_RandomEngine));
        break;
    }
    case Astro::FStellarClass::EStellarType::kNeutronStar:
    {
        MagneticGenerator = &_MagneticGenerators[7];
        MagneticField = (*MagneticGenerator)(_RandomEngine) /
            static_cast<float>(std::pow((0.034f * StarData.GetAge() / 1e4f), 1.17f) + 0.84f);
        break;
    }
    case Astro::FStellarClass::EStellarType::kBlackHole:
    {
        MagneticField = 0.0f;
        break;
    }
    case Astro::FStellarClass::EStellarType::kDeathStarPlaceholder:
    {
        break;
    }
    default:
        break;
    }

    StarData.SetMagneticField(MagneticField);
}

void FStellarGenerator::GenerateSpin(Astro::AStar& StarData)
{
    Astro::FStellarClass::EStellarType StellarType = StarData.GetStellarClass().GetStellarType();
    float StarAge   = static_cast<float>(StarData.GetAge());
    float MassSol   = static_cast<float>(StarData.GetMass() / kSolarMass);
    float RadiusSol = StarData.GetRadius() / kSolarRadius;
    float Spin      = 0.0f;

    Util::TDistribution<float>* SpinGenerator = nullptr;

    switch (StellarType)
    {
    case Astro::FStellarClass::EStellarType::kNormalStar:
    {
        float Base = 1.0f + _CommonGenerator(_RandomEngine);
        if (StarData.GetStellarClass().Data().SpecialMark & std::to_underlying(Astro::FStellarClass::ESpecialMark::kCode_p))
        {
            Base *= 10;
        }

        float LogMass = std::log10(MassSol);
        float Term1   = 0.0f;
        float Term2   = 0.0f;
        float Term3   = std::pow(2.0f, std::sqrt(Base * (StarAge + 1e6f) * 1e-9f));

        if (MassSol <= 1.4f)
        {
            Term1 = std::pow(10.0f, 30.893f - 25.34303f * std::exp(LogMass) + 21.7577f * LogMass +
                             7.34205f * std::pow(LogMass, 2.0f) + 0.12951f * std::pow(LogMass, 3.0f));
            Term2 = std::pow(RadiusSol / std::pow(MassSol, 0.9f), 2.5f);
        }
        else
        {
            Term1 = std::pow(10.0f, 28.0784f - 22.15753f * std::exp(LogMass) + 12.55134f * LogMass +
                             30.9045f * std::pow(LogMass, 2.0f) - 10.1479f * std::pow(LogMass, 3.0f) +
                             4.6894f  * std::pow(LogMass, 4.0f));
            Term2 = std::pow(RadiusSol / (1.1062f * std::pow(MassSol, 0.6f)), 2.5f);
        }

        Spin = Term1 * Term2 * Term3;

        break;
    }
    case Astro::FStellarClass::EStellarType::kWhiteDwarf:
    {
        SpinGenerator = &_SpinGenerators[0];
        Spin = std::pow(10.0f, (*SpinGenerator)(_RandomEngine));
        break;
    }
    case Astro::FStellarClass::EStellarType::kNeutronStar:
    {
        Spin = (StarAge * 3 * 1e-9f) + 1e-3f;
        break;
    }
    case Astro::FStellarClass::EStellarType::kBlackHole: // 此处表示无量纲自旋参数，而非自转时间
    {
        SpinGenerator = &_SpinGenerators[1];
        Spin = (*SpinGenerator)(_RandomEngine);
        break;
    }
    default:
        break;
    }

    if (StellarType != Astro::FStellarClass::EStellarType::kBlackHole)
    {
        float Oblateness = 4.0f * std::pow(Math::kPi, 2.0f) * std::pow(StarData.GetRadius(), 3.0f);
        Oblateness /= (std::pow(Spin, 2.0f) * kGravityConstant * static_cast<float>(StarData.GetMass()));
        StarData.SetOblateness(Oblateness);
    }

    StarData.SetSpin(Spin);
}

void FStellarGenerator::ExpandMistData(double TargetMass, std::vector<double>& StarData)
{
    double RadiusSol     = std::pow(10.0, StarData[_kLogRIndex]);
    double Teff          = std::pow(10.0, StarData[_kLogTeffIndex]);
    double LuminositySol = std::pow(RadiusSol, 2.0) * std::pow((Teff / kSolarTeff), 4.0);

    double& StarMass = StarData[_kStarMassIndex];
    double& StarMdot = StarData[_kStarMdotIndex];
    double& LogR     = StarData[_kLogRIndex];
    double& LogTeff  = StarData[_kLogTeffIndex];

    double LogL = std::log10(LuminositySol);

    StarMass = TargetMass * (StarMass / 0.1);
    StarMdot = TargetMass * (StarMdot / 0.1);

    RadiusSol = std::pow(10.0, LogR) * std::pow(TargetMass / 0.1, 2.3);
    LuminositySol = std::pow(10.0, LogL) * std::pow(TargetMass / 0.1, 2.3);

    Teff    = kSolarTeff * std::pow((LuminositySol / std::pow(RadiusSol, 2.0)), 0.25);
    LogTeff = std::log10(Teff);

    LogR = std::log10(RadiusSol);
}

const int FStellarGenerator::_kStarAgeIndex        = 0;
const int FStellarGenerator::_kStarMassIndex       = 1;
const int FStellarGenerator::_kStarMdotIndex       = 2;
const int FStellarGenerator::_kLogTeffIndex        = 3;
const int FStellarGenerator::_kLogRIndex           = 4;
const int FStellarGenerator::_kLogSurfZIndex       = 5;
const int FStellarGenerator::_kSurfaceH1Index      = 6;
const int FStellarGenerator::_kSurfaceHe3Index     = 7;
const int FStellarGenerator::_kLogCenterTIndex     = 8;
const int FStellarGenerator::_kLogCenterRhoIndex   = 9;
const int FStellarGenerator::_kPhaseIndex          = 10;
const int FStellarGenerator::_kXIndex              = 11;
const int FStellarGenerator::_kLifetimeIndex       = 12;

const int FStellarGenerator::_kWdStarAgeIndex      = 0;
const int FStellarGenerator::_kWdLogRIndex         = 1;
const int FStellarGenerator::_kWdLogTeffIndex      = 2;
const int FStellarGenerator::_kWdLogCenterTIndex   = 3;
const int FStellarGenerator::_kWdLogCenterRhoIndex = 4;

const std::vector<std::string> FStellarGenerator::_kMistHeaders
{
    "star_age", "star_mass", "star_mdot", "log_Teff", "log_R", "log_surf_z",
    "surface_h1", "surface_he3", "log_center_T", "log_center_Rho", "phase", "x"
};

const std::vector<std::string> FStellarGenerator::_kWdMistHeaders
{
    "star_age", "log_R", "log_Teff", "log_center_T", "log_center_Rho"
};

const std::vector<std::string> FStellarGenerator::_kHrDiagramHeaders{ "B-V", "Ia", "Ib", "II", "III", "IV", "V" };
std::unordered_map<std::string, std::vector<float>> FStellarGenerator::_kMassFilesCache;
std::unordered_map<const FStellarGenerator::FMistData*, std::vector<std::vector<double>>> FStellarGenerator::_kPhaseChangesCache;
std::shared_mutex FStellarGenerator::_kCacheMutex;
bool FStellarGenerator::_kbMistDataInitiated = false;

_GENERATOR_END
_SYSTEM_END
_NPGS_END
