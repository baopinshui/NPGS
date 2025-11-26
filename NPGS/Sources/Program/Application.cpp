#include "Application.h"

#include <cmath>
#include <cstddef>
#include <cstdint>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_aligned.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Engine/Core/Base/Config/EngineConfig.h"
#include "Engine/Core/Math/NumericConstants.h"
#include "Engine/Core/Runtime/AssetLoaders/AssetManager.h"
#include "Engine/Core/Runtime/AssetLoaders/Shader.h"
#include "Engine/Core/Runtime/AssetLoaders/Texture.h"
#include "Engine/Core/Runtime/Graphics/Renderers/PipelineManager.h"
#include "Engine/Core/Runtime/Graphics/Vulkan/ShaderResourceManager.h"
#include "Engine/Utils/Logger.h"
#include "Engine/Utils/TooolfuncForStarMap.h"
#include "DataStructures.h"

_NPGS_BEGIN

namespace Art = Runtime::Asset;
namespace Grt = Runtime::Graphics;
namespace SysSpa = System::Spatial;
namespace UI = Npgs::System::UI;
FApplication::FApplication(const vk::Extent2D& WindowSize, const std::string& WindowTitle,
    bool bEnableVSync, bool bEnableFullscreen)
    :
    _VulkanContext(Grt::FVulkanContext::GetClassInstance()),
    _WindowTitle(WindowTitle),
    _WindowSize(WindowSize),
    _bEnableVSync(bEnableVSync),
    _bEnableFullscreen(bEnableFullscreen),
    m_beam_energy("1.919 E+30"),
    m_rkkv_mass("5.14 E+13"),
    m_is_beam_button_active(false),
    m_is_rkkv_button_active(false)
{
    if (!InitializeWindow())
    {
        NpgsCoreError("Failed to create application.");
    }
}

FApplication::~FApplication()
{}

