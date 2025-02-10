#include "Resources.h"

#include <cstddef>
#include <algorithm>
#include <utility>
#include <vector>

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_to_string.hpp>

#include "Engine/Core/Runtime/Graphics/Vulkan/Context.h"
#include "Engine/Core/Runtime/Graphics/Vulkan/Core.h"
#include "Engine/Utils/Logger.h"

_NPGS_BEGIN
_RUNTIME_BEGIN
_GRAPHICS_BEGIN

FColorAttachment::FColorAttachment(vk::Format Format, vk::Extent2D Extent, std::uint32_t LayerCount,
                                   vk::SampleCountFlagBits SampleCount, vk::ImageUsageFlagBits ExtraUsage)
{
    vk::Result Result = CreateAttachment(Format, Extent, LayerCount, SampleCount, ExtraUsage);
    if (Result != vk::Result::eSuccess)
    {
        NpgsCoreError("Failed to create color attachment: {}", vk::to_string(Result));
    }
}

bool FColorAttachment::CheckFormatAvailability(vk::Format Format, bool bSupportBlend)
{
    vk::FormatProperties FormatProperties = FVulkanCore::GetClassInstance()->GetPhysicalDevice().getFormatProperties(Format);
    if (bSupportBlend)
    {
        if (FormatProperties.optimalTilingFeatures & (vk::FormatFeatureFlagBits::eColorAttachment | vk::FormatFeatureFlagBits::eColorAttachmentBlend))
        {
            return true;
        }
    }
    else
    {
        if (FormatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eColorAttachment)
        {
            return true;
        }
    }

    return false;
}

vk::Result FColorAttachment::CreateAttachment(vk::Format Format, vk::Extent2D Extent, std::uint32_t LayerCount,
                                              vk::SampleCountFlagBits SampleCount, vk::ImageUsageFlagBits ExtraUsage)
{
    vk::ImageCreateInfo ImageCreateInfo;
    ImageCreateInfo.setImageType(vk::ImageType::e2D)
                   .setFormat(Format)
                   .setExtent({ Extent.width, Extent.height, 1 })
                   .setMipLevels(1)
                   .setArrayLayers(LayerCount)
                   .setSamples(SampleCount)
                   .setUsage(vk::ImageUsageFlagBits::eColorAttachment | ExtraUsage);

    vk::MemoryPropertyFlags MemoryPropertyFlags = vk::MemoryPropertyFlagBits::eDeviceLocal;
    if (ExtraUsage & vk::ImageUsageFlagBits::eTransientAttachment)
    {
        MemoryPropertyFlags |= vk::MemoryPropertyFlagBits::eLazilyAllocated;
    }

    _ImageMemory = std::make_unique<FVulkanImageMemory>(ImageCreateInfo, MemoryPropertyFlags);
    if (!_ImageMemory->IsValid())
    {
        return vk::Result::eErrorInitializationFailed;
    }

    vk::ImageSubresourceRange SubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, LayerCount);

    _ImageView = std::make_unique<FVulkanImageView>(_ImageMemory->GetResource(),
                                                    LayerCount > 1 ? vk::ImageViewType::e2DArray : vk::ImageViewType::e2D,
                                                    Format,
                                                    vk::ComponentMapping(),
                                                    SubresourceRange);
    if (!_ImageView->IsValid())
    {
        return vk::Result::eErrorInitializationFailed;
    }

    return vk::Result::eSuccess;
}

FDepthStencilAttachment::FDepthStencilAttachment(vk::Format Format, vk::Extent2D Extent, std::uint32_t LayerCount, vk::SampleCountFlagBits SampleCount, vk::ImageUsageFlagBits ExtraUsage, bool bStencilOnly)
{
    vk::Result Result = CreateAttachment(Format, Extent, LayerCount, SampleCount, ExtraUsage, bStencilOnly);
    if (Result != vk::Result::eSuccess)
    {
        NpgsCoreError("Failed to create depth-stencil attachment: {}", vk::to_string(Result));
    }
}

