#include "Buffers.h"

#include "Engine/Core/Base/Assert.h"

_NPGS_BEGIN
_RUNTIME_BEGIN
_GRAPHICS_BEGIN

NPGS_INLINE FStagingBuffer::operator FVulkanBuffer& ()
{
    return _BufferMemory->GetResource();
}

NPGS_INLINE FStagingBuffer::operator const FVulkanBuffer& () const
{
    return _BufferMemory->GetResource();
}

NPGS_INLINE FStagingBuffer::operator FVulkanDeviceMemory& ()
{
    return _BufferMemory->GetMemory();
}

NPGS_INLINE FStagingBuffer::operator const FVulkanDeviceMemory& () const
{
    return _BufferMemory->GetMemory();
}

NPGS_INLINE void* FStagingBuffer::MapMemory(vk::DeviceSize Size)
{
    Expand(Size);
    void* Target = nullptr;
    _BufferMemory->MapMemoryForSubmit(0, Size, Target);
    _MemoryUsage = Size;
    return Target;
}

NPGS_INLINE void FStagingBuffer::UnmapMemory()
{
    _BufferMemory->UnmapMemory(0, _MemoryUsage);
    _MemoryUsage = 0;
}

NPGS_INLINE void FStagingBuffer::SubmitBufferData(vk::DeviceSize Size, const void* Data)
{
    Expand(Size);
    _BufferMemory->SubmitBufferData(0, Size, Data);
}

NPGS_INLINE void FStagingBuffer::FetchBufferData(vk::DeviceSize Size, void* Target) const
{
    _BufferMemory->FetchBufferData(0, Size, Target);
}

NPGS_INLINE void FStagingBuffer::Release()
{
    _BufferMemory.reset();
}

NPGS_INLINE FVulkanBuffer& FStagingBuffer::GetBuffer()
{
    return _BufferMemory->GetResource();
}

NPGS_INLINE const FVulkanBuffer& FStagingBuffer::GetBuffer() const
{
    return _BufferMemory->GetResource();
}

NPGS_INLINE FVulkanImage& FStagingBuffer::GetImage()
{
    return *_AliasedImage;
}

NPGS_INLINE const FVulkanImage& FStagingBuffer::GetImage() const
{
    return *_AliasedImage;
}

NPGS_INLINE FVulkanDeviceMemory& FStagingBuffer::GetMemory()
{
    return _BufferMemory->GetMemory();
}

NPGS_INLINE const FVulkanDeviceMemory& FStagingBuffer::GetMemory() const
{
    return _BufferMemory->GetMemory();
}

NPGS_INLINE FDeviceLocalBuffer::operator FVulkanBuffer& ()
{
    return _BufferMemory->GetResource();
}

NPGS_INLINE FDeviceLocalBuffer::operator const FVulkanBuffer& () const
{
    return _BufferMemory->GetResource();
}

NPGS_INLINE void FDeviceLocalBuffer::UpdateData(const FVulkanCommandBuffer& CommandBuffer, vk::DeviceSize Offset,
                                                vk::DeviceSize Size, const void* Data) const
{
    CommandBuffer->updateBuffer(*_BufferMemory->GetResource(), Offset, Size, Data);
}

template <typename ContainerType>
requires std::is_class_v<ContainerType>
NPGS_INLINE void FDeviceLocalBuffer::CopyData(const ContainerType& Data) const
{
    using ValueType = typename ContainerType::value_type;
    NpgsStaticAssert(std::is_standard_layout_v<ValueType>, "Container value_type must be standard layout type");

    vk::DeviceSize DataSize = Data.size() * sizeof(ValueType);
    return CopyData(0, DataSize, static_cast<const void*>(Data.data()));
}

template <typename ContainerType>
requires std::is_class_v<ContainerType>
NPGS_INLINE void FDeviceLocalBuffer::UpdateData(const FVulkanCommandBuffer& CommandBuffer, const ContainerType& Data) const
{
    using ValueType = typename ContainerType::value_type;
    NpgsStaticAssert(std::is_standard_layout_v<ValueType>, "Container value_type must be standard layout type");

    vk::DeviceSize DataSize = Data.size() * sizeof(ValueType);
    return UpdateData(CommandBuffer, 0, DataSize, static_cast<const void*>(Data.data()));
}

NPGS_INLINE void FDeviceLocalBuffer::EnablePersistentMapping() const
{
    _BufferMemory->GetMemory().EnablePersistentMapping();
}

NPGS_INLINE void FDeviceLocalBuffer::DisablePersistentMapping() const
{
    _BufferMemory->GetMemory().DisablePersistentMapping();
}

NPGS_INLINE FVulkanBuffer& FDeviceLocalBuffer::GetBuffer()
{
    return _BufferMemory->GetResource();
}

NPGS_INLINE const FVulkanBuffer& FDeviceLocalBuffer::GetBuffer() const
{
    return _BufferMemory->GetResource();
}

_GRAPHICS_END
_RUNTIME_END
_NPGS_END
