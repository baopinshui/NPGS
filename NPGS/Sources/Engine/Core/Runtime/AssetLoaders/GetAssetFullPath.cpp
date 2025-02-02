#include "GetAssetFullPath.h"

#include "Engine/Core/Base/Assert.h"

_NPGS_BEGIN
_RUNTIME_BEGIN
_ASSET_BEGIN

std::string GetAssetFullPath(EAssetType Type, const std::string& Filename)
{
    std::string RootFolderName = Type == EAssetType::kBinaryShader ? "" : "Assets/";
#ifdef _RELEASE
    RootFolderName = std::string("../") + RootFolderName;
#endif // _RELEASE

    auto AssetFolderName = [Type]() -> std::string
    {
        switch (Type)
        {
        case EAssetType::kBinaryShader:
            return "Cache/Shaders/";
        case EAssetType::kDataTable:
            return "DataTables/";
        case EAssetType::kFont:
            return "Fonts/";
        case EAssetType::kModel:
            return "Models/";
        case EAssetType::kShader:
            return "Shaders/";
        case EAssetType::kTexture:
            return "Textures/";
        default:
            NpgsAssert(false, "Invalid asset type");
            return "";
        }
    }();

    return RootFolderName + AssetFolderName + Filename;
}

_ASSET_END
_RUNTIME_END
_NPGS_END
