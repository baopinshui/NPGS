#include "Texture.h"

#include <format>
#include <type_traits>
#include <utility>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "Engine/Core/Runtime/AssetLoaders/GetAssetFullPath.h"
#include "Engine/Utils/Logger.h"

_NPGS_BEGIN
_RUNTIME_BEGIN
_ASSET_BEGIN

FTextureBase::FTextureBase(const std::string& Filename, const Graphics::FFormatInfo& RequiredFormat)
{
    _ImageData = LoadImage(GetAssetFullPath(Asset::EAssetType::kTexture, Filename).c_str(), 0, RequiredFormat);
}

FTextureBase::FImageData FTextureBase::LoadImage(const auto* Source, std::size_t Size, const Graphics::FFormatInfo& RequiredFormat)
{
    int   ImageWidth    = 0;
    int   ImageHeight   = 0;
    int   ImageChannels = 0;
    void* ImageData     = 0;
    std::string ErrorMessage;

    if constexpr (std::is_same_v<decltype(Source), const char*>)
    {
        ErrorMessage = std::format("Failed to load image: \"{}\": No such file or directory.", Source);
        if (RequiredFormat.RawDataType == Graphics::FFormatInfo::ERawDataType::kInteger)
        {
            if (RequiredFormat.ComponentSize == 1)
            {
                ImageData = stbi_load(Source, &ImageWidth, &ImageHeight, &ImageChannels, RequiredFormat.ComponentCount);
            }
            else
            {
                ImageData = stbi_load_16(Source, &ImageWidth, &ImageHeight, &ImageChannels, RequiredFormat.ComponentCount);
            }
        }
        else
        {
            ImageData = stbi_loadf(Source, &ImageWidth, &ImageHeight, &ImageChannels, RequiredFormat.ComponentCount);
        }
    }
    else if constexpr (std::is_same_v<decltype(Source), const std::byte*>)
    {
        ErrorMessage = "Failed to load image from memory.";
        if (RequiredFormat.RawDataType == Graphics::FFormatInfo::ERawDataType::kInteger)
        {
            if (RequiredFormat.ComponentSize == 1)
            {
                ImageData = stbi_load_from_memory(static_cast<const stbi_uc*>(Source), Size, &ImageWidth, &ImageHeight, &ImageChannels, RequiredFormat.ComponentCount);
            }
            else
            {
                ImageData = stbi_load_16_from_memory(static_cast<const stbi_uc*>(Source), Size, &ImageWidth, &ImageHeight, &ImageChannels, RequiredFormat.ComponentCount);
            }
        }
        else
        {
            ImageData = stbi_loadf_from_memory(static_cast<const stbi_uc*>(Source), Size, &ImageWidth, &ImageHeight, &ImageChannels, RequiredFormat.ComponentCount);
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
    Data.ImageExtent.width  = ImageWidth;
    Data.ImageExtent.height = ImageHeight;
    switch (ImageChannels)
    {
    case 1:
        Data.ImageFormat = vk::Format::eR8Unorm;
        break;
    case 2:
        Data.ImageFormat = vk::Format::eR8G8Unorm;
        break;
    case 3:
        Data.ImageFormat = vk::Format::eR8G8B8Unorm;
        break;
    case 4:
        Data.ImageFormat = vk::Format::eR8G8B8A8Unorm;
        break;
    default:
        NpgsCoreError("Failed to load image: Unknown image format.");
        return {};
    }

    Data.Data = std::move(std::vector<std::byte>(static_cast<std::byte*>(ImageData),
                          static_cast<std::byte*>(ImageData) + ImageWidth * ImageHeight * RequiredFormat.PixelSize));

    return Data;
}

_ASSET_END
_RUNTIME_END
_NPGS_END
