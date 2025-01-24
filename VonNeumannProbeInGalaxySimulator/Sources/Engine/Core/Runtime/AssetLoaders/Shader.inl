#include "Shader.h"

_NPGS_BEGIN
_RUNTIME_BEGIN
_ASSET_BEGIN

NPGS_INLINE std::vector<vk::PushConstantRange> FShader::GetPushConstantRanges() const
{
    return _ReflectionInfo.PushConstants;
}

NPGS_INLINE const std::vector<vk::DescriptorSet>& FShader::GetDescriptorSets()
{
    UpdateDescriptorSets();
    return _DescriptorSets;
}

_ASSET_END
_RUNTIME_END
_NPGS_END
