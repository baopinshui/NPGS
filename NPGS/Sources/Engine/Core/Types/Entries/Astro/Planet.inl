#include "Planet.h"

_NPGS_BEGIN
_ASTRO_BEGIN

NPGS_INLINE APlanet& APlanet::SetExtendedProperties(FExtendedProperties&& ExtraProperties)
{
    _ExtraProperties = std::move(ExtraProperties);
    return *this;
}

NPGS_INLINE const APlanet::FExtendedProperties& APlanet::GetExtendedProperties() const
{
    return _ExtraProperties;
}

NPGS_INLINE APlanet& APlanet::SetAtmosphereMass(const FComplexMass& AtmosphereMass)
{
    _ExtraProperties.AtmosphereMass = AtmosphereMass;
    return *this;
}

NPGS_INLINE APlanet& APlanet::SetCoreMass(const FComplexMass& CoreMass)
{
    _ExtraProperties.CoreMass = CoreMass;
    return *this;
}

NPGS_INLINE APlanet& APlanet::SetOceanMass(const FComplexMass& OceanMass)
{
    _ExtraProperties.OceanMass = OceanMass;
    return *this;
}

NPGS_INLINE APlanet& APlanet::SetCrustMineralMass(float CrustMineralMass)
{
    _ExtraProperties.CrustMineralMass = boost::multiprecision::uint128_t(CrustMineralMass);
    return *this;
}

NPGS_INLINE APlanet& APlanet::SetCrustMineralMass(const boost::multiprecision::uint128_t& CrustMineralMass)
{
    _ExtraProperties.CrustMineralMass = CrustMineralMass;
    return *this;
}

NPGS_INLINE APlanet& APlanet::SetCivilizationData(std::unique_ptr<Intelli::FStandard>&& CivilizationData)
{
    _ExtraProperties.CivilizationData = std::move(CivilizationData);
    return *this;
}

NPGS_INLINE APlanet& APlanet::SetBalanceTemperature(float BalanceTemperature)
{
    _ExtraProperties.BalanceTemperature = BalanceTemperature;
    return *this;
}

NPGS_INLINE APlanet& APlanet::SetMigration(bool bIsMigrated)
{
    _ExtraProperties.bIsMigrated = bIsMigrated;
    return *this;
}

NPGS_INLINE APlanet& APlanet::SetPlanetType(EPlanetType Type)
{
    _ExtraProperties.Type = Type;
    return *this;
}

NPGS_INLINE APlanet& APlanet::SetAtmosphereMassZ(float AtmosphereMassZ)
{
    _ExtraProperties.AtmosphereMass.Z = boost::multiprecision::uint128_t(AtmosphereMassZ);
    return *this;
}

NPGS_INLINE APlanet& APlanet::SetAtmosphereMassZ(const boost::multiprecision::uint128_t& AtmosphereMassZ)
{
    _ExtraProperties.AtmosphereMass.Z = AtmosphereMassZ;
    return *this;
}

NPGS_INLINE APlanet& APlanet::SetAtmosphereMassVolatiles(float AtmosphereMassVolatiles)
{
    _ExtraProperties.AtmosphereMass.Volatiles = boost::multiprecision::uint128_t(AtmosphereMassVolatiles);
    return *this;
}

NPGS_INLINE APlanet& APlanet::SetAtmosphereMassVolatiles(const boost::multiprecision::uint128_t& AtmosphereMassVolatiles)
{
    _ExtraProperties.AtmosphereMass.Volatiles = AtmosphereMassVolatiles;
    return *this;
}

NPGS_INLINE APlanet& APlanet::SetAtmosphereMassEnergeticNuclide(float AtmosphereMassEnergeticNuclide)
{
    _ExtraProperties.AtmosphereMass.EnergeticNuclide = boost::multiprecision::uint128_t(AtmosphereMassEnergeticNuclide);
    return *this;
}

NPGS_INLINE APlanet& APlanet::SetAtmosphereMassEnergeticNuclide(const boost::multiprecision::uint128_t& AtmosphereMassEnergeticNuclide)
{
    _ExtraProperties.AtmosphereMass.EnergeticNuclide = AtmosphereMassEnergeticNuclide;
    return *this;
}

NPGS_INLINE APlanet& APlanet::SetCoreMassZ(float CoreMassZ)
{
    _ExtraProperties.CoreMass.Z = boost::multiprecision::uint128_t(CoreMassZ);
    return *this;
}

