#include "CivilizationGenerator.h"

#include <cmath>
#include <print>
#include <utility>

#include "Engine/Core/Math/NumericConstants.h"
#include "Engine/Core/Types/Properties/Intelli/Civilization.h"

#define DEBUG_OUTPUT

_NPGS_BEGIN
_SYSTEM_BEGIN
_GENERATOR_BEGIN

FCivilizationGenerator::FCivilizationGenerator(const FCivilizationGenerationInfo& GenerationInfo)
    :
    _RandomEngine(*GenerationInfo.SeedSequence),
    _CommonGenerator(0.0f, 1.0f),
    _AsiFiltedProbability(static_cast<double>(GenerationInfo.bEnableAsiFilter) * 0.2),
    _DestroyedByDisasterProbability(GenerationInfo.DestroyedByDisasterProbability),
    _LifeOccurrenceProbability(GenerationInfo.LifeOccurrenceProbability)
{
}

FCivilizationGenerator::FCivilizationGenerator(const std::seed_seq& SeedSequence, float LifeOccurrenceProbability,
                                               bool bEnableAsiFilter, float DestroyedByDisasterProbability)
    :
    FCivilizationGenerator(FCivilizationGenerationInfo{
        .SeedSequence                   = &SeedSequence,
        .LifeOccurrenceProbability      = LifeOccurrenceProbability,
        .bEnableAsiFilter               = bEnableAsiFilter,
        .DestroyedByDisasterProbability = DestroyedByDisasterProbability
    })
{
}

FCivilizationGenerator::FCivilizationGenerator(const FCivilizationGenerator& Other)
    :
    _RandomEngine(Other._RandomEngine),
    _CommonGenerator(Other._CommonGenerator),
    _AsiFiltedProbability(Other._AsiFiltedProbability),
    _DestroyedByDisasterProbability(Other._DestroyedByDisasterProbability),
    _LifeOccurrenceProbability(Other._LifeOccurrenceProbability)
{
}

FCivilizationGenerator::FCivilizationGenerator(FCivilizationGenerator&& Other) noexcept
    :
    _RandomEngine(std::move(Other._RandomEngine)),
    _CommonGenerator(std::move(Other._CommonGenerator)),
    _AsiFiltedProbability(std::move(Other._AsiFiltedProbability)),
    _DestroyedByDisasterProbability(std::move(Other._DestroyedByDisasterProbability)),
    _LifeOccurrenceProbability(std::move(Other._LifeOccurrenceProbability))
{
}

FCivilizationGenerator& FCivilizationGenerator::operator=(const FCivilizationGenerator& Other)
{
    if (this != &Other)
    {
        _RandomEngine                   = Other._RandomEngine;
        _CommonGenerator                = Other._CommonGenerator;
        _AsiFiltedProbability           = Other._AsiFiltedProbability;
        _DestroyedByDisasterProbability = Other._DestroyedByDisasterProbability;
        _LifeOccurrenceProbability      = Other._LifeOccurrenceProbability;
    }

    return *this;
}

FCivilizationGenerator& FCivilizationGenerator::operator=(FCivilizationGenerator&& Other)
{
    if (this != &Other)
    {
        _RandomEngine                   = std::move(Other._RandomEngine);
        _CommonGenerator                = std::move(Other._CommonGenerator);
        _AsiFiltedProbability           = std::move(Other._AsiFiltedProbability);
        _DestroyedByDisasterProbability = std::move(Other._DestroyedByDisasterProbability);
        _LifeOccurrenceProbability      = std::move(Other._LifeOccurrenceProbability);
    }

    return *this;
}

