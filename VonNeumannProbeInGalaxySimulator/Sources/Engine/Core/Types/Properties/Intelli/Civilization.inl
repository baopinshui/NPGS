#include "Civilization.h"

_NPGS_BEGIN
_INTELLI_BEGIN

NPGS_INLINE FStandard& FStandard::SetOrganismBiomass(float OrganismBiomass)
{
    _LifeProperties.OrganismBiomass = boost::multiprecision::uint128_t(OrganismBiomass);
    return *this;
}

NPGS_INLINE FStandard& FStandard::SetOrganismBiomass(const boost::multiprecision::uint128_t& OrganismBiomass)
{
    _LifeProperties.OrganismBiomass = OrganismBiomass;
    return *this;
}

NPGS_INLINE FStandard& FStandard::SetOrganismUsedPower(float OrganismUsedPower)
{
    _LifeProperties.OrganismUsedPower = OrganismUsedPower;
    return *this;
}

NPGS_INLINE FStandard& FStandard::SetLifePhase(ELifePhase Phase)
{
    _LifeProperties.Phase = Phase;
    return *this;
}

NPGS_INLINE FStandard& FStandard::SetAtrificalStructureMass(float AtrificalStructureMass)
{
    _CivilizationProperties.AtrificalStructureMass = boost::multiprecision::uint128_t(AtrificalStructureMass);
    return *this;
}

NPGS_INLINE FStandard& FStandard::SetAtrificalStructureMass(const boost::multiprecision::uint128_t& AtrificalStructureMass)
{
    _CivilizationProperties.AtrificalStructureMass = AtrificalStructureMass;
    return *this;
}

NPGS_INLINE FStandard& FStandard::SetCitizenBiomass(float CitizenBiomass)
{
    _CivilizationProperties.CitizenBiomass = boost::multiprecision::uint128_t(CitizenBiomass);
    return *this;
}

NPGS_INLINE FStandard& FStandard::SetCitizenBiomass(const boost::multiprecision::uint128_t& CitizenBiomass)
{
    _CivilizationProperties.CitizenBiomass = CitizenBiomass;
    return *this;
}

NPGS_INLINE FStandard& FStandard::SetUseableEnergeticNuclide(float UseableEnergeticNuclide)
{
    _CivilizationProperties.UseableEnergeticNuclide = boost::multiprecision::uint128_t(UseableEnergeticNuclide);
    return *this;
}

NPGS_INLINE FStandard& FStandard::SetUseableEnergeticNuclide(const boost::multiprecision::uint128_t& UseableEnergeticNuclide)
{
    _CivilizationProperties.UseableEnergeticNuclide = UseableEnergeticNuclide;
    return *this;
}

NPGS_INLINE FStandard& FStandard::SetOrbitAssetsMass(float OrbitAssetsMass)
{
    _CivilizationProperties.OrbitAssetsMass = boost::multiprecision::uint128_t(OrbitAssetsMass);
    return *this;
}

NPGS_INLINE FStandard& FStandard::SetOrbitAssetsMass(const boost::multiprecision::uint128_t& OrbitAssetsMass)
{
    _CivilizationProperties.OrbitAssetsMass = OrbitAssetsMass;
    return *this;
}

NPGS_INLINE FStandard& FStandard::SetGeneralintelligenceCount(std::uint64_t GeneralintelligenceCount)
{
    _CivilizationProperties.GeneralintelligenceCount = GeneralintelligenceCount;
    return *this;
}

NPGS_INLINE FStandard& FStandard::SetGeneralIntelligenceAverageSynapseActivationCount(float GeneralIntelligenceAverageSynapseActivationCount)
{
    _CivilizationProperties.GeneralIntelligenceAverageSynapseActivationCount = GeneralIntelligenceAverageSynapseActivationCount;
    return *this;
}

NPGS_INLINE FStandard& FStandard::SetGeneralIntelligenceSynapseCount(float GeneralIntelligenceSynapseCount)
{
    _CivilizationProperties.GeneralIntelligenceSynapseCount = GeneralIntelligenceSynapseCount;
    return *this;
}

NPGS_INLINE FStandard& FStandard::SetGeneralIntelligenceAverageLifetime(float GeneralIntelligenceAverageLifetime)
{
    _CivilizationProperties.GeneralIntelligenceAverageLifetime = GeneralIntelligenceAverageLifetime;
    return *this;
}

NPGS_INLINE FStandard& FStandard::SetCitizenUsedPower(float CitizenUsedPower)
{
    _CivilizationProperties.CitizenUsedPower = CitizenUsedPower;
    return *this;
}

