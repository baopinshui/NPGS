#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <type_traits>
#include <vector>

#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan_handles.hpp>

#include "Engine/Core/Base/Base.h"
#include "Engine/Core/Runtime/Graphics/Vulkan/Wrappers.h"

_NPGS_BEGIN
_RUNTIME_BEGIN
_GRAPHICS_BEGIN

class FAttachment
{
public:
    FAttachment(VmaAllocator Allocator);
    virtual ~FAttachment() = default;

    vk::DescriptorImageInfo CreateDescriptorImageInfo(const FVulkanSampler& Sampler) const;
    vk::DescriptorImageInfo CreateDescriptorImageInfo(const vk::Sampler& Sampler) const;

    Graphics::FVulkanImage& GetImage();
    const Graphics::FVulkanImage& GetImage() const;
    Graphics::FVulkanImageView& GetImageView();
    const Graphics::FVulkanImageView& GetImageView() const;

protected:
    std::unique_ptr<FVulkanImageMemory> _ImageMemory;
    std::unique_ptr<FVulkanImageView>   _ImageView;
    VmaAllocator                        _Allocator;
};

class FColorAttachment : public FAttachment
{
public:
    using Base = FAttachment;
    using Base::Base;

    FColorAttachment() = delete;
    FColorAttachment(const VmaAllocationCreateInfo& AllocationCreateInfo, vk::Format Format, vk::Extent2D Extent,
                     std::uint32_t LayerCount = 1, vk::SampleCountFlagBits SampleCount = vk::SampleCountFlagBits::e1,
                     vk::ImageUsageFlags ExtraUsage = static_cast<vk::ImageUsageFlagBits>(0));

    FColorAttachment(VmaAllocator Allocator, const VmaAllocationCreateInfo& AllocationCreateInfo, vk::Format Format,
                     vk::Extent2D Extent, std::uint32_t LayerCount = 1, vk::SampleCountFlagBits SampleCount = vk::SampleCountFlagBits::e1,
                     vk::ImageUsageFlags ExtraUsage = static_cast<vk::ImageUsageFlagBits>(0));

    FColorAttachment(vk::Format Format, vk::Extent2D Extent, std::uint32_t LayerCount = 1,
                     vk::SampleCountFlagBits SampleCount = vk::SampleCountFlagBits::e1,
                     vk::ImageUsageFlags ExtraUsage = static_cast<vk::ImageUsageFlagBits>(0));

    static bool CheckFormatAvailability(vk::Format Format, bool bSupportBlend = true);

private:
    vk::Result CreateAttachment(const VmaAllocationCreateInfo* AllocationCreateInfo, vk::Format Format, vk::Extent2D Extent,
                                std::uint32_t LayerCount, vk::SampleCountFlagBits SampleCount, vk::ImageUsageFlags ExtraUsage);
};

class FDepthStencilAttachment : public FAttachment
{
public:
    using Base = FAttachment;
    using Base::Base;

    FDepthStencilAttachment() = delete;
    FDepthStencilAttachment(const VmaAllocationCreateInfo& AllocationCreateInfo, vk::Format Format, vk::Extent2D Extent,
                            std::uint32_t LayerCount = 1, vk::SampleCountFlagBits SampleCount = vk::SampleCountFlagBits::e1,
                            vk::ImageUsageFlags ExtraUsage = static_cast<vk::ImageUsageFlagBits>(0), bool bStencilOnly = false);

    FDepthStencilAttachment(VmaAllocator Allocator, const VmaAllocationCreateInfo& AllocationCreateInfo, vk::Format Format,
                            vk::Extent2D Extent, std::uint32_t LayerCount = 1, vk::SampleCountFlagBits SampleCount = vk::SampleCountFlagBits::e1,
                            vk::ImageUsageFlags ExtraUsage = static_cast<vk::ImageUsageFlagBits>(0), bool bStencilOnly = false);

    FDepthStencilAttachment(vk::Format Format, vk::Extent2D Extent, std::uint32_t LayerCount = 1,
                            vk::SampleCountFlagBits SampleCount = vk::SampleCountFlagBits::e1,
                            vk::ImageUsageFlags ExtraUsage = static_cast<vk::ImageUsageFlagBits>(0),
                            bool bStencilOnly = false);

    static bool CheckFormatAvailability(vk::Format Format);

private:
    vk::Result CreateAttachment(const VmaAllocationCreateInfo* AllocationCreateInfo, vk::Format Format, vk::Extent2D Extent,
                                std::uint32_t LayerCount, vk::SampleCountFlagBits SampleCount, vk::ImageUsageFlags ExtraUsage, bool bStencilOnly);
};

class FStagingBuffer
{
public:
    FStagingBuffer() = delete;
    FStagingBuffer(vk::DeviceSize Size);
    FStagingBuffer(vk::Device Device, const vk::PhysicalDeviceProperties& PhysicalDeviceProperties,
                   const vk::PhysicalDeviceMemoryProperties& PhysicalDeviceMemoryProperties, vk::DeviceSize Size);

