#include "Camera.h"

#include <glm/gtc/matrix_transform.hpp>

_NPGS_BEGIN
_SYSTEM_BEGIN
_SPATIAL_BEGIN

NPGS_INLINE void FCamera::ProcessMouseScroll(double OffsetY)
{
    _Speed += static_cast<float>(OffsetY * 0.1);

    if (_Speed <= 0)
    {
        _Speed = 0;
    }
}

NPGS_INLINE void FCamera::SetOrientation(const glm::quat& Orientation)
{
    _Orientation = Orientation;
}

NPGS_INLINE const glm::quat& FCamera::GetOrientation() const
{
    return _Orientation;
}

NPGS_INLINE glm::mat4x4 FCamera::GetViewMatrix() const
{
    return glm::lookAt(_Position, _Position + _Front, _Up);
}

NPGS_INLINE glm::mat4x4 FCamera::GetProjectionMatrix(float WindowAspect, float Near) const
{
    glm::mat4x4 Matrix = glm::infinitePerspective(glm::radians(_Zoom), WindowAspect, Near);
    return Matrix;
}

NPGS_INLINE float FCamera::GetCameraZoom() const
{
    return _Zoom;
}

_SPATIAL_END
_SYSTEM_END
_NPGS_END
