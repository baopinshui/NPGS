#include "Application.h"

#include <cmath>
#include <cstddef>
#include <cstdint>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Engine/Core/Runtime/AssetLoaders/AssetManager.h"
#include "Engine/Core/Runtime/AssetLoaders/Shader.h"
#include "Engine/Core/Runtime/AssetLoaders/Texture.h"
#include "Engine/Core/Runtime/Graphics/Vulkan/Buffers.h"
#include "Engine/Core/Runtime/Graphics/Vulkan/ShaderBufferManager.h"
#include "Engine/Utils/Logger.h"

_NPGS_BEGIN

namespace Art    = Runtime::Asset;
namespace Grt    = Runtime::Graphics;
namespace SysSpa = System::Spatial;

FApplication::FApplication(const vk::Extent2D& WindowSize, const std::string& WindowTitle,
                           bool bEnableVSync, bool bEnableFullscreen)
    :
    _VulkanContext(Grt::FVulkanContext::GetClassInstance()),
    _WindowTitle(WindowTitle),
    _WindowSize(WindowSize),
    _bEnableVSync(bEnableVSync),
    _bEnableFullscreen(bEnableFullscreen)
{
    if (!InitializeWindow())
    {
        NpgsCoreError("Failed to create application.");
    }
}

FApplication::~FApplication()
{
}