void FApplication::ExecuteMainRender()
{    // 初始化 UI 渲染器
    _uiRenderer = std::make_unique<Grt::FVulkanUIRenderer>();
    // =========================================================================
    // [修改] UI 初始化逻辑
    // =========================================================================

    if (!_uiRenderer->Initialize(_Window))
    {
        NpgsCoreError("Failed to initialize UI renderer");
        return;
    }
   // using namespace Npgs::System::UI::;

    // 1. 设置自定义主题 (可以在这里或者在构造函数中完成)
    auto& theme = UI::UIContext::Get().m_theme;

    auto& ctx = UI::UIContext::Get();
    //theme.color_accent = ImVec4(0.745f, 0.745f, 0.561f, 1.0f); // #BEBE8F
    //theme.color_panel_bg = ImVec4(0.0f, 0.0f, 0.0f, 0.5f);

    // 2. 创建唯一的 UI 根
    m_ui_root = std::make_shared<UI::UIRoot>();

    // 3. 创建 NeuralMenuController
    m_neural_menu_controller = std::make_shared<UI::NeuralMenuController>();

    // 4. 将菜单的根面板添加到 UI 根中
    m_ui_root->AddChild(m_neural_menu_controller->GetRootPanel());

    // 5. 创建 PulsarButtons
    // Button 1: Dyson Beam
    m_beam_button = std::make_shared<UI::PulsarButton>("beam", "发送戴森光束", "☼", "ENERGY", &m_beam_energy, "J", true);
    m_beam_button->m_rect.x = 50;
    m_beam_button->m_rect.y = 400;
    m_beam_button->m_font = ctx.m_font_large;
    m_beam_button->on_click_callback = [this]()
    {
        m_is_beam_button_active = !m_is_beam_button_active;
        m_beam_button->SetActive(m_is_beam_button_active);
        if (m_is_beam_button_active )
        {
            m_is_rkkv_button_active = false;
            m_rkkv_button->SetActive(false);
        }
    };
    m_beam_button->on_stat_change_callback = [](const std::string& id, const std::string& value)
    {
        NpgsCoreInfo("Button '{}' value changed to: {}", id, value);
    };

    // Button 2: RKKV
    m_rkkv_button = std::make_shared<UI::PulsarButton>("rkkv", "发射RKKV", "☢", "MASS", &m_rkkv_mass, "kg", true);
    m_rkkv_button->m_rect.x = 50;
    m_rkkv_button->m_rect.y = 460;
    m_rkkv_button->m_font = ctx.m_font_large;
    m_rkkv_button->on_click_callback = [this]()
    {
        m_is_rkkv_button_active = !m_is_rkkv_button_active;
        m_rkkv_button->SetActive(m_is_rkkv_button_active);
        if (m_is_rkkv_button_active )
        {
            m_is_beam_button_active = false;
            m_beam_button->SetActive(false);
        }
    };

    // 6. 将按钮也添加到 UI 根中
    m_ui_root->AddChild(m_beam_button);
    m_ui_root->AddChild(m_rkkv_button);
    // =========================================================================
    std::unique_ptr<Grt::FColorAttachment> HistoryAttachment;
    std::unique_ptr<Grt::FColorAttachment> BlackHoleAttachment;
    std::unique_ptr<Grt::FColorAttachment> PreBloomAttachment;
    std::unique_ptr<Grt::FColorAttachment> GaussBlurAttachment;

    vk::RenderingAttachmentInfo BlackHoleAttachmentInfo = vk::RenderingAttachmentInfo()
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eStore);

    vk::RenderingAttachmentInfo HistoryAttachmentInfo = vk::RenderingAttachmentInfo()
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eStore);

    vk::RenderingAttachmentInfo PreBloomAttachmentInfo = vk::RenderingAttachmentInfo()
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eStore);

    vk::RenderingAttachmentInfo GaussBlurAttachmentInfo = vk::RenderingAttachmentInfo()
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eStore);

    vk::RenderingAttachmentInfo BlendAttachmentInfo = vk::RenderingAttachmentInfo()
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eStore);

    auto CreateFramebuffers = [&]() -> void
    {
        _VulkanContext->WaitIdle();

        HistoryAttachment = std::make_unique<Grt::FColorAttachment>(
            vk::Format::eR16G16B16A16Sfloat, _WindowSize, 1, vk::SampleCountFlagBits::e1,
            vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst);

        BlackHoleAttachment = std::make_unique<Grt::FColorAttachment>(
            vk::Format::eR16G16B16A16Sfloat, _WindowSize, 1, vk::SampleCountFlagBits::e1,
            vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage |
            vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst);

        PreBloomAttachment = std::make_unique<Grt::FColorAttachment>(
            vk::Format::eR16G16B16A16Sfloat, _WindowSize, 1, vk::SampleCountFlagBits::e1,
            vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage |
            vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst);

        GaussBlurAttachment = std::make_unique<Grt::FColorAttachment>(
            vk::Format::eR16G16B16A16Sfloat, _WindowSize, 1, vk::SampleCountFlagBits::e1,
            vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage |
            vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst);

        HistoryAttachmentInfo.setImageView(*HistoryAttachment->GetImageView());
        BlackHoleAttachmentInfo.setImageView(*BlackHoleAttachment->GetImageView());
        PreBloomAttachmentInfo.setImageView(*PreBloomAttachment->GetImageView());
        GaussBlurAttachmentInfo.setImageView(*GaussBlurAttachment->GetImageView());
    };

    auto DestroyFramebuffers = [&]() -> void
    {
        _VulkanContext->WaitIdle();
    };

    CreateFramebuffers();

    _VulkanContext->RegisterAutoRemovedCallbacks(Grt::FVulkanContext::ECallbackType::kCreateSwapchain, "CreateFramebuffers", CreateFramebuffers);
    _VulkanContext->RegisterAutoRemovedCallbacks(Grt::FVulkanContext::ECallbackType::kDestroySwapchain, "DestroyFramebuffers", DestroyFramebuffers);

    // Create pipeline layout
    // ----------------------
    auto* AssetManager = Art::FAssetManager::GetInstance();

    Art::FShader::FResourceInfo BlackHoleResourceInfo
    {
        { { 0, sizeof(FQuadOnlyVertex), false } },
        { { 0, 0, offsetof(FQuadOnlyVertex, Position) } },
        {
            { 0, 0, false },
            { 0, 1, false }
        }
    };

    Art::FShader::FResourceInfo BloomResourceInfo
    {
        {}, {},
        { { 0, 0, false } },
        { { vk::ShaderStageFlagBits::eCompute, { "ibHorizontal" } } }
    };

    Art::FShader::FResourceInfo BlendResourceInfo
    {
        { { 0, sizeof(FQuadOnlyVertex), false } },
        { { 0, 0, offsetof(FQuadOnlyVertex, Position) } },
        { { 0, 0, false } }
    };

    std::vector<std::string> BlackHoleShaderFiles({ "ScreenQuad.vert.spv", "BlackHole.frag.spv" });
    std::vector<std::string> PreBloomShaderFiles({ "PreBloom.comp.spv" });
    std::vector<std::string> GaussBlurShaderFiles({ "GaussBlur.comp.spv" });
    std::vector<std::string> BlendShaderFiles({ "ScreenQuad.vert.spv", "ColorBlend.frag.spv" });

    VmaAllocationCreateInfo TextureAllocationCreateInfo
    {
        .flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        .usage = VMA_MEMORY_USAGE_GPU_ONLY
    };

    AssetManager->AddAsset<Art::FShader>("BlackHole", BlackHoleShaderFiles, BlackHoleResourceInfo);
    AssetManager->AddAsset<Art::FShader>("PreBloom", PreBloomShaderFiles, BloomResourceInfo);
    AssetManager->AddAsset<Art::FShader>("GaussBlur", GaussBlurShaderFiles, BloomResourceInfo);
    AssetManager->AddAsset<Art::FShader>("Blend", BlendShaderFiles, BlendResourceInfo);
    AssetManager->AddAsset<Art::FTextureCube>(
        "Background", TextureAllocationCreateInfo, "UNSkybox", vk::Format::eR8G8B8A8Unorm, vk::Format::eR8G8B8A8Unorm,
        vk::ImageCreateFlagBits::eMutableFormat, true, false);
    auto* BlackHoleShader = AssetManager->GetAsset<Art::FShader>("BlackHole");
    auto* PreBloomShader = AssetManager->GetAsset<Art::FShader>("PreBloom");
    auto* GaussBlurShader = AssetManager->GetAsset<Art::FShader>("GaussBlur");
    auto* BlendShader = AssetManager->GetAsset<Art::FShader>("Blend");
    auto* Background = AssetManager->GetAsset<Art::FTextureCube>("Background");
    Grt::FShaderResourceManager::FUniformBufferCreateInfo GameArgsCreateInfo
    {
        .Name = "GameArgs",
        .Fields = { "Resolution", "FovRadians", "Time","GameTime", "TimeDelta", "TimeRate" },
        .Set = 0,
        .Binding = 0,
        .Usage = vk::DescriptorType::eUniformBuffer
    };

    Grt::FShaderResourceManager::FUniformBufferCreateInfo BlackHoleArgsCreateInfo
    {
        .Name = "BlackHoleArgs",
        .Fields = { "InverseCamRot;","WorldUpView", "BlackHoleRelativePosRs", "BlackHoleRelativeDiskNormal",
                     "BlackHoleTime","BlackHoleMassSol", "Spin", "Mu", "AccretionRate", "InterRadiusRs", "OuterRadiusRs","ThinRs","Hopper", "Brightmut","Darkmut","Reddening","Saturation"
                     , "BlackbodyIntensityExponent","RedShiftColorExponent","RedShiftIntensityExponent","JetRedShiftIntensityExponent","JetBrightmut","JetSaturation","JetShiftMax","BlendWeight"},
        .Set = 0,
        .Binding = 1,
        .Usage = vk::DescriptorType::eUniformBuffer
    };

    auto ShaderResourceManager = Grt::FShaderResourceManager::GetInstance();
    ShaderResourceManager->CreateBuffers<FGameArgs>(GameArgsCreateInfo);
    ShaderResourceManager->CreateBuffers<FBlackHoleArgs>(BlackHoleArgsCreateInfo);

    // Create graphics pipeline
    // ------------------------
    vk::SamplerCreateInfo SamplerCreateInfo = Art::FTextureBase::CreateDefaultSamplerCreateInfo();
    std::vector<vk::DescriptorImageInfo> ImageInfos;

    Grt::FVulkanSampler Sampler(SamplerCreateInfo);

    SamplerCreateInfo
        .setMagFilter(vk::Filter::eLinear)
        .setMinFilter(vk::Filter::eLinear)
        .setMipmapMode(vk::SamplerMipmapMode::eNearest);

    Grt::FVulkanSampler FramebufferSampler(SamplerCreateInfo);

    auto CreatePostDescriptors = [&]() -> void
    {
        ImageInfos.clear();

        vk::DescriptorImageInfo HistoryFrameImageInfo(
            nullptr, *HistoryAttachment->GetImageView(), vk::ImageLayout::eShaderReadOnlyOptimal);
        vk::DescriptorImageInfo BlackHoleImageInfo(
            *FramebufferSampler, *BlackHoleAttachment->GetImageView(), vk::ImageLayout::eShaderReadOnlyOptimal);
        vk::DescriptorImageInfo PreBloomImageInfoForSample(
            *FramebufferSampler, *PreBloomAttachment->GetImageView(), vk::ImageLayout::eShaderReadOnlyOptimal);
        vk::DescriptorImageInfo PreBloomImageInfoForStore(
            *FramebufferSampler, *PreBloomAttachment->GetImageView(), vk::ImageLayout::eGeneral);
        vk::DescriptorImageInfo GaussBlurImageInfoForSample(
            *FramebufferSampler, *GaussBlurAttachment->GetImageView(), vk::ImageLayout::eShaderReadOnlyOptimal);
        vk::DescriptorImageInfo GaussBlurImageInfoForStore(
            *FramebufferSampler, *GaussBlurAttachment->GetImageView(), vk::ImageLayout::eGeneral);

        ImageInfos.push_back(HistoryFrameImageInfo);
        BlackHoleShader->WriteSharedDescriptors(1, 0, vk::DescriptorType::eSampledImage, ImageInfos);

        ImageInfos.clear();
        ImageInfos.push_back(Background->CreateDescriptorImageInfo(Sampler));
        BlackHoleShader->WriteSharedDescriptors(1, 1, vk::DescriptorType::eCombinedImageSampler, ImageInfos);

        ImageInfos.clear();
        ImageInfos.push_back(BlackHoleImageInfo);
        PreBloomShader->WriteSharedDescriptors(1, 0, vk::DescriptorType::eCombinedImageSampler, ImageInfos);
        ImageInfos.clear();
        ImageInfos.push_back(PreBloomImageInfoForStore);
        PreBloomShader->WriteSharedDescriptors(1, 1, vk::DescriptorType::eStorageImage, ImageInfos);

        ImageInfos.clear();
        ImageInfos.push_back(PreBloomImageInfoForSample);
        GaussBlurShader->WriteSharedDescriptors(1, 0, vk::DescriptorType::eCombinedImageSampler, ImageInfos);
        ImageInfos.clear();
        ImageInfos.push_back(GaussBlurImageInfoForStore);
        GaussBlurShader->WriteSharedDescriptors(1, 1, vk::DescriptorType::eStorageImage, ImageInfos);

        ImageInfos.clear();
        ImageInfos.push_back(BlackHoleImageInfo);
        ImageInfos.push_back(GaussBlurImageInfoForSample);
        BlendShader->WriteSharedDescriptors(1, 0, vk::DescriptorType::eCombinedImageSampler, ImageInfos);
    };

    CreatePostDescriptors();

    _VulkanContext->RegisterAutoRemovedCallbacks(Grt::FVulkanContext::ECallbackType::kCreateSwapchain, "CreatePostDescriptor", CreatePostDescriptors);

    std::vector<std::string> BindShaders{ "BlackHole", "PreBloom", "GaussBlur", "Blend" };

    ShaderResourceManager->BindShadersToBuffers("GameArgs", BindShaders);
    ShaderResourceManager->BindShaderToBuffers("BlackHoleArgs", "BlackHole");

