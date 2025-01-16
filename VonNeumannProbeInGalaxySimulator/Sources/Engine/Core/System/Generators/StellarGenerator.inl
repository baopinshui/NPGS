#include "StellarGenerator.h"

_NPGS_BEGIN
_SYSTEM_BEGIN
_GENERATOR_BEGIN

NPGS_INLINE FStellarGenerator&
FStellarGenerator::SetLogMassSuggestDistribution(std::unique_ptr<Util::TDistribution<>>&& Distribution)
{
    _LogMassGenerator = std::move(Distribution);
    return *this;
}

NPGS_INLINE FStellarGenerator& FStellarGenerator::SetUniverseAge(float Age)
{
    _UniverseAge = Age;
    return *this;
}

NPGS_INLINE FStellarGenerator& FStellarGenerator::SetAgeLowerLimit(float Limit)
{
    _AgeLowerLimit = Limit;
    return *this;
}

NPGS_INLINE FStellarGenerator& FStellarGenerator::SetAgeUpperLimit(float Limit)
{
    _AgeUpperLimit = Limit;
    return *this;
}

NPGS_INLINE FStellarGenerator& FStellarGenerator::SetFeHLowerLimit(float Limit)
{
    _FeHLowerLimit = Limit;
    return *this;
}

NPGS_INLINE FStellarGenerator& FStellarGenerator::SetFeHUpperLimit(float Limit)
{
    _FeHUpperLimit = Limit;
    return *this;
}

NPGS_INLINE FStellarGenerator& FStellarGenerator::SetMassLowerLimit(float Limit)
{
    _MassLowerLimit = Limit;
    return *this;
}

NPGS_INLINE FStellarGenerator& FStellarGenerator::SetMassUpperLimit(float Limit)
{
    _MassUpperLimit = Limit;
    return *this;
}

NPGS_INLINE FStellarGenerator& FStellarGenerator::SetCoilTempLimit(float Limit)
{
    _CoilTemperatureLimit = Limit;
    return *this;
}

NPGS_INLINE FStellarGenerator& FStellarGenerator::SetdEpdM(float dEpdM)
{
    _dEpdM = dEpdM;
    return *this;
}

NPGS_INLINE FStellarGenerator& FStellarGenerator::SetAgePdf(const std::function<float(const glm::vec3&, float, float)>& AgePdf)
{
    _AgePdf = AgePdf;
    return *this;
}

NPGS_INLINE FStellarGenerator& FStellarGenerator::SetAgeMaxPdf(const glm::vec2& MaxPdf)
{
    _AgeMaxPdf = MaxPdf;
    return *this;
}

NPGS_INLINE FStellarGenerator& FStellarGenerator::SetMassPdfs(const std::array<std::function<float(float)>, 2>& MassPdfs)
{
    _MassPdfs = MassPdfs;
    return *this;
}

NPGS_INLINE FStellarGenerator& FStellarGenerator::SetMassMaxPdfs(const std::array<glm::vec2, 2>& MaxPdfs)
{
    _MassMaxPdfs = MaxPdfs;
    return *this;
}

NPGS_INLINE FStellarGenerator& FStellarGenerator::SetAgeDistribution(EGenerationDistribution Distribution)
{
    _AgeDistribution = Distribution;
    return *this;
}

NPGS_INLINE FStellarGenerator& FStellarGenerator::SetFeHDistribution(EGenerationDistribution Distribution)
{
    _FeHDistribution = Distribution;
    return *this;
}

NPGS_INLINE FStellarGenerator& FStellarGenerator::SetMassDistribution(EGenerationDistribution Distribution)
{
    _MassDistribution = Distribution;
    return *this;
}

NPGS_INLINE FStellarGenerator& FStellarGenerator::SetStellarTypeGenerationOption(EStellarTypeGenerationOption Option)
{
    _StellarTypeOption = Option;
    return *this;
}

_GENERATOR_END
_SYSTEM_END
_NPGS_END
