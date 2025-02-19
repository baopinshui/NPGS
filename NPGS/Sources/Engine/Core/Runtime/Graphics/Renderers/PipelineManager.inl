#include "PipelineManager.h"

_NPGS_BEGIN
_RUNTIME_BEGIN
_GRAPHICS_BEGIN

NPGS_INLINE vk::PipelineLayout FPipelineManager::GetPipelineLayout(const std::string& Name) const
{
    return *_PipelineLayouts.at(Name);
}

NPGS_INLINE vk::Pipeline FPipelineManager::GetPipeline(const std::string& Name) const
{
    return *_Pipelines.at(Name);
}

_GRAPHICS_END
_RUNTIME_END
_NPGS_END