#include "Vertices.inc"

    Grt::FDeviceLocalBuffer QuadOnlyVertexBuffer(QuadOnlyVertices.size() * sizeof(FQuadOnlyVertex), vk::BufferUsageFlagBits::eVertexBuffer);
    QuadOnlyVertexBuffer.CopyData(QuadOnlyVertices);

    auto BlackHoleShaderStageCreateInfos = BlackHoleShader->CreateShaderStageCreateInfo();

    auto* PipelineManager = Grt::FPipelineManager::GetInstance();

    vk::PipelineColorBlendAttachmentState ColorBlendAttachmentState = vk::PipelineColorBlendAttachmentState()
        .setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
            vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);

    std::array<vk::Format, 1> ColorFormat{ vk::Format::eR16G16B16A16Sfloat };

    vk::PipelineRenderingCreateInfo BlackHoleRenderingCreateInfo = vk::PipelineRenderingCreateInfo()
        .setColorAttachmentCount(1)
        .setColorAttachmentFormats(ColorFormat);

    Grt::FGraphicsPipelineCreateInfoPack BlackHoleCreateInfoPack;
    BlackHoleCreateInfoPack.GraphicsPipelineCreateInfo.setPNext(&BlackHoleRenderingCreateInfo);
    BlackHoleCreateInfoPack.InputAssemblyStateCreateInfo.setTopology(vk::PrimitiveTopology::eTriangleList);
    BlackHoleCreateInfoPack.ColorBlendAttachmentStates.emplace_back(ColorBlendAttachmentState);

    BlackHoleCreateInfoPack.Viewports.emplace_back(0.0f, static_cast<float>(_WindowSize.height), static_cast<float>(_WindowSize.width),
        -static_cast<float>(_WindowSize.height), 0.0f, 1.0f);

    BlackHoleCreateInfoPack.Scissors.emplace_back(vk::Offset2D(), _WindowSize);

    PipelineManager->CreateGraphicsPipeline("BlackHolePipeline", "BlackHole", BlackHoleCreateInfoPack);

    vk::PipelineRenderingCreateInfo BlendRenderingCreateInfo = vk::PipelineRenderingCreateInfo()
        .setColorAttachmentCount(1)
        .setColorAttachmentFormats(_VulkanContext->GetSwapchainCreateInfo().imageFormat);

    BlackHoleCreateInfoPack.GraphicsPipelineCreateInfo.setPNext(&BlendRenderingCreateInfo);
    PipelineManager->CreateGraphicsPipeline("BlendPipeline", "Blend", BlackHoleCreateInfoPack);

    PipelineManager->CreateComputePipeline("PreBloomPipeline", "PreBloom");
    PipelineManager->CreateComputePipeline("GaussBlurPipeline", "GaussBlur");

    vk::Pipeline BlackHolePipeline;
    vk::Pipeline PreBloomPipeline;
    vk::Pipeline GaussBlurPipeline;
    vk::Pipeline BlendPipeline;

    auto GetPipelines = [&]() -> void
    {
        BlackHolePipeline = PipelineManager->GetPipeline("BlackHolePipeline");
        PreBloomPipeline = PipelineManager->GetPipeline("PreBloomPipeline");
        GaussBlurPipeline = PipelineManager->GetPipeline("GaussBlurPipeline");
        BlendPipeline = PipelineManager->GetPipeline("BlendPipeline");
    };

    GetPipelines();

    _VulkanContext->RegisterAutoRemovedCallbacks(Grt::FVulkanContext::ECallbackType::kCreateSwapchain, "GetPipelines", GetPipelines);

    auto BlackHolePipelineLayout = PipelineManager->GetPipelineLayout("BlackHolePipeline");
    auto PreBloomPipelineLayout = PipelineManager->GetPipelineLayout("PreBloomPipeline");
    auto GaussBlurPipelineLayout = PipelineManager->GetPipelineLayout("GaussBlurPipeline");
    auto BlendPipelineLayout = PipelineManager->GetPipelineLayout("BlendPipeline");

    std::vector<Grt::FVulkanFence> InFlightFences;
    std::vector<Grt::FVulkanSemaphore> Semaphores_ImageAvailable;
    std::vector<Grt::FVulkanSemaphore> Semaphores_RenderFinished;
    for (std::size_t i = 0; i != Config::Graphics::kMaxFrameInFlight; ++i)
    {
        InFlightFences.emplace_back(vk::FenceCreateFlagBits::eSignaled);
        Semaphores_ImageAvailable.emplace_back(vk::SemaphoreCreateFlags());
        Semaphores_RenderFinished.emplace_back(vk::SemaphoreCreateFlags());
    }

    std::vector<Grt::FVulkanCommandBuffer> BlackHoleCommandBuffers(Config::Graphics::kMaxFrameInFlight);
    std::vector<Grt::FVulkanCommandBuffer> PreBloomCommandBuffers(Config::Graphics::kMaxFrameInFlight);
    std::vector<Grt::FVulkanCommandBuffer> GaussBlurCommandBuffers(Config::Graphics::kMaxFrameInFlight);
    std::vector<Grt::FVulkanCommandBuffer> BlendCommandBuffers(Config::Graphics::kMaxFrameInFlight);
    _VulkanContext->GetGraphicsCommandPool().AllocateBuffers(vk::CommandBufferLevel::ePrimary, BlackHoleCommandBuffers);
    _VulkanContext->GetGraphicsCommandPool().AllocateBuffers(vk::CommandBufferLevel::ePrimary, PreBloomCommandBuffers);
    _VulkanContext->GetGraphicsCommandPool().AllocateBuffers(vk::CommandBufferLevel::ePrimary, GaussBlurCommandBuffers);
    _VulkanContext->GetGraphicsCommandPool().AllocateBuffers(vk::CommandBufferLevel::ePrimary, BlendCommandBuffers);

    vk::DeviceSize Offset = 0;
    std::uint32_t  CurrentFrame = 0;
    glm::vec4      WorldUp(0.0f, 1.0f, 0.0f, 0.0f);

    vk::ImageSubresourceRange SubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);

    auto InitHistoryFrame = [&]() -> void
    {
        vk::ImageMemoryBarrier2 InitHistoryBarrier(
            vk::PipelineStageFlagBits2::eTopOfPipe,
            vk::AccessFlagBits2::eNone,
            vk::PipelineStageFlagBits2::eFragmentShader,
            vk::AccessFlagBits2::eShaderRead,
            vk::ImageLayout::eUndefined,
            vk::ImageLayout::eShaderReadOnlyOptimal,
            vk::QueueFamilyIgnored,
            vk::QueueFamilyIgnored,
            *HistoryAttachment->GetImage(),
            SubresourceRange);

        vk::DependencyInfo InitialDependencyInfo = vk::DependencyInfo()
            .setDependencyFlags(vk::DependencyFlagBits::eByRegion)
            .setImageMemoryBarriers(InitHistoryBarrier);

        auto& CommandBuffer = _VulkanContext->GetTransferCommandBuffer();
        CommandBuffer.Begin();
        CommandBuffer->pipelineBarrier2(InitialDependencyInfo);
        CommandBuffer.End();
        _VulkanContext->ExecuteGraphicsCommands(CommandBuffer);
    };

    InitHistoryFrame();

    _VulkanContext->RegisterAutoRemovedCallbacks(Grt::FVulkanContext::ECallbackType::kCreateSwapchain, "InitHistoryFrame", InitHistoryFrame);
    glm::vec4    LastBlackHoleRelativePos(0.0f, 0.0f, 0.0f, 1.0f);
    glm::mat4x4 lastdir(0.0f);
    //LastFrameTime = glfwGetTime();
    while (!glfwWindowShouldClose(_Window))
    {
        while (glfwGetWindowAttrib(_Window, GLFW_ICONIFIED))
        {
            glfwWaitEvents();
        }

        InFlightFences[CurrentFrame].WaitAndReset();

        glfwPollEvents();
        // 开始 UI 帧
        _uiRenderer->BeginFrame();

        // =========================================================================
        // [修改] 游戏主循环中的 UI 处理
        // =========================================================================
        //float dt = ImGui::GetIO().DeltaTime;

        m_ui_root->Update(_DeltaTime);
        m_ui_root->Draw();
        // =========================================================================

        // Render other standard ImGui windows
       // RenderDebugUI();

        _uiRenderer->EndFrame();
        // Uniform update
        // --------------

        {
            float Rs = 2.0 * BlackHoleArgs.BlackHoleMassSol * kGravityConstant / pow(kSpeedOfLight, 2) * kSolarMass / kLightYearToMeter;
            if (FrameCount == 0)
            {
                GameArgs.Resolution = glm::vec2(_WindowSize.width, _WindowSize.height);
                GameArgs.FovRadians = glm::radians(_FreeCamera->GetCameraZoom());
                GameArgs.Time = RealityTime;
                GameArgs.GameTime = GameTime;
                GameArgs.TimeDelta = static_cast<float>(_DeltaTime);
                GameArgs.TimeRate = TimeRate;
                LastBlackHoleRelativePos = BlackHoleArgs.BlackHoleRelativePosRs;
                lastdir = BlackHoleArgs.InverseCamRot;
                ShaderResourceManager->UpdateEntrieBuffer(CurrentFrame, "GameArgs", GameArgs);
                BlackHoleArgs.InverseCamRot = glm::mat4_cast(glm::conjugate(_FreeCamera->GetOrientation()));
                BlackHoleArgs.WorldUpView = (glm::mat4_cast(_FreeCamera->GetOrientation()) * WorldUp);
                BlackHoleArgs.BlackHoleRelativePosRs = glm::vec4(glm::vec3(_FreeCamera->GetViewMatrix() * glm::vec4(0.0 * BlackHoleArgs.BlackHoleMassSol * kGravityConstant / pow(kSpeedOfLight, 2) * kSolarMass / kLightYearToMeter, 0.0f, -0.000f, 1.0f)) / Rs, 1.0);
                BlackHoleArgs.BlackHoleRelativeDiskNormal = (glm::mat4_cast(_FreeCamera->GetOrientation()) * glm::vec4(0.0f, 1.0f, 0.00001f, 1.0f));
                BlackHoleArgs.BlackHoleTime = GameTime * kSpeedOfLight / Rs / kLightYearToMeter;
                BlackHoleArgs.BlackHoleMassSol = 0.000000001*1.49e7f;
                BlackHoleArgs.Spin = 0.0f;
                BlackHoleArgs.Mu = 1.0f;
                BlackHoleArgs.AccretionRate = (2e1);
                BlackHoleArgs.InterRadiusRs = 2.1;
                BlackHoleArgs.OuterRadiusRs = 3 * 35.5;
                BlackHoleArgs.ThinRs = 3 * 1.4;
                BlackHoleArgs.Hopper = 0.06 + 0.5;
                BlackHoleArgs.Brightmut = 1.0;
                BlackHoleArgs.Darkmut = 1.0;
                BlackHoleArgs.Reddening = 0.3;
                BlackHoleArgs.Saturation = 0.5;
                BlackHoleArgs.BlackbodyIntensityExponent = 0.5;
                BlackHoleArgs.RedShiftColorExponent = 3.0;
                BlackHoleArgs.RedShiftIntensityExponent = 4.0;
                BlackHoleArgs.JetRedShiftIntensityExponent = 2.0;
                BlackHoleArgs.JetBrightmut = 1.0;
                BlackHoleArgs.JetSaturation = 0.0;
                BlackHoleArgs.JetShiftMax = 3.0;
                m_neural_menu_controller->AddThrottle("TimeRate", &TimeRate);
                m_neural_menu_controller->AddThrottle("BlackHoleMassSol", &BlackHoleArgs.BlackHoleMassSol);
                m_neural_menu_controller->AddThrottle("Spin", &BlackHoleArgs.Spin);
                m_neural_menu_controller->AddThrottle("Mu", &BlackHoleArgs.Mu);
                m_neural_menu_controller->AddThrottle("AccretionRate", &BlackHoleArgs.AccretionRate);
                m_neural_menu_controller->AddThrottle("InterRadiusLy", &BlackHoleArgs.InterRadiusRs);
                m_neural_menu_controller->AddThrottle("OuterRadiusLy", &BlackHoleArgs.OuterRadiusRs);
                m_neural_menu_controller->AddThrottle("ThinLy", &BlackHoleArgs.ThinRs);
                m_neural_menu_controller->AddThrottle("Hopper", &BlackHoleArgs.Hopper);
                m_neural_menu_controller->AddThrottle("Brightmut", &BlackHoleArgs.Brightmut);
                m_neural_menu_controller->AddThrottle("Darkmut", &BlackHoleArgs.Darkmut);
                m_neural_menu_controller->AddThrottle("Reddening", &BlackHoleArgs.Reddening);
                m_neural_menu_controller->AddThrottle("Saturation", &BlackHoleArgs.Saturation, 0.1f);
                m_neural_menu_controller->AddThrottle("BlackbodyIntensityExponent", &BlackHoleArgs.BlackbodyIntensityExponent);
                m_neural_menu_controller->AddThrottle("RedShiftColorExponent", &BlackHoleArgs.RedShiftColorExponent);
                m_neural_menu_controller->AddThrottle("RedShiftIntensityExponent", &BlackHoleArgs.RedShiftIntensityExponent);
                m_neural_menu_controller->AddThrottle("JetRedShiftIntensityExponent", &BlackHoleArgs.JetRedShiftIntensityExponent);
                m_neural_menu_controller->AddThrottle("JetBrightmut", &BlackHoleArgs.JetBrightmut);
                m_neural_menu_controller->AddThrottle("JetSaturation", &BlackHoleArgs.JetSaturation);
                m_neural_menu_controller->AddThrottle("JetShiftMax", &BlackHoleArgs.JetShiftMax);
            }
            else
            {
                GameArgs.Resolution = glm::vec2(_WindowSize.width, _WindowSize.height);
                GameArgs.FovRadians = glm::radians(_FreeCamera->GetCameraZoom());
                GameArgs.Time = RealityTime;
                GameArgs.GameTime = GameTime;
                GameArgs.TimeDelta = static_cast<float>(_DeltaTime);
                GameArgs.TimeRate = TimeRate;
                LastBlackHoleRelativePos = BlackHoleArgs.BlackHoleRelativePosRs;
                lastdir = BlackHoleArgs.InverseCamRot;
                ShaderResourceManager->UpdateEntrieBuffer(CurrentFrame, "GameArgs", GameArgs);
                BlackHoleArgs.BlackHoleTime = GameTime * kSpeedOfLight / Rs / kLightYearToMeter;
                BlackHoleArgs.InverseCamRot = glm::mat4_cast(glm::conjugate(_FreeCamera->GetOrientation()));
                BlackHoleArgs.WorldUpView = (glm::mat4_cast(_FreeCamera->GetOrientation()) * WorldUp);
                BlackHoleArgs.BlackHoleRelativePosRs = glm::vec4(glm::vec3(_FreeCamera->GetViewMatrix() * glm::vec4(0.0 * BlackHoleArgs.BlackHoleMassSol * kGravityConstant / pow(kSpeedOfLight, 2) * kSolarMass / kLightYearToMeter, 0.0f, -0.000f, 1.0f)) / Rs, 1.0);
                BlackHoleArgs.BlackHoleRelativeDiskNormal = (glm::mat4_cast(_FreeCamera->GetOrientation()) * glm::vec4(0.0f, 1.0f, 0.00001f, 0.0f));

            }

            Rs = 2.0 * BlackHoleArgs.BlackHoleMassSol * kGravityConstant / pow(kSpeedOfLight, 2) * kSolarMass / kLightYearToMeter;
            BlackHoleArgs.BlendWeight = (1.0 - pow(0.5, (_DeltaTime) / std::max(std::min((0.131 * 36.0 / (GameArgs.TimeRate) * (Rs / 0.00000465)), 0.5), 0.06)));
            if (!(abs(glm::quat((lastdir - BlackHoleArgs.InverseCamRot)).w - 0.5) < 0.001 * _DeltaTime || abs(glm::quat((lastdir - BlackHoleArgs.InverseCamRot)).w - 0.0) < 0.001 * _DeltaTime) ||
                glm::length(glm::vec3(LastBlackHoleRelativePos - BlackHoleArgs.BlackHoleRelativePosRs)) > (glm::length(glm::vec3(LastBlackHoleRelativePos)) - 1.0) * 0.006 * _DeltaTime)
            {
                BlackHoleArgs.BlendWeight = 1.0f;
            }
            if (int(glfwGetTime()) < 1)
            {
                _FreeCamera->SetTargetOrbitAxis(glm::vec3(0., 1., 0.)); _FreeCamera->SetTargetOrbitCenter(glm::vec3(0, 0, 0));
            }//else{ _FreeCamera->SetTargetOrbitAxis(glm::vec3(0., -1., -0.)); _FreeCamera->SetTargetOrbitCenter(glm::vec3(0.,0.0*5.586e-5f, 0));
           // }
           // _FreeCamera->ProcessMouseMovement(10, 0);
            ShaderResourceManager->UpdateEntrieBuffer(CurrentFrame, "BlackHoleArgs", BlackHoleArgs);

            _VulkanContext->SwapImage(*Semaphores_ImageAvailable[CurrentFrame]);
            std::uint32_t ImageIndex = _VulkanContext->GetCurrentImageIndex();

            BlendAttachmentInfo.setImageView(_VulkanContext->GetSwapchainImageView(ImageIndex));

            std::uint32_t WorkgroundX = (_WindowSize.width + 9) / 10;
            std::uint32_t WorkgroundY = (_WindowSize.height + 9) / 10;

            // Record BlackHole rendering commands
            // -----------------------------------
            auto& CurrentBuffer = BlackHoleCommandBuffers[CurrentFrame];
            CurrentBuffer.Begin(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

            vk::ImageMemoryBarrier2 InitBlackHoleBarrier(
                vk::PipelineStageFlagBits2::eTopOfPipe,
                vk::AccessFlagBits2::eNone,
                vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                vk::AccessFlagBits2::eColorAttachmentWrite,
                vk::ImageLayout::eUndefined,
                vk::ImageLayout::eColorAttachmentOptimal,
                vk::QueueFamilyIgnored,
                vk::QueueFamilyIgnored,
                *BlackHoleAttachment->GetImage(),
                SubresourceRange);

            vk::DependencyInfo BlackHoleInitialDependencyInfo = vk::DependencyInfo()
                .setDependencyFlags(vk::DependencyFlagBits::eByRegion)
                .setImageMemoryBarriers(InitBlackHoleBarrier);

            CurrentBuffer->pipelineBarrier2(BlackHoleInitialDependencyInfo);

            vk::RenderingInfo BlackHoleRenderingInfo = vk::RenderingInfo()
                .setRenderArea(vk::Rect2D({ 0, 0 }, _WindowSize))
                .setLayerCount(1)
                .setColorAttachments(BlackHoleAttachmentInfo);

            CurrentBuffer->beginRendering(BlackHoleRenderingInfo);
            CurrentBuffer->bindVertexBuffers(0, *QuadOnlyVertexBuffer.GetBuffer(), Offset);
            CurrentBuffer->bindPipeline(vk::PipelineBindPoint::eGraphics, BlackHolePipeline);

            CurrentBuffer->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, BlackHolePipelineLayout, 0,
                BlackHoleShader->GetDescriptorSets(CurrentFrame), {});
            CurrentBuffer->draw(6, 1, 0, 0);
            CurrentBuffer->endRendering();

            vk::ImageMemoryBarrier2 PreCopySrcBarrier(
                vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                vk::AccessFlagBits2::eColorAttachmentWrite,
                vk::PipelineStageFlagBits2::eTransfer,
                vk::AccessFlagBits2::eTransferRead,
                vk::ImageLayout::eColorAttachmentOptimal,
                vk::ImageLayout::eTransferSrcOptimal,
                vk::QueueFamilyIgnored,
                vk::QueueFamilyIgnored,
                *BlackHoleAttachment->GetImage(),
                SubresourceRange);

            vk::ImageMemoryBarrier2 PreCopyDstBarrier(
                vk::PipelineStageFlagBits2::eFragmentShader,
                vk::AccessFlagBits2::eShaderRead,
                vk::PipelineStageFlagBits2::eTransfer,
                vk::AccessFlagBits2::eTransferWrite,
                vk::ImageLayout::eShaderReadOnlyOptimal,
                vk::ImageLayout::eTransferDstOptimal,
                vk::QueueFamilyIgnored,
                vk::QueueFamilyIgnored,
                *HistoryAttachment->GetImage(),
                SubresourceRange);

            std::array PreCopyBarriers{ PreCopySrcBarrier, PreCopyDstBarrier };
            vk::DependencyInfo PreCopyDependencyInfo = vk::DependencyInfo()
                .setDependencyFlags(vk::DependencyFlagBits::eByRegion)
                .setImageMemoryBarriers(PreCopyBarriers);

            vk::ImageCopy HistoryCopyRegion = vk::ImageCopy()
                .setSrcSubresource(vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1))
                .setDstSubresource(vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1))
                .setExtent(vk::Extent3D(_WindowSize.width, _WindowSize.height, 1));

            CurrentBuffer->pipelineBarrier2(PreCopyDependencyInfo);
            CurrentBuffer->copyImage(*BlackHoleAttachment->GetImage(), vk::ImageLayout::eTransferSrcOptimal,
                *HistoryAttachment->GetImage(), vk::ImageLayout::eTransferDstOptimal, HistoryCopyRegion);

            vk::ImageMemoryBarrier2 PostCopySrcBarrier(
                vk::PipelineStageFlagBits2::eTransfer,
                vk::AccessFlagBits2::eTransferRead,
                vk::PipelineStageFlagBits2::eComputeShader,
                vk::AccessFlagBits2::eShaderRead,
                vk::ImageLayout::eTransferSrcOptimal,
                vk::ImageLayout::eShaderReadOnlyOptimal,
                vk::QueueFamilyIgnored,
                vk::QueueFamilyIgnored,
                *BlackHoleAttachment->GetImage(),
                SubresourceRange);

            vk::ImageMemoryBarrier2 PostCopyDstBarrier(
                vk::PipelineStageFlagBits2::eTransfer,
                vk::AccessFlagBits2::eTransferWrite,
                vk::PipelineStageFlagBits2::eFragmentShader,
                vk::AccessFlagBits2::eShaderRead,
                vk::ImageLayout::eTransferDstOptimal,
                vk::ImageLayout::eShaderReadOnlyOptimal,
                vk::QueueFamilyIgnored,
                vk::QueueFamilyIgnored,
                *HistoryAttachment->GetImage(),
                SubresourceRange);

            std::array PostCopyBarriers{ PostCopySrcBarrier, PostCopyDstBarrier };
            vk::DependencyInfo PostCopyDependencyInfo = vk::DependencyInfo()
                .setDependencyFlags(vk::DependencyFlagBits::eByRegion)
                .setImageMemoryBarriers(PostCopyBarriers);

            CurrentBuffer->pipelineBarrier2(PostCopyDependencyInfo);
            //CurrentBuffer.End();

            //_VulkanContext->ExecuteGraphicsCommands(CurrentBuffer);

            //// Record PreBloom rendering commands
            //// ----------------------------------
            //CurrentBuffer = PreBloomCommandBuffers[CurrentFrame];
            //CurrentBuffer.Begin(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

            vk::ImageMemoryBarrier2 InitPreBloomBarrier(
                vk::PipelineStageFlagBits2::eTopOfPipe,
                vk::AccessFlagBits2::eNone,
                vk::PipelineStageFlagBits2::eComputeShader,
                vk::AccessFlagBits2::eShaderWrite,
                vk::ImageLayout::eUndefined,
                vk::ImageLayout::eGeneral,
                vk::QueueFamilyIgnored,
                vk::QueueFamilyIgnored,
                *PreBloomAttachment->GetImage(),
                SubresourceRange);

            vk::DependencyInfo PreBloomInitialDependencyInfo = vk::DependencyInfo()
                .setDependencyFlags(vk::DependencyFlagBits::eByRegion)
                .setImageMemoryBarriers(InitPreBloomBarrier);

            CurrentBuffer->pipelineBarrier2(PreBloomInitialDependencyInfo);

            CurrentBuffer->bindPipeline(vk::PipelineBindPoint::eCompute, PreBloomPipeline);
            CurrentBuffer->bindDescriptorSets(vk::PipelineBindPoint::eCompute, PreBloomPipelineLayout, 0,
                PreBloomShader->GetDescriptorSets(CurrentFrame), {});

            CurrentBuffer->dispatch(WorkgroundX, WorkgroundY, 1);

            vk::ImageMemoryBarrier2 FirstBlurBarrier(
                vk::PipelineStageFlagBits2::eComputeShader,
                vk::AccessFlagBits2::eShaderWrite,
                vk::PipelineStageFlagBits2::eComputeShader,
                vk::AccessFlagBits2::eShaderRead,
                vk::ImageLayout::eGeneral,
                vk::ImageLayout::eShaderReadOnlyOptimal,
                vk::QueueFamilyIgnored,
                vk::QueueFamilyIgnored,
                *PreBloomAttachment->GetImage(),
                SubresourceRange);

            vk::DependencyInfo FirstBlurDependencyInfo = vk::DependencyInfo()
                .setDependencyFlags(vk::DependencyFlagBits::eByRegion)
                .setImageMemoryBarriers(FirstBlurBarrier);

            CurrentBuffer->pipelineBarrier2(FirstBlurDependencyInfo);
            //CurrentBuffer.End();
            //_VulkanContext->ExecuteComputeCommands(CurrentBuffer);

            //// Record GaussBlur rendering commands
            //// -----------------------------------
            //CurrentBuffer = GaussBlurCommandBuffers[CurrentFrame];
            //CurrentBuffer.Begin(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

            vk::ImageMemoryBarrier2 InitGaussBlurBarrier(
                vk::PipelineStageFlagBits2::eTopOfPipe,
                vk::AccessFlagBits2::eNone,
                vk::PipelineStageFlagBits2::eComputeShader,
                vk::AccessFlagBits2::eShaderWrite,
                vk::ImageLayout::eUndefined,
                vk::ImageLayout::eGeneral,
                vk::QueueFamilyIgnored,
                vk::QueueFamilyIgnored,
                *GaussBlurAttachment->GetImage(),
                SubresourceRange);

            vk::DependencyInfo GaussBlurInitialDependencyInfo = vk::DependencyInfo()
                .setDependencyFlags(vk::DependencyFlagBits::eByRegion)
                .setImageMemoryBarriers(InitGaussBlurBarrier);

            CurrentBuffer->pipelineBarrier2(GaussBlurInitialDependencyInfo);

            vk::Bool32 bHorizontal = vk::True;

            CurrentBuffer->bindPipeline(vk::PipelineBindPoint::eCompute, GaussBlurPipeline);
            CurrentBuffer->pushConstants(GaussBlurPipelineLayout, vk::ShaderStageFlagBits::eCompute, 0,
                sizeof(vk::Bool32), &bHorizontal);

            CurrentBuffer->bindDescriptorSets(vk::PipelineBindPoint::eCompute, GaussBlurPipelineLayout, 0,
                GaussBlurShader->GetDescriptorSets(CurrentFrame), {});

            CurrentBuffer->dispatch(WorkgroundX, WorkgroundY, 1);

            vk::ImageMemoryBarrier2 CopybackSrcBarrier(
                vk::PipelineStageFlagBits2::eComputeShader,
                vk::AccessFlagBits2::eShaderWrite,
                vk::PipelineStageFlagBits2::eTransfer,
                vk::AccessFlagBits2::eTransferRead,
                vk::ImageLayout::eGeneral,
                vk::ImageLayout::eTransferSrcOptimal,
                vk::QueueFamilyIgnored,
                vk::QueueFamilyIgnored,
                *GaussBlurAttachment->GetImage(),
                SubresourceRange);

            vk::ImageMemoryBarrier2 CopybackDstBarrier(
                vk::PipelineStageFlagBits2::eComputeShader,
                vk::AccessFlagBits2::eShaderRead,
                vk::PipelineStageFlagBits2::eTransfer,
                vk::AccessFlagBits2::eTransferWrite,
                vk::ImageLayout::eShaderReadOnlyOptimal,
                vk::ImageLayout::eTransferDstOptimal,
                vk::QueueFamilyIgnored,
                vk::QueueFamilyIgnored,
                *PreBloomAttachment->GetImage(),
                SubresourceRange);

            std::array CopybackBarriers{ CopybackSrcBarrier, CopybackDstBarrier };
            vk::DependencyInfo CopybackDependencyInfo = vk::DependencyInfo()
                .setDependencyFlags(vk::DependencyFlagBits::eByRegion)
                .setImageMemoryBarriers(CopybackBarriers);

            CurrentBuffer->pipelineBarrier2(CopybackDependencyInfo);

            vk::ImageCopy CopybackRegion = vk::ImageCopy()
                .setSrcSubresource(vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1))
                .setDstSubresource(vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1))
                .setExtent(vk::Extent3D(_WindowSize.width, _WindowSize.height, 1));

            CurrentBuffer->copyImage(*GaussBlurAttachment->GetImage(), vk::ImageLayout::eTransferSrcOptimal,
                *PreBloomAttachment->GetImage(), vk::ImageLayout::eTransferDstOptimal, CopybackRegion);

            vk::ImageMemoryBarrier2 ResampleBarrier(
                vk::PipelineStageFlagBits2::eTransfer,
                vk::AccessFlagBits2::eTransferWrite,
                vk::PipelineStageFlagBits2::eComputeShader,
                vk::AccessFlagBits2::eShaderRead,
                vk::ImageLayout::eTransferDstOptimal,
                vk::ImageLayout::eShaderReadOnlyOptimal,
                vk::QueueFamilyIgnored,
                vk::QueueFamilyIgnored,
                *PreBloomAttachment->GetImage(),
                SubresourceRange);

            vk::ImageMemoryBarrier2 RewriteBarrier(
                vk::PipelineStageFlagBits2::eTransfer,
                vk::AccessFlagBits2::eTransferRead,
                vk::PipelineStageFlagBits2::eComputeShader,
                vk::AccessFlagBits2::eShaderWrite,
                vk::ImageLayout::eTransferSrcOptimal,
                vk::ImageLayout::eGeneral,
                vk::QueueFamilyIgnored,
                vk::QueueFamilyIgnored,
                *GaussBlurAttachment->GetImage(),
                SubresourceRange);

            std::array RestoreBarriers{ ResampleBarrier, RewriteBarrier };
            vk::DependencyInfo RerenderDependencyInfo = vk::DependencyInfo()
                .setDependencyFlags(vk::DependencyFlagBits::eByRegion)
                .setImageMemoryBarriers(RestoreBarriers);

            CurrentBuffer->pipelineBarrier2(RerenderDependencyInfo);

            bHorizontal = vk::False;

            CurrentBuffer->pushConstants(GaussBlurPipelineLayout, vk::ShaderStageFlagBits::eCompute, 0,
                sizeof(vk::Bool32), &bHorizontal);

            CurrentBuffer->bindDescriptorSets(vk::PipelineBindPoint::eCompute, GaussBlurPipelineLayout, 0,
                GaussBlurShader->GetDescriptorSets(CurrentFrame), {});

            CurrentBuffer->dispatch(WorkgroundX, WorkgroundY, 1);

            vk::ImageMemoryBarrier2 BlendSampleBarrier(
                vk::PipelineStageFlagBits2::eComputeShader,
                vk::AccessFlagBits2::eShaderWrite,
                vk::PipelineStageFlagBits2::eFragmentShader,
                vk::AccessFlagBits2::eShaderRead,
                vk::ImageLayout::eGeneral,
                vk::ImageLayout::eShaderReadOnlyOptimal,
                vk::QueueFamilyIgnored,
                vk::QueueFamilyIgnored,
                *GaussBlurAttachment->GetImage(),
                SubresourceRange);

            vk::DependencyInfo BlendSampleDepencencyInfo = vk::DependencyInfo()
                .setDependencyFlags(vk::DependencyFlagBits::eByRegion)
                .setImageMemoryBarriers(BlendSampleBarrier);

            CurrentBuffer->pipelineBarrier2(BlendSampleDepencencyInfo);
            //CurrentBuffer.End();

            //_VulkanContext->ExecuteComputeCommands(CurrentBuffer);

            //// Record Blend rendering commands
            //// ------------------------------
            //CurrentBuffer = BlendCommandBuffers[CurrentFrame];
            //CurrentBuffer.Begin(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

            vk::ImageMemoryBarrier2 InitSwapchainBarrier(
                vk::PipelineStageFlagBits2::eTopOfPipe,
                vk::AccessFlagBits2::eNone,
                vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                vk::AccessFlagBits2::eColorAttachmentWrite,
                vk::ImageLayout::eUndefined,
                vk::ImageLayout::eColorAttachmentOptimal,
                vk::QueueFamilyIgnored,
                vk::QueueFamilyIgnored,
                _VulkanContext->GetSwapchainImage(ImageIndex),
                SubresourceRange);

            vk::DependencyInfo SwapchainInitialDependencyInfo = vk::DependencyInfo()
                .setDependencyFlags(vk::DependencyFlagBits::eByRegion)
                .setImageMemoryBarriers(InitSwapchainBarrier);

            CurrentBuffer->pipelineBarrier2(SwapchainInitialDependencyInfo);

            vk::RenderingInfo BlendRenderingInfo = vk::RenderingInfo()
                .setRenderArea(vk::Rect2D({ 0, 0 }, _WindowSize))
                .setLayerCount(1)
                .setColorAttachments(BlendAttachmentInfo);

            CurrentBuffer->beginRendering(BlendRenderingInfo);
            CurrentBuffer->bindVertexBuffers(0, *QuadOnlyVertexBuffer.GetBuffer(), Offset);
            CurrentBuffer->bindPipeline(vk::PipelineBindPoint::eGraphics, BlendPipeline);
            CurrentBuffer->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, BlendPipelineLayout, 0, BlendShader->GetDescriptorSets(CurrentFrame), {});
            CurrentBuffer->draw(6, 1, 0, 0);
            CurrentBuffer->endRendering();
            _uiRenderer->Render(*CurrentBuffer);

            vk::ImageMemoryBarrier2 PresentBarrier(
                vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                vk::AccessFlagBits2::eColorAttachmentWrite,
                vk::PipelineStageFlagBits2::eBottomOfPipe,
                vk::AccessFlagBits2::eNone,
                vk::ImageLayout::eColorAttachmentOptimal,
                vk::ImageLayout::ePresentSrcKHR,
                vk::QueueFamilyIgnored,
                vk::QueueFamilyIgnored,
                _VulkanContext->GetSwapchainImage(ImageIndex),
                SubresourceRange);

            vk::DependencyInfo PresentDependencyInfo = vk::DependencyInfo()
                .setDependencyFlags(vk::DependencyFlagBits::eByRegion)
                .setImageMemoryBarriers(PresentBarrier);

            CurrentBuffer->pipelineBarrier2(PresentDependencyInfo);
            CurrentBuffer.End();

            _VulkanContext->SubmitCommandBufferToGraphics(
                *CurrentBuffer, *Semaphores_ImageAvailable[CurrentFrame],
                *Semaphores_RenderFinished[CurrentFrame], *InFlightFences[CurrentFrame]);
            _VulkanContext->PresentImage(*Semaphores_RenderFinished[CurrentFrame]);
        }
        CurrentFrame = (CurrentFrame + 1) % Config::Graphics::kMaxFrameInFlight;

        ProcessInput();
        update();
    }

    _VulkanContext->WaitIdle();
    Terminate();
}