bool FDepthStencilAttachment::CheckFormatAvailability(vk::Format Format)
{
    vk::FormatProperties FormatProperties = FVulkanCore::GetClassInstance()->GetPhysicalDevice().getFormatProperties(Format);
    if (FormatProperties.bufferFeatures & vk::FormatFeatureFlagBits::eDepthStencilAttachment)
    {
        return true;
    }

    return false;
}

vk::Result FDepthStencilAttachment::CreateAttachment(vk::Format Format, vk::Extent2D Extent, std::uint32_t LayerCount, vk::SampleCountFlagBits SampleCount, vk::ImageUsageFlagBits ExtraUsage, bool bStencilOnly)
{
    vk::ImageCreateInfo ImageCreateInfo;
    ImageCreateInfo.setImageType(vk::ImageType::e2D)
        .setFormat(Format)
        .setExtent({ Extent.width, Extent.height, 1 })
        .setMipLevels(1)
        .setArrayLayers(LayerCount)
        .setSamples(SampleCount)
        .setUsage(vk::ImageUsageFlagBits::eDepthStencilAttachment | ExtraUsage);

    vk::MemoryPropertyFlags MemoryPropertyFlags = vk::MemoryPropertyFlagBits::eDeviceLocal;
    if (ExtraUsage & vk::ImageUsageFlagBits::eTransientAttachment)
    {
        MemoryPropertyFlags |= vk::MemoryPropertyFlagBits::eLazilyAllocated;
    }

    _ImageMemory = std::make_unique<FVulkanImageMemory>(ImageCreateInfo, MemoryPropertyFlags);
    if (!_ImageMemory->IsValid())
    {
        return vk::Result::eErrorInitializationFailed;
    }

    vk::ImageAspectFlags AspectFlags = bStencilOnly ? vk::ImageAspectFlagBits::eStencil : vk::ImageAspectFlagBits::eDepth;
    if (Format > vk::Format::eS8Uint)
    {
        AspectFlags |= vk::ImageAspectFlagBits::eStencil;
    }
    else if (Format == vk::Format::eS8Uint)
    {
        AspectFlags = vk::ImageAspectFlagBits::eStencil;
    }

    vk::ImageSubresourceRange SubresourceRange(AspectFlags, 0, 1, 0, LayerCount);

    _ImageView = std::make_unique<FVulkanImageView>(_ImageMemory->GetResource(),
                                                    LayerCount > 1 ? vk::ImageViewType::e2DArray : vk::ImageViewType::e2D,
                                                    Format,
                                                    vk::ComponentMapping(),
                                                    SubresourceRange);
    if (!_ImageView->IsValid())
    {
        return vk::Result::eErrorInitializationFailed;
    }

    return vk::Result::eSuccess;
}

FStagingBuffer::FStagingBuffer(vk::DeviceSize Size)
    : FStagingBuffer(FVulkanCore::GetClassInstance()->GetDevice(),
                     FVulkanCore::GetClassInstance()->GetPhysicalDeviceProperties(),
                     FVulkanCore::GetClassInstance()->GetPhysicalDeviceMemoryProperties(),
                     Size)
{
}

FStagingBuffer::FStagingBuffer(vk::Device Device, const vk::PhysicalDeviceProperties& PhysicalDeviceProperties,
                               const vk::PhysicalDeviceMemoryProperties& PhysicalDeviceMemoryProperties, vk::DeviceSize Size)
    :
    _Device(Device),
    _PhysicalDeviceProperties(&PhysicalDeviceProperties),
    _PhysicalDeviceMemoryProperties(&PhysicalDeviceMemoryProperties),
    _BufferMemory(nullptr),
    _AliasedImage(nullptr)
{
    Expand(Size);
}

