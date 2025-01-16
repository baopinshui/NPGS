#include "Civilization.h"

_NPGS_BEGIN
_INTELLI_BEGIN

FStandard::FStandard(const FLifeProperties& LifeProperties, const FCivilizationProperties& CivilizationProperties)
    : _LifeProperties(LifeProperties), _CivilizationProperties(CivilizationProperties)
{
}

const float FStandard::_kNull                       = 0.0f;
const float FStandard::_kInitialGeneralIntelligence = 1.0f;
const float FStandard::_kUrgesellschaft             = 2.0f;
const float FStandard::_kEarlyIndustrielle          = 3.0f;
const float FStandard::_kSteamAge                   = 4.0f;
const float FStandard::_kElectricAge                = 5.0f;
const float FStandard::_kAtomicAge                  = 6.0f;
const float FStandard::_kDigitalAge                 = 7.0f;
const float FStandard::_kEarlyAsiAge                = 8.0f;

_INTELLI_END
_NPGS_END