void FCivilizationGenerator::GenerateCivilization(const Astro::AStar* Star, float PoyntingVector, Astro::APlanet* Planet)
{
    if (Star->GetAge() < 2.4e9 || !_LifeOccurrenceProbability(_RandomEngine))
    {
        return;
    }

    Planet->SetCivilizationData(std::make_unique<Intelli::FStandard>());

    GenerateLife(Star->GetAge(), PoyntingVector, Planet);
    GenerateCivilizationDetails(Star, PoyntingVector, Planet);

#ifdef DEBUG_OUTPUT
    std::println("");
    std::println("Life details:");
    std::println("Life phase: {}",                                                std::to_underlying(Planet->CivilizationData().GetLifePhase()));
    std::println("Organism biomass: {:.2E} kg",                                   Planet->CivilizationData().GetOrganismBiomassDigital<float>());
    std::println("Organism used power: {:.2E} W",                                 Planet->CivilizationData().GetOrganismUsedPower());
    std::println("Standard civilization details:");
    std::println("Civilization progress: {}",                                     Planet->CivilizationData().GetCivilizationProgress());
    std::println("Atrifical structure mass: {:.2E} kg",                           Planet->CivilizationData().GetAtrificalStructureMassDigital<float>());
    std::println("Citizen biomass: {:.2E} kg",                                    Planet->CivilizationData().GetCitizenBiomassDigital<float>());
    std::println("Useable energetic nuclide: {:.2E} kg",                          Planet->CivilizationData().GetUseableEnergeticNuclideDigital<float>());
    std::println("Orbit assets mass: {:.2E} kg",                                  Planet->CivilizationData().GetOrbitAssetsMassDigital<float>());
    std::println("General intelligence count: {}",                                Planet->CivilizationData().GetGeneralintelligenceCount());
    std::println("General intelligence average synapse activation count: {} o/s", Planet->CivilizationData().GetGeneralIntelligenceAverageSynapseActivationCount());
    std::println("General intelligence synapse count: {}",                        Planet->CivilizationData().GetGeneralIntelligenceSynapseCount());
    std::println("General intelligence average lifetime: {} yr",                  Planet->CivilizationData().GetGeneralIntelligenceAverageLifetime());
    std::println("Storaged history data size: {:.2E} bit",                        Planet->CivilizationData().GetStoragedHistoryDataSize());
    std::println("Citizen used power: {:.2E} W",                                  Planet->CivilizationData().GetCitizenUsedPower());
    std::println("Teamwork coefficient: {}",                                      Planet->CivilizationData().GetTeamworkCoefficient());
    std::println("Is independent individual: {}",                                 Planet->CivilizationData().IsIndependentIndividual());
    std::println("");
#endif // DEBUG_OUTPUT
}

