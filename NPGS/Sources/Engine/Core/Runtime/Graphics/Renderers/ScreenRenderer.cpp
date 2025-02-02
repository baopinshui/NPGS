#include "ScreenRenderer.h"

_NPGS_BEGIN
_RUNTIME_BEGIN
_GRAPHICS_BEGIN

FScreenRenderer::FScreenRenderer()
    : _VulkanContext(FVulkanContext::GetClassInstance())
{
}

vk::Result FScreenRenderer::CreateFramebuffers(vk::Extent2D Extent, vk::SampleCountFlags Flags)
{
    _VulkanContext->WaitIdle();
    _Framebuffers.clear();
    _Framebuffers.reserve(_VulkanContext->GetSwapchainImageCount());

    vk::FramebufferCreateInfo FramebufferCreateInfo = vk::FramebufferCreateInfo()
        .setRenderPass(**_RenderPass)
        .setAttachmentCount(1);

    return vk::Result();
}

vk::Result FScreenRenderer::DestroyFramebuffers()
{
    return vk::Result();
}

_GRAPHICS_END
_RUNTIME_END
_NPGS_END
