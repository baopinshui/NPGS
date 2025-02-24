#include "Texture.h"

#include <cmath>
#include <algorithm>
#include <array>
#include <exception>
#include <filesystem>
#include <format>
#include <string_view>
#include <type_traits>
#include <utility>

#include <gli/gli.hpp>
#include <stb_image.h>

#include "Engine/Core/Runtime/AssetLoaders/GetAssetFullPath.h"
#include "Engine/Core/Runtime/Graphics/Vulkan/Context.h"
#include "Engine/Utils/Logger.h"

_NPGS_BEGIN
_RUNTIME_BEGIN
_ASSET_BEGIN

namespace
{
    std::uint32_t CalculateMipLevels(vk::Extent2D Extent)
    {
        return static_cast<std::uint32_t>(std::floor(std::log2(std::max(Extent.width, Extent.height)))) + 1;
    }

    vk::Offset3D MipmapExtent(vk::Extent2D Extent, std::uint32_t MipLevel)
    {
        return { static_cast<std::int32_t>(std::max(1u, Extent.width  >> MipLevel)),
                 static_cast<std::int32_t>(std::max(1u, Extent.height >> MipLevel)), 1 };
    }

    void CopyBufferToImage(const Graphics::FVulkanCommandBuffer& CommandBuffer, vk::Buffer SrcBuffer,
                           const Graphics::FImageMemoryBarrierParameterPack& SrcBarrier,
                           const Graphics::FImageMemoryBarrierParameterPack& DstBarrier,
                           const vk::BufferImageCopy& Region, vk::Image DstImage)
    {
        vk::ImageSubresourceRange ImageSubresourceRange(
            Region.imageSubresource.aspectMask,
            Region.imageSubresource.mipLevel,
            1,
            Region.imageSubresource.baseArrayLayer,
            Region.imageSubresource.layerCount);

        vk::ImageMemoryBarrier2 Barrier;
        vk::DependencyInfo DependencyInfo = vk::DependencyInfo()
            .setDependencyFlags(vk::DependencyFlagBits::eByRegion);

        if (SrcBarrier.kbEnable)
        {
            Barrier.setSrcStageMask(SrcBarrier.kPipelineStageFlagBits)
                   .setSrcAccessMask(SrcBarrier.kAccessFlagBits)
                   .setDstStageMask(vk::PipelineStageFlagBits2::eTransfer)
                   .setDstAccessMask(vk::AccessFlagBits2::eTransferWrite)
                   .setOldLayout(SrcBarrier.kImageLayout)
                   .setNewLayout(vk::ImageLayout::eTransferDstOptimal)
                   .setImage(DstImage)
                   .setSubresourceRange(ImageSubresourceRange);

            DependencyInfo.setImageMemoryBarriers(Barrier);

            CommandBuffer->pipelineBarrier2(DependencyInfo);
        }

        CommandBuffer->copyBufferToImage(SrcBuffer, DstImage, vk::ImageLayout::eTransferDstOptimal, Region);

        if (DstBarrier.kbEnable)
        {
            Barrier.setSrcStageMask(vk::PipelineStageFlagBits2::eTransfer)
                   .setSrcAccessMask(vk::AccessFlagBits2::eTransferWrite)
                   .setDstStageMask(DstBarrier.kPipelineStageFlagBits)
                   .setDstAccessMask(DstBarrier.kAccessFlagBits)
                   .setOldLayout(vk::ImageLayout::eTransferDstOptimal)
                   .setNewLayout(DstBarrier.kImageLayout);

            CommandBuffer->pipelineBarrier2(DependencyInfo);
        }
    }

