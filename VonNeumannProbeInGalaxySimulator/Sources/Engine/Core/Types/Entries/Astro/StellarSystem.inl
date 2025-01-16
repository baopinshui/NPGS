#include "StellarSystem.h"

#include <type_traits>

_NPGS_BEGIN
_ASTRO_BEGIN

NPGS_INLINE FOrbit::FOrbitalObject& FOrbit::FOrbitalObject::SetObject(FBaryCenter* SystemBary)
{
    _Object.SystemBary = SystemBary;
    _Type = EObjectType::kBaryCenter;
    return *this;
}

NPGS_INLINE FOrbit::FOrbitalObject& FOrbit::FOrbitalObject::SetObject(AStar* Star)
{
    _Object.Star = Star;
    _Type = EObjectType::kStar;
    return *this;
}

NPGS_INLINE FOrbit::FOrbitalObject& FOrbit::FOrbitalObject::SetObject(APlanet* Planet)
{
    _Object.Planet = Planet;
    _Type = EObjectType::kPlanet;
    return *this;
}

NPGS_INLINE FOrbit::FOrbitalObject& FOrbit::FOrbitalObject::SetObject(AAsteroidCluster* AsteroidCluster)
{
    _Object.Asteroids = AsteroidCluster;
    _Type = EObjectType::kAsteroidCluster;
    return *this;
}

NPGS_INLINE FOrbit::FOrbitalObject& FOrbit::FOrbitalObject::SetObject(Intelli::AArtifact* ArtifactCluster)
{
    _Object.Artifacts = ArtifactCluster;
    _Type = EObjectType::kArtifactCluster;
    return *this;
}

NPGS_INLINE FOrbit::EObjectType FOrbit::FOrbitalObject::GetObjectType() const
{
    return _Type;
}

template <typename ObjectType>
NPGS_INLINE ObjectType* FOrbit::FOrbitalObject::GetObject() const
{
    if constexpr (std::is_same_v<ObjectType, FBaryCenter>)
    {
        return _Type == EObjectType::kBaryCenter ? _Object.SystemBary : nullptr;
    }
    else if constexpr (std::is_same_v<ObjectType, AStar>)
    {
        return _Type == EObjectType::kStar ? _Object.Star : nullptr;
    }
    else if constexpr (std::is_same_v<ObjectType, APlanet>)
    {
        return _Type == EObjectType::kPlanet ? _Object.Planet : nullptr;
    }
    else if constexpr (std::is_same_v<ObjectType, AAsteroidCluster>)
    {
        return _Type == EObjectType::kAsteroidCluster ? _Object.Asteroids : nullptr;
    }
    else if constexpr (std::is_same_v<ObjectType, Intelli::AArtifact>)
    {
        return _Type == EObjectType::kArtifactCluster ? _Object.Artifacts : nullptr;
    }

    return nullptr;
}

NPGS_INLINE FOrbit::FOrbitalDetails& FOrbit::FOrbitalDetails::SetHostOrbit(FOrbit* HostFOrbit)
{
    _HostOrbit = HostFOrbit;
    return *this;
}

NPGS_INLINE FOrbit::FOrbitalDetails& FOrbit::FOrbitalDetails::SetInitialTrueAnomaly(float InitialTrueAnomaly)
{
    _InitialTrueAnomaly = InitialTrueAnomaly;
    return *this;
}

NPGS_INLINE FOrbit* FOrbit::FOrbitalDetails::GetHostOrbit()
{
    return _HostOrbit;
}

NPGS_INLINE FOrbit::FOrbitalObject& FOrbit::FOrbitalDetails::GetOrbitalObject()
{
    return _Object;
}

NPGS_INLINE float FOrbit::FOrbitalDetails::GetInitialTrueAnomaly() const
{
    return _InitialTrueAnomaly;
}

NPGS_INLINE std::vector<FOrbit*>& FOrbit::FOrbitalDetails::DirectOrbitsData()
{
    return _DirectOrbits;
}

NPGS_INLINE FOrbit& FOrbit::SetSemiMajorAxis(float SemiMajorAxis)
{
    _OrbitElements.SemiMajorAxis = SemiMajorAxis;
    return *this;
}

NPGS_INLINE FOrbit& FOrbit::SetEccentricity(float Eccentricity)
{
    _OrbitElements.Eccentricity = Eccentricity;
    return *this;
}

NPGS_INLINE FOrbit& FOrbit::SetInclination(float Inclination)
{
    _OrbitElements.Inclination = Inclination;
    return *this;
}

