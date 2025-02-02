#include "ScreenRenderer.h"

_NPGS_BEGIN
_RUNTIME_BEGIN
_GRAPHICS_BEGIN

NPGS_INLINE vk::Result FScreenRenderer::CreateRenderPass(const vk::RenderPassCreateInfo& CreateInfo)
{
    _RenderPass = std::make_unique<FVulkanRenderPass>(CreateInfo);
    return _RenderPass->GetStatus();
}

NPGS_INLINE vk::Result FScreenRenderer::CreatePipelineLayout(const vk::PipelineLayoutCreateInfo& CreateInfo)
{
    _PipelineLayout = std::make_unique<FVulkanPipelineLayout>(CreateInfo);
    return _PipelineLayout->GetStatus();
}

NPGS_INLINE vk::Result FScreenRenderer::CreateGraphicsPipeline(const vk::GraphicsPipelineCreateInfo& CreateInfo)
{
    _GraphicsPipeline = std::make_unique<FVulkanPipeline>(CreateInfo);
    return _GraphicsPipeline->GetStatus();
}

NPGS_INLINE std::vector<FVulkanFramebuffer>& FScreenRenderer::FramebufferData()
{
    return _Framebuffers;
}

NPGS_INLINE const FVulkanPipelineLayout& FScreenRenderer::GetPipelineLayout() const
{
    return *_PipelineLayout;
}

NPGS_INLINE const FVulkanPipeline& FScreenRenderer::GetGraphicsPipeline() const
{
    return *_GraphicsPipeline;
}

_GRAPHICS_END
_RUNTIME_END
_NPGS_END