    void BlitImage(const Graphics::FVulkanCommandBuffer& CommandBuffer, vk::Image SrcImage,
                   const Graphics::FImageMemoryBarrierParameterPack& SrcBarrier,
                   const Graphics::FImageMemoryBarrierParameterPack& DstBarrier,
                   const vk::ImageBlit& Region, vk::Filter Filter, vk::Image DstImage)
    {
        vk::ImageSubresourceRange SrcImageSubresourceRange(
            Region.srcSubresource.aspectMask,
            Region.srcSubresource.mipLevel,
            1,
            Region.srcSubresource.baseArrayLayer,
            Region.srcSubresource.layerCount);

        vk::ImageSubresourceRange DstImageSubresourceRange(
            Region.dstSubresource.aspectMask,
            Region.dstSubresource.mipLevel,
            1,
            Region.dstSubresource.baseArrayLayer,
            Region.dstSubresource.layerCount);

        if (SrcBarrier.kbEnable)
        {
            vk::ImageMemoryBarrier2 ConvertToTransferSrcBarrier(
                SrcBarrier.kPipelineStageFlagBits,
                SrcBarrier.kAccessFlagBits,
                vk::PipelineStageFlagBits2::eTransfer,
                vk::AccessFlagBits2::eTransferRead,
                SrcBarrier.kImageLayout,
                vk::ImageLayout::eTransferSrcOptimal,
                vk::QueueFamilyIgnored,
                vk::QueueFamilyIgnored,
                SrcImage,
                SrcImageSubresourceRange);

            vk::ImageMemoryBarrier2 ConvertToTransferDstBarrier(
                SrcBarrier.kPipelineStageFlagBits,
                SrcBarrier.kAccessFlagBits,
                vk::PipelineStageFlagBits2::eTransfer,
                vk::AccessFlagBits2::eTransferWrite,
                SrcBarrier.kImageLayout,
                vk::ImageLayout::eTransferDstOptimal,
                vk::QueueFamilyIgnored,
                vk::QueueFamilyIgnored,
                DstImage,
                DstImageSubresourceRange);

            std::array InitImageBarriers{ ConvertToTransferSrcBarrier, ConvertToTransferDstBarrier };

            vk::DependencyInfo InitTransferDependencyInfo = vk::DependencyInfo()
                .setDependencyFlags(vk::DependencyFlagBits::eByRegion)
                .setImageMemoryBarriers(InitImageBarriers);

            CommandBuffer->pipelineBarrier2(InitTransferDependencyInfo);
        }

        CommandBuffer->blitImage(SrcImage, vk::ImageLayout::eTransferSrcOptimal,
                                 DstImage, vk::ImageLayout::eTransferDstOptimal, Region, Filter);

        if (DstBarrier.kbEnable)
        {
            vk::ImageMemoryBarrier2 FinalBarrier(
                vk::PipelineStageFlagBits2::eTransfer,
                vk::AccessFlagBits2::eTransferWrite,
                DstBarrier.kPipelineStageFlagBits,
                DstBarrier.kAccessFlagBits,
                vk::ImageLayout::eTransferDstOptimal,
                DstBarrier.kImageLayout,
                vk::QueueFamilyIgnored,
                vk::QueueFamilyIgnored,
                DstImage,
                DstImageSubresourceRange);

            vk::DependencyInfo FinalDependencyInfo = vk::DependencyInfo()
                .setDependencyFlags(vk::DependencyFlagBits::eByRegion)
                .setImageMemoryBarriers(FinalBarrier);

            CommandBuffer->pipelineBarrier2(FinalDependencyInfo);
        }
    }

    void GenerateMipmaps(const Graphics::FVulkanCommandBuffer& CommandBuffer, vk::Image Image, vk::Extent2D Extent,
                         std::uint32_t MipLevels, std::uint32_t ArrayLayers, vk::Filter Filter,
                         const Graphics::FImageMemoryBarrierParameterPack& FinalBarrier)
    {
        if (ArrayLayers > 1)
        {
            std::vector<vk::ImageBlit> Regions(ArrayLayers);
            vk::ImageSubresourceRange InitialMipRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, ArrayLayers);

            vk::ImageMemoryBarrier2 InitImageBarrier(
                vk::PipelineStageFlagBits2::eTransfer,
                vk::AccessFlagBits2::eNone,
                vk::PipelineStageFlagBits2::eTransfer,
                vk::AccessFlagBits2::eTransferRead,
                vk::ImageLayout::eUndefined,
                vk::ImageLayout::eTransferSrcOptimal,
                vk::QueueFamilyIgnored,
                vk::QueueFamilyIgnored,
                Image,
                InitialMipRange);

            vk::DependencyInfo InitDependencyInfo = vk::DependencyInfo()
                .setDependencyFlags(vk::DependencyFlagBits::eByRegion)
                .setImageMemoryBarriers(InitImageBarrier);

            CommandBuffer->pipelineBarrier2(InitDependencyInfo);

            for (std::uint32_t MipLevel = 1; MipLevel != MipLevels; ++MipLevel)
            {
                vk::Offset3D SrcExtent = MipmapExtent(Extent, MipLevel - 1);
                vk::Offset3D DstExtent = MipmapExtent(Extent, MipLevel);

                vk::ImageSubresourceRange CurrentMipRange(vk::ImageAspectFlagBits::eColor, MipLevel, 1, 0, ArrayLayers);

                vk::ImageMemoryBarrier2 ConvertToTransferDstBarrier(
                    vk::PipelineStageFlagBits2::eTransfer,
                    vk::AccessFlagBits2::eNone,
                    vk::PipelineStageFlagBits2::eTransfer,
                    vk::AccessFlagBits2::eTransferWrite,
                    vk::ImageLayout::eUndefined,
                    vk::ImageLayout::eTransferDstOptimal,
                    vk::QueueFamilyIgnored,
                    vk::QueueFamilyIgnored,
                    Image,
                    CurrentMipRange);

                vk::DependencyInfo ConvertToTransferDstDependencyInfo = vk::DependencyInfo()
                    .setDependencyFlags(vk::DependencyFlagBits::eByRegion)
                    .setImageMemoryBarriers(ConvertToTransferDstBarrier);

                CommandBuffer->pipelineBarrier2(ConvertToTransferDstDependencyInfo);

                for (std::uint32_t Layer = 0; Layer != ArrayLayers; ++Layer)
                {
                    Regions[Layer] = vk::ImageBlit(
                        vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, MipLevel - 1, Layer, 1),
                        { vk::Offset3D(0, 0, 0), SrcExtent },
                        vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, MipLevel,     Layer, 1),
                        { vk::Offset3D(0, 0, 0), DstExtent }
                    );
                }