void FApplication::Terminate()
{
    if (_uiRenderer)
    {
        _uiRenderer->Shutdown();
        _uiRenderer.reset();
    }
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
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, true);

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

    _FreeCamera = std::make_unique<SysSpa::FCamera>(glm::vec3(0.0f, 0.0f, 0.0f), 0.2, 2.5, 60.0);
    _FreeCamera->SetCameraMode(true);
    return true;
}

void FApplication::InitializeInputCallbacks()
{
    glfwSetWindowUserPointer(_Window, this);
    glfwSetFramebufferSizeCallback(_Window, &FApplication::FramebufferSizeCallback);

    glfwSetScrollCallback(_Window, &FApplication::ScrollCallback);
    glfwSetMouseButtonCallback(_Window, &FApplication::MouseButtonCallback);
    glfwSetCursorPosCallback(_Window, &FApplication::CursorPosCallback);
    glfwSetKeyCallback(_Window, &FApplication::KeyCallback);
    glfwSetCharCallback(_Window, &FApplication::CharCallback);
}


void FApplication::update()
{

    _FreeCamera->ProcessTimeEvolution(_DeltaTime);

    CurrentTime = glfwGetTime();
    _DeltaTime = CurrentTime - LastFrameTime;
    RealityTime += _DeltaTime;;
    GameTime += TimeRate * _DeltaTime;
    LastFrameTime = CurrentTime;
    ++FramePerSec;
    FrameCount++;
    if (CurrentTime - PreviousTime >= 1.0)
    {
        glfwSetWindowTitle(_Window, (std::string(_WindowTitle) + " " + std::to_string(FramePerSec)).c_str());
        FramePerSec = 0;
        PreviousTime = CurrentTime;
    }
}

