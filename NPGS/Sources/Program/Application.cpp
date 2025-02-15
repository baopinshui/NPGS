#include "Application.h"

#include <cmath>
#include <cstddef>
#include <cstdint>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Engine/Core/Base/Config/EngineConfig.h"
#include "Engine/Core/Runtime/AssetLoaders/AssetManager.h"
#include "Engine/Core/Runtime/AssetLoaders/Shader.h"
#include "Engine/Core/Runtime/AssetLoaders/Texture.h"
#include "Engine/Core/Runtime/Graphics/Vulkan/ShaderResourceManager.h"
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
    std::unique_ptr<Runtime::Graphics::FVulkanPipelineLayout>   PipelineLayout;
    std::unique_ptr<Runtime::Graphics::FVulkanPipeline>         GraphicsPipeline;
    std::unique_ptr<Runtime::Graphics::FVulkanPipeline>         GraphicsPipeline2;
    std::unique_ptr<Runtime::Graphics::FColorAttachment>        ColorAttachment;
    std::unique_ptr<Runtime::Graphics::FDepthStencilAttachment> DepthStencilAttachment;

    auto CreateFramebuffers = [&]() -> void
    {
        _VulkanContext->WaitIdle();

        ColorAttachment = std::make_unique<Grt::FColorAttachment>(
            _VulkanContext->GetSwapchainCreateInfo().imageFormat, _WindowSize, 1, vk::SampleCountFlagBits::e1);

        DepthStencilAttachment = std::make_unique<Grt::FDepthStencilAttachment>(
            vk::Format::eD32SfloatS8Uint, _WindowSize, 1, vk::SampleCountFlagBits::e1);
    };

    auto DestroyFramebuffers = [&]() -> void
    {
        _VulkanContext->WaitIdle();
    };

    CreateFramebuffers();

    // Dynamic rendering test
    // ----------------------
    vk::RenderingAttachmentInfo ColorAttachmentInfo = vk::RenderingAttachmentInfo()
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eStore)
        .setClearValue(vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f));

    vk::RenderingAttachmentInfo DepthStencilAttachmentInfo = vk::RenderingAttachmentInfo()
        .setImageLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eStore)
        .setClearValue(vk::ClearDepthStencilValue(1.0f, 0));

    vk::RenderingAttachmentInfo ResolveAttachmentInfo = vk::RenderingAttachmentInfo()
        .setImageLayout(vk::ImageLayout::ePresentSrcKHR)
        .setLoadOp(vk::AttachmentLoadOp::eDontCare)
        .setStoreOp(vk::AttachmentStoreOp::eStore);

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

    Art::FShader::FResourceInfo ResourceInfo
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
        {
            { vk::ShaderStageFlagBits::eVertex, { "iModel" } }
        }
    };

    std::vector<std::string> BasicLightingShaderFiles({ "BasicLighting.vert.spv", "BasicLighting.frag.spv" });
    std::vector<std::string> LampShaderFiles({ "BasicLighting.vert.spv", "BasicLighting_Lamp.frag.spv" });
    AssetManager->AddAsset<Art::FShader>("BasicLightingShader", BasicLightingShaderFiles, ResourceInfo);
    AssetManager->AddAsset<Art::FShader>("LampShader", LampShaderFiles, ResourceInfo);
    AssetManager->AddAsset<Art::FTexture2D>("ContainerDiffuse", "ContainerDiffuse.png", vk::Format::eR8G8B8A8Unorm, vk::Format::eR8G8B8A8Unorm, vk::ImageCreateFlagBits::eMutableFormat, true);

    auto* BasicLightingShader = AssetManager->GetAsset<Art::FShader>("BasicLightingShader");
    auto* LampShader          = AssetManager->GetAsset<Art::FShader>("LampShader");
    auto* ContainerDiffuse    = AssetManager->GetAsset<Art::FTexture2D>("ContainerDiffuse");

    vk::PipelineLayoutCreateInfo PipelineLayoutCreateInfo;
    auto NativeArray = BasicLightingShader->GetDescriptorSetLayouts();
    PipelineLayoutCreateInfo.setSetLayouts(NativeArray);
    auto PushConstantRanges = BasicLightingShader->GetPushConstantRanges();
    PipelineLayoutCreateInfo.setPushConstantRanges(PushConstantRanges);

    PipelineLayout = std::make_unique<Grt::FVulkanPipelineLayout>(PipelineLayoutCreateInfo);

    // Test
    struct FVpMatrices
    {
        glm::mat4x4 View{ glm::mat4x4(1.0f) };
        glm::mat4x4 Projection{ glm::mat4x4(1.0f) };
    } VpMatrices;

    Grt::FShaderResourceManager::FUniformBufferCreateInfo VpMatricesCreateInfo
    {
        .Name    = "VpMatrices",
        .Fields  = { "View", "Projection" },
        .Set     = 0,
        .Binding = 0,
        .Usage   = vk::DescriptorType::eUniformBufferDynamic
    };

    auto ShaderResourceManager = Grt::FShaderResourceManager::GetInstance();
    ShaderResourceManager->CreateBuffer<FVpMatrices>(VpMatricesCreateInfo);

    const auto ViewUpdaters       = ShaderResourceManager->GetFieldUpdaters<glm::mat4x4>("VpMatrices", "View");
    const auto ProjectionUpdaters = ShaderResourceManager->GetFieldUpdaters<glm::mat4x4>("VpMatrices", "Projection");

    // Create graphics pipeline
    // ------------------------
    vk::SamplerCreateInfo SamplerCreateInfo = Art::FTextureBase::CreateDefaultSamplerCreateInfo();
    Grt::FVulkanSampler Sampler(SamplerCreateInfo);

    std::vector<vk::DescriptorImageInfo> ImageInfos;
    ImageInfos.push_back(ContainerDiffuse->CreateDescriptorImageInfo(Sampler));
    BasicLightingShader->WriteSharedDescriptors(1, 0, vk::DescriptorType::eCombinedImageSampler, ImageInfos);

    ShaderResourceManager->BindShaderToBuffers("VpMatrices", "BasicLightingShader");

