#pragma once

#include "Engine/Core/Base/Base.h"

_NPGS_BEGIN

#ifdef NPGS_ENABLE_ENUM_BIT_OPERATOR
template <typename EnumType>
constexpr EnumType operator&(EnumType Lhs, EnumType Rhs);

template <typename EnumType>
constexpr EnumType operator|(EnumType Lhs, EnumType Rhs);

template <typename EnumType>
constexpr EnumType operator^(EnumType Lhs, EnumType Rhs);

template <typename EnumType>
constexpr EnumType operator~(EnumType Value);

template <typename EnumType>
constexpr EnumType operator&=(EnumType Lhs, EnumType Rhs);

template <typename EnumType>
constexpr EnumType operator|=(EnumType Lhs, EnumType Rhs);

template <typename EnumType>
constexpr EnumType operator^=(EnumType Lhs, EnumType Rhs);
#endif // NPGS_ENABLE_ENUM_BIT_OPERATOR

_UTIL_BEGIN

bool Equal(const char* Lhs, const char* Rhs);
bool Equal(float Lhs, float Rhs);
bool Equal(double Lhs, double Rhs);

_UTIL_END
_NPGS_END

#include "Utils.inl"
