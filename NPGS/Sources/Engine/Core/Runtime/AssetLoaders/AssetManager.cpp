#include "AssetManager.h"

#include <cstdlib>
#include "Engine/Core/Base/Assert.h"

_NPGS_BEGIN
_RUNTIME_BEGIN
_ASSET_BEGIN

FAssetManager::~FAssetManager()
{
    ClearAssets();
}

FAssetManager* FAssetManager::GetInstance()
{
    static FAssetManager Instance;
    return &Instance;
}

_ASSET_END
_RUNTIME_END
_NPGS_END