#include "Vertices.inc"

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

    Grt::FDeviceLocalBuffer VertexBuffer(CubeVertices.size() * sizeof(FVertex), vk::BufferUsageFlagBits::eVertexBuffer);
    VertexBuffer.CopyData(CubeVertices);
    Grt::FDeviceLocalBuffer IndexBuffer(Indices.size() * sizeof(std::uint16_t), vk::BufferUsageFlagBits::eIndexBuffer);
    IndexBuffer.CopyData(Indices);

    static auto BasicLightingShaderStageCreateInfos = BasicLightingShader->CreateShaderStageCreateInfo();
    static auto LampShaderStageCreateInfos = LampShader->CreateShaderStageCreateInfo();

    auto CreateGraphicsPipeline = [&]() -> void
    {
        vk::PipelineRenderingCreateInfo PipelineRenderingCreateInfo = vk::PipelineRenderingCreateInfo()
            .setColorAttachmentCount(1)
            .setColorAttachmentFormats(_VulkanContext->GetSwapchainCreateInfo().imageFormat)
            .setDepthAttachmentFormat(vk::Format::eD32SfloatS8Uint);

        // 先创建新管线，再销毁旧管线
        Grt::FGraphicsPipelineCreateInfoPack CreateInfoPack;
        CreateInfoPack.GraphicsPipelineCreateInfo.setPNext(&PipelineRenderingCreateInfo);
        CreateInfoPack.GraphicsPipelineCreateInfo.setLayout(**PipelineLayout);
        CreateInfoPack.VertexInputBindings.append_range(BasicLightingShader->GetVertexInputBindings());
        CreateInfoPack.VertexInputAttributes.append_range(BasicLightingShader->GetVertexInputAttributes());
        CreateInfoPack.InputAssemblyStateCreateInfo.setTopology(vk::PrimitiveTopology::eTriangleList);
        //CreateInfoPack.MultisampleStateCreateInfo.setRasterizationSamples(vk::SampleCountFlagBits::e8)
        //                                         .setSampleShadingEnable(vk::True)
        //                                         .setMinSampleShading(1.0f);
        CreateInfoPack.DepthStencilStateCreateInfo.setDepthTestEnable(vk::True)
                                                  .setDepthWriteEnable(vk::True)
                                                  .setDepthCompareOp(vk::CompareOp::eLess)
                                                  .setDepthBoundsTestEnable(vk::False)
                                                  .setStencilTestEnable(vk::False);

        CreateInfoPack.ShaderStages = BasicLightingShaderStageCreateInfos;

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
        GraphicsPipeline = std::make_unique<Grt::FVulkanPipeline>(CreateInfoPack);
    
        CreateInfoPack.ShaderStages = LampShaderStageCreateInfos;
        GraphicsPipeline2 = std::make_unique<Grt::FVulkanPipeline>(CreateInfoPack);
    };

    auto DestroyGraphicsPipeline = [&]() -> void
    {
        if (GraphicsPipeline->IsValid())
        {
            _VulkanContext->WaitIdle();
            GraphicsPipeline.reset();
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

    std::vector<Grt::FVulkanFence> InFlightFences;
    std::vector<Grt::FVulkanSemaphore> Semaphores_ImageAvailable;
    std::vector<Grt::FVulkanSemaphore> Semaphores_RenderFinished;
    std::vector<Grt::FVulkanCommandBuffer> CommandBuffers(Config::Graphics::kMaxFrameInFlight);
    for (std::size_t i = 0; i != Config::Graphics::kMaxFrameInFlight; ++i)
    {
        InFlightFences.emplace_back(vk::FenceCreateFlagBits::eSignaled);
        Semaphores_ImageAvailable.emplace_back(vk::SemaphoreCreateFlags());
        Semaphores_RenderFinished.emplace_back(vk::SemaphoreCreateFlags());
    }

    _VulkanContext->GetGraphicsCommandPool().AllocateBuffers(vk::CommandBufferLevel::ePrimary, CommandBuffers);

    vk::DeviceSize Offset        = 0;
    std::uint32_t  DynamicOffset = 0;
    std::uint32_t  CurrentFrame  = 0;

    while (!glfwWindowShouldClose(_Window))
    {
        while (glfwGetWindowAttrib(_Window, GLFW_ICONIFIED))
        {
            glfwWaitEvents();
        }

        InFlightFences[CurrentFrame].WaitAndReset();

        glm::mat4x4 Model(1.0f);
        VpMatrices.View       = _FreeCamera->GetViewMatrix();
        VpMatrices.Projection = _FreeCamera->GetProjectionMatrix(static_cast<float>(_WindowSize.width) / _WindowSize.height, 0.1f);

        ShaderResourceManager->UpdateEntrieBuffer(CurrentFrame, "VpMatrices", VpMatrices);

        auto& CurrentBuffer = CommandBuffers[CurrentFrame];

        _VulkanContext->SwapImage(*Semaphores_ImageAvailable[CurrentFrame]);
        std::uint32_t ImageIndex = _VulkanContext->GetCurrentImageIndex();

        CurrentBuffer.Begin(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

        vk::ImageSubresourceRange TransitionRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);
        vk::ImageMemoryBarrier2 InitialTransitionBarrier(vk::PipelineStageFlagBits2::eTopOfPipe,
                                                         vk::AccessFlagBits2::eNone,
                                                         vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                                                         vk::AccessFlagBits2::eColorAttachmentWrite,
                                                         vk::ImageLayout::eUndefined,
                                                         vk::ImageLayout::eColorAttachmentOptimal,
                                                         vk::QueueFamilyIgnored,
                                                         vk::QueueFamilyIgnored,
                                                         _VulkanContext->GetSwapchainImage(ImageIndex),
                                                         TransitionRange);

        vk::DependencyInfo InitialDependencyInfo = vk::DependencyInfo()
            .setImageMemoryBarriers(InitialTransitionBarrier);

        CurrentBuffer->pipelineBarrier2(InitialDependencyInfo);

        ColorAttachmentInfo.setImageView(_VulkanContext->GetSwapchainImageView(ImageIndex));
        DepthStencilAttachmentInfo.setImageView(*DepthStencilAttachment->GetImageView());

        vk::RenderingInfo RenderingInfo = vk::RenderingInfo()
            .setRenderArea(vk::Rect2D({ 0, 0 }, _WindowSize))
            .setLayerCount(1)
            .setColorAttachments(ColorAttachmentInfo)
            .setPDepthAttachment(&DepthStencilAttachmentInfo);

        CurrentBuffer->beginRendering(RenderingInfo);
        CurrentBuffer->bindPipeline(vk::PipelineBindPoint::eGraphics, **GraphicsPipeline);
        CurrentBuffer->bindVertexBuffers(0, *VertexBuffer.GetBuffer(), Offset);
        CurrentBuffer->pushConstants(**PipelineLayout, vk::ShaderStageFlagBits::eVertex, BasicLightingShader->GetPushConstantOffset("iModel"), sizeof(glm::mat4x4), glm::value_ptr(Model));
        CurrentBuffer->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, **PipelineLayout, 0, BasicLightingShader->GetDescriptorSets(CurrentFrame), DynamicOffset);
        CurrentBuffer->draw(36, 1, 0, 0);

        Model = glm::translate(Model, glm::vec3(1.0f, 0.0f, 0.0f));

        CurrentBuffer->bindPipeline(vk::PipelineBindPoint::eGraphics, **GraphicsPipeline2);
        CurrentBuffer->pushConstants(**PipelineLayout, vk::ShaderStageFlagBits::eVertex, BasicLightingShader->GetPushConstantOffset("iModel"), sizeof(glm::mat4x4), glm::value_ptr(Model));
        CurrentBuffer->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, **PipelineLayout, 0, BasicLightingShader->GetDescriptorSets(CurrentFrame), DynamicOffset);
        CurrentBuffer->draw(36, 1, 0, 0);
        CurrentBuffer->endRendering();

        vk::ImageMemoryBarrier2 FinalTransitionBarrier(vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                                                       vk::AccessFlagBits2::eColorAttachmentWrite,
                                                       vk::PipelineStageFlagBits2::eBottomOfPipe,
                                                       vk::AccessFlagBits2::eNone,
                                                       vk::ImageLayout::eColorAttachmentOptimal,
                                                       vk::ImageLayout::ePresentSrcKHR,
                                                       vk::QueueFamilyIgnored,
                                                       vk::QueueFamilyIgnored,
                                                       _VulkanContext->GetSwapchainImage(ImageIndex),
                                                       TransitionRange);

        vk::DependencyInfo FinalDependencyInfo = vk::DependencyInfo()
            .setImageMemoryBarriers(FinalTransitionBarrier);

        CurrentBuffer->pipelineBarrier2(FinalDependencyInfo);
        CurrentBuffer.End();

        _VulkanContext->SubmitCommandBufferToGraphics(*CurrentBuffer, *Semaphores_ImageAvailable[CurrentFrame],
                                                      *Semaphores_RenderFinished[CurrentFrame], *InFlightFences[CurrentFrame]);
        _VulkanContext->PresentImage(*Semaphores_RenderFinished[CurrentFrame]);

        CurrentFrame = (CurrentFrame + 1) % Config::Graphics::kMaxFrameInFlight;

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
        _VulkanContext->CreateSwapchain(_WindowSize, _bEnableVSync) != vk::Result::eSuccess)
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
