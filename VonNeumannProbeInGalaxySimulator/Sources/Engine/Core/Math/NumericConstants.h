#pragma once

#include <cstdint>
#include <numbers>

#include "Engine/Core/Base/Base.h"

_NPGS_BEGIN
_MATH_BEGIN

// Math constants
// --------------
inline constexpr float kPi                     = std::numbers::pi_v<float>;
inline constexpr float kEuler                  = std::numbers::e_v<float>;

_MATH_END

// Astronomy constants
// -------------------
inline constexpr float kSolarMass              = 1.9884e30f;
inline constexpr float kSolarLuminosity        = 3.828e26f;
inline constexpr int   kSolarRadius            = 695700000;
inline constexpr int   kSolarFeH               = 0;
inline constexpr int   kSolarTeff              = 5772;
inline constexpr int   kSolarCoreTemp          = 15700000;
inline constexpr float kSolarAbsoluteMagnitude = 4.83f;

inline constexpr int   kSolarConstantOfEarth   = 1361;

inline constexpr float kJupiterMass            = 1.8982e27f;
inline constexpr int   kJupiterRadius          = 69911000;

inline constexpr float kEarthMass              = 5.972168e24f;
inline constexpr int   kEarthRadius            = 6371000;

inline constexpr float kMoonMass               = 7.346e22f;
inline constexpr int   kMoonRadius             = 1737400;

inline constexpr int   kSpeedOfLight           = 299792458;
inline constexpr float kGravityConstant        = 6.6743e-11f;
inline constexpr float kStefanBoltzmann        = 5.6703744e-8f;

// Convert constants
// -----------------
inline constexpr float kSolarMassToEarth       = static_cast<float>(kSolarMass   / kEarthMass);
inline constexpr float kSolarRadiusToEarth     = static_cast<float>(kSolarRadius / kEarthRadius);
inline constexpr float kEarthMassToSolar       = static_cast<float>(kEarthMass   / kSolarMass);
inline constexpr float kEarthRadiusToSolar     = static_cast<float>(kEarthRadius / kSolarRadius);

inline constexpr int   kYearToSecond           = 31536000;
inline constexpr int   kDayToSecond            = 86400;
inline constexpr int   kPascalToAtm            = 101325;
inline constexpr std::uint64_t kAuToMeter      = 149597870700;

_NPGS_END
