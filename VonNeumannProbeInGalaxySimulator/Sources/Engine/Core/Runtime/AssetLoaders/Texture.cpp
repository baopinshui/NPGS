#include "Texture.h"

#include <cmath>
#include <algorithm>
#include <array>
#include <filesystem>
#include <format>
#include <type_traits>
#include <utility>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "Engine/Core/Runtime/AssetLoaders/GetAssetFullPath.h"
#include "Engine/Core/Runtime/Graphics/Vulkan/Context.h"
#include "Engine/Utils/Logger.h"

_NPGS_BEGIN
_RUNTIME_BEGIN
_ASSET_BEGIN

namespace
{
    std::uint32_t CalculateMipLevels(vk::Extent2D Extent);
    vk::Offset3D MipmapExtent(vk::Extent2D Extent, std::uint32_t Mipmaps);

    void CopyBufferToImage(const Graphics::FVulkanCommandBuffer& CommandBuffer, vk::Buffer SrcBuffer,
                           const Graphics::FImageMemoryBarrierParameterPack& SrcBarrier,
                           const Graphics::FImageMemoryBarrierParameterPack& DstBarrier,
                           const vk::BufferImageCopy& Region, vk::Image DstImage);

    void BlitImage(const Graphics::FVulkanCommandBuffer& CommandBuffer, vk::Image SrcImage,
                   const Graphics::FImageMemoryBarrierParameterPack& SrcBarrier,
                   const Graphics::FImageMemoryBarrierParameterPack& DstBarrier,
                   const vk::ImageBlit& Region, vk::Filter Filter, vk::Image DstImage);

    void GenerateMipmaps(const Graphics::FVulkanCommandBuffer& CommandBuffer, vk::Image Image, vk::Extent2D Extent,
                         std::uint32_t MipLevels, std::uint32_t ArrayLayers, vk::Filter Filter,
                         const Graphics::FImageMemoryBarrierParameterPack& FinalBarrier);
}

