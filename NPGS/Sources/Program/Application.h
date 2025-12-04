#pragma once

#include <memory>
#include <string>
#include <vector>

#include <GLFW/glfw3.h>
#include <vulkan/vulkan.hpp>

#include "Engine/Core/Base/Base.h"
#include "Engine/Core/Runtime/Graphics/Vulkan/Context.h"
#include "Engine/Core/Runtime/Graphics/Vulkan/Resources.h"
#include "Engine/Core/Runtime/Graphics/Vulkan/Wrappers.h"
#include "Engine/Core/Runtime/Graphics/Vulkan/VulkanUIRenderer.h"
#define GLM_FORCE_ALIGNED_GENTYPES
#include "Engine/Core/System/Spatial/Camera.h"
#include "Engine/Core/System/UI/neural_ui.h" 


#include "Engine/Core/Types/Entries/Astro/Star.h"

#include <Engine/Core/System/Generators/StellarGenerator.h>
_NPGS_BEGIN

class FApplication
{
private:
    struct FRenderer
    {
        std::vector<Runtime::Graphics::FVulkanFramebuffer> Framebuffers;
        std::unique_ptr<Runtime::Graphics::FVulkanRenderPass> RenderPass;
    };

public:
    FApplication(const vk::Extent2D& WindowSize, const std::string& WindowTitle, bool bEnableVSync, bool bEnableFullscreen);
    ~FApplication();

    void ExecuteMainRender();
    void Terminate();

private:
    bool InitializeWindow();
    void InitializeInputCallbacks();
    void ProcessInput();
    void update();
    void RenderDebugUI();


    static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void CharCallback(GLFWwindow* window, unsigned int codepoint);
    static void FramebufferSizeCallback(GLFWwindow* Window, int Width, int Height);
    static void CursorPosCallback(GLFWwindow* Window, double PosX, double PosY);
    static void ScrollCallback(GLFWwindow* Window, double OffsetX, double OffsetY);

    void HandleMouseButton(int button, int action, int mods);
    void HandleKey(int key, int scancode, int action, int mods);
    void HandleChar(unsigned int codepoint);
    void HandleFramebufferSize(int width, int height);
    void HandleCursorPos(double posX, double posY);
    void HandleScroll(double offsetX, double offsetY); 





    void SimulateStarSelectionAndUpdateUI();
private:
    Runtime::Graphics::FVulkanContext*        _VulkanContext;

    std::string                               _WindowTitle;
    vk::Extent2D                              _WindowSize;
    GLFWwindow*                               _Window = nullptr;
    bool                                      _bEnableVSync;
    bool                                      _bEnableFullscreen;

    std::unique_ptr<Runtime::Graphics::FVulkanUIRenderer> _uiRenderer;
    std::shared_ptr<System::UI::UIRoot> m_ui_root; // 新增：唯一的UI根节点

    // =========================================================================
    std::shared_ptr<System::UI::CelestialInfoPanel> m_celestial_info;
    std::shared_ptr<System::UI::NeuralMenu> m_neural_menu_controller;
    std::shared_ptr<System::UI::PulsarButton> m_beam_button;
    std::shared_ptr<System::UI::PulsarButton> m_rkkv_button;
    std::shared_ptr<System::UI::CinematicInfoPanel> m_top_Info;
    std::shared_ptr<System::UI::CinematicInfoPanel> m_bottom_Info;
	std::shared_ptr<System::UI::TimeControlPanel> m_time_control_panel;
    // Data for the buttons to bind to
    std::string m_beam_energy;
    std::string m_rkkv_mass;

    // State trackers for the buttons
    bool m_is_beam_button_active;
    bool m_is_rkkv_button_active;


    std::unique_ptr<System::Spatial::FCamera> _FreeCamera;
    bool _bLeftMousePressedInWorld = false;
    bool _bIsRotatingCamera = false;
    double CurrentTime = 0.0;
    double RealityTime = 0.0;
    double GameTime = 0.0;
    double TimeRate = 1.0;
    double PreviousTime = glfwGetTime();
    double LastFrameTime = 0.0;
    int    FramePerSec= 0;
    long long int  FrameCount = 0;
    double                                    _DeltaTime   = 0.0;
    double                                    _DragStartX  = 0.0;
    double                                    _DragStartY  = 0.0;
    double                                    _LastX       = 0.0;
    double                                    _LastY       = 0.0;
    float _buffered_scroll_y = 0.0f;
    bool                                      _bFirstMouse = true;
};

_NPGS_END
