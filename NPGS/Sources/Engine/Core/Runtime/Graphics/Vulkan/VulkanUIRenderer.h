// VulkanUIRenderer.h
#pragma once

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include "Engine/Core/Runtime/Graphics/Vulkan/Context.h"
#include "Engine/Core/Runtime/Graphics/Vulkan/Resources.h"

_NPGS_BEGIN
_RUNTIME_BEGIN
_GRAPHICS_BEGIN

class FVulkanUIRenderer
{
public:
    FVulkanUIRenderer();
    ~FVulkanUIRenderer();

    bool Initialize(GLFWwindow* window);
    void Shutdown();

    void BeginFrame();
    void EndFrame();
    void Render(vk::CommandBuffer commandBuffer);

    void Resize(uint32_t width, uint32_t height);

    ImTextureID AddTexture(vk::Sampler sampler, vk::ImageView imageView, vk::ImageLayout layout);
private:
    void CreateDescriptorPool();
    void CleanupDescriptorPool();

private:
    bool _initialized = false;
    vk::DescriptorPool _descriptorPool;
    FVulkanContext* _context = nullptr;
};

_GRAPHICS_END
_RUNTIME_END
_NPGS_END