NPGS_INLINE FStandard& FStandard::SetCivilizationProgress(float CivilizationProgress)
{
    _CivilizationProperties.CivilizationProgress = CivilizationProgress;
    return *this;
}

NPGS_INLINE FStandard& FStandard::SetStoragedHistoryDataSize(float StoragedHistoryDataSize)
{
    _CivilizationProperties.StoragedHistoryDataSize = StoragedHistoryDataSize;
    return *this;
}

NPGS_INLINE FStandard& FStandard::SetTeamworkCoefficient(float TeamworkCoefficient)
{
    _CivilizationProperties.TeamworkCoefficient = TeamworkCoefficient;
    return *this;
}

NPGS_INLINE FStandard& FStandard::SetIsIndependentIndividual(bool bIsIndependentIndividual)
{
    _CivilizationProperties.bIsIndependentIndividual = bIsIndependentIndividual;
    return *this;
}

NPGS_INLINE const boost::multiprecision::uint128_t& FStandard::GetOrganismBiomass() const
{
    return _LifeProperties.OrganismBiomass;
}

NPGS_INLINE float FStandard::GetOrganismUsedPower() const
{
    return _LifeProperties.OrganismUsedPower;
}

NPGS_INLINE FStandard::ELifePhase FStandard::GetLifePhase() const
{
    return _LifeProperties.Phase;
}

template <typename DigitalType>
NPGS_INLINE DigitalType FStandard::GetOrganismBiomassDigital() const
{
    return _LifeProperties.OrganismBiomass.convert_to<DigitalType>();
}

NPGS_INLINE const boost::multiprecision::uint128_t& FStandard::GetAtrificalStructureMass() const
{
    return _CivilizationProperties.AtrificalStructureMass;
}

NPGS_INLINE const boost::multiprecision::uint128_t& FStandard::GetCitizenBiomass() const
{
    return _CivilizationProperties.CitizenBiomass;
}

NPGS_INLINE const boost::multiprecision::uint128_t& FStandard::GetUseableEnergeticNuclide() const
{
    return _CivilizationProperties.UseableEnergeticNuclide;
}

NPGS_INLINE const boost::multiprecision::uint128_t& FStandard::GetOrbitAssetsMass() const
{
    return _CivilizationProperties.OrbitAssetsMass;
}

NPGS_INLINE std::uint64_t FStandard::GetGeneralintelligenceCount() const
{
    return _CivilizationProperties.GeneralintelligenceCount;
}

NPGS_INLINE float FStandard::GetGeneralIntelligenceAverageSynapseActivationCount() const
{
    return _CivilizationProperties.GeneralIntelligenceAverageSynapseActivationCount;
}

NPGS_INLINE float FStandard::GetGeneralIntelligenceSynapseCount() const
{
    return _CivilizationProperties.GeneralIntelligenceSynapseCount;
}

NPGS_INLINE float FStandard::GetGeneralIntelligenceAverageLifetime() const
{
    return _CivilizationProperties.GeneralIntelligenceAverageLifetime;
}

NPGS_INLINE float FStandard::GetCitizenUsedPower() const
{
    return _CivilizationProperties.CitizenUsedPower;
}

NPGS_INLINE float FStandard::GetCivilizationProgress() const
{
    return _CivilizationProperties.CivilizationProgress;
}

NPGS_INLINE float FStandard::GetStoragedHistoryDataSize() const
{
    return _CivilizationProperties.StoragedHistoryDataSize;
}

NPGS_INLINE float FStandard::GetTeamworkCoefficient() const
{
    return _CivilizationProperties.TeamworkCoefficient;
}

NPGS_INLINE bool FStandard::IsIndependentIndividual() const
{
    return _CivilizationProperties.bIsIndependentIndividual;
}

template <typename DigitalType>
NPGS_INLINE DigitalType FStandard::GetAtrificalStructureMassDigital() const
{
    return _CivilizationProperties.AtrificalStructureMass.convert_to<DigitalType>();
}

template <typename DigitalType>
NPGS_INLINE DigitalType FStandard::GetCitizenBiomassDigital() const
{
    return _CivilizationProperties.CitizenBiomass.convert_to<DigitalType>();
}

template <typename DigitalType>
NPGS_INLINE DigitalType FStandard::GetUseableEnergeticNuclideDigital() const
{
    return _CivilizationProperties.UseableEnergeticNuclide.convert_to<DigitalType>();
}

template <typename DigitalType>
NPGS_INLINE DigitalType FStandard::GetOrbitAssetsMassDigital() const
{
    return _CivilizationProperties.OrbitAssetsMass.convert_to<DigitalType>();
}

_INTELLI_END
_NPGS_END