NPGS_INLINE APlanet& APlanet::SetCoreMassZ(const boost::multiprecision::uint128_t& CoreMassZ)
{
    _ExtraProperties.CoreMass.Z = CoreMassZ;
    return *this;
}

NPGS_INLINE APlanet& APlanet::SetCoreMassVolatiles(float CoreMassVolatiles)
{
    _ExtraProperties.CoreMass.Volatiles = boost::multiprecision::uint128_t(CoreMassVolatiles);
    return *this;
}

NPGS_INLINE APlanet& APlanet::SetCoreMassVolatiles(const boost::multiprecision::uint128_t& CoreMassVolatiles)
{
    _ExtraProperties.CoreMass.Volatiles = CoreMassVolatiles;
    return *this;
}

NPGS_INLINE APlanet& APlanet::SetCoreMassEnergeticNuclide(float CoreMassEnergeticNuclide)
{
    _ExtraProperties.CoreMass.EnergeticNuclide = boost::multiprecision::uint128_t(CoreMassEnergeticNuclide);
    return *this;
}

NPGS_INLINE APlanet& APlanet::SetCoreMassEnergeticNuclide(const boost::multiprecision::uint128_t& CoreMassEnergeticNuclide)
{
    _ExtraProperties.CoreMass.EnergeticNuclide = CoreMassEnergeticNuclide;
    return *this;
}

NPGS_INLINE APlanet& APlanet::SetOceanMassZ(float OceanMassZ)
{
    _ExtraProperties.OceanMass.Z = boost::multiprecision::uint128_t(OceanMassZ);
    return *this;
}

NPGS_INLINE APlanet& APlanet::SetOceanMassZ(const boost::multiprecision::uint128_t& OceanMassZ)
{
    _ExtraProperties.OceanMass.Z = OceanMassZ;
    return *this;
}

NPGS_INLINE APlanet& APlanet::SetOceanMassVolatiles(float OceanMassVolatiles)
{
    _ExtraProperties.OceanMass.Volatiles = boost::multiprecision::uint128_t(OceanMassVolatiles);
    return *this;
}

NPGS_INLINE APlanet& APlanet::SetOceanMassVolatiles(const boost::multiprecision::uint128_t& OceanMassVolatiles)
{
    _ExtraProperties.OceanMass.Volatiles = OceanMassVolatiles;
    return *this;
}

NPGS_INLINE APlanet& APlanet::SetOceanMassEnergeticNuclide(float OceanMassEnergeticNuclide)
{
    _ExtraProperties.OceanMass.EnergeticNuclide = boost::multiprecision::uint128_t(OceanMassEnergeticNuclide);
    return *this;
}

NPGS_INLINE APlanet& APlanet::SetOceanMassEnergeticNuclide(const boost::multiprecision::uint128_t& OceanMassEnergeticNuclide)
{
    _ExtraProperties.OceanMass.EnergeticNuclide = OceanMassEnergeticNuclide;
    return *this;
}

NPGS_INLINE const FComplexMass& APlanet::GetAtmosphereMassStruct() const
{
    return _ExtraProperties.AtmosphereMass;
}

NPGS_INLINE const boost::multiprecision::uint128_t APlanet::GetAtmosphereMass() const
{
    return GetAtmosphereMassZ() + GetAtmosphereMassVolatiles() + GetAtmosphereMassEnergeticNuclide();
}

NPGS_INLINE const boost::multiprecision::uint128_t& APlanet::GetAtmosphereMassZ() const
{
    return _ExtraProperties.AtmosphereMass.Z;
}

NPGS_INLINE const boost::multiprecision::uint128_t& APlanet::GetAtmosphereMassVolatiles() const
{
    return _ExtraProperties.AtmosphereMass.Volatiles;
}

NPGS_INLINE const boost::multiprecision::uint128_t& APlanet::GetAtmosphereMassEnergeticNuclide() const
{
    return _ExtraProperties.AtmosphereMass.EnergeticNuclide;
}

NPGS_INLINE const FComplexMass& APlanet::GetCoreMassStruct() const
{
    return _ExtraProperties.CoreMass;
}

NPGS_INLINE const boost::multiprecision::uint128_t APlanet::GetCoreMass() const
{
    return GetCoreMassZ() + GetCoreMassVolatiles() + GetCoreMassEnergeticNuclide();
}