FStagingBuffer::FStagingBuffer(FStagingBuffer&& Other) noexcept
    :
    _Device(std::exchange(Other._Device, nullptr)),
    _PhysicalDeviceProperties(std::exchange(Other._PhysicalDeviceProperties, nullptr)),
    _PhysicalDeviceMemoryProperties(std::exchange(Other._PhysicalDeviceMemoryProperties, nullptr)),
    _BufferMemory(std::move(Other._BufferMemory)),
    _AliasedImage(std::move(Other._AliasedImage)),
    _MemoryUsage(std::exchange(Other._MemoryUsage, 0))
{
}

FStagingBuffer& FStagingBuffer::operator=(FStagingBuffer&& Other) noexcept
{
    if (this != &Other)
    {
        _Device                         = std::exchange(Other._Device, nullptr);
        _PhysicalDeviceProperties       = std::exchange(Other._PhysicalDeviceProperties, nullptr);
        _PhysicalDeviceMemoryProperties = std::exchange(Other._PhysicalDeviceMemoryProperties, nullptr);
        _BufferMemory                   = std::move(Other._BufferMemory);
        _AliasedImage                   = std::move(Other._AliasedImage);
        _MemoryUsage                    = std::exchange(Other._MemoryUsage, 0);
    }

    return *this;
}

FVulkanImage* FStagingBuffer::CreateAliasedImage(vk::Format Format, vk::Extent2D Extent)
{
    vk::PhysicalDevice   PhysicalDevice   = FVulkanCore::GetClassInstance()->GetPhysicalDevice();
    vk::FormatProperties FormatProperties = PhysicalDevice.getFormatProperties(Format);

    if (!(FormatProperties.linearTilingFeatures & vk::FormatFeatureFlagBits::eBlitSrc))
    {
        return nullptr;
    }

    vk::DeviceSize ImageDataSize = static_cast<vk::DeviceSize>(Extent.width * Extent.height * GetFormatInfo(Format).PixelSize);
    if (ImageDataSize > _BufferMemory->GetMemory().GetAllocationSize())
    {
        return nullptr;
    }

    vk::ImageFormatProperties ImageFormatProperties =
        PhysicalDevice.getImageFormatProperties(Format, vk::ImageType::e2D, vk::ImageTiling::eLinear,
                                                vk::ImageUsageFlagBits::eTransferSrc);
    if (Extent.width  > ImageFormatProperties.maxExtent ||
        Extent.height > ImageFormatProperties.maxExtent ||
        ImageDataSize > ImageFormatProperties.maxResourceSize)
    {
        return nullptr;
    }

    vk::Extent3D Extent3D = { Extent.width, Extent.height, 1 };

    vk::ImageCreateInfo ImageCreateInfo({}, vk::ImageType::e2D, Format, Extent3D, 1, 1, vk::SampleCountFlagBits::e1,
                                        vk::ImageTiling::eLinear, vk::ImageUsageFlagBits::eTransferSrc,
                                        vk::SharingMode::eExclusive, 0, nullptr, vk::ImageLayout::ePreinitialized);
    _AliasedImage = std::make_unique<FVulkanImage>(_Device, *_PhysicalDeviceMemoryProperties, ImageCreateInfo);

    vk::ImageSubresource ImageSubresource(vk::ImageAspectFlagBits::eColor, 0, 0);
    vk::SubresourceLayout SubresourceLayout = _Device.getImageSubresourceLayout(**_AliasedImage, ImageSubresource);
    if (SubresourceLayout.size != ImageDataSize)
    {
        _AliasedImage.reset();
        return nullptr;
    }

    _AliasedImage->BindMemory(_BufferMemory->GetMemory(), 0);
    return _AliasedImage.get();
}