void FApplication::ProcessInput()
{
    // -----------------------------------------------------------
    // 1. 获取状态 & UI 阻挡判断
    // -----------------------------------------------------------
    ImGuiIO& io = ImGui::GetIO();
    auto& ui_ctx = Npgs::System::UI::UIContext::Get();

    bool bMouseBlocked = io.WantCaptureMouse;
    bool bKeyboardBlocked = io.WantCaptureKeyboard || (ui_ctx.m_focused_element != nullptr);

    // -----------------------------------------------------------
    // 2. 处理鼠标旋转 (带冷却机制)
    // -----------------------------------------------------------
    double currX, currY;
    glfwGetCursorPos(_Window, &currX, &currY);

    // 检测左键状态
    bool isLeftDown = glfwGetMouseButton(_Window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    static bool wasLeftDown = false; // 上一帧状态

    // [新增] 冷却计数器：防止松开鼠标后的瞬间再次误触或跳变
    static int s_FrameCooldown = 0;
    if (s_FrameCooldown > 0)
    {
        s_FrameCooldown--;
    }

    // [Case A]: 刚按下左键 (Rising Edge)
    if (isLeftDown && !wasLeftDown)
    {
        // 判定条件：
        // 1. UI 没有捕获鼠标
        // 2. 冷却时间已过 (s_FrameCooldown == 0)
        if (!bMouseBlocked && s_FrameCooldown == 0)
        {
            _bLeftMousePressedInWorld = true;
            _DragStartX = currX;
            _DragStartY = currY;

            _bIsRotatingCamera = true;
            glfwSetInputMode(_Window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

            // 标记 FirstMouse，让 Case C 在下一帧重置坐标基准
            _bFirstMouse = true;
        }
    }
    // [Case B]: 刚松开左键 (Falling Edge)
    else if (!isLeftDown && wasLeftDown)
    {
        if (_bLeftMousePressedInWorld)
        {
            _bLeftMousePressedInWorld = false;
            _bIsRotatingCamera = false;

            glfwSetInputMode(_Window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

            // 归位光标
            glfwSetCursorPos(_Window, _DragStartX, _DragStartY);
            io.MousePos = ImVec2((float)_DragStartX, (float)_DragStartY);

            // [核心修改] 强制冷却 3 帧
            // 这期间即便 isLeftDown 为 true (极快连点) 或数据波动，也不会进入 Case A
            s_FrameCooldown = 3;
        }
    }
    // [Case C]: 持续按住并拖动
    // 只有在冷却完毕，且确实处于旋转模式下才执行
    else if (_bLeftMousePressedInWorld && _bIsRotatingCamera && s_FrameCooldown == 0)
    {
        if (_bFirstMouse)
        {
            // 模式切换后的第一帧数据，仅用于重置基准，不移动
            // 此时 currX 是 DISABLED 模式下的新坐标
            _LastX = currX;
            _LastY = currY;
            _bFirstMouse = false;
        }
        else
        {
            double deltaX = currX - _LastX;
            double deltaY = _LastY - currY;

            // 立即更新基准
            _LastX = currX;
            _LastY = currY;

            if (deltaX != 0.0 || deltaY != 0.0)
            {
                _FreeCamera->ProcessMouseMovement(deltaX, deltaY);
            }
        }
    }

    // 更新按键历史
    wasLeftDown = isLeftDown;

    // -----------------------------------------------------------
    // 3. 处理滚轮缩放 (消费 Buffer)
    // -----------------------------------------------------------
    if (_buffered_scroll_y != 0.0f)
    {
        if (!bMouseBlocked)
        {
            _FreeCamera->ProcessMouseScroll(_buffered_scroll_y);
        }
        _buffered_scroll_y = 0.0f;
    }

    // -----------------------------------------------------------
    // 4. 处理键盘移动
    // -----------------------------------------------------------
    if (!bKeyboardBlocked)
    {
        // 模式切换
        static bool wasTDown = false;
        bool isTDown = glfwGetKey(_Window, GLFW_KEY_T) == GLFW_PRESS;
        if (isTDown && !wasTDown)
        {
            _FreeCamera->ProcessModeChange();
        }
        wasTDown = isTDown;


        if (glfwGetKey(_Window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        {
            glfwSetWindowShouldClose(_Window, GLFW_TRUE);
        }

        if (glfwGetKey(_Window, GLFW_KEY_W) == GLFW_PRESS)
            _FreeCamera->ProcessKeyboard(SysSpa::FCamera::EMovement::kForward);

        if (glfwGetKey(_Window, GLFW_KEY_S) == GLFW_PRESS)
            _FreeCamera->ProcessKeyboard(SysSpa::FCamera::EMovement::kBack);

        if (glfwGetKey(_Window, GLFW_KEY_A) == GLFW_PRESS)
            _FreeCamera->ProcessKeyboard(SysSpa::FCamera::EMovement::kLeft);

        if (glfwGetKey(_Window, GLFW_KEY_D) == GLFW_PRESS)
            _FreeCamera->ProcessKeyboard(SysSpa::FCamera::EMovement::kRight);

        if (glfwGetKey(_Window, GLFW_KEY_R) == GLFW_PRESS)
            _FreeCamera->ProcessKeyboard(SysSpa::FCamera::EMovement::kUp);

        if (glfwGetKey(_Window, GLFW_KEY_F) == GLFW_PRESS)
            _FreeCamera->ProcessKeyboard(SysSpa::FCamera::EMovement::kDown);

        if (glfwGetKey(_Window, GLFW_KEY_Q) == GLFW_PRESS)
            _FreeCamera->ProcessKeyboard(SysSpa::FCamera::EMovement::kRollLeft);

        if (glfwGetKey(_Window, GLFW_KEY_E) == GLFW_PRESS)
            _FreeCamera->ProcessKeyboard(SysSpa::FCamera::EMovement::kRollRight);
    }
}
void FApplication::MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    auto* App = static_cast<FApplication*>(glfwGetWindowUserPointer(window));
    if (App)
    {
        App->HandleMouseButton(button, action, mods);
    }
}

void FApplication::KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    auto* App = static_cast<FApplication*>(glfwGetWindowUserPointer(window));
    if (App)
    {
        App->HandleKey(key, scancode, action, mods);
    }
}

void FApplication::CharCallback(GLFWwindow* window, unsigned int codepoint)
{
    auto* App = static_cast<FApplication*>(glfwGetWindowUserPointer(window));
    if (App)
    {
        App->HandleChar(codepoint);
    }
}

void FApplication::FramebufferSizeCallback(GLFWwindow* Window, int Width, int Height)
{
    auto* App = reinterpret_cast<FApplication*>(glfwGetWindowUserPointer(Window));
    if (App) App->HandleFramebufferSize(Width, Height);
}

void FApplication::CursorPosCallback(GLFWwindow* window, double posX, double posY)
{
    auto* App = static_cast<FApplication*>(glfwGetWindowUserPointer(window));
    if (App)
    {
        App->HandleCursorPos(posX, posY);
    }
}

void FApplication::ScrollCallback(GLFWwindow* Window, double OffsetX, double OffsetY)
{
    auto* App = reinterpret_cast<FApplication*>(glfwGetWindowUserPointer(Window));
    if (App) App->HandleScroll(OffsetX, OffsetY);
}


// Application.cpp

void FApplication::HandleMouseButton(int button, int action, int mods)
{

}

void FApplication::HandleKey(int key, int scancode, int action, int mods)
{

}

void FApplication::HandleChar(unsigned int codepoint)
{

}

void FApplication::HandleFramebufferSize(int Width, int Height)
{
    if (Width == 0 || Height == 0) return;

    _WindowSize.width = Width;
    _WindowSize.height = Height;
    _VulkanContext->WaitIdle();
    _VulkanContext->RecreateSwapchain();

    // 可以在这里处理其他需要感知窗口大小变化的逻辑（如 UI 布局更新）
}
// Application.cpp

void FApplication::HandleCursorPos(double posX, double posY)
{

}
void FApplication::HandleScroll(double OffsetX, double OffsetY)
{
    _buffered_scroll_y += (float)OffsetY;
}
// 添加调试 UI 渲染函数
void FApplication::RenderDebugUI()
{
    // 显示帧率信息
    ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 200), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Black Hole Renderer Info"))
    {
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
        ImGui::Text("Frame Time: %.3f ms", 1000.0f / ImGui::GetIO().Framerate);
        ImGui::Text("Window Size: %d x %d", _WindowSize.width, _WindowSize.height);
        ImGui::Separator();

        // 显示相机信息
        auto cameraPos = _FreeCamera->GetCameraVector(System::Spatial::FCamera::EVectorType::kPosition);
        ImGui::Text("Camera Position: (%.3f, %.3f, %.3f)",
            cameraPos.x, cameraPos.y, cameraPos.z);
        ImGui::Text("Camera FOV: %.1f", _FreeCamera->GetCameraZoom());

        ImGui::Separator();
        // 显示黑洞参数
        ImGui::Text("Black Hole Time: %.2e ", GameTime);
        ImGui::Text("Black Hole Mass: %.2e Solar Masses", BlackHoleArgs.BlackHoleMassSol);
        ImGui::Text("Accretion Rate: %.2e", BlackHoleArgs.AccretionRate);
        ImGui::Text("Blend Weight: %.3f", BlackHoleArgs.BlendWeight);
    }
    ImGui::End();

    // 显示控制说明
    ImGui::SetNextWindowPos(ImVec2(20, 240), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 200), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Controls"))
    {
        ImGui::Text("WASD: Move Camera");
        ImGui::Text("RF: Move Up/Down");
        ImGui::Text("QE: Roll Camera");
        ImGui::Text("Mouse + Left Click: Rotate Camera");
        ImGui::Text("Mouse Wheel: Zoom");
        ImGui::Text("T: Toggle Camera Mode");
        ImGui::Text("ESC: Exit");
    }
    ImGui::End();
}
_NPGS_END
