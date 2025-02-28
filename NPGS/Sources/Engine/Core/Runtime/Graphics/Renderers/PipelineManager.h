#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include <vulkan/vulkan_handles.hpp>

#include "Engine/Core/Base/Base.h"
#include "Engine/Core/Runtime/Graphics/Vulkan/Wrappers.h"

_NPGS_BEGIN
_RUNTIME_BEGIN
_GRAPHICS_BEGIN

class FPipelineManager
{
private:
    enum class EPipelineType
    {
        kGraphics,
        kCompute
    };

public:
    void CreatePipeline(const std::string& PipelineName, const std::string& ShaderName,
                        FGraphicsPipelineCreateInfoPack& GraphicsPipelineCreateInfoPack);

    void CreatePipeline(const std::string& PipelineName, const std::string& ShaderName,
                        vk::ComputePipelineCreateInfo* ComputePipelineCreateInfo = nullptr);

    void RemovePipeline(const std::string& Name);
    vk::PipelineLayout GetPipelineLayout(const std::string& Name) const;
    vk::Pipeline GetPipeline(const std::string& Name) const;

    static FPipelineManager* GetInstance();

private:
    FPipelineManager()                        = default;
    FPipelineManager(const FPipelineManager&) = delete;
    FPipelineManager(FPipelineManager&&)      = delete;
    ~FPipelineManager()                       = default;

    FPipelineManager& operator=(const FPipelineManager&) = delete;
    FPipelineManager& operator=(FPipelineManager&&)      = delete;

    void RegisterCallback(const std::string& Name, EPipelineType Type);

private:
    std::unordered_map<std::string, FGraphicsPipelineCreateInfoPack> _GraphicsPipelineCreateInfoPacks;
    std::unordered_map<std::string, vk::ComputePipelineCreateInfo>   _ComputePipelineCreateInfos;
    std::unordered_map<std::string, FVulkanPipelineLayout>           _PipelineLayouts;
    std::unordered_map<std::string, FVulkanPipeline>                 _Pipelines;
};

_GRAPHICS_END
_RUNTIME_END
_NPGS_END

#include "PipelineManager.inl"
