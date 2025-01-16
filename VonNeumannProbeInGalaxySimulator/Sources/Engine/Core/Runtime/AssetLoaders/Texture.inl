#include "Texture.h"

_NPGS_BEGIN
_RUNTIME_BEGIN
_ASSET_BEGIN

NPGS_INLINE const FTextureBase::FImageData& FTextureBase::GetImageData() const
{
    return _ImageData;
}

_ASSET_END
_RUNTIME_END
_NPGS_END
