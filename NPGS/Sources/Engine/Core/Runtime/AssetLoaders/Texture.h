#pragma once

#include <cstddef>
#include <cstdint>
#include <array>
#include <memory>
#include <string>
#include <vector>

#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan_handles.hpp>

#include "Engine/Core/Runtime/Graphics/Vulkan/Resources.h"
#include "Engine/Core/Runtime/Graphics/Vulkan/Wrappers.h"

_NPGS_BEGIN
_RUNTIME_BEGIN
_ASSET_BEGIN

class FTextureBase
{
protected:
    struct FImageData
    {
        std::vector<std::byte> Data;
        vk::Extent3D           Extent{};
    };

public:
    vk::DescriptorImageInfo CreateDescriptorImageInfo(const Graphics::FVulkanSampler& Sampler) const;
    vk::DescriptorImageInfo CreateDescriptorImageInfo(const vk::Sampler& Sampler) const;

    Graphics::FVulkanImage& GetImage();
    const Graphics::FVulkanImage& GetImage() const;
    Graphics::FVulkanImageView& GetImageView();
    const Graphics::FVulkanImageView& GetImageView() const;

    static vk::SamplerCreateInfo CreateDefaultSamplerCreateInfo();

protected:
    FTextureBase(VmaAllocator Allocator, const VmaAllocationCreateInfo* AllocationCreateInfo);
    FTextureBase(const FTextureBase&)     = delete;
    FTextureBase(FTextureBase&&) noexcept = default;
    virtual ~FTextureBase()               = default;

    FTextureBase& operator=(const FTextureBase&)     = delete;
    FTextureBase& operator=(FTextureBase&&) noexcept = default;

    void CreateImageMemory(vk::ImageType ImageType, vk::Format Format, vk::Extent3D Extent, std::uint32_t MipLevels,
                           std::uint32_t ArrayLayers, vk::ImageCreateFlags Flags = {});

    void CreateImageView(vk::ImageViewType ImageViewType, vk::Format Format, std::uint32_t MipmapLevelCount,
                         std::uint32_t ArrayLayers, vk::ImageViewCreateFlags Flags = {});

    void CopyBlitGenerateTexture(vk::Buffer SrcBuffer, vk::Extent2D Extent, std::uint32_t MipLevels, std::uint32_t ArrayLayers,
                                 vk::Filter Filter, vk::Image DstImageSrcBlit, vk::Image DstImageDstBlit);

    void BlitGenerateTexture(vk::Image SrcImage, vk::Extent2D Extent, std::uint32_t MipLevels,
                             std::uint32_t ArrayLayers, vk::Filter Filter, vk::Image DstImage);

    FImageData LoadImage(const auto* Source, std::size_t Size, vk::Format ImageFormat, bool bFlipVertically);

protected:
    std::unique_ptr<Graphics::FVulkanImageMemory> _ImageMemory;
    std::unique_ptr<Graphics::FVulkanImageView>   _ImageView;
    VmaAllocator                                  _Allocator;
    const VmaAllocationCreateInfo*                _AllocationCreateInfo;
};

class FTexture2D : public FTextureBase
{
public:
    using Base = FTextureBase;
    using Base::Base;

    FTexture2D(const std::string& Filename, vk::Format InitialFormat, vk::Format FinalFormat,
               vk::ImageCreateFlags Flags = {}, bool bGenerateMipmaps = true, bool bFlipVertically = true);

    FTexture2D(const std::byte* Source, vk::Extent2D Extent, vk::Format InitialFormat, vk::Format FinalFormat,
               vk::ImageCreateFlags Flags = {}, bool bGenerateMipmaps = true);

    FTexture2D(const VmaAllocationCreateInfo& AllocationCreateInfo, const std::string& Filename, vk::Format InitialFormat,
               vk::Format FinalFormat, vk::ImageCreateFlags Flags = {}, bool bGenerateMipmaps = true, bool bFlipVertically = true);

    FTexture2D(VmaAllocator Allocator, const VmaAllocationCreateInfo& AllocationCreateInfo, const std::string& Filename, vk::Format InitialFormat,
               vk::Format FinalFormat, vk::ImageCreateFlags Flags = {}, bool bGenerateMipmaps = true, bool bFlipVertically = true);

    FTexture2D(const VmaAllocationCreateInfo& AllocationCreateInfo, const std::byte* Source, vk::Extent2D Extent,
               vk::Format InitialFormat, vk::Format FinalFormat, vk::ImageCreateFlags Flags = {}, bool bGenerateMipmaps = true);

