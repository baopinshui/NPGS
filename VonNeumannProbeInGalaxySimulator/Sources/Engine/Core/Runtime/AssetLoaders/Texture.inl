#include "Texture.h"

#include "Engine/Core/Runtime/Graphics/Vulkan/Context.h"

_NPGS_BEGIN
_RUNTIME_BEGIN
_ASSET_BEGIN

NPGS_INLINE vk::DescriptorImageInfo FTextureBase::CreateDescriptorImageInfo(const Graphics::FVulkanSampler& Sampler)
{
    return { *Sampler, **_ImageView, vk::ImageLayout::eShaderReadOnlyOptimal };
}

NPGS_INLINE Graphics::FVulkanImage& FTextureBase::GetImage()
{
    return _ImageMemory->GetResource();
}

NPGS_INLINE const Graphics::FVulkanImage& FTextureBase::GetImage() const
{
    return _ImageMemory->GetResource();
}

NPGS_INLINE Graphics::FVulkanImageView& FTextureBase::GetImageView()
{
    return *_ImageView;
}

NPGS_INLINE const Graphics::FVulkanImageView& FTextureBase::GetImageView() const
{
    return *_ImageView;
}

NPGS_INLINE vk::SamplerCreateInfo FTextureBase::CreateSamplerCreateInfo()
{
    vk::SamplerCreateInfo SamplerCreateInfo(
        {},
        vk::Filter::eLinear,
        vk::Filter::eLinear,
        vk::SamplerMipmapMode::eLinear,
        vk::SamplerAddressMode::eRepeat,
        vk::SamplerAddressMode::eRepeat,
        vk::SamplerAddressMode::eRepeat,
        0.0f,
        vk::True,
        Graphics::FVulkanContext::GetClassInstance()->GetPhysicalDeviceProperties().limits.maxSamplerAnisotropy,
        vk::False,
        vk::CompareOp::eAlways,
        0.0f,
        vk::LodClampNone,
        vk::BorderColor::eFloatOpaqueWhite,
        vk::False
    );

    return SamplerCreateInfo;
}

NPGS_INLINE std::uint32_t FTexture2D::GetImageWidth() const
{
    return _ImageExtent.width;
}

NPGS_INLINE std::uint32_t FTexture2D::GetImageHeight() const
{
    return _ImageExtent.height;
}

NPGS_INLINE vk::Extent2D FTexture2D::GetImageExtent() const
{
    return _ImageExtent;
}

_ASSET_END
_RUNTIME_END
_NPGS_END
