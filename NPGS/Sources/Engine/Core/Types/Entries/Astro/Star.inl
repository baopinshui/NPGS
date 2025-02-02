#include "Star.h"

_NPGS_BEGIN
_ASTRO_BEGIN

NPGS_INLINE AStar& AStar::SetExtendedProperties(const FExtendedProperties& ExtraProperties)
{
    _ExtraProperties = ExtraProperties;
    return *this;
}

NPGS_INLINE const AStar::FExtendedProperties& AStar::GetExtendedProperties() const
{
    return _ExtraProperties;
}

NPGS_INLINE AStar& AStar::SetMass(double Mass)
{
    _ExtraProperties.Mass = Mass;
    return *this;
}

NPGS_INLINE AStar& AStar::SetLuminosity(double Luminosity)
{
    _ExtraProperties.Luminosity = Luminosity;
    return *this;
}

NPGS_INLINE AStar& AStar::SetLifetime(double Lifetime)
{
    _ExtraProperties.Lifetime = Lifetime;
    return *this;
}

NPGS_INLINE AStar& AStar::SetEvolutionProgress(double EvolutionProgress)
{
    _ExtraProperties.EvolutionProgress = EvolutionProgress;
    return *this;
}

NPGS_INLINE AStar& AStar::SetFeH(float FeH)
{
    _ExtraProperties.FeH = FeH;
    return *this;
}

NPGS_INLINE AStar& AStar::SetInitialMass(float InitialMass)
{
    _ExtraProperties.InitialMass = InitialMass;
    return *this;
}

NPGS_INLINE AStar& AStar::SetSurfaceH1(float SurfaceH1)
{
    _ExtraProperties.SurfaceH1 = SurfaceH1;
    return *this;
}

NPGS_INLINE AStar& AStar::SetSurfaceZ(float SurfaceZ)
{
    _ExtraProperties.SurfaceZ = SurfaceZ;
    return *this;
}

NPGS_INLINE AStar& AStar::SetSurfaceEnergeticNuclide(float SurfaceEnergeticNuclide)
{
    _ExtraProperties.SurfaceEnergeticNuclide = SurfaceEnergeticNuclide;
    return *this;
}

NPGS_INLINE AStar& AStar::SetSurfaceVolatiles(float SurfaceVolatiles)
{
    _ExtraProperties.SurfaceVolatiles = SurfaceVolatiles;
    return *this;
}

NPGS_INLINE AStar& AStar::SetTeff(float Teff)
{
    _ExtraProperties.Teff = Teff;
    return *this;
}

NPGS_INLINE AStar& AStar::SetCoreTemp(float CoreTemp)
{
    _ExtraProperties.CoreTemp = CoreTemp;
    return *this;
}

NPGS_INLINE AStar& AStar::SetCoreDensity(float CoreDensity)
{
    _ExtraProperties.CoreDensity = CoreDensity;
    return *this;
}

NPGS_INLINE AStar& AStar::SetStellarWindSpeed(float StellarWindSpeed)
{
    _ExtraProperties.StellarWindSpeed = StellarWindSpeed;
    return *this;
}

NPGS_INLINE AStar& AStar::SetStellarWindMassLossRate(float StellarWindMassLossRate)
{
    _ExtraProperties.StellarWindMassLossRate = StellarWindMassLossRate;
    return *this;
}

NPGS_INLINE AStar& AStar::SetMinCoilMass(float MinCoilMass)
{
    _ExtraProperties.MinCoilMass = MinCoilMass;
    return *this;
}

NPGS_INLINE AStar& AStar::SetIsSingleStar(bool bIsSingleStar)
{
    _ExtraProperties.bIsSingleStar = bIsSingleStar;
    return *this;
}

NPGS_INLINE AStar& AStar::SetHasPlanets(bool bHasPlanets)
{
    _ExtraProperties.bHasPlanets = bHasPlanets;
    return *this;
}

NPGS_INLINE AStar& AStar::SetStarFrom(EStarFrom From)
{
    _ExtraProperties.From = From;
    return *this;
}

NPGS_INLINE AStar& AStar::SetEvolutionPhase(EEvolutionPhase Phase)
{
    _ExtraProperties.Phase = Phase;
    return *this;
}

NPGS_INLINE AStar& AStar::SetStellarClass(const Astro::FStellarClass& Class)
{
    _ExtraProperties.Class = Class;
    return *this;
}

NPGS_INLINE double AStar::GetMass() const
{
    return _ExtraProperties.Mass;
}

NPGS_INLINE double AStar::GetLuminosity() const
{
    return _ExtraProperties.Luminosity;
}

NPGS_INLINE double AStar::GetLifetime() const
{
    return _ExtraProperties.Lifetime;
}

NPGS_INLINE double AStar::GetEvolutionProgress() const
{
    return _ExtraProperties.EvolutionProgress;
}

NPGS_INLINE float AStar::GetFeH() const
{
    return _ExtraProperties.FeH;
}

NPGS_INLINE float AStar::GetInitialMass() const
{
    return _ExtraProperties.InitialMass;
}

NPGS_INLINE float AStar::GetSurfaceH1() const
{
    return _ExtraProperties.SurfaceH1;
}

NPGS_INLINE float AStar::GetSurfaceZ() const
{
    return _ExtraProperties.SurfaceZ;
}

NPGS_INLINE float AStar::GetSurfaceEnergeticNuclide() const
{
    return _ExtraProperties.SurfaceEnergeticNuclide;
}

NPGS_INLINE float AStar::GetSurfaceVolatiles() const
{
    return _ExtraProperties.SurfaceVolatiles;
}

NPGS_INLINE float AStar::GetTeff() const
{
    return _ExtraProperties.Teff;
}

NPGS_INLINE float AStar::GetCoreTemp() const
{
    return _ExtraProperties.CoreTemp;
}

NPGS_INLINE float AStar::GetCoreDensity() const
{
    return _ExtraProperties.CoreDensity;
}

NPGS_INLINE float AStar::GetStellarWindSpeed() const
{
    return _ExtraProperties.StellarWindSpeed;
}

NPGS_INLINE float AStar::GetStellarWindMassLossRate() const
{
    return _ExtraProperties.StellarWindMassLossRate;
}

NPGS_INLINE float AStar::GetMinCoilMass() const
{
    return _ExtraProperties.MinCoilMass;
}

NPGS_INLINE bool AStar::GetIsSingleStar() const
{
    return _ExtraProperties.bIsSingleStar;
}

NPGS_INLINE bool AStar::GetHasPlanets() const
{
    return _ExtraProperties.bHasPlanets;
}

NPGS_INLINE AStar::EStarFrom AStar::GetStarFrom() const
{
    return _ExtraProperties.From;
}

NPGS_INLINE AStar::EEvolutionPhase AStar::GetEvolutionPhase() const
{
    return _ExtraProperties.Phase;
}

NPGS_INLINE const Astro::FStellarClass& AStar::GetStellarClass() const
{
    return _ExtraProperties.Class;
}

_ASTRO_END
_NPGS_END