                CommandBuffer->blitImage(Image, vk::ImageLayout::eTransferSrcOptimal,
                                         Image, vk::ImageLayout::eTransferDstOptimal, Regions, Filter);

                vk::ImageMemoryBarrier2 ConvertToTransferSrcBarrier(
                    vk::PipelineStageFlagBits2::eTransfer,
                    vk::AccessFlagBits2::eTransferWrite,
                    vk::PipelineStageFlagBits2::eTransfer,
                    vk::AccessFlagBits2::eTransferRead,
                    vk::ImageLayout::eTransferDstOptimal,
                    vk::ImageLayout::eTransferSrcOptimal,
                    vk::QueueFamilyIgnored,
                    vk::QueueFamilyIgnored,
                    Image,
                    CurrentMipRange);

                vk::DependencyInfo ConvertToTransferSrcDependencyInfo = vk::DependencyInfo()
                    .setDependencyFlags(vk::DependencyFlagBits::eByRegion)
                    .setImageMemoryBarriers(ConvertToTransferSrcBarrier);

                CommandBuffer->pipelineBarrier2(ConvertToTransferSrcDependencyInfo);
            }
        }
        else
        {
            Graphics::FImageMemoryBarrierParameterPack SrcBarrier(
                vk::PipelineStageFlagBits2::eTransfer,
                vk::AccessFlagBits2::eNone,
                vk::ImageLayout::eUndefined);

            Graphics::FImageMemoryBarrierParameterPack DstBarrier(
                vk::PipelineStageFlagBits2::eTransfer,
                vk::AccessFlagBits2::eTransferRead,
                vk::ImageLayout::eTransferSrcOptimal);

            for (std::uint32_t MipLevel = 1; MipLevel != MipLevels; ++MipLevel)
            {
                vk::ImageBlit Region(
                    vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, MipLevel - 1, 0, 1),
                    { vk::Offset3D(), MipmapExtent(Extent, MipLevel - 1) },
                    vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, MipLevel,     0, 1),
                    { vk::Offset3D(), MipmapExtent(Extent, MipLevel) }
                );

                BlitImage(CommandBuffer, Image, SrcBarrier, DstBarrier, Region, Filter, Image);
            }
        }

        if (FinalBarrier.kbEnable)
        {
            vk::ImageSubresourceRange FinalMipRange(vk::ImageAspectFlagBits::eColor, 0, MipLevels, 0, ArrayLayers);

            vk::ImageMemoryBarrier2 FinalTransitionBarrier(
                vk::PipelineStageFlagBits2::eTransfer,
                vk::AccessFlagBits2::eNone,
                FinalBarrier.kPipelineStageFlagBits,
                FinalBarrier.kAccessFlagBits,
                vk::ImageLayout::eTransferSrcOptimal,
                FinalBarrier.kImageLayout,
                vk::QueueFamilyIgnored,
                vk::QueueFamilyIgnored,
                Image,
                FinalMipRange);

            vk::DependencyInfo FinalDependencyInfo = vk::DependencyInfo()
                .setDependencyFlags(vk::DependencyFlagBits::eByRegion)
                .setImageMemoryBarriers(FinalTransitionBarrier);

            CommandBuffer->pipelineBarrier2(FinalDependencyInfo);
        }
    }
}

FTextureBase::FTextureBase(VmaAllocator Allocator, const VmaAllocationCreateInfo* AllocationCreateInfo)
    : _Allocator(Allocator), _AllocationCreateInfo(AllocationCreateInfo)
{
}