    FStagingBuffer(const VmaAllocationCreateInfo& AllocationCreateInfo, const vk::BufferCreateInfo& BufferCreateInfo);
    FStagingBuffer(VmaAllocator Allocator, const VmaAllocationCreateInfo& AllocationCreateInfo,
                   const vk::BufferCreateInfo& BufferCreateInfo);

    FStagingBuffer(const FStagingBuffer&) = delete;
    FStagingBuffer(FStagingBuffer&& Other) noexcept;
    ~FStagingBuffer() = default;

    FStagingBuffer& operator=(const FStagingBuffer&) = delete;
    FStagingBuffer& operator=(FStagingBuffer&& Other) noexcept;

    operator FVulkanBuffer& ();
    operator const FVulkanBuffer& () const;
    operator FVulkanDeviceMemory& ();
    operator const FVulkanDeviceMemory& () const;

    void* MapMemory(vk::DeviceSize Size);
    void  UnmapMemory();
    void  SubmitBufferData(vk::DeviceSize MapOffset, vk::DeviceSize SubmitOffset, vk::DeviceSize Size, const void* Data);
    void  FetchBufferData(vk::DeviceSize MapOffset, vk::DeviceSize FetchOffset, vk::DeviceSize Size, void* Target) const;
    void  Release();
    bool  IsUsingVma() const;

    FVulkanImage* CreateAliasedImage(vk::Format OriganFormat, vk::Format NewFormat, vk::Extent2D Extent);

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
    VmaAllocator                              _Allocator;
    VmaAllocationCreateInfo                   _AllocationCreateInfo;
};

class FStagingBufferPool
{
public:
    FStagingBuffer* AcquireBuffer(vk::DeviceSize Size, const VmaAllocationCreateInfo* AllocationCreateInfo = nullptr);
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
    FDeviceLocalBuffer(const VmaAllocationCreateInfo& AllocationCreateInfo, const vk::BufferCreateInfo& BufferCreateInfo);
    FDeviceLocalBuffer(VmaAllocator Allocator, const VmaAllocationCreateInfo& AllocationCreateInfo, const vk::BufferCreateInfo& BufferCreateInfo);
    FDeviceLocalBuffer(const FDeviceLocalBuffer&) = delete;
    FDeviceLocalBuffer(FDeviceLocalBuffer&& Other) noexcept;
    ~FDeviceLocalBuffer() = default;

    FDeviceLocalBuffer& operator=(const FDeviceLocalBuffer&) = delete;
    FDeviceLocalBuffer& operator=(FDeviceLocalBuffer&& Other) noexcept;

    operator FVulkanBuffer& ();
    operator const FVulkanBuffer& () const;

    void CopyData(vk::DeviceSize MapOffset, vk::DeviceSize TargetOffset, vk::DeviceSize Size, const void* Data) const;

    void CopyData(vk::DeviceSize ElementIndex, vk::DeviceSize ElementCount, vk::DeviceSize ElementSize,
                  vk::DeviceSize SrcStride, vk::DeviceSize DstStride, vk::DeviceSize MapOffset, const void* Data) const;

    template <typename ContainerType>
    requires std::is_class_v<ContainerType>
    void CopyData(const ContainerType& Data) const;

    void UpdateData(const FVulkanCommandBuffer& CommandBuffer, vk::DeviceSize Offset, vk::DeviceSize Size, const void* Data) const;

    template <typename ContainerType>
    requires std::is_class_v<ContainerType>
    void UpdateData(const FVulkanCommandBuffer& CommandBuffer, const ContainerType& Data) const;

    void EnablePersistentMapping() const;
    void DisablePersistentMapping() const;

    FVulkanBuffer& GetBuffer();
    const FVulkanBuffer& GetBuffer() const;

    bool IsUsingVma() const;

private:
    vk::Result CreateBuffer(vk::DeviceSize Size, vk::BufferUsageFlags Usage);
    vk::Result CreateBuffer(const VmaAllocationCreateInfo& AllocationCreateInfo,
                            const vk::BufferCreateInfo& BufferCreateInfo);

    vk::Result RecreateBuffer(vk::DeviceSize Size, vk::BufferUsageFlags Usage);
    vk::Result RecreateBuffer(const VmaAllocationCreateInfo& AllocationCreateInfo,
                              const vk::BufferCreateInfo& BufferCreateInfo);

private:
    std::unique_ptr<FVulkanBufferMemory> _BufferMemory;
    FStagingBufferPool*                  _StagingBufferPool;
    VmaAllocator                         _Allocator;
};

_GRAPHICS_END
_RUNTIME_END
_NPGS_END

#include "Resources.inl"
