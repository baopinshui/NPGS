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

NPGS_INLINE float FCamera::GetCameraZoom() const
{
    return _Zoom;
}

NPGS_INLINE glm::mat4x4 FCamera::GetViewMatrix() const
{
    static const glm::mat4x4 kVulkanCorrection = glm::mat4x4(
        1.0f,  0.0f, 0.0f, 0.0f,
        0.0f, -1.0f, 0.0f, 0.0f,
        0.0f,  0.0f, 1.0f, 0.0f,
        0.0f,  0.0f, 0.0f, 1.0f
    );

    return kVulkanCorrection * glm::mat4_cast(_Orientation) * glm::translate(glm::mat4(1.0f), -_Position);
}

NPGS_INLINE glm::mat4x4 FCamera::GetProjectionMatrix(float WindowAspect, float Near, float Far) const
{
    glm::mat4x4 Matrix = glm::perspective(glm::radians(_Zoom), WindowAspect, Near, Far);
    Matrix[1][1] *= -1.0f;
    return Matrix;
}

NPGS_INLINE void FCamera::SetOrientation(const glm::quat& Orientation)
{
    _Orientation = Orientation;
}

NPGS_INLINE const glm::quat& FCamera::GetOrientation() const
{
    return _Orientation;
}

_SPATIAL_END
_SYSTEM_END
_NPGS_END
