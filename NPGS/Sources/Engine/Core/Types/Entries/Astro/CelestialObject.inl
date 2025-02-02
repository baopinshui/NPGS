#include "CelestialObject.h"

_NPGS_BEGIN
_ASTRO_BEGIN

NPGS_INLINE FCelestialBody& FCelestialBody::SetBasicProperties(const FBasicProperties& Properties)
{
    _Properties = Properties;
    return *this;
}

NPGS_INLINE const FCelestialBody::FBasicProperties& FCelestialBody::GetBasicProperties() const
{
    return _Properties;
}

NPGS_INLINE FCelestialBody& FCelestialBody::SetNormal(const glm::vec2& Normal)
{
    _Properties.Normal = Normal;
    return *this;
}

NPGS_INLINE FCelestialBody& FCelestialBody::SetName(const std::string& Name)
{
    _Properties.Name = Name;
    return *this;
}

NPGS_INLINE FCelestialBody& FCelestialBody::SetAge(double Age)
{
    _Properties.Age = Age;
    return *this;
}

NPGS_INLINE FCelestialBody& FCelestialBody::SetRadius(float Radius)
{
    _Properties.Radius = Radius;
    return *this;
}

NPGS_INLINE FCelestialBody& FCelestialBody::SetSpin(float Spin)
{
    _Properties.Spin = Spin;
    return *this;
}

NPGS_INLINE FCelestialBody& FCelestialBody::SetOblateness(float Oblateness)
{
    _Properties.Oblateness = Oblateness;
    return *this;
}

NPGS_INLINE FCelestialBody& FCelestialBody::SetEscapeVelocity(float EscapeVelocity)
{
    _Properties.EscapeVelocity = EscapeVelocity;
    return *this;
}

NPGS_INLINE FCelestialBody& FCelestialBody::SetMagneticField(float MagneticField)
{
    _Properties.MagneticField = MagneticField;
    return *this;
}

NPGS_INLINE const glm::vec2& FCelestialBody::GetNormal() const
{
    return _Properties.Normal;
}

NPGS_INLINE const std::string& FCelestialBody::GetName() const
{
    return _Properties.Name;
}

NPGS_INLINE double FCelestialBody::GetAge() const
{
    return _Properties.Age;
}

NPGS_INLINE float FCelestialBody::GetRadius() const
{
    return _Properties.Radius;
}

NPGS_INLINE float FCelestialBody::GetSpin() const
{
    return _Properties.Spin;
}

NPGS_INLINE float FCelestialBody::GetOblateness() const
{
    return _Properties.Oblateness;
}

NPGS_INLINE float FCelestialBody::GetEscapeVelocity() const
{
    return _Properties.EscapeVelocity;
}

NPGS_INLINE float FCelestialBody::GetMagneticField() const
{
    return _Properties.MagneticField;
}

_ASTRO_END
_NPGS_END