    FTexture2D(VmaAllocator Allocator, const VmaAllocationCreateInfo& AllocationCreateInfo, const std::byte* Source, vk::Extent2D Extent,
               vk::Format InitialFormat, vk::Format FinalFormat, vk::ImageCreateFlags Flags = {}, bool bGenerateMipmaps = true);

    FTexture2D(const FTexture2D&) = delete;
    FTexture2D(FTexture2D&& Other) noexcept;

    FTexture2D& operator=(const FTexture2D&) = delete;
    FTexture2D& operator=(FTexture2D&& Other) noexcept;

    std::uint32_t GetImageWidth()  const;
    std::uint32_t GetImageHeight() const;
    vk::Extent2D  GetImageExtent() const;

private:
    void CreateTexture(const std::string& Filename, vk::Format  InitialFormat, vk::Format FinalFormat,
                       vk::ImageCreateFlags Flags, bool bGenreteMipmaps, bool bFlipVertically);

    void CreateTexture(const std::byte* Source, vk::Extent2D Extent, vk::Format InitialFormat,
                       vk::Format FinalFormat, vk::ImageCreateFlags Flags, bool bGenerateMipmaps);

    void CreateTextureInternal(Graphics::FStagingBuffer* StagingBuffer, vk::Format InitialFormat,
                               vk::Format FinalFormat, vk::ImageCreateFlags Flags, bool bGenerateMipmaps);

private:
    Graphics::FStagingBufferPool* _StagingBufferPool;
    vk::Extent2D                  _ImageExtent;
};

class FTextureCube : public FTextureBase
{
public:
    using Base = FTextureBase;
    using Base::Base;

    FTextureCube(const std::string& Filename, vk::Format InitialFormat, vk::Format FinalFormat,
                 vk::ImageCreateFlags Flags = {}, bool bGenerateMipmaps = true, bool bFlipVertically = true);

    FTextureCube(const std::array<std::byte*, 6>& Sources, vk::Extent2D Extent, vk::Format InitialFormat,
                 vk::Format FinalFormat, vk::ImageCreateFlags Flags = {}, bool bGenerateMipmaps = true);

    FTextureCube(vk::Extent2D Extent, vk::Format Format, vk::ImageCreateFlags Flags = {},
                 vk::ImageUsageFlags Usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled);

    FTextureCube(const VmaAllocationCreateInfo& AllocationCreateInfo, const std::string& Filename, vk::Format InitialFormat,
                 vk::Format FinalFormat, vk::ImageCreateFlags Flags = {}, bool bGenerateMipmaps = true, bool bFlipVertically = true);

    FTextureCube(VmaAllocator Allocator, const VmaAllocationCreateInfo& AllocationCreateInfo, const std::string& Filename,
                 vk::Format InitialFormat, vk::Format FinalFormat, vk::ImageCreateFlags Flags = {},
                 bool bGenerateMipmaps = true, bool bFlipVertically = true);

    FTextureCube(const FTextureCube&) = delete;
    FTextureCube(FTextureCube&& Other) noexcept;

    FTextureCube& operator=(const FTextureCube&) = delete;
    FTextureCube& operator=(FTextureCube&& Other) noexcept;

    std::uint32_t GetImageWidth()  const;
    std::uint32_t GetImageHeight() const;
    vk::Extent2D  GetImageExtent() const;

private:
    void CreateCubemap(const std::string& Filename, vk::Format InitialFormat, vk::Format FinalFormat,
                       vk::ImageCreateFlags Flags, bool bGenerateMipmaps, bool bFlipVertically);

    void CreateCubemap(const std::array<std::string, 6>& Filenames, vk::Format InitialFormat, vk::Format FinalFormat,
                       vk::ImageCreateFlags Flags, bool bGenerateMipmaps, bool bFlipVertically);

    void CreateCubemap(const std::array<std::byte*, 6>& Sources, vk::Extent2D Extent, vk::Format InitialFormat,
                       vk::Format FinalFormat, vk::ImageCreateFlags Flags, bool bGenerateMipmaps);

    void CreateCubemapInternal(Graphics::FStagingBuffer* StagingBuffer, vk::Format InitialFormat,
                               vk::Format FinalFormat, vk::ImageCreateFlags Flags, bool bGenerateMipmaps);

private:
    Graphics::FStagingBufferPool* _StagingBufferPool;
    vk::Extent2D                  _ImageExtent;
};


_ASSET_END
_RUNTIME_END
_NPGS_END

#include "Texture.inl"
