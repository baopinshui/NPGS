#pragma once

#include <cstdint>
#include <memory>
#include <vector>
#include <utility>

#include <vulkan/vulkan_handles.hpp>

#include "Engine/Core/Runtime/Graphics/Vulkan/Wrappers.h"
#include "Engine/Core/Runtime/Graphics/Vulkan/Context.h"
#include "Engine/Core/Base/Base.h"

_NPGS_BEGIN
_RUNTIME_BEGIN
_GRAPHICS_BEGIN

class FScreenRenderer
{
public:
    FScreenRenderer();
    ~FScreenRenderer() = default;

    vk::Result CreateRenderPass(const vk::RenderPassCreateInfo& CreateInfo);
    vk::Result CreateFramebuffers(vk::Extent2D Extent, vk::SampleCountFlags Flags);
    vk::Result CreatePipelineLayout(const vk::PipelineLayoutCreateInfo& CreateInfo);
    vk::Result CreateGraphicsPipeline(const vk::GraphicsPipelineCreateInfo& CreateInfo);
    vk::Result DestroyFramebuffers();

    std::vector<FVulkanFramebuffer>& FramebufferData();

    const FVulkanPipelineLayout& GetPipelineLayout() const;
    const FVulkanPipeline& GetGraphicsPipeline() const;

private:
    FVulkanContext* _VulkanContext;

    std::vector<FVulkanFramebuffer>        _Framebuffers;
    std::unique_ptr<FVulkanRenderPass>     _RenderPass;
    std::unique_ptr<FVulkanPipelineLayout> _PipelineLayout;
    std::unique_ptr<FVulkanPipeline>       _GraphicsPipeline;
};

_GRAPHICS_END
_RUNTIME_END
_NPGS_END

#include "ScreenRenderer.inl"