NPGS_INLINE FOrbit& FOrbit::SetLongitudeOfAscendingNode(float LongitudeOfAscendingNode)
{
    _OrbitElements.LongitudeOfAscendingNode = LongitudeOfAscendingNode;
    return *this;
}

NPGS_INLINE FOrbit& FOrbit::SetArgumentOfPeriapsis(float ArgumentOfPeriapsis)
{
    _OrbitElements.ArgumentOfPeriapsis = ArgumentOfPeriapsis;
    return *this;
}

NPGS_INLINE FOrbit& FOrbit::SetTrueAnomaly(float TrueAnomaly)
{
    _OrbitElements.TrueAnomaly = TrueAnomaly;
    return *this;
}

NPGS_INLINE FOrbit& FOrbit::SetNormal(const glm::vec2& Normal)
{
    _Normal = Normal;
    return *this;
}

NPGS_INLINE FOrbit& FOrbit::SetPeriod(float Period)
{
    _Period = Period;
    return *this;
}

NPGS_INLINE float FOrbit::GetSemiMajorAxis() const
{
    return _OrbitElements.SemiMajorAxis;
}

NPGS_INLINE float FOrbit::GetEccentricity() const
{
    return _OrbitElements.Eccentricity;
}

NPGS_INLINE float FOrbit::GetInclination() const
{
    return _OrbitElements.Inclination;
}

NPGS_INLINE float FOrbit::GetLongitudeOfAscendingNode() const
{
    return _OrbitElements.LongitudeOfAscendingNode;
}

NPGS_INLINE float FOrbit::GetArgumentOfPeriapsis() const
{
    return _OrbitElements.ArgumentOfPeriapsis;
}

NPGS_INLINE float FOrbit::GetTrueAnomaly() const
{
    return _OrbitElements.TrueAnomaly;
}

NPGS_INLINE const FOrbit::FOrbitalObject& FOrbit::GetParent() const
{
    return _Parent;
}

NPGS_INLINE const glm::vec2& FOrbit::GetNormal() const
{
    return _Normal;
}

NPGS_INLINE float FOrbit::GetPeriod() const
{
    return _Period;
}

NPGS_INLINE std::vector<FOrbit::FOrbitalDetails>& FOrbit::ObjectsData()
{
    return _Objects;
}

NPGS_INLINE FStellarSystem& FStellarSystem::SetBaryPosition(const glm::vec3& Position)
{
    _SystemBary.Position = Position;
    return *this;
}

NPGS_INLINE FStellarSystem& FStellarSystem::SetBaryNormal(const glm::vec2& Normal)
{
    _SystemBary.Normal = Normal;
    return *this;
}

NPGS_INLINE FStellarSystem& FStellarSystem::SetBaryDistanceRank(std::size_t DistanceRank)
{
    _SystemBary.DistanceRank = DistanceRank;
    return *this;
}

NPGS_INLINE FStellarSystem& FStellarSystem::SetBaryName(const std::string& Name)
{
    _SystemBary.Name = Name;
    return *this;
}

NPGS_INLINE const glm::vec3& FStellarSystem::GetBaryPosition() const
{
    return _SystemBary.Position;
}

NPGS_INLINE const glm::vec2& FStellarSystem::GetBaryNormal() const
{
    return _SystemBary.Normal;
}

NPGS_INLINE std::size_t FStellarSystem::GetBaryDistanceRank() const
{
    return _SystemBary.DistanceRank;
}

NPGS_INLINE const std::string& FStellarSystem::GetBaryName() const
{
    return _SystemBary.Name;
}

NPGS_INLINE FBaryCenter* FStellarSystem::GetBaryCenter()
{
    return &_SystemBary;
}

NPGS_INLINE std::vector<std::unique_ptr<Astro::AStar>>& FStellarSystem::StarsData()
{
    return _Stars;
}

NPGS_INLINE std::vector<std::unique_ptr<Astro::APlanet>>& FStellarSystem::PlanetsData()
{
    return _Planets;
}

NPGS_INLINE std::vector<std::unique_ptr<Astro::AAsteroidCluster>>& FStellarSystem::AsteroidClustersData()
{
    return _AsteroidClusters;
}

NPGS_INLINE std::vector<std::unique_ptr<FOrbit>>& FStellarSystem::OrbitsData()
{
    return _Orbits;
}

_ASTRO_END
_NPGS_END
