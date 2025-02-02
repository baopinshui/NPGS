#pragma once

#include <cstdint>
#include <boost/multiprecision/cpp_int.hpp>

#include "Engine/Core/Base/Base.h"
#include "Engine/Core/Types/Entries/NpgsObject.h"

_NPGS_BEGIN
_INTELLI_BEGIN

class FStandard : public INpgsObject
{
public:
    enum class ELifePhase : int
    {
        kNull                  = 0,
        kLuca                  = 1,
        kGreatOxygenationEvent = 2,
        kMultiCellularLife     = 3,
        kCenoziocEra           = 4,
        kSatTeeTouy            = 5, // 被土著去城市化了
        kSatTeeTouyButByAsi    = 6, // 土著被 Artifical Stupid Idiot 图图了
        kNewCivilization       = 7,
    };

    struct FLifeProperties
    {
        boost::multiprecision::uint128_t OrganismBiomass; // 生物量，单位 kg
        float OrganismUsedPower{};                        // 生物圈使用的总功率，单位 W
        ELifePhase Phase{ ELifePhase::kNull };            // 生命阶段
    };

    struct FCivilizationProperties
    {
        boost::multiprecision::uint128_t AtrificalStructureMass;  // 文明造物总质量，单位 kg
        boost::multiprecision::uint128_t CitizenBiomass;          // 文明生物生物量，单位 kg
        boost::multiprecision::uint128_t UseableEnergeticNuclide; // 可用含能核素总质量，单位 kg
        boost::multiprecision::uint128_t OrbitAssetsMass;         // 轨道资产总质量，单位 kg
        std::uint64_t GeneralintelligenceCount{};                 // 通用智能个体的数量
        float GeneralIntelligenceAverageSynapseActivationCount{}; // 通用智能个体的智力活动，单位 o/s
        float GeneralIntelligenceSynapseCount{};                  // 通用智能个体的突触数
        float GeneralIntelligenceAverageLifetime{};               // 通用智能个体的平均寿命，单位 yr
        float CitizenUsedPower{};                                 // 文明使用的总功率，单位 W
        float CivilizationProgress{};                             // 文明演化进度
        float StoragedHistoryDataSize{};                          // 存储的历史数据总大小，单位 bit
        float TeamworkCoefficient{};                              // 合作系数
        bool  bIsIndependentIndividual{ false };                  // 是否为独立个体
    };

public:
    FStandard() = default;
    FStandard(const FLifeProperties& LifeProperties, const FCivilizationProperties& CivilizationProperties);
    ~FStandard() = default;

    // Setters
    // Setters for LifeProperties
    // --------------------------
    FStandard& SetOrganismBiomass(float OrganismBiomass);
    FStandard& SetOrganismBiomass(const boost::multiprecision::uint128_t& OrganismBiomass);
    FStandard& SetOrganismUsedPower(float OrganismUsedPower);
    FStandard& SetLifePhase(ELifePhase Phase);

    // Setters for CivilizationProperties
    // ----------------------------------
    FStandard& SetAtrificalStructureMass(float AtrificalStructureMass);
    FStandard& SetAtrificalStructureMass(const boost::multiprecision::uint128_t& AtrificalStructureMass);
    FStandard& SetCitizenBiomass(float CitizenBiomass);
    FStandard& SetCitizenBiomass(const boost::multiprecision::uint128_t& CitizenBiomass);
    FStandard& SetUseableEnergeticNuclide(float UseableEnergeticNuclide);
    FStandard& SetUseableEnergeticNuclide(const boost::multiprecision::uint128_t& UseableEnergeticNuclide);
    FStandard& SetOrbitAssetsMass(float OrbitAssetsMass);
    FStandard& SetOrbitAssetsMass(const boost::multiprecision::uint128_t& OrbitAssetsMass);
    FStandard& SetGeneralintelligenceCount(std::uint64_t GeneralintelligenceCount);
    FStandard& SetGeneralIntelligenceAverageSynapseActivationCount(float GeneralIntelligenceAverageSynapseActivationCount);
    FStandard& SetGeneralIntelligenceSynapseCount(float GeneralIntelligenceSynapseCount);
    FStandard& SetGeneralIntelligenceAverageLifetime(float GeneralIntelligenceAverageLifetime);
    FStandard& SetCitizenUsedPower(float CitizenUsedPower);
    FStandard& SetCivilizationProgress(float CivilizationProgress);
    FStandard& SetStoragedHistoryDataSize(float StoragedHistoryDataSize);
    FStandard& SetTeamworkCoefficient(float TeamworkCoefficient);
    FStandard& SetIsIndependentIndividual(bool bIsIndependentIndividual);

    // Getters
    // Getters for LifeProperties
    // --------------------------
    const boost::multiprecision::uint128_t& GetOrganismBiomass() const;
    float GetOrganismUsedPower() const;
    ELifePhase GetLifePhase() const;

    template <typename DigitalType>
    DigitalType GetOrganismBiomassDigital() const;

    // Getters for CivilizationProperties
    // ----------------------------------
    const boost::multiprecision::uint128_t& GetAtrificalStructureMass() const;
    const boost::multiprecision::uint128_t& GetCitizenBiomass() const;
    const boost::multiprecision::uint128_t& GetUseableEnergeticNuclide() const;
    const boost::multiprecision::uint128_t& GetOrbitAssetsMass() const;
    std::uint64_t GetGeneralintelligenceCount() const;
    float GetGeneralIntelligenceAverageSynapseActivationCount() const;
    float GetGeneralIntelligenceSynapseCount() const;
    float GetGeneralIntelligenceAverageLifetime() const;
    float GetCitizenUsedPower() const;
    float GetCivilizationProgress() const;
    float GetStoragedHistoryDataSize() const;
    float GetTeamworkCoefficient() const;
    bool  IsIndependentIndividual() const;

    template <typename DigitalType>
    DigitalType GetAtrificalStructureMassDigital() const;

    template <typename DigitalType>
    DigitalType GetCitizenBiomassDigital() const;

    template <typename DigitalType>
    DigitalType GetUseableEnergeticNuclideDigital() const;

    template <typename DigitalType>
    DigitalType GetOrbitAssetsMassDigital() const;

public:
    static const float _kNull;
    static const float _kInitialGeneralIntelligence;
    static const float _kUrgesellschaft;
    static const float _kEarlyIndustrielle;
    static const float _kSteamAge;
    static const float _kElectricAge;
    static const float _kAtomicAge;
    static const float _kDigitalAge;
    static const float _kEarlyAsiAge;

private:
    FCivilizationProperties _CivilizationProperties;
    FLifeProperties         _LifeProperties;
};

class FAdvanced : public INpgsObject
{
};

_INTELLI_END
_NPGS_END

#include "Civilization.inl"
