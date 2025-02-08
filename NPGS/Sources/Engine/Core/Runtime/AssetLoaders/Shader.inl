#include "Shader.h"

_NPGS_BEGIN
_RUNTIME_BEGIN
_ASSET_BEGIN

template <typename DescriptorInfoType>
requires std::is_class_v<DescriptorInfoType>
inline void FShader::WriteSharedDescriptors(std::uint32_t Set, std::uint32_t Binding, vk::DescriptorType Type,
                                            const std::vector<DescriptorInfoType>& DescriptorInfos)
{
    auto SetIt = _DescriptorSetsMap.find(Set);
    if (SetIt == _DescriptorSetsMap.end())
    {
        return;
    }

    for (const auto& DescriptorSet : SetIt->second)
    {
        DescriptorSet.Write(DescriptorInfos, Type, Binding);
    }

    _bDescriptorSetsNeedUpdate = true;
}

template <typename DescriptorInfoType>
requires std::is_class_v<DescriptorInfoType>
inline void FShader::WriteDynamicDescriptors(std::uint32_t Set, std::uint32_t Binding, std::uint32_t FrameIndex,
                                             vk::DescriptorType Type, const std::vector<DescriptorInfoType>& DescriptorInfos)
{
    auto SetIt = _DescriptorSetsMap.find(Set);
    if (SetIt == _DescriptorSetsMap.end())
    {
        return;
    }

    const auto& DescriptorSet = SetIt->second[FrameIndex];
    DescriptorSet.Write(DescriptorInfos, Type, Binding);

    _bDescriptorSetsNeedUpdate = true;
}

NPGS_INLINE std::vector<vk::PushConstantRange> FShader::GetPushConstantRanges() const
{
    return _ReflectionInfo.PushConstants;
}

NPGS_INLINE std::uint32_t FShader::GetPushConstantOffset(const std::string& Name) const
{
    return _PushConstantOffsetsMap.at(Name);
}

NPGS_INLINE const std::vector<vk::VertexInputBindingDescription>& FShader::GetVertexInputBindings() const
{
    return _ReflectionInfo.VertexInputBindings;
}

NPGS_INLINE const std::vector<vk::VertexInputAttributeDescription>& FShader::GetVertexInputAttributes() const
{
    return _ReflectionInfo.VertexInputAttributes;
}

NPGS_INLINE const std::vector<vk::DescriptorSet>& FShader::GetDescriptorSets()
{
    UpdateDescriptorSets();
    return _DescriptorSets;
}

_ASSET_END
_RUNTIME_END
_NPGS_END
