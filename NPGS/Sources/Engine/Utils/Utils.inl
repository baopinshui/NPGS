#include "Utils.h"

#include <cmath>
#include <cstring>
#include <limits>

_NPGS_BEGIN

#ifdef NPGS_ENABLE_ENUM_BIT_OPERATOR
template <typename EnumType>
inline constexpr EnumType operator&(EnumType Lhs, EnumType Rhs)
{
    return static_cast<EnumType>(static_cast<std::underlying_type_t<EnumType>>(Lhs) &
                                 static_cast<std::underlying_type_t<EnumType>>(Rhs));
}

template <typename EnumType>
inline constexpr EnumType operator|(EnumType Lhs, EnumType Rhs)
{
    return static_cast<EnumType>(static_cast<std::underlying_type_t<EnumType>>(Lhs) |
                                 static_cast<std::underlying_type_t<EnumType>>(Rhs));
}

template <typename EnumType>
inline constexpr EnumType operator^(EnumType Lhs, EnumType Rhs)
{
    return static_cast<EnumType>(static_cast<std::underlying_type_t<EnumType>>(Lhs) ^
                                 static_cast<std::underlying_type_t<EnumType>>(Rhs));
}

template <typename EnumType>
inline constexpr EnumType operator~(EnumType Value)
{
    return static_cast<EnumType>(~static_cast<std::underlying_type_t<EnumType>>(Value));
}

template <typename EnumType>
inline constexpr EnumType operator&=(EnumType Lhs, EnumType Rhs)
{
    return static_cast<EnumType>(static_cast<std::underlying_type_t<EnumType>>(Lhs) &=
                                 static_cast<std::underlying_type_t<EnumType>>(Rhs));
}

template <typename EnumType>
inline constexpr EnumType operator|=(EnumType Lhs, EnumType Rhs)
{
    return static_cast<EnumType>(static_cast<std::underlying_type_t<EnumType>>(Lhs) |=
                                 static_cast<std::underlying_type_t<EnumType>>(Rhs));
}

template <typename EnumType>
inline constexpr EnumType operator^=(EnumType Lhs, EnumType Rhs)
{
    return static_cast<EnumType>(static_cast<std::underlying_type_t<EnumType>>(Lhs) ^=
                                 static_cast<std::underlying_type_t<EnumType>>(Rhs));
}
#endif // NPGS_ENABLE_ENUM_BIT_OPERATOR

_UTIL_BEGIN

NPGS_INLINE bool Equal(const char* Lhs, const char* Rhs)
{
    return std::strcmp(Lhs, Rhs) == 0;
}

NPGS_INLINE bool Equal(float Lhs, float Rhs)
{
    return std::abs(Lhs - Rhs) <= std::numeric_limits<float>::epsilon();
}

NPGS_INLINE bool Equal(double Lhs, double Rhs)
{
    return std::abs(Lhs - Rhs) <= std::numeric_limits<double>::epsilon();
}

_UTIL_END
_NPGS_END