void FStagingBuffer::Expand(vk::DeviceSize Size)
{
    if (_BufferMemory != nullptr && Size <= _BufferMemory->GetMemory().GetAllocationSize())
    {
        return;
    }

    Release();

    vk::BufferCreateInfo BufferCreateInfo({}, Size, vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst);
    _BufferMemory = std::make_unique<FVulkanBufferMemory>(_Device, *_PhysicalDeviceProperties, *_PhysicalDeviceMemoryProperties,
                                                          BufferCreateInfo, vk::MemoryPropertyFlagBits::eHostVisible);
}

FStagingBuffer* FStagingBufferPool::AcquireBuffer(vk::DeviceSize Size)
{
    std::lock_guard<std::mutex> Lock(_Mutex);
    auto it = std::find_if(_FreeBuffers.begin(), _FreeBuffers.end(),
    [Size](const std::unique_ptr<FStagingBuffer>& Buffer) -> bool
    {
        return Buffer->GetMemory().GetAllocationSize() >= Size;
    });

    if (it != _FreeBuffers.end())
    {
        _BusyBuffers.push_back(std::move(*it));
        _FreeBuffers.erase(it);
        return _BusyBuffers.back().get();
    }

    _BusyBuffers.push_back(std::make_unique<FStagingBuffer>(Size));
    return _BusyBuffers.back().get();
}

void FStagingBufferPool::ReleaseBuffer(FStagingBuffer* Buffer)
{
    std::lock_guard<std::mutex> Lock(_Mutex);
    auto it = std::find_if(_BusyBuffers.begin(), _BusyBuffers.end(),
    [Buffer](const std::unique_ptr<FStagingBuffer>& BusyBuffer) -> bool
    {
        return BusyBuffer.get() == Buffer;
    });

    if (it != _BusyBuffers.end())
    {
        _FreeBuffers.push_back(std::move(*it));
        _BusyBuffers.erase(it);
    }
}

FStagingBufferPool* FStagingBufferPool::GetInstance()
{
    static FStagingBufferPool Instance;
    return &Instance;
}

FDeviceLocalBuffer::FDeviceLocalBuffer(vk::DeviceSize Size, vk::BufferUsageFlags Usage)
    : _BufferMemory(nullptr), _StagingBufferPool(FStagingBufferPool::GetInstance())
{
    CreateBuffer(Size, Usage);
}

FDeviceLocalBuffer::FDeviceLocalBuffer(FDeviceLocalBuffer&& Other) noexcept
    :
    _BufferMemory(std::move(Other._BufferMemory)),
    _StagingBufferPool(std::exchange(Other._StagingBufferPool, nullptr))
{
}

FDeviceLocalBuffer& FDeviceLocalBuffer::operator=(FDeviceLocalBuffer&& Other) noexcept
{
    if (this != &Other)
    {
        _BufferMemory      = std::move(Other._BufferMemory);
        _StagingBufferPool = std::exchange(Other._StagingBufferPool, nullptr);
    }

    return *this;
}

