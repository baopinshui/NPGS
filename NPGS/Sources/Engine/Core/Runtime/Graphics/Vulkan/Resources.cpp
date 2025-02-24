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

namespace
{
    // 格式描述，用于统一管理格式属性
    struct FFormatDescription
    {
        std::uint32_t Family;        // 格式所属族
        std::uint32_t BitDepth;      // 位深度
        bool          bIsSrgb;       // 是否为 sRGB 格式
        bool          bIsCompressed; // 是否为压缩格式
        bool          bIsDepth;      // 是否为深度格式
    };

    // 格式族枚举
    enum class EFormatFamily : std::uint32_t
    {
        kUnknown = 0,
        kR8,             // R8 系列
        kRG8,            // RG8 系列
        kRGBA8,          // RGBA8 系列
        kBGRA8,          // BGRA8 系列
        kDepth16,        // 16位深度格式
        kDepth24,        // 24位深度格式
        kDepth32,        // 32位深度格式
        kBC1,            // BC1压缩格式
        kBC2,            // BC2压缩格式
        kBC3,            // BC3压缩格式
        // 可以继续添加更多格式族...
    };

    // 获取格式描述符
    FFormatDescription GetFormatDescription(vk::Format Format)
    {
        switch (Format)
        {
            // R8 格式族
        case vk::Format::eR8Unorm:
            return { static_cast<uint32_t>(EFormatFamily::kR8), 8, false, false, false };
        case vk::Format::eR8Srgb:
            return { static_cast<uint32_t>(EFormatFamily::kR8), 8, true, false, false };

            // RG8 格式族
        case vk::Format::eR8G8Unorm:
            return { static_cast<uint32_t>(EFormatFamily::kRG8), 16, false, false, false };
        case vk::Format::eR8G8Srgb:
            return { static_cast<uint32_t>(EFormatFamily::kRG8), 16, true, false, false };

            // RGBA8 格式族
        case vk::Format::eR8G8B8A8Unorm:
            return { static_cast<uint32_t>(EFormatFamily::kRGBA8), 32, false, false, false };
        case vk::Format::eR8G8B8A8Srgb:
            return { static_cast<uint32_t>(EFormatFamily::kRGBA8), 32, true, false, false };

            // BGRA8 格式族
        case vk::Format::eB8G8R8A8Unorm:
            return { static_cast<uint32_t>(EFormatFamily::kBGRA8), 32, false, false, false };
        case vk::Format::eB8G8R8A8Srgb:
            return { static_cast<uint32_t>(EFormatFamily::kBGRA8), 32, true, false, false };

            // 深度格式
        case vk::Format::eD16Unorm:
            return { static_cast<uint32_t>(EFormatFamily::kDepth16), 16, false, false, true };
        case vk::Format::eD24UnormS8Uint:
            return { static_cast<uint32_t>(EFormatFamily::kDepth24), 32, false, false, true };
        case vk::Format::eD32Sfloat:
            return { static_cast<uint32_t>(EFormatFamily::kDepth32), 32, false, false, true };

            // 压缩格式
        case vk::Format::eBc1RgbaUnormBlock:
            return { static_cast<uint32_t>(EFormatFamily::kBC1), 64, false, true, false };
        case vk::Format::eBc1RgbaSrgbBlock:
            return { static_cast<uint32_t>(EFormatFamily::kBC1), 64, true, true, false };

        default:
            return { static_cast<uint32_t>(EFormatFamily::kUnknown), 0, false, false, false };
        }
    }