FTextureBase::FImageData FTextureBase::LoadImage(const auto* Source, std::size_t Size, vk::Format ImageFormat, bool bFlipVertically)
{
    int   ImageWidth    = 0;
    int   ImageHeight   = 0;
    int   ImageChannels = 0;
    void* ImageData     = 0;
    std::string ErrorMessage;

    Graphics::FFormatInfo FormatInfo = Graphics::GetFormatInfo(ImageFormat);

    stbi_set_flip_vertically_on_load(bFlipVertically);

    if constexpr (std::is_same_v<decltype(Source), const char*>)
    {
        if (!std::filesystem::exists(Source))
        {
            NpgsCoreError("Failed to load image: \"{}\": No such file or directory.", Source);
            return {};
        }

        std::string_view Filename(Source);
        bool bIsKtx = Filename.ends_with(".ktx") || Filename.ends_with(".KTX");
        bool bIsDds = Filename.ends_with(".dds") || Filename.ends_with(".DDS");

        if (bIsKtx || bIsDds)
        {
            try
            {
                gli::texture Texture = gli::load(Source);
                if (!Texture.empty())
                {
                    gli::extent3d Extent = Texture.extent();
                    std::size_t   Size   = Texture.size(0);

                    FImageData Data;
                    Data.Extent.width  = static_cast<uint32_t>(Extent.x);
                    Data.Extent.height = static_cast<uint32_t>(Extent.y);

                    Data.Data.resize(Size);
                    std::copy(static_cast<const std::byte*>(Texture.data()),
                              static_cast<const std::byte*>(Texture.data()) + Size,
                              Data.Data.begin());

                    return Data;
                }
                else
                {
                    NpgsCoreError("Failed to load compressed texture: \"{}\".", Source);
                    return {};
                }
            }
            catch (const std::exception& e)
            {
                NpgsCoreError("Failed to load image: \"{}\": {}", Source, e.what());
                return {};
            }
        }

        ErrorMessage = std::format("Failed to load image: \"{}\".", Source);
        if (FormatInfo.RawDataType == Graphics::FFormatInfo::ERawDataType::kInteger)
        {
            if (FormatInfo.ComponentSize == 1)
            {
                ImageData = stbi_load(Source, &ImageWidth, &ImageHeight, &ImageChannels, FormatInfo.ComponentCount);
            }
            else
            {
                ImageData = stbi_load_16(Source, &ImageWidth, &ImageHeight, &ImageChannels, FormatInfo.ComponentCount);
            }
        }
        else
        {
            ImageData = stbi_loadf(Source, &ImageWidth, &ImageHeight, &ImageChannels, FormatInfo.ComponentCount);
        }
    }
    else if constexpr (std::is_same_v<decltype(Source), const std::byte*>)
    {
        ErrorMessage = "Failed to load image from memory.";
        if (FormatInfo.RawDataType == Graphics::FFormatInfo::ERawDataType::kInteger)
        {
            if (FormatInfo.ComponentSize == 1)
            {
                ImageData = stbi_load_from_memory(static_cast<const stbi_uc*>(Source), Size, &ImageWidth, &ImageHeight,
                                                  &ImageChannels, FormatInfo.ComponentCount);
            }
            else
            {
                ImageData = stbi_load_16_from_memory(static_cast<const stbi_uc*>(Source), Size, &ImageWidth, &ImageHeight,
                                                     &ImageChannels, FormatInfo.ComponentCount);
            }
        }
        else
        {
            ImageData = stbi_loadf_from_memory(static_cast<const stbi_uc*>(Source), Size, &ImageWidth, &ImageHeight,
                                               &ImageChannels, FormatInfo.ComponentCount);
        }
    }
    else
    {
        ErrorMessage = "Failed to determine image source.";
    }

    if (ImageData == nullptr)
    {
        NpgsCoreError(ErrorMessage);
        return {};
    }

    FImageData Data;
    Data.Extent.width  = ImageWidth;
    Data.Extent.height = ImageHeight;

    Data.Data = std::move(std::vector<std::byte>(
        static_cast<std::byte*>(ImageData), static_cast<std::byte*>(ImageData) + ImageWidth * ImageHeight * FormatInfo.PixelSize));

    return Data;
}

void FTextureBase::CreateImageMemory(vk::ImageType ImageType, vk::Format Format, vk::Extent3D Extent, std::uint32_t MipLevels,
                                     std::uint32_t ArrayLayers, vk::ImageCreateFlags Flags)
{
    vk::ImageCreateInfo ImageCreateInfo = vk::ImageCreateInfo()
        .setFlags(Flags)
        .setImageType(ImageType)
        .setFormat(Format)
        .setExtent(Extent)
        .setMipLevels(MipLevels)
        .setArrayLayers(ArrayLayers)
        .setSamples(vk::SampleCountFlagBits::e1)
        .setUsage(vk::ImageUsageFlagBits::eTransferSrc |
                  vk::ImageUsageFlagBits::eTransferDst |
                  vk::ImageUsageFlagBits::eSampled);

    if (_Allocator != nullptr)
    {
        _ImageMemory = std::make_unique<Graphics::FVulkanImageMemory>(_Allocator, *_AllocationCreateInfo, ImageCreateInfo);
    }
    else
    {
        _ImageMemory = std::make_unique<Graphics::FVulkanImageMemory>(ImageCreateInfo, vk::MemoryPropertyFlagBits::eDeviceLocal);
    }
}

void FTextureBase::CreateImageView(vk::ImageViewType ImageViewType, vk::Format Format, std::uint32_t MipmapLevelCount,
                                   std::uint32_t ArrayLayers, vk::ImageViewCreateFlags Flags)
{
    vk::ImageSubresourceRange ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, MipmapLevelCount, 0, ArrayLayers);

    _ImageView = std::make_unique<Graphics::FVulkanImageView>(_ImageMemory->GetResource(), ImageViewType, Format,
                                                              vk::ComponentMapping(), ImageSubresourceRange, Flags);

}