void FDeviceLocalBuffer::CopyData(vk::DeviceSize MapOffset, vk::DeviceSize TargetOffset, vk::DeviceSize Size, const void* Data) const
{
    if (_BufferMemory->GetMemory().GetMemoryPropertyFlags() & vk::MemoryPropertyFlagBits::eHostVisible)
    {
        _BufferMemory->SubmitBufferData(MapOffset, TargetOffset, Size, Data);
        return;
    }

    auto* VulkanContext = FVulkanContext::GetClassInstance();

    FStagingBuffer* StagingBuffer = _StagingBufferPool->AcquireBuffer(Size);
    StagingBuffer->SubmitBufferData(Size, Data);

    auto& TransferCommandBuffer = VulkanContext->GetTransferCommandBuffer();
    TransferCommandBuffer.Begin(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    vk::BufferCopy Region(0, TargetOffset, Size);
    TransferCommandBuffer->copyBuffer(*StagingBuffer->GetBuffer(), *_BufferMemory->GetResource(), Region);
    TransferCommandBuffer.End();
    _StagingBufferPool->ReleaseBuffer(StagingBuffer);

    VulkanContext->ExecuteGraphicsCommands(TransferCommandBuffer);
}

void FDeviceLocalBuffer::CopyData(vk::DeviceSize ElementIndex, vk::DeviceSize ElementCount, vk::DeviceSize ElementSize,
                                  vk::DeviceSize SrcStride, vk::DeviceSize DstStride, vk::DeviceSize MapOffset, const void* Data) const
{
    if (_BufferMemory->GetMemory().GetMemoryPropertyFlags() & vk::MemoryPropertyFlagBits::eHostVisible)
    {
        void* Target = _BufferMemory->GetMemory().GetMappedTargetMemory();
        if (Target == nullptr || !_BufferMemory->GetMemory().IsPereistentlyMapped())
        {
            _BufferMemory->MapMemoryForSubmit(MapOffset, ElementCount * DstStride, Target);
        }

        for (std::size_t i = 0; i != ElementCount; ++i)
        {
            std::copy(static_cast<const std::byte*>(Data) + SrcStride * (i + ElementIndex),
                      static_cast<const std::byte*>(Data) + SrcStride * (i + ElementIndex) + ElementSize,
                      static_cast<std::byte*>(Target)     + DstStride * (i + ElementIndex));
        }

        if (!_BufferMemory->GetMemory().IsPereistentlyMapped())
        {
            _BufferMemory->UnmapMemory(MapOffset, ElementCount * DstStride);
        }

        return;
    }

    auto* VulkanContext = FVulkanContext::GetClassInstance();

    FStagingBuffer* StagingBuffer = _StagingBufferPool->AcquireBuffer(DstStride * ElementSize);
    StagingBuffer->SubmitBufferData(SrcStride * ElementSize, Data);

    auto& TransferCommandBuffer = VulkanContext->GetTransferCommandBuffer();
    TransferCommandBuffer.Begin(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

    std::vector<vk::BufferCopy> Regions(ElementCount);
    for (std::size_t i = 0; i < ElementCount; ++i)
    {
        Regions[i] = vk::BufferCopy(SrcStride * (i + ElementIndex), DstStride * (i + ElementIndex), ElementSize);
    }

    TransferCommandBuffer->copyBuffer(*StagingBuffer->GetBuffer(), *_BufferMemory->GetResource(), Regions);
    TransferCommandBuffer.End();
    _StagingBufferPool->ReleaseBuffer(StagingBuffer);

    VulkanContext->ExecuteGraphicsCommands(TransferCommandBuffer);
}

vk::Result FDeviceLocalBuffer::CreateBuffer(vk::DeviceSize Size, vk::BufferUsageFlags Usage)
{
    vk::BufferCreateInfo BufferCreateInfo({}, Size, Usage | vk::BufferUsageFlagBits::eTransferDst);

    vk::MemoryPropertyFlags PreferredMemoryFlags =
        vk::MemoryPropertyFlagBits::eDeviceLocal | vk::MemoryPropertyFlagBits::eHostVisible;
    vk::MemoryPropertyFlags FallbackMemoryFlags  = vk::MemoryPropertyFlagBits::eDeviceLocal;

    _BufferMemory = std::make_unique<FVulkanBufferMemory>(BufferCreateInfo, PreferredMemoryFlags);
    if (!_BufferMemory->IsValid())
    {
        _BufferMemory = std::make_unique<FVulkanBufferMemory>(BufferCreateInfo, FallbackMemoryFlags);
        if (!_BufferMemory->IsValid())
        {
            return vk::Result::eErrorInitializationFailed;
        }
    }

    return vk::Result::eSuccess;
}

vk::Result FDeviceLocalBuffer::RecreateBuffer(vk::DeviceSize Size, vk::BufferUsageFlags Usage)
{
    FVulkanCore::GetClassInstance()->WaitIdle();
    _BufferMemory.reset();
    return CreateBuffer(Size, Usage);
}

_GRAPHICS_END
_RUNTIME_END
_NPGS_END