    bool IsFormatAliasingCompatible(vk::Format SrcFormat, vk::Format DstFormat)
    {
        FFormatDescription SrcDesc = GetFormatDescription(SrcFormat);
        FFormatDescription DstDesc = GetFormatDescription(DstFormat);

        if (SrcDesc.Family == static_cast<uint32_t>(EFormatFamily::kUnknown) ||
            DstDesc.Family == static_cast<uint32_t>(EFormatFamily::kUnknown) ||
            SrcDesc.Family        != DstDesc.Family        || // 必须是同一格式族
            SrcDesc.BitDepth      != DstDesc.BitDepth      || // 位深度必须相同
            SrcDesc.bIsCompressed != DstDesc.bIsCompressed || // 压缩状态必须相同
            SrcDesc.bIsSrgb       != DstDesc.bIsSrgb       || // sRGB 和非 sRGB 不能混叠
            SrcDesc.bIsDepth)                                 // 深度格式不允许混叠
        {
            return false;
        }

        auto* VulkanContext = FVulkanContext::GetClassInstance();
        vk::FormatProperties SrcFormatProperties = VulkanContext->GetPhysicalDevice().getFormatProperties(SrcFormat);
        vk::FormatProperties DstFormatProperties = VulkanContext->GetPhysicalDevice().getFormatProperties(DstFormat);

        bool bLinearTilingCompatible =
            (SrcFormatProperties.linearTilingFeatures & vk::FormatFeatureFlagBits::eSampledImage) &&
            (DstFormatProperties.linearTilingFeatures & vk::FormatFeatureFlagBits::eSampledImage);

        bool bOptimalTilingCompatible =
            (SrcFormatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eSampledImage) &&
            (DstFormatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eSampledImage);

        if (!bLinearTilingCompatible && !bOptimalTilingCompatible)
        {
            return false;
        }

        FFormatInfo SrcFormatInfo = GetFormatInfo(SrcFormat);
        FFormatInfo DstFormatInfo = GetFormatInfo(DstFormat);

        if (SrcDesc.bIsCompressed)
        {
            // 确保组件数量和每个组件的位深度相同
            const auto SrcComponents = SrcFormatInfo.ComponentCount;
            const auto DstComponents = SrcFormatInfo.ComponentCount;
            if (SrcComponents != DstComponents)
            {
                return false;
            }
        }

        return true;
    }
}

FAttachment::FAttachment(VmaAllocator Allocator)
    : _Allocator(Allocator)
{
}

FColorAttachment::FColorAttachment(const VmaAllocationCreateInfo& AllocationCreateInfo, vk::Format Format, vk::Extent2D Extent,
                                   std::uint32_t LayerCount, vk::SampleCountFlagBits SampleCount, vk::ImageUsageFlags ExtraUsage)
    : FColorAttachment(FVulkanCore::GetClassInstance()->GetVmaAllocator(), AllocationCreateInfo,
                       Format, Extent, LayerCount, SampleCount, ExtraUsage)
{
}

FColorAttachment::FColorAttachment(VmaAllocator Allocator, const VmaAllocationCreateInfo& AllocationCreateInfo, vk::Format Format,
                                   vk::Extent2D Extent, std::uint32_t LayerCount, vk::SampleCountFlagBits SampleCount, vk::ImageUsageFlags ExtraUsage)
    : Base(Allocator)
{
    vk::Result Result = CreateAttachment(&AllocationCreateInfo, Format, Extent, LayerCount, SampleCount, ExtraUsage);
    if (Result != vk::Result::eSuccess)
    {
        NpgsCoreError("Failed to create color attachment: {}", vk::to_string(Result));
    }
}