void FCivilizationGenerator::GenerateLife(double StarAge, float PoyntingVector, Astro::APlanet* Planet)
{
    // 计算生命演化阶段
    float Random           = 0.5f + _CommonGenerator(_RandomEngine) + 1.5f;
    auto  LifePhase        =
        static_cast<Intelli::FStandard::ELifePhase>(std::min(4, std::max(1, static_cast<int>(Random * StarAge / (5e8)))));
    auto& CivilizationData = Planet->CivilizationData();

    // 处理生命成矿机制以及 ASI 大过滤器
    if (LifePhase == Intelli::FStandard::ELifePhase::kCenoziocEra)
    {
        Random = 1.0f + _CommonGenerator(_RandomEngine) * 999.0f;
        if (_AsiFiltedProbability(_RandomEngine))
        {
            LifePhase = Intelli::FStandard::ELifePhase::kSatTeeTouyButByAsi; // 被 ASI 去城市化了
            Planet->SetCrustMineralMass(Random * 1e16f + Planet->GetCrustMineralMassDigital<float>());
        }
        else
        {
            if (_DestroyedByDisasterProbability(_RandomEngine))
            {
                LifePhase = Intelli::FStandard::ELifePhase::kSatTeeTouyButByAsi; // 被天灾去城市化或者自己玩死了
            }
            else
            {
                Planet->SetCrustMineralMass(Random * 1e15f + Planet->GetCrustMineralMassDigital<float>());
            }
        }
    }

    CivilizationData.SetLifePhase(LifePhase);

    // 计算生物量和总功率
    double Scale             = (PoyntingVector / kSolarConstantOfEarth) * (Planet->GetRadius() / kEarthRadius);
    float  Exponent          = 0.0f;
    float  CommonRandom      = 0.01f + _CommonGenerator(_RandomEngine) * 0.04f;
    double OrganismBiomass   = 0.0;
    double OrganismUsedPower = 0.0;

    switch (LifePhase)
    {
    case Intelli::FStandard::ELifePhase::kLuca:
        Random            = _CommonGenerator(_RandomEngine);
        OrganismBiomass   = Random * 1e11 * Scale;
        OrganismUsedPower = CommonRandom * 0.1 * OrganismBiomass;
        break;
    case Intelli::FStandard::ELifePhase::kGreatOxygenationEvent:
        Exponent          = -1.0f + _CommonGenerator(_RandomEngine) * 2.0f;
        Random            = std::pow(10.0f, Exponent);
        OrganismBiomass   = Random * 1e12 * Scale;
        OrganismUsedPower = CommonRandom * 0.1 * OrganismBiomass;
        break;
    case Intelli::FStandard::ELifePhase::kMultiCellularLife:
        Exponent          = _CommonGenerator(_RandomEngine) * 2.0f;
        Random            = std::pow(10.0f, Exponent);
        OrganismBiomass   = Random * 1e13 * Scale;
        OrganismUsedPower = CommonRandom * 0.01 * OrganismBiomass;
        break;
    case Intelli::FStandard::ELifePhase::kCenoziocEra:
    case Intelli::FStandard::ELifePhase::kSatTeeTouyButByAsi:
        Exponent          = -1.0f + _CommonGenerator(_RandomEngine) * 2.0f;
        Random            = std::pow(10.0f, Exponent);
        OrganismBiomass   = Random * 1e16 * Scale;
        OrganismUsedPower = CommonRandom * OrganismBiomass;
        break;
    case Intelli::FStandard::ELifePhase::kSatTeeTouy:
    case Intelli::FStandard::ELifePhase::kNewCivilization:
        Random            = _CommonGenerator(_RandomEngine);
        OrganismBiomass   = Random * 1e15 * Scale;
        OrganismUsedPower = CommonRandom * OrganismBiomass;
        break;
    default:
        break;
    }

    if (Planet->GetSpin() > 10 * kDayToSecond) {
        CommonRandom       = 0.01f + _CommonGenerator(_RandomEngine) * 0.04f;
        OrganismBiomass   *= CommonRandom;
        OrganismUsedPower *= CommonRandom;
    }

    CivilizationData.SetOrganismBiomass(boost::multiprecision::uint128_t(OrganismBiomass));
    CivilizationData.SetOrganismUsedPower(static_cast<float>(OrganismUsedPower));
}