void FApplication::ExecuteMainRender()
{
    const std::uint32_t kFramesInFlightCount = 2;

    // Create screen renderer
    // ----------------------
    vk::AttachmentDescription AttachmentDescription = vk::AttachmentDescription()
        .setFormat(_VulkanContext->GetSwapchainCreateInfo().imageFormat)
        .setSamples(vk::SampleCountFlagBits::e1)
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eStore)
        .setInitialLayout(vk::ImageLayout::eUndefined)
        .setFinalLayout(vk::ImageLayout::ePresentSrcKHR);

    vk::AttachmentReference AttachmentReference(0, vk::ImageLayout::eColorAttachmentOptimal);

    vk::SubpassDescription SubpassDescription = vk::SubpassDescription()
        .setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
        .setColorAttachments(AttachmentReference);

    vk::RenderPassCreateInfo RenderPassCreateInfo({}, AttachmentDescription, SubpassDescription, {});
    _Renderer.RenderPass = std::make_unique<Grt::FVulkanRenderPass>(RenderPassCreateInfo);

    auto CreateFramebuffers = [this]() -> void
    {
        _VulkanContext->WaitIdle();
        _Renderer.Framebuffers.clear();
        _Renderer.Framebuffers.reserve(_VulkanContext->GetSwapchainImageCount());

        vk::FramebufferCreateInfo FramebufferCreateInfo = vk::FramebufferCreateInfo()
            .setRenderPass(**_Renderer.RenderPass)
            .setAttachmentCount(1)
            .setWidth(_WindowSize.width)
            .setHeight(_WindowSize.height)
            .setLayers(1);

        for (std::uint32_t i = 0; i != _VulkanContext->GetSwapchainImageCount(); ++i)
        {
            vk::ImageView Attachment = _VulkanContext->GetSwapchainImageView(i);
            FramebufferCreateInfo.setAttachments(Attachment);
            _Renderer.Framebuffers.push_back(FramebufferCreateInfo);
        }
    };

    auto DestroyFramebuffers = [this]() -> void
    {
        _VulkanContext->WaitIdle();
        _Renderer.Framebuffers.clear();
    };

    CreateFramebuffers();

    static bool bFramebufferCallbackAdded = false;
    if (!bFramebufferCallbackAdded)
    {
        _VulkanContext->RegisterAutoRemovedCallbacks(Grt::FVulkanContext::ECallbackType::kCreateSwapchain, "CreateFramebuffers", CreateFramebuffers);
        _VulkanContext->RegisterAutoRemovedCallbacks(Grt::FVulkanContext::ECallbackType::kDestroySwapchain, "DestroyFramebuffers", DestroyFramebuffers);
        bFramebufferCallbackAdded = true;
    }

    // Create pipeline layout
    // ----------------------
    struct FVertex
    {
        glm::vec3 Position;
        glm::vec2 TexCoord;
    };

    auto* AssetManager = Art::FAssetManager::GetInstance();

    AssetManager->AddAsset<Art::FTexture2D>("AwesomeFace", Art::FTexture2D("AwesomeFace.png", vk::Format::eR8G8B8A8Unorm, vk::Format::eR8G8B8A8Unorm, vk::ImageCreateFlagBits::eMutableFormat, true));
    
    vk::SamplerCreateInfo SamplerCreateInfo = Art::FTextureBase::CreateDefaultSamplerCreateInfo();
    Grt::FVulkanSampler Sampler(SamplerCreateInfo);

    Art::FShader::FResourceInfo ResInfo
    {
        {
            { 0, sizeof(FVertex), false }
        },
        {
            { 0, 0, offsetof(FVertex, Position) },
            { 0, 1, offsetof(FVertex, TexCoord) }
        },
        {
            { 0, 0, true }
        },
        //{
        //    { vk::ShaderStageFlagBits::eVertex, { "iOffset" }}
        //}
    };

    std::vector<std::string> TriangleShaders({ "Triangle.vert.spv", "Triangle.frag.spv" });
    AssetManager->AddAsset<Art::FShader>("Shader", TriangleShaders, ResInfo);

    auto& Shader  = *AssetManager->GetAsset<Art::FShader>("Shader");
    auto& Texture = *AssetManager->GetAsset<Art::FTexture2D>("AwesomeFace");

    vk::PipelineLayoutCreateInfo PipelineLayoutCreateInfo;
    auto NativeArray = Shader.GetDescriptorSetLayouts();
    PipelineLayoutCreateInfo.setSetLayouts(NativeArray);
    //auto PushConstantRanges = Shader.GetPushConstantRanges();
    //PipelineLayoutCreateInfo.setPushConstantRanges(PushConstantRanges);

    _PipelineLayout = std::make_unique<Grt::FVulkanPipelineLayout>(PipelineLayoutCreateInfo);

    // Test
    struct FMvpMatrices
    {
        glm::mat4x4 Model{ glm::mat4x4(1.0f) };
        glm::mat4x4 View{ glm::mat4x4(1.0f) };
        glm::mat4x4 Projection{ glm::mat4x4(1.0f) };
    } MvpMatrices;

    Grt::FShaderBufferManager::FUniformBufferCreateInfo MvpMatricesCreateInfo
    {
        .Name    = "MvpMatrices",
        .Fields  = { "Model", "View", "Projection" },
        .Set     = 0,
        .Binding = 0,
        .Usage   = vk::DescriptorType::eUniformBufferDynamic
    };

    auto ShaderBufferManager = Grt::FShaderBufferManager::GetInstance();
    ShaderBufferManager->CreateBuffer<FMvpMatrices>(MvpMatricesCreateInfo);

    const auto ModelUpdaters      = ShaderBufferManager->GetFieldUpdaters<glm::mat4x4>("MvpMatrices", "Model");
    const auto ViewUpdaters       = ShaderBufferManager->GetFieldUpdaters<glm::mat4x4>("MvpMatrices", "View");
    const auto ProjectionUpdaters = ShaderBufferManager->GetFieldUpdaters<glm::mat4x4>("MvpMatrices", "Projection");

    // Create graphics pipeline
    // ------------------------
    std::vector<glm::vec2> Offsets
    {
        glm::vec2( 0.0f, 0.5f),
        glm::vec2(-0.5f, 0.0f),
        glm::vec2( 0.5f, 0.0f)
    };

    //std::vector<glm::mat4x4> Matrices(3, glm::mat4x4(1.0f));

    //vk::DeviceSize MinUniformAlignment = _VulkanContext->GetPhysicalDeviceProperties().limits.minUniformBufferOffsetAlignment;
    //vk::DeviceSize UniformAlignment    = (sizeof(glm::mat4x4) + MinUniformAlignment - 1) & ~(MinUniformAlignment - 1);
    //std::vector<Grt::FDeviceLocalBuffer> UniformBuffers;
    //for (std::uint32_t i = 0; i != kFramesInFlightCount; ++i)
    //{
    //    UniformBuffers.emplace_back(Matrices.size() * UniformAlignment, vk::BufferUsageFlagBits::eUniformBuffer);
    //    UniformBuffers[i].CopyData(0, 3 * sizeof(glm::mat4), Matrices.data());
    //    UniformBuffers[i].EnablePersistentMapping();
    //}

    //for (std::uint32_t i = 0; i != kFramesInFlightCount; ++i)
    //{
    //    vk::DescriptorBufferInfo BufferInfo(*UniformBuffers[i].GetBuffer(), 0, Matrices.size() * UniformAlignment);
    //    Shader.WriteDynamicDescriptors<vk::DescriptorBufferInfo>(0, 0, i, vk::DescriptorType::eUniformBufferDynamic, { BufferInfo });
    //}

    vk::DescriptorImageInfo ImageInfo(*Sampler, *Texture.GetImageView(), vk::ImageLayout::eShaderReadOnlyOptimal);
    Shader.WriteSharedDescriptors<vk::DescriptorImageInfo>(0, 1, vk::DescriptorType::eCombinedImageSampler, { ImageInfo });

    ShaderBufferManager->BindShaderToBuffers("MvpMatrices", "Shader");

    std::vector<FVertex> Vertices
    {
        { { -0.5f, -0.5f, 0.0f }, { 0.0f, 0.0f } },
        { {  0.5f, -0.5f, 0.0f }, { 1.0f, 0.0f } },
        { { -0.5f,  0.5f, 0.0f }, { 0.0f, 1.0f } },
        { {  0.5f,  0.5f, 0.0f }, { 1.0f, 1.0f } }
    };

    std::vector<std::uint16_t> Indices
    {
        0, 1, 2,
        1, 2, 3
    };

    //std::vector<FVertex> Vertices
    //{
    //    // X轴 - 红色
    //    { { 0.0f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f } },
    //    { { 1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f } },

    //    // Y轴 - 绿色
    //    { { 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f } },
    //    { { 0.0f, 1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f } },

    //    // Z轴 - 蓝色
    //    { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f } },
    //    { { 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f } }
    //};

    //std::vector<std::uint16_t> Indices
    //{
    //    0, 1,  // X轴
    //    2, 3,  // Y轴
    //    4, 5   // Z轴
    //};

    Grt::FDeviceLocalBuffer VertexBuffer(Vertices.size() * sizeof(FVertex), vk::BufferUsageFlagBits::eVertexBuffer);
    VertexBuffer.CopyData(Vertices);
    Grt::FDeviceLocalBuffer IndexBuffer(Indices.size() * sizeof(std::uint16_t), vk::BufferUsageFlagBits::eIndexBuffer);
    IndexBuffer.CopyData(Indices);

    static auto ShaderStageCreateInfos = Shader.GetShaderStageCreateInfo();

    auto CreateGraphicsPipeline = [this, &Shader]() -> void
    {
        // 先创建新管线，再销毁旧管线
        Grt::FGraphicsPipelineCreateInfoPack CreateInfoPack;
        CreateInfoPack.GraphicsPipelineCreateInfo.setLayout(**_PipelineLayout);
        CreateInfoPack.GraphicsPipelineCreateInfo.setRenderPass(**_Renderer.RenderPass);

        CreateInfoPack.VertexInputBindings.append_range(Shader.GetVertexInputBindings());
        CreateInfoPack.VertexInputAttributes.append_range(Shader.GetVertexInputAttributes());

        CreateInfoPack.InputAssemblyStateCreateInfo.setTopology(vk::PrimitiveTopology::eTriangleList);
        CreateInfoPack.MultisampleStateCreateInfo.setRasterizationSamples(vk::SampleCountFlagBits::e1);

        CreateInfoPack.ShaderStages = ShaderStageCreateInfos;

        vk::PipelineColorBlendAttachmentState ColorBlendAttachmentState = vk::PipelineColorBlendAttachmentState()
            .setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                               vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);

        CreateInfoPack.Viewports.emplace_back(0.0f, static_cast<float>(_WindowSize.height),
                                              static_cast<float>(_WindowSize.width), -static_cast<float>(_WindowSize.height),
                                              0.0f, 1.0f);
        CreateInfoPack.Scissors.emplace_back(vk::Offset2D(), _WindowSize);
        CreateInfoPack.ColorBlendAttachmentStates.emplace_back(ColorBlendAttachmentState);

        CreateInfoPack.Update();

        _VulkanContext->WaitIdle();
        _GraphicsPipeline = std::make_unique<Grt::FVulkanPipeline>(CreateInfoPack);
    };

    auto DestroyGraphicsPipeline = [this]() -> void
    {
        if (_GraphicsPipeline->IsValid())
        {
            _VulkanContext->WaitIdle();
            _GraphicsPipeline.reset();
        }
    };

    CreateGraphicsPipeline();

    static bool bPipelineCallbackAdded = false;
    if (!bPipelineCallbackAdded)
    {
        _VulkanContext->RegisterAutoRemovedCallbacks(Grt::FVulkanContext::ECallbackType::kCreateSwapchain, "CreatePipeline", CreateGraphicsPipeline);
        _VulkanContext->RegisterAutoRemovedCallbacks(Grt::FVulkanContext::ECallbackType::kDestroySwapchain, "DestroyPipeline", DestroyGraphicsPipeline);
        bPipelineCallbackAdded = true;
    }

    const auto& [Framebuffers, RenderPass] = _Renderer;

    std::vector<Grt::FVulkanFence> InFlightFences;
    std::vector<Grt::FVulkanSemaphore> Semaphores_ImageAvailable;
    std::vector<Grt::FVulkanSemaphore> Semaphores_RenderFinished;
    std::vector<Grt::FVulkanCommandBuffer> CommandBuffers(kFramesInFlightCount);
    for (std::size_t i = 0; i != kFramesInFlightCount; ++i)
    {
        InFlightFences.emplace_back(vk::FenceCreateFlagBits::eSignaled);
        Semaphores_ImageAvailable.emplace_back(vk::SemaphoreCreateFlags());
        Semaphores_RenderFinished.emplace_back(vk::SemaphoreCreateFlags());
    }

    _VulkanContext->GetGraphicsCommandPool().AllocateBuffers(vk::CommandBufferLevel::ePrimary, CommandBuffers);

    vk::ClearValue ColorValue({ 0.0f, 0.0f, 0.0f, 0.0f });
    vk::DeviceSize Offset       = 0;
    std::uint32_t  CurrentFrame = 0;

    while (!glfwWindowShouldClose(_Window))
    {
        while (glfwGetWindowAttrib(_Window, GLFW_ICONIFIED))
        {
            glfwWaitEvents();
        }

        Offsets[0].x += static_cast<float>(0.0001f * std::sin(glfwGetTime()));
        Offsets[0].y += static_cast<float>(0.0001f * std::cos(glfwGetTime()));
        Offsets[1].x += static_cast<float>(0.0001f * std::cos(glfwGetTime()));
        Offsets[1].y += static_cast<float>(0.0001f * std::sin(glfwGetTime()));
        Offsets[2].x -= static_cast<float>(0.0001f * std::sin(glfwGetTime()));
        Offsets[2].y -= static_cast<float>(0.0001f * std::cos(glfwGetTime()));

        MvpMatrices.Model      = glm::rotate(MvpMatrices.Model, static_cast<float>(0.0001f * glfwGetTime()), glm::vec3(0.0f, 0.0f, 1.0f));
        MvpMatrices.View       = _FreeCamera->GetViewMatrix();
        MvpMatrices.Projection = _FreeCamera->GetProjectionMatrix(static_cast<float>(_WindowSize.width) / _WindowSize.height, 0.1f, 100.0f);

        ViewUpdaters[CurrentFrame] << MvpMatrices.View;
        ProjectionUpdaters[CurrentFrame] << MvpMatrices.Projection;

        InFlightFences[CurrentFrame].WaitAndReset();

        _VulkanContext->SwapImage(*Semaphores_ImageAvailable[CurrentFrame]);
        std::uint32_t ImageIndex = _VulkanContext->GetCurrentImageIndex();

        CommandBuffers[CurrentFrame].Begin(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
        RenderPass->CommandBegin(CommandBuffers[CurrentFrame], Framebuffers[ImageIndex], { {}, _WindowSize }, { ColorValue });
        CommandBuffers[CurrentFrame]->bindPipeline(vk::PipelineBindPoint::eGraphics, **_GraphicsPipeline);
        CommandBuffers[CurrentFrame]->bindVertexBuffers(0, *VertexBuffer.GetBuffer(), Offset);
        CommandBuffers[CurrentFrame]->bindIndexBuffer(*IndexBuffer.GetBuffer(), Offset, vk::IndexType::eUint16);
        std::uint32_t DynamicOffset = 0;// 0 * static_cast<std::uint32_t>(UniformAlignment);
        CommandBuffers[CurrentFrame]->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, **_PipelineLayout, 0, Shader.GetDescriptorSets()[CurrentFrame], DynamicOffset);
        CommandBuffers[CurrentFrame]->drawIndexed(6, 1, 0, 0, 0);
        RenderPass->CommandEnd(CommandBuffers[CurrentFrame]);
        CommandBuffers[CurrentFrame].End();

        _VulkanContext->SubmitCommandBufferToGraphics(*CommandBuffers[CurrentFrame], *Semaphores_ImageAvailable[CurrentFrame],
                                                      *Semaphores_RenderFinished[CurrentFrame], *InFlightFences[CurrentFrame]);
        _VulkanContext->PresentImage(*Semaphores_RenderFinished[CurrentFrame]);

        CurrentFrame = (CurrentFrame + 1) % kFramesInFlightCount;

        ProcessInput();
        glfwPollEvents();
        ShowTitleFps();
    }

    _VulkanContext->WaitIdle();
    _VulkanContext->GetGraphicsCommandPool().FreeBuffers(CommandBuffers);

    Terminate();
}