FColorAttachment::FColorAttachment(vk::Format Format, vk::Extent2D Extent, std::uint32_t LayerCount,
                                   vk::SampleCountFlagBits SampleCount, vk::ImageUsageFlags ExtraUsage)
    : Base(nullptr)
{
    vk::Result Result = CreateAttachment(nullptr, Format, Extent, LayerCount, SampleCount, ExtraUsage);
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

vk::Result FColorAttachment::CreateAttachment(const VmaAllocationCreateInfo* AllocationCreateInfo, vk::Format Format, vk::Extent2D Extent,
                                              std::uint32_t LayerCount, vk::SampleCountFlagBits SampleCount, vk::ImageUsageFlags ExtraUsage)
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

    _ImageMemory = AllocationCreateInfo
                 ? std::make_unique<FVulkanImageMemory>(_Allocator, *AllocationCreateInfo, ImageCreateInfo)
                 : std::make_unique<FVulkanImageMemory>(ImageCreateInfo, MemoryPropertyFlags);
    
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

FDepthStencilAttachment::FDepthStencilAttachment(const VmaAllocationCreateInfo& AllocationCreateInfo, vk::Format Format,
                                                 vk::Extent2D Extent, std::uint32_t LayerCount, vk::SampleCountFlagBits SampleCount,
                                                 vk::ImageUsageFlags ExtraUsage, bool bStencilOnly)
    : FDepthStencilAttachment(FVulkanCore::GetClassInstance()->GetVmaAllocator(), AllocationCreateInfo,
                              Format, Extent, LayerCount, SampleCount, ExtraUsage, bStencilOnly)
{
}

FDepthStencilAttachment::FDepthStencilAttachment(VmaAllocator Allocator, const VmaAllocationCreateInfo& AllocationCreateInfo,
                                                 vk::Format Format, vk::Extent2D Extent, std::uint32_t LayerCount,
                                                 vk::SampleCountFlagBits SampleCount, vk::ImageUsageFlags ExtraUsage, bool bStencilOnly)
    : Base(Allocator)
{
    vk::Result Result = CreateAttachment(&AllocationCreateInfo, Format, Extent, LayerCount, SampleCount, ExtraUsage, bStencilOnly);
    if (Result != vk::Result::eSuccess)
    {
        NpgsCoreError("Failed to create depth-stencil attachment: {}", vk::to_string(Result));
    }
}

FDepthStencilAttachment::FDepthStencilAttachment(vk::Format Format, vk::Extent2D Extent, std::uint32_t LayerCount,
                                                 vk::SampleCountFlagBits SampleCount, vk::ImageUsageFlags ExtraUsage, bool bStencilOnly)
    : Base(nullptr)
{
    vk::Result Result = CreateAttachment(nullptr, Format, Extent, LayerCount, SampleCount, ExtraUsage, bStencilOnly);
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

vk::Result FDepthStencilAttachment::CreateAttachment(const VmaAllocationCreateInfo* AllocationCreateInfo, vk::Format Format, vk::Extent2D Extent,
                                                     std::uint32_t LayerCount, vk::SampleCountFlagBits SampleCount, vk::ImageUsageFlags ExtraUsage, bool bStencilOnly)
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

    _ImageMemory = AllocationCreateInfo
                 ? std::make_unique<FVulkanImageMemory>(_Allocator, *AllocationCreateInfo, ImageCreateInfo)
                 : std::make_unique<FVulkanImageMemory>(ImageCreateInfo, MemoryPropertyFlags);
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
    _AliasedImage(nullptr),
    _Allocator(nullptr)
{
    Expand(Size);
}

FStagingBuffer::FStagingBuffer(const VmaAllocationCreateInfo& AllocationCreateInfo, const vk::BufferCreateInfo& BufferCreateInfo)
    : FStagingBuffer(FVulkanCore::GetClassInstance()->GetVmaAllocator(), AllocationCreateInfo, BufferCreateInfo)
{
}

FStagingBuffer::FStagingBuffer(VmaAllocator Allocator, const VmaAllocationCreateInfo& AllocationCreateInfo,
                               const vk::BufferCreateInfo& BufferCreateInfo)
    :
    _Device(FVulkanCore::GetClassInstance()->GetDevice()),
    _PhysicalDeviceProperties(&FVulkanCore::GetClassInstance()->GetPhysicalDeviceProperties()),
    _PhysicalDeviceMemoryProperties(&FVulkanCore::GetClassInstance()->GetPhysicalDeviceMemoryProperties()),
    _BufferMemory(nullptr),
    _AliasedImage(nullptr),
    _Allocator(Allocator),
    _AllocationCreateInfo(AllocationCreateInfo)
{
    Expand(BufferCreateInfo.size);
}

FStagingBuffer::FStagingBuffer(FStagingBuffer&& Other) noexcept
    :
    _Device(std::exchange(Other._Device, nullptr)),
    _PhysicalDeviceProperties(std::exchange(Other._PhysicalDeviceProperties, nullptr)),
    _PhysicalDeviceMemoryProperties(std::exchange(Other._PhysicalDeviceMemoryProperties, nullptr)),
    _BufferMemory(std::move(Other._BufferMemory)),
    _AliasedImage(std::move(Other._AliasedImage)),
    _MemoryUsage(std::exchange(Other._MemoryUsage, 0)),
    _Allocator(std::exchange(Other._Allocator, nullptr)),
    _AllocationCreateInfo(std::exchange(Other._AllocationCreateInfo, {}))
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
        _Allocator                      = std::exchange(Other._Allocator, nullptr);
        _AllocationCreateInfo           = std::exchange(Other._AllocationCreateInfo, {});
    }

    return *this;
}