FTextureBase::FImageData FTextureBase::LoadImage(const auto* Source, std::size_t Size, vk::Format ImageFormat)
{
    int   ImageWidth    = 0;
    int   ImageHeight   = 0;
    int   ImageChannels = 0;
    void* ImageData     = 0;
    std::string ErrorMessage;

    Graphics::FFormatInfo FormatInfo = Graphics::GetFormatInfo(ImageFormat);

    if constexpr (std::is_same_v<decltype(Source), const char*>)
    {
        if (!std::filesystem::exists(Source))
        {
            NpgsCoreError("Failed to load image: \"{}\": No such file or directory.", Source);
            return {};
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

    Data.Data = std::move(std::vector<std::byte>(static_cast<std::byte*>(ImageData),
                          static_cast<std::byte*>(ImageData) + ImageWidth * ImageHeight * FormatInfo.PixelSize));

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

    _ImageMemory = std::make_unique<Graphics::FVulkanImageMemory>(ImageCreateInfo, vk::MemoryPropertyFlagBits::eDeviceLocal);
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
    static constexpr std::array Barriers
    {
        Graphics::FImageMemoryBarrierParameterPack(vk::PipelineStageFlagBits::eFragmentShader,
                                                   vk::AccessFlagBits::eShaderRead,
                                                   vk::ImageLayout::eShaderReadOnlyOptimal),

        Graphics::FImageMemoryBarrierParameterPack(vk::PipelineStageFlagBits::eTransfer,
                                                   vk::AccessFlagBits::eTransferWrite,
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

    Graphics::FImageMemoryBarrierParameterPack CopyBarrier(vk::PipelineStageFlagBits::eTopOfPipe,
                                                           vk::AccessFlagBits::eNone,
                                                           vk::ImageLayout::eUndefined);
    CopyBufferToImage(CommandBuffer, SrcBuffer, CopyBarrier, Barriers[bGenerateMipmaps || bNeedBlit], BufferImageCopy, DstImageSrcBlit);

    if (bNeedBlit)
    {
        vk::ImageSubresourceLayers Subresource(vk::ImageAspectFlagBits::eColor, 0, 0, ArrayLayers);
        std::array<vk::Offset3D, 2> Offsets;
        Offsets[1] = vk::Offset3D(static_cast<std::int32_t>(Extent.width), static_cast<std::int32_t>(Extent.height), 1);

        vk::ImageBlit Region(Subresource, Offsets, Subresource, Offsets);

        Graphics::FImageMemoryBarrierParameterPack BlitBarrier(vk::PipelineStageFlagBits::eTopOfPipe,
                                                               vk::AccessFlagBits::eNone,
                                                               vk::ImageLayout::eUndefined);
        BlitImage(CommandBuffer, DstImageSrcBlit, BlitBarrier, Barriers[bGenerateMipmaps], Region, Filter, DstImageDstBlit);
    }

    if (bGenerateMipmaps)
    {
        GenerateMipmaps(CommandBuffer, DstImageDstBlit, Extent, MipLevels, ArrayLayers, Filter, Barriers[0]);
    }

    CommandBuffer.End();
    VulkanContext->ExecuteGraphicsCommands(CommandBuffer);
}

void FTextureBase::BlitGenerateTexture(vk::Image SrcImage, vk::Extent2D Extent, std::uint32_t MipLevels, std::uint32_t ArrayLayers, vk::Filter Filter, vk::Image DstImage)
{
    static constexpr std::array Barriers
    {
        Graphics::FImageMemoryBarrierParameterPack(vk::PipelineStageFlagBits::eFragmentShader,
                                                   vk::AccessFlagBits::eShaderRead,
                                                   vk::ImageLayout::eShaderReadOnlyOptimal),

        Graphics::FImageMemoryBarrierParameterPack(vk::PipelineStageFlagBits::eTransfer,
                                                   vk::AccessFlagBits::eTransferWrite,
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
            vk::ImageMemoryBarrier PipelineBarrier(vk::AccessFlagBits::eNone,
                                                   vk::AccessFlagBits::eTransferRead,
                                                   vk::ImageLayout::ePreinitialized,
                                                   vk::ImageLayout::eTransferSrcOptimal,
                                                   vk::QueueFamilyIgnored,
                                                   vk::QueueFamilyIgnored,
                                                   SrcImage,
                                                   { vk::ImageAspectFlagBits::eColor, 0, 1, 0, ArrayLayers });

            CommandBuffer->pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer,
                                           {}, {}, {}, PipelineBarrier);

            vk::ImageSubresourceLayers Subresource(vk::ImageAspectFlagBits::eColor, 0, 0, ArrayLayers);
            std::array<vk::Offset3D, 2> Offsets;
            Offsets[1] = vk::Offset3D(static_cast<std::int32_t>(Extent.width), static_cast<std::int32_t>(Extent.height), 1);

            vk::ImageBlit Region(Subresource, Offsets, Subresource, Offsets);

            Graphics::FImageMemoryBarrierParameterPack BlitBarrier(vk::PipelineStageFlagBits::eTopOfPipe,
                                                                   vk::AccessFlagBits::eNone,
                                                                   vk::ImageLayout::eUndefined);
            BlitImage(CommandBuffer, SrcImage, BlitBarrier, Barriers[bGenerateMipmaps], Region, Filter, DstImage);
        }

        if (bGenerateMipmaps)
        {
            GenerateMipmaps(CommandBuffer, DstImage, Extent, MipLevels, ArrayLayers, Filter, Barriers[0]);
        }

        CommandBuffer.End();
        VulkanContext->ExecuteGraphicsCommands(CommandBuffer);
    }
}

FTexture2D::FTexture2D(const std::string& Filename, vk::Format InitialFormat, vk::Format FinalFormat,
                       vk::ImageCreateFlags Flags, bool bGenerateMipmaps)
    : _StagingBufferPool(Graphics::FStagingBufferPool::GetInstance())
{
    CreateTexture(GetAssetFullPath(EAssetType::kTexture, Filename), InitialFormat, FinalFormat, Flags, bGenerateMipmaps);
}

FTexture2D::FTexture2D(const std::byte* Source, vk::Extent2D Extent, vk::Format InitialFormat,
                       vk::Format FinalFormat, vk::ImageCreateFlags Flags, bool bGenerateMipmaps)
    : _StagingBufferPool(Graphics::FStagingBufferPool::GetInstance())
{
    CreateTexture(Source, Extent, InitialFormat, FinalFormat, Flags, bGenerateMipmaps);
}

void FTexture2D::CreateTexture(const std::string& Filename, vk::Format InitialFormat, vk::Format FinalFormat,
                               vk::ImageCreateFlags Flags, bool bGenreteMipmaps)
{
    FImageData ImageData = LoadImage(Filename.c_str(), 0, InitialFormat);
    vk::Extent2D Extent  = { ImageData.Extent.width, ImageData.Extent.height };
    CreateTexture(ImageData.Data.data(), Extent, InitialFormat, FinalFormat, Flags, bGenreteMipmaps);
}

void FTexture2D::CreateTexture(const std::byte* Source, vk::Extent2D Extent, vk::Format InitialFormat,
                               vk::Format FinalFormat, vk::ImageCreateFlags Flags, bool bGenerateMipmaps)
{
    _ImageExtent = Extent;
    vk::DeviceSize ImageSize = _ImageExtent.width * _ImageExtent.height * Graphics::GetFormatInfo(InitialFormat).PixelSize;
    auto* StagingBuffer = _StagingBufferPool->AcquireBuffer(ImageSize);
    StagingBuffer->SubmitBufferData(ImageSize, Source);
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
        vk::Image ConvertedImage = **StagingBuffer->CreateAliasedImage(FinalFormat, _ImageExtent);

        if (ConvertedImage)
        {
            BlitGenerateTexture(ConvertedImage, _ImageExtent, MipLevels, 1, vk::Filter::eLinear, *_ImageMemory->GetResource());
        }
        else
        {
            vk::ImageCreateInfo ImageCreateInfo = vk::ImageCreateInfo()
                .setImageType(vk::ImageType::e2D)
                .setFormat(InitialFormat)
                .setExtent({ _ImageExtent.width, _ImageExtent.height, 1 })
                .setMipLevels(1)
                .setArrayLayers(1)
                .setSamples(vk::SampleCountFlagBits::e1)
                .setUsage(vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst);

            Graphics::FVulkanImageMemory Conversion(ImageCreateInfo, vk::MemoryPropertyFlagBits::eDeviceLocal);
            CopyBlitGenerateTexture(*StagingBuffer->GetBuffer(), _ImageExtent, MipLevels, 1, vk::Filter::eLinear,
                                    *Conversion.GetResource(), *Conversion.GetResource());
        }
    }
}

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
        vk::ImageSubresourceRange ImageSubresourceRange(Region.imageSubresource.aspectMask,
                                                        Region.imageSubresource.mipLevel,
                                                        1,
                                                        Region.imageSubresource.baseArrayLayer,
                                                        Region.imageSubresource.layerCount);

        vk::ImageMemoryBarrier ImageMemoryBarrier(SrcBarrier.kAccessFlags,
                                                  vk::AccessFlagBits::eTransferWrite,
                                                  SrcBarrier.kImageLayout,
                                                  vk::ImageLayout::eTransferDstOptimal,
                                                  vk::QueueFamilyIgnored,
                                                  vk::QueueFamilyIgnored,
                                                  DstImage,
                                                  ImageSubresourceRange);

        if (SrcBarrier.kbEnable)
        {
            CommandBuffer->pipelineBarrier(SrcBarrier.kPipelineStageFlags, vk::PipelineStageFlagBits::eTransfer,
                                           {}, {}, {}, ImageMemoryBarrier);
        }

        CommandBuffer->copyBufferToImage(SrcBuffer, DstImage, vk::ImageLayout::eTransferDstOptimal, Region);

        if (DstBarrier.kbEnable)
        {
            ImageMemoryBarrier.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite)
                              .setDstAccessMask(DstBarrier.kAccessFlags)
                              .setOldLayout(vk::ImageLayout::eTransferDstOptimal)
                              .setNewLayout(DstBarrier.kImageLayout);

            CommandBuffer->pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, DstBarrier.kPipelineStageFlags,
                                           {}, {}, {}, ImageMemoryBarrier);
        }
    }

    void BlitImage(const Graphics::FVulkanCommandBuffer& CommandBuffer, vk::Image SrcImage,
                   const Graphics::FImageMemoryBarrierParameterPack& SrcBarrier,
                   const Graphics::FImageMemoryBarrierParameterPack& DstBarrier,
                   const vk::ImageBlit& Region, vk::Filter Filter, vk::Image DstImage)
    {
        vk::ImageSubresourceRange SrcImageSubresourceRange(Region.srcSubresource.aspectMask,
                                                           Region.srcSubresource.mipLevel,
                                                           1,
                                                           Region.srcSubresource.baseArrayLayer,
                                                           Region.srcSubresource.layerCount);

        vk::ImageMemoryBarrier ConvertToTransferSrcBarrier(SrcBarrier.kAccessFlags,
                                                           vk::AccessFlagBits::eTransferRead,
                                                           SrcBarrier.kImageLayout,
                                                           vk::ImageLayout::eTransferSrcOptimal,
                                                           vk::QueueFamilyIgnored,
                                                           vk::QueueFamilyIgnored,
                                                           SrcImage,
                                                           SrcImageSubresourceRange);

        vk::ImageSubresourceRange DstImageSubresourceRange(Region.dstSubresource.aspectMask,
                                                           Region.dstSubresource.mipLevel,
                                                           1,
                                                           Region.dstSubresource.baseArrayLayer,
                                                           Region.dstSubresource.layerCount);

        vk::ImageMemoryBarrier ConvertToTransferDstBarrier(SrcBarrier.kAccessFlags,
                                                           vk::AccessFlagBits::eTransferWrite,
                                                           SrcBarrier.kImageLayout,
                                                           vk::ImageLayout::eTransferDstOptimal,
                                                           vk::QueueFamilyIgnored,
                                                           vk::QueueFamilyIgnored,
                                                           DstImage,
                                                           DstImageSubresourceRange);

        std::array ImageMemoryBarriers{ ConvertToTransferSrcBarrier, ConvertToTransferDstBarrier };

        if (SrcBarrier.kbEnable)
        {
            CommandBuffer->pipelineBarrier(SrcBarrier.kPipelineStageFlags, vk::PipelineStageFlagBits::eTransfer,
                                           {}, {}, {}, ImageMemoryBarriers);
        }

        CommandBuffer->blitImage(SrcImage, vk::ImageLayout::eTransferSrcOptimal,
                                 DstImage, vk::ImageLayout::eTransferDstOptimal, Region, Filter);

        if (DstBarrier.kbEnable)
        {
            vk::ImageMemoryBarrier FinalBarrier(vk::AccessFlagBits::eTransferWrite,
                                                DstBarrier.kAccessFlags,
                                                vk::ImageLayout::eTransferDstOptimal,
                                                DstBarrier.kImageLayout,
                                                vk::QueueFamilyIgnored,
                                                vk::QueueFamilyIgnored,
                                                DstImage,
                                                DstImageSubresourceRange);

            CommandBuffer->pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, DstBarrier.kPipelineStageFlags,
                                           {}, {}, {}, FinalBarrier);
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
            vk::ImageMemoryBarrier InitialBarrier(vk::AccessFlagBits::eNone,
                                                  vk::AccessFlagBits::eTransferRead,
                                                  vk::ImageLayout::eUndefined,
                                                  vk::ImageLayout::eTransferSrcOptimal,
                                                  vk::QueueFamilyIgnored,
                                                  vk::QueueFamilyIgnored,
                                                  Image,
                                                  InitialMipRange);

            CommandBuffer->pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer,
                                           {}, {}, {}, InitialBarrier);

            for (std::uint32_t MipLevel = 1; MipLevel != MipLevels; ++MipLevel)
            {
                vk::Offset3D SrcExtent = MipmapExtent(Extent, MipLevel - 1);
                vk::Offset3D DstExtent = MipmapExtent(Extent, MipLevel);

                vk::ImageSubresourceRange CurrentMipRange(vk::ImageAspectFlagBits::eColor, MipLevel, 1, 0, ArrayLayers);
                vk::ImageMemoryBarrier ConvertToTransferDstBarrier(vk::AccessFlagBits::eNone,
                                                                   vk::AccessFlagBits::eTransferWrite,
                                                                   vk::ImageLayout::eUndefined,
                                                                   vk::ImageLayout::eTransferDstOptimal,
                                                                   vk::QueueFamilyIgnored,
                                                                   vk::QueueFamilyIgnored,
                                                                   Image,
                                                                   CurrentMipRange);

                CommandBuffer->pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer,
                                               {}, {}, {}, ConvertToTransferDstBarrier);

                for (std::uint32_t Layer = 0; Layer != ArrayLayers; ++Layer)
                {
                    Regions[Layer] = vk::ImageBlit(
                        vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, MipLevel - 1, Layer, 1),
                        { vk::Offset3D{0, 0, 0}, SrcExtent },
                        vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, MipLevel, Layer, 1),
                        { vk::Offset3D{0, 0, 0}, DstExtent }
                    );
                }

                CommandBuffer->blitImage(Image, vk::ImageLayout::eTransferSrcOptimal, Image, vk::ImageLayout::eTransferDstOptimal,
                                         Regions, Filter);

                vk::ImageMemoryBarrier ConvertToTransferSrcBarrier(vk::AccessFlagBits::eTransferWrite,
                                                                   vk::AccessFlagBits::eTransferRead,
                                                                   vk::ImageLayout::eTransferDstOptimal,
                                                                   vk::ImageLayout::eTransferSrcOptimal,
                                                                   vk::QueueFamilyIgnored,
                                                                   vk::QueueFamilyIgnored,
                                                                   Image,
                                                                   CurrentMipRange);

                CommandBuffer->pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer,
                                               {}, {}, {}, ConvertToTransferSrcBarrier);
            }
        }
        else
        {
            Graphics::FImageMemoryBarrierParameterPack SrcBarrier(vk::PipelineStageFlagBits::eTransfer,
                                                                  vk::AccessFlagBits::eNone,
                                                                  vk::ImageLayout::eUndefined);
            Graphics::FImageMemoryBarrierParameterPack DstBarrier(vk::PipelineStageFlagBits::eTransfer,
                                                                  vk::AccessFlagBits::eTransferRead,
                                                                  vk::ImageLayout::eTransferSrcOptimal);
            for (std::uint32_t MipLevel = 1; MipLevel != MipLevels; ++MipLevel)
            {
                vk::ImageBlit Region(
                    vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, MipLevel - 1, 0, 1),
                    { vk::Offset3D(), MipmapExtent(Extent, MipLevel - 1)},
                    vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, MipLevel, 0, 1),
                    { vk::Offset3D(), MipmapExtent(Extent, MipLevel)}
                );

                BlitImage(CommandBuffer, Image, SrcBarrier, DstBarrier, Region, Filter, Image);
            }
        }

        if (FinalBarrier.kbEnable)
        {
            vk::ImageSubresourceRange FinalMipRange(vk::ImageAspectFlagBits::eColor, 0, MipLevels, 0, ArrayLayers);
            vk::ImageMemoryBarrier FinalTransitionBarrier(vk::AccessFlagBits::eNone,
                                                          FinalBarrier.kAccessFlags,
                                                          vk::ImageLayout::eTransferSrcOptimal,
                                                          FinalBarrier.kImageLayout,
                                                          vk::QueueFamilyIgnored,
                                                          vk::QueueFamilyIgnored,
                                                          Image,
                                                          FinalMipRange);

            CommandBuffer->pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, FinalBarrier.kPipelineStageFlags,
                                           {}, {}, {}, FinalTransitionBarrier);
        }
    }
}

_ASSET_END
_RUNTIME_END
_NPGS_END