NPGS_INLINE const boost::multiprecision::uint128_t& APlanet::GetCoreMassZ() const
{
    return _ExtraProperties.CoreMass.Z;
}

NPGS_INLINE const boost::multiprecision::uint128_t& APlanet::GetCoreMassVolatiles() const
{
    return _ExtraProperties.CoreMass.Volatiles;
}

NPGS_INLINE const boost::multiprecision::uint128_t& APlanet::GetCoreMassEnergeticNuclide() const
{
    return _ExtraProperties.CoreMass.EnergeticNuclide;
}

NPGS_INLINE const FComplexMass& APlanet::GetOceanMassStruct() const
{
    return _ExtraProperties.OceanMass;
}

NPGS_INLINE const boost::multiprecision::uint128_t APlanet::GetOceanMass() const
{
    return GetOceanMassZ() + GetOceanMassVolatiles() + GetOceanMassEnergeticNuclide();
}

NPGS_INLINE const boost::multiprecision::uint128_t& APlanet::GetOceanMassZ() const
{
    return _ExtraProperties.OceanMass.Z;
}

NPGS_INLINE const boost::multiprecision::uint128_t& APlanet::GetOceanMassVolatiles() const
{
    return _ExtraProperties.OceanMass.Volatiles;
}

NPGS_INLINE const boost::multiprecision::uint128_t& APlanet::GetOceanMassEnergeticNuclide() const
{
    return _ExtraProperties.OceanMass.EnergeticNuclide;
}

NPGS_INLINE const boost::multiprecision::uint128_t APlanet::GetMass() const
{
    return GetAtmosphereMass() + GetOceanMass() + GetCoreMass() + GetCrustMineralMass();
}

NPGS_INLINE const boost::multiprecision::uint128_t& APlanet::GetCrustMineralMass() const
{
    return _ExtraProperties.CrustMineralMass;
}

NPGS_INLINE float APlanet::GetBalanceTemperature() const
{
    return _ExtraProperties.BalanceTemperature;
}

NPGS_INLINE bool APlanet::GetMigration() const
{
    return _ExtraProperties.bIsMigrated;
}

NPGS_INLINE APlanet::EPlanetType APlanet::GetPlanetType() const
{
    return _ExtraProperties.Type;
}

template <typename DigitalType>
NPGS_INLINE DigitalType APlanet::GetAtmosphereMassDigital() const
{
    return GetAtmosphereMass().convert_to<DigitalType>();
}

template <typename DigitalType>
NPGS_INLINE DigitalType APlanet::GetAtmosphereMassZDigital() const
{
    return GetAtmosphereMassZ().convert_to<DigitalType>();
}

template <typename DigitalType>
NPGS_INLINE DigitalType APlanet::GetAtmosphereMassVolatilesDigital() const
{
    return GetAtmosphereMassVolatiles().convert_to<DigitalType>();
}

template <typename DigitalType>
NPGS_INLINE DigitalType APlanet::GetAtmosphereMassEnergeticNuclideDigital() const
{
    return GetAtmosphereMassEnergeticNuclide().convert_to<DigitalType>();
}

template <typename DigitalType>
NPGS_INLINE DigitalType APlanet::GetCoreMassDigital() const
{
    return GetCoreMass().convert_to<DigitalType>();
}

template <typename DigitalType>
NPGS_INLINE DigitalType APlanet::GetCoreMassZDigital() const
{
    return GetCoreMassZ().convert_to<DigitalType>();
}

template <typename DigitalType>
NPGS_INLINE DigitalType APlanet::GetCoreMassVolatilesDigital() const
{
    return GetCoreMassVolatiles().convert_to<DigitalType>();
}

template <typename DigitalType>
NPGS_INLINE DigitalType APlanet::GetCoreMassEnergeticNuclideDigital() const
{
    return GetCoreMassEnergeticNuclide().convert_to<DigitalType>();
}

template <typename DigitalType>
NPGS_INLINE DigitalType APlanet::GetOceanMassDigital() const
{
    return GetOceanMass().convert_to<DigitalType>();
}

template <typename DigitalType>
NPGS_INLINE DigitalType APlanet::GetOceanMassZDigital() const
{
    return GetOceanMassZ().convert_to<DigitalType>();
}

template <typename DigitalType>
NPGS_INLINE DigitalType APlanet::GetOceanMassVolatilesDigital() const
{
    return GetOceanMassVolatiles().convert_to<DigitalType>();
}