FVulkanImage* FStagingBuffer::CreateAliasedImage(vk::Format OriganFormat, vk::Format NewFormat, vk::Extent2D Extent)
{
    if (!IsFormatAliasingCompatible(OriganFormat, NewFormat))
    {
        return nullptr;
    }

    vk::PhysicalDevice   PhysicalDevice   = FVulkanCore::GetClassInstance()->GetPhysicalDevice();
    vk::FormatProperties FormatProperties = PhysicalDevice.getFormatProperties(NewFormat);

    if (!(FormatProperties.linearTilingFeatures & vk::FormatFeatureFlagBits::eBlitSrc))
    {
        return nullptr;
    }

    vk::DeviceSize ImageDataSize = static_cast<vk::DeviceSize>(Extent.width * Extent.height * GetFormatInfo(NewFormat).PixelSize);
    if (ImageDataSize > _BufferMemory->GetMemory().GetAllocationSize())
    {
        return nullptr;
    }

    vk::ImageFormatProperties ImageFormatProperties =
        PhysicalDevice.getImageFormatProperties(NewFormat, vk::ImageType::e2D, vk::ImageTiling::eLinear,
                                                vk::ImageUsageFlagBits::eTransferSrc);
    if (Extent.width  > ImageFormatProperties.maxExtent ||
        Extent.height > ImageFormatProperties.maxExtent ||
        ImageDataSize > ImageFormatProperties.maxResourceSize)
    {
        return nullptr;
    }

    vk::Extent3D Extent3D = { Extent.width, Extent.height, 1 };

    vk::ImageCreateInfo ImageCreateInfo({}, vk::ImageType::e2D, NewFormat, Extent3D, 1, 1, vk::SampleCountFlagBits::e1,
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

    if (_Allocator != nullptr)
    {
        _BufferMemory = std::make_unique<FVulkanBufferMemory>(_Allocator, _AllocationCreateInfo, BufferCreateInfo);
    }
    else
    {
        _BufferMemory = std::make_unique<FVulkanBufferMemory>(_Device, *_PhysicalDeviceProperties, *_PhysicalDeviceMemoryProperties,
                                                              BufferCreateInfo, vk::MemoryPropertyFlagBits::eHostVisible);
    }
}

FStagingBuffer* FStagingBufferPool::AcquireBuffer(vk::DeviceSize Size, const VmaAllocationCreateInfo* AllocationCreateInfo)
{
    std::lock_guard<std::mutex> Lock(_Mutex);
    auto it = std::find_if(_FreeBuffers.begin(), _FreeBuffers.end(),
    [Size, AllocationCreateInfo](const std::unique_ptr<FStagingBuffer>& Buffer) -> bool
    {
        bool bEnoughSize             = Buffer->GetMemory().GetAllocationSize() >= Size;
        bool bAllocationMethodNeeded = (Buffer->IsUsingVma()) == (AllocationCreateInfo != nullptr);
        return bEnoughSize && bAllocationMethodNeeded;
    });

    if (it != _FreeBuffers.end())
    {
        _BusyBuffers.push_back(std::move(*it));
        _FreeBuffers.erase(it);
        return _BusyBuffers.back().get();
    }

    std::unique_ptr<FStagingBuffer> NewBuffer;
    if (AllocationCreateInfo != nullptr)
    {
        vk::BufferCreateInfo BufferCreateInfo({}, Size, vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst);
        NewBuffer = std::make_unique<FStagingBuffer>(*AllocationCreateInfo, BufferCreateInfo);
    }
    else
    {
        NewBuffer = std::make_unique<FStagingBuffer>(Size);
    }

    NewBuffer->GetMemory().SetPersistentMapping(true);
    _BusyBuffers.push_back(std::move(NewBuffer));
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
    static FStagingBufferPool kInstance;
    return &kInstance;
}

FDeviceLocalBuffer::FDeviceLocalBuffer(vk::DeviceSize Size, vk::BufferUsageFlags Usage)
    : _BufferMemory(nullptr), _StagingBufferPool(FStagingBufferPool::GetInstance()), _Allocator(nullptr)
{
    CreateBuffer(Size, Usage);
}

FDeviceLocalBuffer::FDeviceLocalBuffer(const VmaAllocationCreateInfo& AllocationCreateInfo, const vk::BufferCreateInfo& BufferCreateInfo)
    : FDeviceLocalBuffer(FVulkanCore::GetClassInstance()->GetVmaAllocator(), AllocationCreateInfo, BufferCreateInfo)
{
}

FDeviceLocalBuffer::FDeviceLocalBuffer(VmaAllocator Allocator, const VmaAllocationCreateInfo& AllocationCreateInfo, const vk::BufferCreateInfo& BufferCreateInfo)
    : _BufferMemory(nullptr), _StagingBufferPool(FStagingBufferPool::GetInstance()), _Allocator(Allocator)
{
    CreateBuffer(AllocationCreateInfo, BufferCreateInfo);
}

FDeviceLocalBuffer::FDeviceLocalBuffer(FDeviceLocalBuffer&& Other) noexcept
    :
    _BufferMemory(std::move(Other._BufferMemory)),
    _StagingBufferPool(std::exchange(Other._StagingBufferPool, nullptr)),
    _Allocator(std::exchange(Other._Allocator, nullptr))
{
}

FDeviceLocalBuffer& FDeviceLocalBuffer::operator=(FDeviceLocalBuffer&& Other) noexcept
{
    if (this != &Other)
    {
        _BufferMemory      = std::move(Other._BufferMemory);
        _StagingBufferPool = std::exchange(Other._StagingBufferPool, nullptr);
        _Allocator         = std::exchange(Other._Allocator, nullptr);
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

    VmaAllocationCreateInfo StagingCreateInfo{ .usage = VMA_MEMORY_USAGE_CPU_TO_GPU };
    VmaAllocationCreateInfo* AllocationCreateInfo = _Allocator ? &StagingCreateInfo : nullptr;

    FStagingBuffer* StagingBuffer = _StagingBufferPool->AcquireBuffer(Size, AllocationCreateInfo);
    StagingBuffer->SubmitBufferData(MapOffset, TargetOffset, Size, Data);

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

    VmaAllocationCreateInfo StagingCreateInfo{ .usage = VMA_MEMORY_USAGE_CPU_TO_GPU };
    VmaAllocationCreateInfo* AllocationCreateInfo = _Allocator ? &StagingCreateInfo : nullptr;

    FStagingBuffer* StagingBuffer = _StagingBufferPool->AcquireBuffer(DstStride * ElementSize, AllocationCreateInfo);
    StagingBuffer->SubmitBufferData(MapOffset, SrcStride * ElementIndex, SrcStride * ElementSize, Data);

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

vk::Result FDeviceLocalBuffer::CreateBuffer(const VmaAllocationCreateInfo& AllocationCreateInfo,
                                            const vk::BufferCreateInfo& BufferCreateInfo)
{
    _BufferMemory = std::make_unique<FVulkanBufferMemory>(_Allocator, AllocationCreateInfo, BufferCreateInfo);
    if (!_BufferMemory->IsValid())
    {
        return vk::Result::eErrorInitializationFailed;
    }

    return vk::Result::eSuccess;
}

vk::Result FDeviceLocalBuffer::RecreateBuffer(vk::DeviceSize Size, vk::BufferUsageFlags Usage)
{
    FVulkanCore::GetClassInstance()->WaitIdle();
    _BufferMemory.reset();
    return CreateBuffer(Size, Usage);
}

vk::Result FDeviceLocalBuffer::RecreateBuffer(const VmaAllocationCreateInfo& AllocationCreateInfo,
                                              const vk::BufferCreateInfo& BufferCreateInfo)
{
    FVulkanCore::GetClassInstance()->WaitIdle();
    _BufferMemory.reset();
    return CreateBuffer(AllocationCreateInfo, BufferCreateInfo);
}

_GRAPHICS_END
_RUNTIME_END
_NPGS_END