void FCivilizationGenerator::GenerateCivilizationDetails(const Astro::AStar* Star, float PoyntingVector, Astro::APlanet* Planet)
{
    const std::array<float, 7>* ProbabilityListPtr = nullptr;
    auto& CivilizationData = Planet->CivilizationData();
    auto  LifePhase        = Planet->CivilizationData().GetLifePhase();

    int   PrimaryLevel      = 0;
    float LevelProgress     = 0.0f;
    float CivilizationLevel = 0.0f;
    if (LifePhase != Intelli::FStandard::ELifePhase::kCenoziocEra &&
        LifePhase != Intelli::FStandard::ELifePhase::kSatTeeTouy  &&
        LifePhase != Intelli::FStandard::ELifePhase::kSatTeeTouyButByAsi)
    {
        CivilizationData.SetCivilizationProgress(0.0f);
    }
    else if (LifePhase == Intelli::FStandard::ELifePhase::kCenoziocEra)
    {
        ProbabilityListPtr = &_kProbabilityListForCenoziocEra;
    }
    else if (LifePhase == Intelli::FStandard::ELifePhase::kSatTeeTouyButByAsi)
    {
        ProbabilityListPtr = &_kProbabilityListForSatTeeTouyButAsi;
    }

    if (ProbabilityListPtr != nullptr)
    {
        float Random = _CommonGenerator(_RandomEngine);
        float CumulativeProbability = 0.0f;

        for (int i = 0; i < 7; ++i)
        {
            CumulativeProbability += (*ProbabilityListPtr)[i];
            if (Random < CumulativeProbability)
            {
                PrimaryLevel = i + 1;
                break;
            }
        }

        if (PrimaryLevel >= Intelli::FStandard::_kDigitalAge)
        {
            if (LifePhase == Intelli::FStandard::ELifePhase::kCenoziocEra)
            {
                LifePhase = Intelli::FStandard::ELifePhase::kSatTeeTouy;
            }
            else if (LifePhase == Intelli::FStandard::ELifePhase::kSatTeeTouyButByAsi)
            {
                LifePhase = Intelli::FStandard::ELifePhase::kNewCivilization;
            }
        }

        LevelProgress     = _CommonGenerator(_RandomEngine);
        CivilizationLevel = static_cast<float>(PrimaryLevel) + LevelProgress;
        CivilizationData.SetCivilizationProgress(CivilizationLevel);
    }

    CivilizationData.SetLifePhase(LifePhase);
    CivilizationData.SetIsIndependentIndividual(true);

    auto GenerateRandom1 = [this]() -> float
    {
        float  Exponent = -1.0f + _CommonGenerator(_RandomEngine) * 2.0f;
        float  Random1  = std::pow(10.0f, Exponent);
        return Random1;
    };

    auto GenerateRandom2 = [this]() -> float
    {
        float  Random2 = 0.01f + _CommonGenerator(_RandomEngine) * 0.04f;
        return Random2;
    };

    float Random1 = GenerateRandom1();
    float Random2 = GenerateRandom2();

    // 文明生物的总生物量（CitizenBiomass）
    boost::multiprecision::uint128_t CitizenBiomass;
    if (CivilizationLevel >= Intelli::FStandard::_kDigitalAge && CivilizationLevel <= Intelli::FStandard::_kEarlyAsiAge)
    {
        double Base        = Random1 * 4e11;
        double Coefficient = std::pow(100.0, LevelProgress);
        double Result      = Base * Coefficient;
        float  Random      = 0.9f + 0.2f * _CommonGenerator(_RandomEngine);
        Result            *= Random;
        CitizenBiomass     = boost::multiprecision::uint128_t(Result);
    }
    else if (CivilizationLevel >= Intelli::FStandard::_kAtomicAge && CivilizationLevel < Intelli::FStandard::_kDigitalAge)
    {
        double Base        = Random1 * 1.15e11;
        double Coefficient = std::pow(3.47826, LevelProgress);
        double Result      = Base * Coefficient;
        float  Random      = 0.9f + 0.2f * _CommonGenerator(_RandomEngine);
        Result            *= Random;
        CitizenBiomass     = boost::multiprecision::uint128_t(Result);
    }
    else if (CivilizationLevel >= Intelli::FStandard::_kElectricAge && CivilizationLevel < Intelli::FStandard::_kAtomicAge)
    {
        double Base        = Random1 * 5e10;
        double Coefficient = std::pow(2.3, LevelProgress);
        double Result      = Base * Coefficient;
        float  Random      = 0.9f + 0.2f * _CommonGenerator(_RandomEngine);
        Result            *= Random;
        CitizenBiomass     = boost::multiprecision::uint128_t(Result);
    }
    else if (CivilizationLevel >= Intelli::FStandard::_kSteamAge && CivilizationLevel < Intelli::FStandard::_kElectricAge)
    {
        double Base        = Random1 * 3e10;
        double Coefficient = std::pow(1.66666, LevelProgress);
        double Result      = Base * Coefficient;
        float  Random      = 0.9f + 0.2f * _CommonGenerator(_RandomEngine);
        Result            *= Random;
        CitizenBiomass     = boost::multiprecision::uint128_t(Result);
    }
    else if (CivilizationLevel >= Intelli::FStandard::_kEarlyIndustrielle && CivilizationLevel < Intelli::FStandard::_kSteamAge)
    {
        double Base        = Random1 * 3e8;
        double Coefficient = std::pow(100.0, LevelProgress);
        double Result      = Base * Coefficient;
        float  Random      = 0.9f + 0.2f * _CommonGenerator(_RandomEngine);
        Result            *= Random;
        CitizenBiomass     = boost::multiprecision::uint128_t(Result);
    }
    else if (CivilizationLevel >= Intelli::FStandard::_kUrgesellschaft && CivilizationLevel < Intelli::FStandard::_kEarlyIndustrielle)
    {
        double Base        = Random1 * 5e7;
        double Coefficient = std::pow(6.0, LevelProgress);
        double Result      = Base * Coefficient;
        float  Random      = 0.9f + 0.2f * _CommonGenerator(_RandomEngine);
        Result            *= Random;
        CitizenBiomass     = boost::multiprecision::uint128_t(Result);
    }
    else if (CivilizationLevel >= Intelli::FStandard::_kInitialGeneralIntelligence && CivilizationLevel < Intelli::FStandard::_kUrgesellschaft)
    {
        double Base        = Random1 * 5e6;
        double Coefficient = std::pow(10.0, LevelProgress);
        double Result      = Base * Coefficient;
        float  Random      = 0.9f + 0.2f * _CommonGenerator(_RandomEngine);
        Result            *= Random;
        CitizenBiomass     = boost::multiprecision::uint128_t(Result);
    }
    else
    {
        CitizenBiomass     = 0;
    }
    CivilizationData.SetCitizenBiomass(CitizenBiomass);

    // 文明造物总质量（AtrificalStructureMass）
    boost::multiprecision::uint128_t AtrificalStructureMass;
    if (CivilizationLevel >= Intelli::FStandard::_kDigitalAge && CivilizationLevel <= Intelli::FStandard::_kEarlyAsiAge)
    {
        double Base            = Random1 * 1e15;
        double Coefficient     = std::pow(1000.0, LevelProgress);
        double Result          = Base * Coefficient;
        float  Random          = 0.9f + 0.2f * _CommonGenerator(_RandomEngine);
        Result                *= Random;
        AtrificalStructureMass = boost::multiprecision::uint128_t(Result);
    }
    else if (CivilizationLevel >= Intelli::FStandard::_kAtomicAge && CivilizationLevel < Intelli::FStandard::_kDigitalAge)
    {
        double Base            = Random1 * 6.25e13;
        double Coefficient     = std::pow(16.0, LevelProgress);
        double Result          = Base * Coefficient;
        float  Random          = 0.9f + 0.2f * _CommonGenerator(_RandomEngine);
        Result                *= Random;
        AtrificalStructureMass = boost::multiprecision::uint128_t(Result);
    }
    else if (CivilizationLevel >= Intelli::FStandard::_kElectricAge && CivilizationLevel < Intelli::FStandard::_kAtomicAge)
    {
        double Base            = Random1 * 1.5e13;
        double Coefficient     = std::pow(4.16666, LevelProgress);
        double Result          = Base * Coefficient;
        float  Random          = 0.9f + 0.2f * _CommonGenerator(_RandomEngine);
        Result                *= Random;
        AtrificalStructureMass = boost::multiprecision::uint128_t(Result);
    }
    else if (CivilizationLevel >= Intelli::FStandard::_kSteamAge && CivilizationLevel < Intelli::FStandard::_kElectricAge)
    {
        double Base            = Random1 * 6e12;
        double Coefficient     = std::pow(2.5, LevelProgress);
        double Result          = Base * Coefficient;
        float  Random          = 0.9f + 0.2f * _CommonGenerator(_RandomEngine);
        Result                *= Random;
        AtrificalStructureMass = boost::multiprecision::uint128_t(Result);
    }
    else if (CivilizationLevel >= Intelli::FStandard::_kEarlyIndustrielle && CivilizationLevel < Intelli::FStandard::_kSteamAge)
    {
        double Base            = Random1 * 6e9;
        double Coefficient     = std::pow(1000.0, LevelProgress);
        double Result          = Base * Coefficient;
        float  Random          = 0.9f + 0.2f * _CommonGenerator(_RandomEngine);
        Result                *= Random;
        AtrificalStructureMass = boost::multiprecision::uint128_t(Result);
    }
    else
    {
        AtrificalStructureMass = 0;
    }
    CivilizationData.SetAtrificalStructureMass(AtrificalStructureMass);

    // 文明总功率（CitizenUsedPower）
    float CitizenUsedPower = 0.0;
    if (CivilizationLevel >= Intelli::FStandard::_kDigitalAge && CivilizationLevel <= Intelli::FStandard::_kEarlyAsiAge)
    {
        float Base         = Random1 * 1e13f;
        float Coefficient  = std::pow(1000.0f, LevelProgress);
        float Result       = Base * Coefficient;
        float CitizenPower = std::max(Result, 0.1f * PoyntingVector * Math::kPi * std::pow(Planet->GetRadius(), 2.0f));
        float Random       = 0.9f + 0.2f * _CommonGenerator(_RandomEngine);
        CitizenUsedPower   = CitizenPower * Random;
    }
    else if (CivilizationLevel >= Intelli::FStandard::_kAtomicAge && CivilizationLevel < Intelli::FStandard::_kDigitalAge)
    {
        float Base         = Random1 * 4e12f;
        float Coefficient  = std::pow(5.0f, LevelProgress);
        float Result       = Base * Coefficient;
        float Random       = 0.9f + 0.2f * _CommonGenerator(_RandomEngine);
        CitizenUsedPower   = Result * Random;
    }
    else if (CivilizationLevel >= Intelli::FStandard::_kElectricAge && CivilizationLevel < Intelli::FStandard::_kAtomicAge)
    {
        float Base         = Random1 * 2.5e11f;
        float Coefficient  = std::pow(16.0f, LevelProgress);
        float Result       = Base * Coefficient;
        float Random       = 0.9f + 0.2f * _CommonGenerator(_RandomEngine);
        CitizenUsedPower   = Result * Random;
    }
    else if (CivilizationLevel >= Intelli::FStandard::_kSteamAge && CivilizationLevel < Intelli::FStandard::_kElectricAge)
    {
        float Base         = Random1 * 6e10f;
        float Coefficient  = std::pow(4.16666f, LevelProgress);
        float Result       = Base * Coefficient;
        float Random       = 0.9f + 0.2f * _CommonGenerator(_RandomEngine);
        CitizenUsedPower   = Result * Random;
    }
    else if (CivilizationLevel >= Intelli::FStandard::_kEarlyIndustrielle && CivilizationLevel < Intelli::FStandard::_kSteamAge)
    {
        float Base         = Random1 * 6e8f;
        float Coefficient  = std::pow(100.0f, LevelProgress);
        float Result       = Base * Coefficient;
        float Random       = 0.9f + 0.2f * _CommonGenerator(_RandomEngine);
        CitizenUsedPower   = Result * Random;
    }
    else if (CivilizationLevel >= Intelli::FStandard::_kUrgesellschaft && CivilizationLevel < Intelli::FStandard::_kEarlyIndustrielle)
    {
        float Base         = Random1 * 1e8f;
        float Coefficient  = std::pow(6.0f, LevelProgress);
        float Result       = Base * Coefficient;
        float Random       = 0.9f + 0.2f * _CommonGenerator(_RandomEngine);
        CitizenUsedPower   = Result * Random;
    }
    else if (CivilizationLevel >= Intelli::FStandard::_kInitialGeneralIntelligence && CivilizationLevel < Intelli::FStandard::_kUrgesellschaft)
    {
        float Base         = Random1 * 1e7f;
        float Coefficient  = std::pow(10.0f, LevelProgress);
        float Result       = Base * Coefficient;
        float Random       = 0.9f + 0.2f * _CommonGenerator(_RandomEngine);
        CitizenUsedPower   = Result * Random;
    }
    else
    {
        CitizenUsedPower   = 0.0;
    }
    CivilizationData.SetCitizenUsedPower(CitizenUsedPower);

    // 存储的历史总信息量（StoragedHistoryDataSize）
    float StoragedHistoryDataSize = 0.0;
    if (CivilizationLevel >= 7.2f && CivilizationLevel <= Intelli::FStandard::_kEarlyAsiAge)
    {
        float Base                = Random1 * 2.5e25f;
        float Coefficient         = std::pow(10.0f, LevelProgress / 0.2f);
        float Result              = Base * Coefficient;
        float Random              = 0.9f + 0.2f * _CommonGenerator(_RandomEngine);
        StoragedHistoryDataSize   = Result * Random;
    }
    else if (CivilizationLevel >= Intelli::FStandard::_kDigitalAge && CivilizationLevel < 7.2f)
    {
        float Base                = Random1 * 1e22f;
        float Coefficient         = std::pow(50.0f, LevelProgress / 0.1f);
        float Result              = Base * Coefficient;
        float Random              = 0.9f + 0.2f * _CommonGenerator(_RandomEngine);
        StoragedHistoryDataSize   = Result * Random;
    }
    else if (CivilizationLevel >= Intelli::FStandard::_kAtomicAge && CivilizationLevel < Intelli::FStandard::_kDigitalAge)
    {
        float Base                = Random1 * 1e20f;
        float Coefficient         = std::pow(100.0f, LevelProgress);
        float Result              = Base * Coefficient;
        float Random              = 0.9f + 0.2f * _CommonGenerator(_RandomEngine);
        StoragedHistoryDataSize   = Result * Random;
    }
    else if (CivilizationLevel >= Intelli::FStandard::_kElectricAge && CivilizationLevel < Intelli::FStandard::_kAtomicAge)
    {
        float Base                = Random1 * 5e17f;
        float Coefficient         = std::pow(200.0f, LevelProgress);
        float Result              = Base * Coefficient;
        float Random              = 0.9f + 0.2f * _CommonGenerator(_RandomEngine);
        StoragedHistoryDataSize   = Result * Random;
    }
    else if (CivilizationLevel >= Intelli::FStandard::_kSteamAge && CivilizationLevel < Intelli::FStandard::_kElectricAge)
    {
        float Base                = Random1 * 1e15f;
        float Coefficient         = std::pow(500.0f, LevelProgress);
        float Result              = Base * Coefficient;
        float Random              = 0.9f + 0.2f * _CommonGenerator(_RandomEngine);
        StoragedHistoryDataSize   = Result * Random;
    }
    else if (CivilizationLevel >= Intelli::FStandard::_kEarlyIndustrielle && CivilizationLevel < Intelli::FStandard::_kSteamAge)
    {
        float Base                = Random1 * 1e12f;
        float Coefficient         = std::pow(1000.0f, LevelProgress);
        float Result              = Base * Coefficient;
        float Random              = 0.9f + 0.2f * _CommonGenerator(_RandomEngine);
        StoragedHistoryDataSize   = Result * Random;
    }
    else if (CivilizationLevel >= Intelli::FStandard::_kUrgesellschaft && CivilizationLevel < Intelli::FStandard::_kEarlyIndustrielle)
    {
        float Base                = Random1 * 1e10f;
        float Coefficient         = std::pow(100.0f, LevelProgress);
        float Result              = Base * Coefficient;
        float Random              = 0.9f + 0.2f * _CommonGenerator(_RandomEngine);
        StoragedHistoryDataSize   = Result * Random;
    }
    else
    {
        StoragedHistoryDataSize   = 0.0;
    }
    CivilizationData.SetStoragedHistoryDataSize(StoragedHistoryDataSize);

    // 通用智能个体平均体重（AverageWeight）
    float AverageWeight = Random1 * Random2 * 1e4f;

    // 通用智能个体的数量（GeneralIntelligenceCount）
    std::uint64_t TotalCount = static_cast<std::uint64_t>(CitizenBiomass.convert_to<float>() / AverageWeight);
    CivilizationData.SetGeneralintelligenceCount(TotalCount);

    // 通用智能个体平均突触数量（GeneralIntelligenceSynapseCount）
    float AverageSynapses = std::sqrt(AverageWeight / 50) * std::sqrt(GenerateRandom1()) * 5e14f;
    CivilizationData.SetGeneralIntelligenceSynapseCount(AverageSynapses);

    // 通用智能个体平均算力（GeneralIntelligenceAverageSynapseActivationCount）
    float AverageCompute = AverageSynapses * 12 * std::sqrt(GenerateRandom2());
    CivilizationData.SetGeneralIntelligenceAverageSynapseActivationCount(AverageCompute);

    // 个体平均寿命（GeneralIntelligenceAverageLifetime）
    float AverageLifetime = AverageSynapses * (7 + CivilizationData.GetCivilizationProgress()) *
                            std::sqrt(GenerateRandom2() * 2e-13f / AverageCompute);
    CivilizationData.SetGeneralIntelligenceAverageLifetime(AverageLifetime);

    // 合作系数（TeamworkCoefficient）
    float TeamworkCoefficient = std::sqrt(GenerateRandom1());
    CivilizationData.SetTeamworkCoefficient(TeamworkCoefficient);

    // 可用含能核素（UseableEnergeticNuclide）
    boost::multiprecision::uint128_t UseableEnergeticNuclide;
    if (CivilizationLevel >= Intelli::FStandard::_kAtomicAge && CivilizationLevel <= Intelli::FStandard::_kEarlyAsiAge)
    {
        double StarAge = Star->GetAge();
        double Base    = Random1 * LevelProgress * 1e9 * 0.63 * std::pow(0.5, StarAge / (8e8));

        if (CivilizationLevel >= Intelli::FStandard::_kDigitalAge)
        {
            double Coefficient = std::pow(1e4, LevelProgress);
            Base *= Coefficient;
        }

        float Random = 0.9f + 0.2f * _CommonGenerator(_RandomEngine);
        Base *= Random;

        UseableEnergeticNuclide = static_cast<boost::multiprecision::uint128_t>(Base);
        CivilizationData.SetUseableEnergeticNuclide(UseableEnergeticNuclide);
    }

    // 最大入轨能力（LaunchCapability）
    double LaunchCapability = 0.0;
    if (CivilizationLevel >= Intelli::FStandard::_kAtomicAge && CivilizationLevel <= Intelli::FStandard::_kEarlyAsiAge)
    {
        double PlanetMass   = Planet->GetMassDigital<double>();
        double PlanetRadius = Planet->GetRadius();
        double Base         = 5e-6;

        LaunchCapability = Base * std::pow(kEarthMass / PlanetMass, 3) *
                           std::pow(PlanetRadius / kEarthRadius, 3) * CitizenUsedPower / std::sqrt(Random1);
    }

    // 轨道资产总质量
    double OrbitAssetsMass = 0.0;
    if (LaunchCapability > 0.0)
    {
        OrbitAssetsMass = std::sqrt(GenerateRandom1()) * LaunchCapability * (CivilizationLevel - 6) / TeamworkCoefficient;
    }
    CivilizationData.SetOrbitAssetsMass(boost::multiprecision::uint128_t(OrbitAssetsMass));

#ifdef DEBUG_OUTPUT
    std::println("");
    std::println("");
#endif // DEBUG_OUTPUT
}

const std::array<float, 7> FCivilizationGenerator::_kProbabilityListForCenoziocEra
{
    0.02f, 0.005f, 1e-4f, 1e-6f, 5e-7f, 4e-7f, 1e-6f
};

const std::array<float, 7> FCivilizationGenerator::_kProbabilityListForSatTeeTouyButAsi
{
    0.2f, 0.05f, 0.001f, 1e-5f, 1e-4f, 1e-4f, 1e-4f
};

_GENERATOR_END
_SYSTEM_END
_NPGS_END