template <typename DigitalType>
NPGS_INLINE DigitalType APlanet::GetOceanMassEnergeticNuclideDigital() const
{
    return GetOceanMassEnergeticNuclide().convert_to<DigitalType>();
}

template <typename DigitalType>
NPGS_INLINE DigitalType APlanet::GetMassDigital() const
{
    return GetMass().convert_to<DigitalType>();
}

template <typename DigitalType>
NPGS_INLINE DigitalType APlanet::GetCrustMineralMassDigital() const
{
    return GetCrustMineralMass().convert_to<DigitalType>();
}

NPGS_INLINE Intelli::FStandard& APlanet::CivilizationData()
{
    return *_ExtraProperties.CivilizationData;
}

NPGS_INLINE AAsteroidCluster& AAsteroidCluster::SetMass(const FComplexMass& Mass)
{
    _Properties.Mass = Mass;
    return *this;
}

NPGS_INLINE AAsteroidCluster& AAsteroidCluster::SetMassZ(float MassZ)
{
    _Properties.Mass.Z = boost::multiprecision::uint128_t(MassZ);
    return *this;
}

NPGS_INLINE AAsteroidCluster& AAsteroidCluster::SetMassZ(const boost::multiprecision::uint128_t& MassZ)
{
    _Properties.Mass.Z = MassZ;
    return *this;
}

NPGS_INLINE AAsteroidCluster& AAsteroidCluster::SetMassVolatiles(float MassVolatiles)
{
    _Properties.Mass.Volatiles = boost::multiprecision::uint128_t(MassVolatiles);
    return *this;
}

NPGS_INLINE AAsteroidCluster& AAsteroidCluster::SetMassVolatiles(const boost::multiprecision::uint128_t& MassVolatiles)
{
    _Properties.Mass.Volatiles = MassVolatiles;
    return *this;
}

NPGS_INLINE AAsteroidCluster& AAsteroidCluster::SetMassEnergeticNuclide(float MassEnergeticNuclide)
{
    _Properties.Mass.EnergeticNuclide = boost::multiprecision::uint128_t(MassEnergeticNuclide);
    return *this;
}

NPGS_INLINE AAsteroidCluster& AAsteroidCluster::SetMassEnergeticNuclide(const boost::multiprecision::uint128_t& MassEnergeticNuclide)
{
    _Properties.Mass.EnergeticNuclide = MassEnergeticNuclide;
    return *this;
}

NPGS_INLINE AAsteroidCluster& AAsteroidCluster::SetAsteroidType(EAsteroidType Type)
{
    _Properties.Type = Type;
    return *this;
}

NPGS_INLINE const boost::multiprecision::uint128_t AAsteroidCluster::GetMass() const
{
    return GetMassZ() + GetMassVolatiles() + GetMassEnergeticNuclide();
}

NPGS_INLINE const boost::multiprecision::uint128_t& AAsteroidCluster::GetMassZ() const
{
    return _Properties.Mass.Z;
}

NPGS_INLINE const boost::multiprecision::uint128_t& AAsteroidCluster::GetMassVolatiles() const
{
    return _Properties.Mass.Volatiles;
}

NPGS_INLINE const boost::multiprecision::uint128_t& AAsteroidCluster::GetMassEnergeticNuclide() const
{
    return _Properties.Mass.EnergeticNuclide;
}

NPGS_INLINE AAsteroidCluster::EAsteroidType AAsteroidCluster::GetAsteroidType() const
{
    return _Properties.Type;
}

template <typename DigitalType>
NPGS_INLINE DigitalType AAsteroidCluster::GetMassDigital() const
{
    return GetMass().convert_to<DigitalType>();
}

template <typename DigitalType>
NPGS_INLINE DigitalType AAsteroidCluster::GetMassZDigital() const
{
    return GetMassZ().convert_to<DigitalType>();
}

template <typename DigitalType>
NPGS_INLINE DigitalType AAsteroidCluster::GetMassVolatilesDigital() const
{
    return GetMassVolatiles().convert_to<DigitalType>();
}

template <typename DigitalType>
NPGS_INLINE DigitalType AAsteroidCluster::GetMassEnergeticNuclideDigital() const
{
    return GetMassEnergeticNuclide().convert_to<DigitalType>();
}

_ASTRO_END
_NPGS_END
