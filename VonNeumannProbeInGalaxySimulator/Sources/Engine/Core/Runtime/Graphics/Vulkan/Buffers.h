#pragma once

#include <memory>
#include <mutex>
#include <vector>

#include <vulkan/vulkan_handles.hpp>

#include "Engine/Core/Base/Base.h"
#include "Engine/Core/Runtime/Graphics/Vulkan/Wrappers.h"

_NPGS_BEGIN
_RUNTIME_BEGIN
_GRAPHICS_BEGIN

class FStagingBuffer
{
public:
    FStagingBuffer() = delete;
    FStagingBuffer(vk::DeviceSize Size);
    FStagingBuffer(vk::Device Device, const vk::PhysicalDeviceProperties& PhysicalDeviceProperties,
                   const vk::PhysicalDeviceMemoryProperties& PhysicalDeviceMemoryProperties, vk::DeviceSize Size);

    operator FVulkanBuffer& ();
    operator const FVulkanBuffer& () const;
    operator FVulkanDeviceMemory& ();
    operator const FVulkanDeviceMemory& () const;

    void* MapMemory(vk::DeviceSize Size);
    void UnmapMemory();
    void SubmitBufferData(vk::DeviceSize Size, const void* Data);
    void FetchBufferData(vk::DeviceSize Size, void* Target) const;
    void Release();

    FVulkanImage* CreateAliasedImage(vk::Format Format, vk::Extent2D Extent);

    FVulkanBuffer& GetBuffer();
    const FVulkanBuffer& GetBuffer() const;
    FVulkanImage& GetImage();
    const FVulkanImage& GetImage() const;
    FVulkanDeviceMemory& GetMemory();
    const FVulkanDeviceMemory& GetMemory() const;

private:
    void Expand(vk::DeviceSize Size);

private:
    vk::Device                                _Device;
    const vk::PhysicalDeviceProperties*       _PhysicalDeviceProperties;
    const vk::PhysicalDeviceMemoryProperties* _PhysicalDeviceMemoryProperties;
    std::unique_ptr<FVulkanBufferMemory>      _BufferMemory;
    std::unique_ptr<FVulkanImage>             _AliasedImage;
    vk::DeviceSize                            _MemoryUsage;
};

class FStagingBufferPool
{
public:
    FStagingBuffer* AcquireBuffer(vk::DeviceSize Size);
    void ReleaseBuffer(FStagingBuffer* Buffer);

    static FStagingBufferPool* GetInstance();

private:
    FStagingBufferPool()                      = default;
    FStagingBufferPool(const FStagingBuffer&) = delete;
    FStagingBufferPool(FStagingBufferPool&&)  = delete;
    ~FStagingBufferPool()                     = default;

    FStagingBufferPool& operator=(const FStagingBuffer&) = delete;
    FStagingBufferPool& operator=(FStagingBufferPool&&)  = delete;

private:
    std::vector<std::unique_ptr<FStagingBuffer>> _BusyBuffers;
    std::vector<std::unique_ptr<FStagingBuffer>> _FreeBuffers;
    std::mutex                                   _Mutex;
};

class FDeviceLocalBuffer
{
public:
    FDeviceLocalBuffer() = delete;
    FDeviceLocalBuffer(vk::DeviceSize Size, vk::BufferUsageFlags Usage);

    operator FVulkanBuffer& ();
    operator const FVulkanBuffer& () const;

    void CopyData(vk::DeviceSize Offset, vk::DeviceSize Size, const void* Data) const;

    void CopyData(vk::DeviceSize ElementCount, vk::DeviceSize ElementSize, vk::DeviceSize SrcStride,
                  vk::DeviceSize DstStride, vk::DeviceSize Offset, const void* Data) const;

    template <typename ContainerType>
    void CopyData(const ContainerType& Data) const;

    void UpdateData(const FVulkanCommandBuffer& CommandBuffer, vk::DeviceSize Offset, vk::DeviceSize Size, const void* Data) const;

    template <typename ContainerType>
    void UpdateData(const FVulkanCommandBuffer& CommandBuffer, const ContainerType& Data) const;

    void EnablePersistentMapping() const;
    void DisablePersistentMapping() const;

    FVulkanBuffer& GetBuffer();
    const FVulkanBuffer& GetBuffer() const;

private:
    vk::Result CreateBuffer(vk::DeviceSize Size, vk::BufferUsageFlags Usage);
    vk::Result RecreateBuffer(vk::DeviceSize Size, vk::BufferUsageFlags Usage);

private:
    std::unique_ptr<FVulkanBufferMemory> _BufferMemory;
    FStagingBufferPool*                  _StagingBufferPool;
};

_GRAPHICS_END
_RUNTIME_END
_NPGS_END

#include "Buffers.inl"
