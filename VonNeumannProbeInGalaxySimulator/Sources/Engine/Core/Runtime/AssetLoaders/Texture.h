#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include <vulkan/vulkan_handles.hpp>
#include "Engine/Core/Runtime/Graphics/Vulkan/Wrappers.h"

_NPGS_BEGIN
_RUNTIME_BEGIN
_ASSET_BEGIN

class FTextureBase
{
public:
    struct FImageData
    {
        vk::Extent2D ImageExtent{};
        vk::Format   ImageFormat{ vk::Format::eUndefined };
        std::vector<std::byte> Data;
    };

public:
    FTextureBase(const std::string& Filename, const Graphics::FFormatInfo& RequiredFormat);
    FTextureBase(const std::byte* Data, const Graphics::FFormatInfo& RequiredFormat);

    const FImageData& GetImageData() const;

private:
    FImageData LoadImage(const auto* Source, std::size_t Size, const Graphics::FFormatInfo& RequiredFormat);

private:
    FImageData _ImageData;
};

_ASSET_END
_RUNTIME_END
_NPGS_END

#include "Texture.inl"