void FApplication::Terminate()
{
    _VulkanContext->WaitIdle();
    glfwDestroyWindow(_Window);
    glfwTerminate();
}

bool FApplication::InitializeWindow()
{
    if (glfwInit() == GLFW_FALSE)
    {
        NpgsCoreError("Failed to initialize GLFW.");
        return false;
    };

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    _Window = glfwCreateWindow(_WindowSize.width, _WindowSize.height, _WindowTitle.c_str(), nullptr, nullptr);
    if (_Window == nullptr)
    {
        NpgsCoreError("Failed to create GLFW window.");
        glfwTerminate();
        return false;
    }

    InitializeInputCallbacks();

    std::uint32_t ExtensionCount = 0;
    const char** Extensions = glfwGetRequiredInstanceExtensions(&ExtensionCount);
    if (Extensions == nullptr)
    {
        NpgsCoreError("Failed to get required instance extensions.");
        glfwDestroyWindow(_Window);
        glfwTerminate();
        return false;
    }

    for (std::uint32_t i = 0; i != ExtensionCount; ++i)
    {
        _VulkanContext->AddInstanceExtension(Extensions[i]);
    }

    _VulkanContext->AddDeviceExtension(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

    vk::Result Result;
    if ((Result = _VulkanContext->CreateInstance()) != vk::Result::eSuccess)
    {
        glfwDestroyWindow(_Window);
        glfwTerminate();
        return false;
    }

    vk::SurfaceKHR Surface;
    if (glfwCreateWindowSurface(_VulkanContext->GetInstance(), _Window, nullptr, reinterpret_cast<VkSurfaceKHR*>(&Surface)) != VK_SUCCESS)
    {
        NpgsCoreError("Failed to create window surface.");
        glfwDestroyWindow(_Window);
        glfwTerminate();
        return false;
    }
    _VulkanContext->SetSurface(Surface);

    if (_VulkanContext->CreateDevice(0) != vk::Result::eSuccess ||
        _VulkanContext->CreateSwapchain(_WindowSize, false) != vk::Result::eSuccess)
    {
        return false;
    }

    _FreeCamera = std::make_unique<SysSpa::FCamera>(glm::vec3(0.0f, 0.0f, 3.0f));

    return true;
}

void FApplication::InitializeInputCallbacks()
{
    glfwSetWindowUserPointer(_Window, this);
    glfwSetFramebufferSizeCallback(_Window, &FApplication::FramebufferSizeCallback);
    glfwSetScrollCallback(_Window, nullptr);
    glfwSetScrollCallback(_Window, &FApplication::ScrollCallback);
}

void FApplication::ShowTitleFps()
{
    static double CurrentTime   = 0.0;
    static double PreviousTime  = glfwGetTime();
    static double LastFrameTime = 0.0;
    static int    FrameCount    = 0;

    CurrentTime   = glfwGetTime();
    _DeltaTime    = CurrentTime - LastFrameTime;
    LastFrameTime = CurrentTime;
    ++FrameCount;
    if (CurrentTime - PreviousTime >= 1.0)
    {
        glfwSetWindowTitle(_Window, (std::string(_WindowTitle) + " " + std::to_string(FrameCount)).c_str());
        FrameCount   = 0;
        PreviousTime = CurrentTime;
    }
}

void FApplication::ProcessInput()
{
    if (glfwGetKey(_Window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(_Window, GLFW_TRUE);
    }

    if (glfwGetMouseButton(_Window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
    {
        glfwSetCursorPosCallback(_Window, &FApplication::CursorPosCallback);
        glfwSetInputMode(_Window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    }

    if (glfwGetMouseButton(_Window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_RELEASE)
    {
        glfwSetCursorPosCallback(_Window, nullptr);
        glfwSetInputMode(_Window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        _bFirstMouse = true;
    }

    if (glfwGetKey(_Window, GLFW_KEY_W) == GLFW_PRESS)
        _FreeCamera->ProcessKeyboard(SysSpa::FCamera::EMovement::kForward, _DeltaTime);
    if (glfwGetKey(_Window, GLFW_KEY_S) == GLFW_PRESS)
        _FreeCamera->ProcessKeyboard(SysSpa::FCamera::EMovement::kBack, _DeltaTime);
    if (glfwGetKey(_Window, GLFW_KEY_A) == GLFW_PRESS)
        _FreeCamera->ProcessKeyboard(SysSpa::FCamera::EMovement::kLeft, _DeltaTime);
    if (glfwGetKey(_Window, GLFW_KEY_D) == GLFW_PRESS)
        _FreeCamera->ProcessKeyboard(SysSpa::FCamera::EMovement::kRight, _DeltaTime);
    if (glfwGetKey(_Window, GLFW_KEY_R) == GLFW_PRESS)
        _FreeCamera->ProcessKeyboard(SysSpa::FCamera::EMovement::kUp, _DeltaTime);
    if (glfwGetKey(_Window, GLFW_KEY_F) == GLFW_PRESS)
        _FreeCamera->ProcessKeyboard(SysSpa::FCamera::EMovement::kDown, _DeltaTime);
    if (glfwGetKey(_Window, GLFW_KEY_Q) == GLFW_PRESS)
        _FreeCamera->ProcessKeyboard(SysSpa::FCamera::EMovement::kRollLeft, _DeltaTime);
    if (glfwGetKey(_Window, GLFW_KEY_E) == GLFW_PRESS)
        _FreeCamera->ProcessKeyboard(SysSpa::FCamera::EMovement::kRollRight, _DeltaTime);
}

void FApplication::FramebufferSizeCallback(GLFWwindow* Window, int Width, int Height)
{
    auto* App = reinterpret_cast<FApplication*>(glfwGetWindowUserPointer(Window));

    if (Width == 0 || Height == 0)
    {
        return;
    }

    App->_WindowSize.width  = Width;
    App->_WindowSize.height = Height;
    App->_VulkanContext->WaitIdle();
    App->_VulkanContext->RecreateSwapchain();
}

void FApplication::CursorPosCallback(GLFWwindow* Window, double PosX, double PosY)
{
    auto* App = static_cast<FApplication*>(glfwGetWindowUserPointer(Window));

    if (App->_bFirstMouse)
    {
        App->_LastX       = PosX;
        App->_LastY       = PosY;
        App->_bFirstMouse = false;
    }

    double OffsetX = PosX - App->_LastX;
    double OffsetY = App->_LastY - PosY;
    App->_LastX = PosX;
    App->_LastY = PosY;

    App->_FreeCamera->ProcessMouseMovement(OffsetX, OffsetY);
}

void FApplication::ScrollCallback(GLFWwindow* Window, double OffsetX, double OffsetY)
{
    auto* App = static_cast<FApplication*>(glfwGetWindowUserPointer(Window));
    App->_FreeCamera->ProcessMouseScroll(OffsetY);
}

_NPGS_END