void FTextureBase::CopyBlitGenerateTexture(vk::Buffer SrcBuffer, vk::Extent2D Extent, std::uint32_t MipLevels, std::uint32_t ArrayLayers,
                                           vk::Filter Filter, vk::Image DstImageSrcBlit, vk::Image DstImageDstBlit)
{
    static constexpr std::array kBarriers
    {
        Graphics::FImageMemoryBarrierParameterPack(
            vk::PipelineStageFlagBits2::eFragmentShader,
            vk::AccessFlagBits2::eShaderRead,
            vk::ImageLayout::eShaderReadOnlyOptimal),

        Graphics::FImageMemoryBarrierParameterPack(
            vk::PipelineStageFlagBits2::eTransfer,
            vk::AccessFlagBits2::eTransferWrite,
            vk::ImageLayout::eTransferDstOptimal)
    };

    bool bGenerateMipmaps = MipLevels > 1;
    bool bNeedBlit        = DstImageSrcBlit != DstImageDstBlit;

    auto* VulkanContext = Graphics::FVulkanContext::GetClassInstance();
    auto& CommandBuffer = VulkanContext->GetTransferCommandBuffer();
    CommandBuffer.Begin(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

    vk::ImageSubresourceLayers Subresource(vk::ImageAspectFlagBits::eColor, 0, 0, ArrayLayers);
    vk::Extent3D Extent3D = { Extent.width, Extent.height, 1 };

    vk::BufferImageCopy BufferImageCopy = vk::BufferImageCopy()
        .setImageSubresource(Subresource)
        .setImageExtent(Extent3D);

    Graphics::FImageMemoryBarrierParameterPack CopyBarrier(
        vk::PipelineStageFlagBits2::eTopOfPipe,
        vk::AccessFlagBits2::eNone,
        vk::ImageLayout::eUndefined);

    CopyBufferToImage(CommandBuffer, SrcBuffer, CopyBarrier, kBarriers[bGenerateMipmaps || bNeedBlit], BufferImageCopy, DstImageSrcBlit);

    if (bNeedBlit)
    {
        vk::ImageSubresourceLayers Subresource(vk::ImageAspectFlagBits::eColor, 0, 0, ArrayLayers);
        std::array<vk::Offset3D, 2> Offsets;
        Offsets[1] = vk::Offset3D(static_cast<std::int32_t>(Extent.width), static_cast<std::int32_t>(Extent.height), 1);

        vk::ImageBlit Region(Subresource, Offsets, Subresource, Offsets);

        Graphics::FImageMemoryBarrierParameterPack BlitBarrier(
            vk::PipelineStageFlagBits2::eTopOfPipe,
            vk::AccessFlagBits2::eNone,
            vk::ImageLayout::eUndefined);

        BlitImage(CommandBuffer, DstImageSrcBlit, BlitBarrier, kBarriers[bGenerateMipmaps], Region, Filter, DstImageDstBlit);
    }

    if (bGenerateMipmaps)
    {
        GenerateMipmaps(CommandBuffer, DstImageDstBlit, Extent, MipLevels, ArrayLayers, Filter, kBarriers[0]);
    }

    CommandBuffer.End();
    VulkanContext->ExecuteGraphicsCommands(CommandBuffer);
}

void FTextureBase::BlitGenerateTexture(vk::Image SrcImage, vk::Extent2D Extent, std::uint32_t MipLevels, std::uint32_t ArrayLayers, vk::Filter Filter, vk::Image DstImage)
{
    static constexpr std::array kBarriers
    {
        Graphics::FImageMemoryBarrierParameterPack(
            vk::PipelineStageFlagBits2::eFragmentShader,
            vk::AccessFlagBits2::eShaderRead,
            vk::ImageLayout::eShaderReadOnlyOptimal),

        Graphics::FImageMemoryBarrierParameterPack(
            vk::PipelineStageFlagBits2::eTransfer,
            vk::AccessFlagBits2::eTransferWrite,
            vk::ImageLayout::eTransferDstOptimal)
    };

    bool bGenerateMipmaps = MipLevels > 1;
    bool bNeedBlit        = SrcImage != DstImage;
    if (bGenerateMipmaps || bNeedBlit)
    {
        auto* VulkanContext = Graphics::FVulkanContext::GetClassInstance();
        auto& CommandBuffer = VulkanContext->GetTransferCommandBuffer();
        CommandBuffer.Begin(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

        if (bNeedBlit)
        {
            vk::ImageMemoryBarrier2 PipelineBarrier(
                vk::PipelineStageFlagBits2::eTopOfPipe,
                vk::AccessFlagBits2::eNone,
                vk::PipelineStageFlagBits2::eTransfer,
                vk::AccessFlagBits2::eTransferRead,
                vk::ImageLayout::ePreinitialized,
                vk::ImageLayout::eTransferSrcOptimal,
                vk::QueueFamilyIgnored,
                vk::QueueFamilyIgnored,
                SrcImage,
                { vk::ImageAspectFlagBits::eColor, 0, 1, 0, ArrayLayers });

            vk::DependencyInfo DependencyInfo = vk::DependencyInfo()
                .setDependencyFlags(vk::DependencyFlagBits::eByRegion)
                .setImageMemoryBarriers(PipelineBarrier);

            CommandBuffer->pipelineBarrier2(DependencyInfo);

            vk::ImageSubresourceLayers Subresource(vk::ImageAspectFlagBits::eColor, 0, 0, ArrayLayers);
            std::array<vk::Offset3D, 2> Offsets;
            Offsets[1] = vk::Offset3D(static_cast<std::int32_t>(Extent.width), static_cast<std::int32_t>(Extent.height), 1);

            vk::ImageBlit Region(Subresource, Offsets, Subresource, Offsets);

            Graphics::FImageMemoryBarrierParameterPack BlitBarrier(
                vk::PipelineStageFlagBits2::eTopOfPipe,
                vk::AccessFlagBits2::eNone,
                vk::ImageLayout::eUndefined);

            BlitImage(CommandBuffer, SrcImage, BlitBarrier, kBarriers[bGenerateMipmaps], Region, Filter, DstImage);
        }

        if (bGenerateMipmaps)
        {
            GenerateMipmaps(CommandBuffer, DstImage, Extent, MipLevels, ArrayLayers, Filter, kBarriers[0]);
        }

        CommandBuffer.End();
        VulkanContext->ExecuteGraphicsCommands(CommandBuffer);
    }
}

FTexture2D::FTexture2D(const std::string& Filename, vk::Format InitialFormat, vk::Format FinalFormat,
                       vk::ImageCreateFlags Flags, bool bGenerateMipmaps, bool bFlipVertically)
    : Base(nullptr, nullptr), _StagingBufferPool(Graphics::FStagingBufferPool::GetInstance())
{
    CreateTexture(GetAssetFullPath(EAssetType::kTexture, Filename), InitialFormat, FinalFormat, Flags, bGenerateMipmaps, bFlipVertically);
}

FTexture2D::FTexture2D(const std::byte* Source, vk::Extent2D Extent, vk::Format InitialFormat,
                       vk::Format FinalFormat, vk::ImageCreateFlags Flags, bool bGenerateMipmaps)
    : Base(nullptr, nullptr), _StagingBufferPool(Graphics::FStagingBufferPool::GetInstance())
{
    CreateTexture(Source, Extent, InitialFormat, FinalFormat, Flags, bGenerateMipmaps);
}

FTexture2D::FTexture2D(const VmaAllocationCreateInfo& AllocationCreateInfo, const std::string& Filename, vk::Format InitialFormat,
                       vk::Format FinalFormat, vk::ImageCreateFlags Flags, bool bGenerateMipmaps, bool bFlipVertically)
    : FTexture2D(Graphics::FVulkanContext::GetClassInstance()->GetVmaAllocator(), AllocationCreateInfo, Filename, InitialFormat,
                 FinalFormat, Flags, bGenerateMipmaps, bFlipVertically)
{
}

FTexture2D::FTexture2D(VmaAllocator Allocator, const VmaAllocationCreateInfo& AllocationCreateInfo, const std::string& Filename,
                       vk::Format InitialFormat, vk::Format FinalFormat, vk::ImageCreateFlags Flags, bool bGenerateMipmaps, bool bFlipVertically)
    : Base(Allocator, &AllocationCreateInfo), _StagingBufferPool(Graphics::FStagingBufferPool::GetInstance())
{
    CreateTexture(GetAssetFullPath(EAssetType::kTexture, Filename), InitialFormat, FinalFormat, Flags, bGenerateMipmaps, bFlipVertically);
}

FTexture2D::FTexture2D(const VmaAllocationCreateInfo& AllocationCreateInfo, const std::byte* Source, vk::Extent2D Extent,
                       vk::Format InitialFormat, vk::Format FinalFormat, vk::ImageCreateFlags Flags, bool bGenerateMipmaps)
    : FTexture2D(Graphics::FVulkanContext::GetClassInstance()->GetVmaAllocator(), AllocationCreateInfo,
                 Source, Extent, InitialFormat, FinalFormat, Flags, bGenerateMipmaps)
{
}

FTexture2D::FTexture2D(VmaAllocator Allocator, const VmaAllocationCreateInfo& AllocationCreateInfo, const std::byte* Source,
                       vk::Extent2D Extent, vk::Format InitialFormat, vk::Format FinalFormat, vk::ImageCreateFlags Flags, bool bGenerateMipmaps)
    : Base(Allocator, &AllocationCreateInfo), _StagingBufferPool(Graphics::FStagingBufferPool::GetInstance())
{
    CreateTexture(Source, Extent, InitialFormat, FinalFormat, Flags, bGenerateMipmaps);
}

FTexture2D::FTexture2D(FTexture2D&& Other) noexcept
    :
    Base(std::move(Other)),
    _StagingBufferPool(std::exchange(Other._StagingBufferPool, nullptr)),
    _ImageExtent(std::exchange(Other._ImageExtent, {}))
{
}

FTexture2D& FTexture2D::operator=(FTexture2D&& Other) noexcept
{
    if (this != &Other)
    {
        Base::operator=(std::move(Other));
        _StagingBufferPool = std::exchange(Other._StagingBufferPool, nullptr);
        _ImageExtent       = std::exchange(Other._ImageExtent, {});
    }

    return *this;
}

void FTexture2D::CreateTexture(const std::string& Filename, vk::Format InitialFormat, vk::Format FinalFormat,
                               vk::ImageCreateFlags Flags, bool bGenreteMipmaps, bool bFlipVertically)
{
    FImageData ImageData = LoadImage(Filename.c_str(), 0, InitialFormat, bFlipVertically);
    vk::Extent2D Extent  = { ImageData.Extent.width, ImageData.Extent.height };
    CreateTexture(ImageData.Data.data(), Extent, InitialFormat, FinalFormat, Flags, bGenreteMipmaps);
}

void FTexture2D::CreateTexture(const std::byte* Source, vk::Extent2D Extent, vk::Format InitialFormat,
                               vk::Format FinalFormat, vk::ImageCreateFlags Flags, bool bGenerateMipmaps)
{
    _ImageExtent = Extent;
    vk::DeviceSize ImageSize = _ImageExtent.width * _ImageExtent.height * Graphics::GetFormatInfo(InitialFormat).PixelSize;

    VmaAllocationCreateInfo StagingCreateInfo{ .usage = VMA_MEMORY_USAGE_CPU_TO_GPU };
    VmaAllocationCreateInfo* AllocationCreateInfo = _Allocator ? &StagingCreateInfo : nullptr;

    auto* StagingBuffer = _StagingBufferPool->AcquireBuffer(ImageSize, AllocationCreateInfo);
    StagingBuffer->SubmitBufferData(0, 0, ImageSize, Source);
    CreateTextureInternal(StagingBuffer, InitialFormat, FinalFormat, Flags, bGenerateMipmaps);
    _StagingBufferPool->ReleaseBuffer(StagingBuffer);
}

void FTexture2D::CreateTextureInternal(Graphics::FStagingBuffer* StagingBuffer, vk::Format InitialFormat,
                                       vk::Format FinalFormat, vk::ImageCreateFlags Flags, bool bGenerateMipmaps)
{
    std::uint32_t MipLevels = bGenerateMipmaps ? CalculateMipLevels(_ImageExtent) : 1;
    CreateImageMemory(vk::ImageType::e2D, InitialFormat, { _ImageExtent.width, _ImageExtent.height, 1 }, MipLevels, 1, Flags);
    CreateImageView(vk::ImageViewType::e2D, FinalFormat, MipLevels, 1);

    if (InitialFormat == FinalFormat)
    {
        CopyBlitGenerateTexture(*StagingBuffer->GetBuffer(), _ImageExtent, MipLevels, 1, vk::Filter::eLinear,
                                *_ImageMemory->GetResource(), *_ImageMemory->GetResource());
    }
    else
    {
        Graphics::FVulkanImage* ConvertedImage = StagingBuffer->CreateAliasedImage(FinalFormat, _ImageExtent);

        if (ConvertedImage)
        {
            BlitGenerateTexture(**ConvertedImage, _ImageExtent, MipLevels, 1, vk::Filter::eLinear, *_ImageMemory->GetResource());
        }
        else
        {
            vk::ImageCreateInfo ImageCreateInfo = vk::ImageCreateInfo()
                .setImageType(vk::ImageType::e2D)
                .setFormat(InitialFormat)
                .setExtent({ _ImageExtent.width, _ImageExtent.height, 1 })
                .setMipLevels(MipLevels)
                .setArrayLayers(1)
                .setSamples(vk::SampleCountFlagBits::e1)
                .setUsage(vk::ImageUsageFlagBits::eTransferSrc |
                          vk::ImageUsageFlagBits::eTransferDst |
                          vk::ImageUsageFlagBits::eSampled);

            std::unique_ptr<Graphics::FVulkanImageMemory> ConversionImage;
            if (_Allocator != nullptr)
            {
                ConversionImage = std::make_unique<Graphics::FVulkanImageMemory>(_Allocator, *_AllocationCreateInfo, ImageCreateInfo);
            }
            else
            {
                ConversionImage = std::make_unique<Graphics::FVulkanImageMemory>(ImageCreateInfo, vk::MemoryPropertyFlagBits::eDeviceLocal);
            }

            CopyBlitGenerateTexture(*StagingBuffer->GetBuffer(), _ImageExtent, MipLevels, 1, vk::Filter::eLinear,
                                    *ConversionImage->GetResource(), *ConversionImage->GetResource());

            CreateImageMemory(vk::ImageType::e2D, FinalFormat, { _ImageExtent.width, _ImageExtent.height, 1 }, MipLevels, 1, Flags);
            CreateImageView(vk::ImageViewType::e2D, FinalFormat, MipLevels, 1);

            auto* VulkanContext = Graphics::FVulkanContext::GetClassInstance();
            auto& CommandBuffer = VulkanContext->GetTransferCommandBuffer();
            CommandBuffer.Begin(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

            Graphics::FImageMemoryBarrierParameterPack SrcBarrier(
                vk::PipelineStageFlagBits2::eTopOfPipe,
                vk::AccessFlagBits2::eNone,
                vk::ImageLayout::eUndefined);

            Graphics::FImageMemoryBarrierParameterPack DstBarrier(
                vk::PipelineStageFlagBits2::eFragmentShader,
                vk::AccessFlagBits2::eShaderRead,
                vk::ImageLayout::eShaderReadOnlyOptimal);

            if (bGenerateMipmaps)
            {
                for (std::uint32_t MipLevel = 0; MipLevel != MipLevels; ++MipLevel)
                {
                    vk::Offset3D SrcExtent = MipmapExtent(_ImageExtent, MipLevel);

                    vk::ImageBlit Region(
                        vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, MipLevel, 0, 1),
                        { vk::Offset3D(0, 0, 0), SrcExtent },
                        vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, MipLevel, 0, 1),
                        { vk::Offset3D(0, 0, 0), SrcExtent }
                    );

                    Graphics::FImageMemoryBarrierParameterPack SrcBarrier(
                        vk::PipelineStageFlagBits2::eTopOfPipe,
                        vk::AccessFlagBits2::eNone,
                        vk::ImageLayout::eUndefined);

                    Graphics::FImageMemoryBarrierParameterPack DstBarrier(
                        vk::PipelineStageFlagBits2::eTransfer,
                        vk::AccessFlagBits2::eTransferWrite,
                        vk::ImageLayout::eTransferDstOptimal);

                    BlitImage(CommandBuffer, *ConversionImage->GetResource(), SrcBarrier, DstBarrier,
                              Region, vk::Filter::eLinear, *_ImageMemory->GetResource());
                }

                vk::ImageSubresourceRange FinalRange(vk::ImageAspectFlagBits::eColor, 0, MipLevels, 0, 1);

                vk::ImageMemoryBarrier2 FinalBarrier(
                    vk::PipelineStageFlagBits2::eTransfer,
                    vk::AccessFlagBits2::eTransferWrite,
                    vk::PipelineStageFlagBits2::eFragmentShader,
                    vk::AccessFlagBits2::eShaderRead,
                    vk::ImageLayout::eTransferDstOptimal,
                    vk::ImageLayout::eShaderReadOnlyOptimal,
                    vk::QueueFamilyIgnored,
                    vk::QueueFamilyIgnored,
                    *_ImageMemory->GetResource(),
                    FinalRange);

                vk::DependencyInfo FinalDependencyInfo = vk::DependencyInfo()
                    .setDependencyFlags(vk::DependencyFlagBits::eByRegion)
                    .setImageMemoryBarriers(FinalBarrier);

                CommandBuffer->pipelineBarrier2(FinalDependencyInfo);
            }
            else
            {
                vk::ImageBlit Region(
                    vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1),
                    { vk::Offset3D(0, 0, 0), vk::Offset3D(_ImageExtent.width, _ImageExtent.height, 1) },
                    vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1),
                    { vk::Offset3D(0, 0, 0), vk::Offset3D(_ImageExtent.width, _ImageExtent.height, 1) }
                );

                Graphics::FImageMemoryBarrierParameterPack SrcBarrier(
                    vk::PipelineStageFlagBits2::eTopOfPipe,
                    vk::AccessFlagBits2::eNone,
                    vk::ImageLayout::eUndefined);

                Graphics::FImageMemoryBarrierParameterPack DstBarrier(
                    vk::PipelineStageFlagBits2::eFragmentShader,
                    vk::AccessFlagBits2::eShaderRead,
                    vk::ImageLayout::eShaderReadOnlyOptimal);

                BlitImage(CommandBuffer, *ConversionImage->GetResource(), SrcBarrier, DstBarrier,
                          Region, vk::Filter::eLinear, *_ImageMemory->GetResource());
            }

            CommandBuffer.End();
            VulkanContext->ExecuteGraphicsCommands(CommandBuffer);
        }
    }
}

_ASSET_END
_RUNTIME_END
_NPGS_END
