#include "Wrappers.h"

#include "Engine/Core/Base/Assert.h"
#include "Engine/Utils/Utils.h"

_NPGS_BEGIN
_RUNTIME_BEGIN
_GRAPHICS_BEGIN

NPGS_INLINE FGraphicsPipelineCreateInfoPack::operator vk::GraphicsPipelineCreateInfo& ()
{
    return GraphicsPipelineCreateInfo;
}

NPGS_INLINE FGraphicsPipelineCreateInfoPack::operator const vk::GraphicsPipelineCreateInfo& () const
{
    return GraphicsPipelineCreateInfo;
}

// Wrapper for vk::DeviceMemory
// ----------------------------
template <typename ContainerType>
requires std::is_class_v<ContainerType>
NPGS_INLINE vk::Result FVulkanDeviceMemory::SubmitData(const ContainerType& Data)
{
    using ValueType = typename ContainerType::value_type;
    NpgsStaticAssert(std::is_standard_layout_v<ValueType>, "Container value_type must be standard layout type");

    vk::DeviceSize DataSize = Data.size() * sizeof(ValueType);
    return SubmitData(0, 0, DataSize, static_cast<const void*>(Data.data()));
}

template <typename ContainerType>
requires std::is_class_v<ContainerType>
NPGS_INLINE vk::Result FVulkanDeviceMemory::FetchData(ContainerType& Data)
{
    using ValueType = typename ContainerType::value_type;
    NpgsStaticAssert(std::is_standard_layout_v<ValueType>, "Container value_type must be standard layout type");

    Data.reserve(_AllocationSize / sizeof(ValueType));
    Data.resize(_AllocationSize / sizeof(ValueType));
    return FetchData(0, 0, _AllocationSize, static_cast<void*>(Data.data()));
}

NPGS_INLINE const void* FVulkanDeviceMemory::GetMappedDataMemory() const
{
    return _MappedDataMemory;
}

NPGS_INLINE void* FVulkanDeviceMemory::GetMappedTargetMemory()
{
    return _MappedTargetMemory;
}

NPGS_INLINE vk::DeviceSize FVulkanDeviceMemory::GetAllocationSize() const
{
    return _AllocationSize;
}

NPGS_INLINE vk::MemoryPropertyFlags FVulkanDeviceMemory::GetMemoryPropertyFlags() const
{
    return _MemoryPropertyFlags;
}

NPGS_INLINE bool FVulkanDeviceMemory::IsPereistentlyMapped() const
{
    return _bPersistentlyMapped;
}

// Wrapper for vk::DescriptorSet
// -----------------------------
NPGS_INLINE void FVulkanDescriptorSet::Write(const std::vector<vk::DescriptorImageInfo>& ImageInfos, vk::DescriptorType Type,
                                             std::uint32_t BindingPoint, std::uint32_t ArrayElement) const
{
    vk::WriteDescriptorSet WriteDescriptorSet(_Handle, BindingPoint, ArrayElement, Type, ImageInfos);
    Update({ WriteDescriptorSet });
}

NPGS_INLINE void FVulkanDescriptorSet::Write(const std::vector<vk::DescriptorBufferInfo>& BufferInfos, vk::DescriptorType Type,
                                             std::uint32_t BindingPoint, std::uint32_t ArrayElement) const
{
    vk::WriteDescriptorSet WriteDescriptorSet(_Handle, BindingPoint, ArrayElement, Type, {}, BufferInfos);
    Update({ WriteDescriptorSet });
}

NPGS_INLINE void FVulkanDescriptorSet::Write(const std::vector<vk::BufferView>& BufferViews, vk::DescriptorType Type,
                                             std::uint32_t BindingPoint, std::uint32_t ArrayElement) const
{
    vk::WriteDescriptorSet WriteDescriptorSet(_Handle, BindingPoint, ArrayElement, Type, {}, {}, BufferViews);
    Update({ WriteDescriptorSet });
}

