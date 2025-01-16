#pragma once

#include <array>
#include <memory>
#include <random>

#include "Engine/Core/Base/Base.h"
#include "Engine/Core/Types/Entries/Astro/Planet.h"
#include "Engine/Core/Types/Entries/Astro/Star.h"
#include "Engine/Utils/Random.hpp"

_NPGS_BEGIN
_SYSTEM_BEGIN
_GENERATOR_BEGIN

class FCivilizationGenerator
{
public:
    struct FCivilizationGenerationInfo
    {
        const std::seed_seq* SeedSequence{ nullptr };
        float                LifeOccurrenceProbability{ 0.0f };
        bool                 bEnableAsiFilter{ false };
        float                DestroyedByDisasterProbability{ 0.001f };
    };

public:
    FCivilizationGenerator() = delete;
    explicit FCivilizationGenerator(const FCivilizationGenerationInfo& GenerationInfo);

    explicit FCivilizationGenerator(const std::seed_seq& SeedSequence, float LifeOccurrenceProbability,
                                    bool bEnableAsiFilter = false, float DestroyedByDisasterProbability = 0.001f);

    FCivilizationGenerator(const FCivilizationGenerator&)     = default;
    FCivilizationGenerator(FCivilizationGenerator&&) noexcept = default;
    ~FCivilizationGenerator()                                 = default;

    FCivilizationGenerator& operator=(const FCivilizationGenerator&)     = default;
    FCivilizationGenerator& operator=(FCivilizationGenerator&&) noexcept = default;

    void GenerateCivilization(const Astro::AStar* Star, float PoyntingVector, Astro::APlanet* Planet);

private:
    void GenerateLife(double StarAge, float PoyntingVector, Astro::APlanet* Planet);
    void GenerateCivilizationDetails(const Astro::AStar* Star, float PoyntingVector, Astro::APlanet* Planet);

private:
    std::mt19937                     _RandomEngine;
    Util::TUniformRealDistribution<> _CommonGenerator;
    Util::TBernoulliDistribution<>   _AsiFiltedProbability;
    Util::TBernoulliDistribution<>   _DestroyedByDisasterProbability;
    Util::TBernoulliDistribution<>   _LifeOccurrenceProbability;

    static const std::array<float, 7> _kProbabilityListForCenoziocEra;
    static const std::array<float, 7> _kProbabilityListForSatTeeTouyButAsi;
};

_GENERATOR_END
_SYSTEM_END
_NPGS_END
