#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <utility>

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>

#include "Engine/Core/Base/Base.h"
#include "Engine/Core/Runtime/Graphics/Renderers/ScreenRenderer.h"
#include "Engine/Core/Runtime/Graphics/Vulkan/Context.h"
#include "Engine/Core/Runtime/Graphics/Vulkan/Wrappers.h"

_NPGS_BEGIN

class FApplication
{
private:
    struct FRenderer
    {
        std::vector<Runtime::Graphics::FVulkanFramebuffer> Framebuffers;
        std::unique_ptr<Runtime::Graphics::FVulkanRenderPass> RenderPass;
    };

    struct FVertex
    {
        glm::vec2 Position;
        glm::vec4 Color;
    };

public:
    FApplication(const vk::Extent2D& WindowSize, const std::string& WindowTitle, bool bEnableVSync, bool bEnableFullscreen);
    ~FApplication();

    void ExecuteMainRender();
    void Terminate();

private:
    bool InitializeWindow();
    void BootScreen(const std::string& Filename, vk::Format ImageFormat);
    void ShowTitleFps();
    void ProcessInput();

    static void FramebufferSizeCallback(GLFWwindow* Window, int Width, int Height);

private:
    Runtime::Graphics::FVulkanContext*                        _VulkanContext;
    std::unique_ptr<Runtime::Graphics::FVulkanPipelineLayout> _PipelineLayout;
    std::unique_ptr<Runtime::Graphics::FVulkanPipeline>       _GraphicsPipeline;
    FRenderer                                                 _Renderer;

    std::string  _WindowTitle;
    vk::Extent2D _WindowSize;
    GLFWwindow*  _Window;
    bool         _bEnableVSync;
    bool         _bEnableFullscreen;
};

_NPGS_END