NPGS_INLINE void FVulkanDescriptorSet::Write(const std::vector<FVulkanBufferView>& BufferViews, vk::DescriptorType Type,
                                             std::uint32_t BindingPoint, std::uint32_t ArrayElement) const
{
    std::vector<vk::BufferView> BufferViewHandles;
    BufferViewHandles.reserve(BufferViews.size());
    for (const auto& BufferView : BufferViews)
    {
        BufferViewHandles.push_back(*BufferView);
    }
    Write(BufferViewHandles, Type, BindingPoint, ArrayElement);
}

NPGS_INLINE void FVulkanDescriptorSet::Update(const std::vector<vk::WriteDescriptorSet>& Writes,
                                              const std::vector<vk::CopyDescriptorSet>& Copies)
{
    FVulkanCore::GetClassInstance()->GetDevice().updateDescriptorSets(Writes, Copies);
}

// Wrapper for vk::RenderPass
// --------------------------
NPGS_INLINE void FVulkanRenderPass::CommandBegin(const FVulkanCommandBuffer& CommandBuffer,
                                                 const vk::RenderPassBeginInfo& BeginInfo,
                                                 const vk::SubpassContents& SubpassContents) const
{
    CommandBuffer->beginRenderPass(BeginInfo, SubpassContents);
}

NPGS_INLINE void FVulkanRenderPass::CommandNext(const FVulkanCommandBuffer& CommandBuffer,
                                                const vk::SubpassContents& SubpassContents) const
{
    CommandBuffer->nextSubpass(SubpassContents);
}

NPGS_INLINE void FVulkanRenderPass::CommandEnd(const FVulkanCommandBuffer& CommandBuffer) const
{
    CommandBuffer->endRenderPass();
}
// -------------------
// Native wrappers end

NPGS_INLINE void FVulkanDeviceMemory::EnablePersistentMapping()
{
    _bPersistentlyMapped = true;
}

NPGS_INLINE void FVulkanDeviceMemory::DisablePersistentMapping()
{
    _bPersistentlyMapped = false;
}

NPGS_INLINE vk::Result FVulkanBufferMemory::MapMemoryForSubmit(vk::DeviceSize Offset, vk::DeviceSize Size, void*& Target) const
{
    return _Memory->MapMemoryForSubmit(Offset, Size, Target);
}

NPGS_INLINE vk::Result FVulkanBufferMemory::MapMemoryForFetch(vk::DeviceSize Offset, vk::DeviceSize Size, void*& Data) const
{
    return _Memory->MapMemoryForFetch(Offset, Size, Data);
}

NPGS_INLINE vk::Result FVulkanBufferMemory::UnmapMemory(vk::DeviceSize Offset, vk::DeviceSize Size) const
{
    return _Memory->UnmapMemory(Offset, Size);
}

NPGS_INLINE vk::Result FVulkanBufferMemory::SubmitBufferData(vk::DeviceSize MapOffset, vk::DeviceSize SubmitOffset,
                                                             vk::DeviceSize Size, const void* Data) const
{
    return _Memory->SubmitData(MapOffset, SubmitOffset, Size, Data);
}

NPGS_INLINE vk::Result FVulkanBufferMemory::FetchBufferData(vk::DeviceSize MapOffset, vk::DeviceSize FetchOffset,
                                                            vk::DeviceSize Size, void* Target) const
{
    return _Memory->FetchData(MapOffset, FetchOffset, Size, Target);
}

template <typename ContainerType>
requires std::is_class_v<ContainerType>
NPGS_INLINE vk::Result FVulkanBufferMemory::SubmitBufferData(const ContainerType& Data) const
{
    return _Memory->SubmitData(Data);
}

template <typename ContainerType>
requires std::is_class_v<ContainerType>
NPGS_INLINE vk::Result FVulkanBufferMemory::FetchBufferData(ContainerType& Data) const
{
    return _Memory->FetchData(Data);
}

_GRAPHICS_END
_RUNTIME_END
_NPGS_END
