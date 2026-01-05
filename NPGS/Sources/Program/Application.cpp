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

#include "Engine/Core/System/UI/AppContext.h"
#include "Engine/Core/System/UI/Screens/MainMenuScreen.h"
#include "Engine/Core/System/UI/Screens/GameScreen.h"

#include <chrono>

FGameArgs GameArgs{};
FBlackHoleArgs BlackHoleArgs{};
FMatrices Matrices;
FLightMaterial LightMaterial;
float cfov = 60.0f;
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
{
    System::I18nManager::Get().UnregisterCallback(this);
}
void FApplication::Quit()
{
    if (_Window)
    {
        glfwSetWindowShouldClose(_Window, GLFW_TRUE);
    }
}

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

   

    // =========================================================================
    std::unique_ptr<Grt::FColorAttachment> HistoryAttachment;
    std::unique_ptr<Grt::FColorAttachment> BlackHoleAttachment;
    std::unique_ptr<Grt::FColorAttachment> PreBloomAttachment;
    std::unique_ptr<Grt::FColorAttachment> GaussBlurAttachment;
    // [新增] 用于存储当前帧无UI的纯净画面 (Full Size)
    std::unique_ptr<Grt::FColorAttachment> SceneColorAttachment;
    // [修改] 我们需要两个大小相同的中间纹理来进行 Ping-Pong
// 它们的大小应该是我们模糊链中最大的尺寸，即 1/2 分辨率
    std::unique_ptr<Grt::FColorAttachment> UIBlurPingAttachment;
    std::unique_ptr<Grt::FColorAttachment> UIBlurPongAttachment;
    // [新增] 用于 UI 背景模糊的最终图 (Half Size)
    std::unique_ptr<Grt::FColorAttachment> UIBlurAttachment;

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
    vk::RenderingAttachmentInfo SceneColorAttachmentInfo = vk::RenderingAttachmentInfo()
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


        SceneColorAttachment = std::make_unique<Grt::FColorAttachment>(
            vk::Format::eR8G8B8A8Unorm, _WindowSize, 1, vk::SampleCountFlagBits::e1,
            vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled |
            vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst
        );

        // [修改] 创建用于 Ping-Pong 的纹理 (Half Size)
        vk::Extent2D halfSize = { _WindowSize.width / 2, _WindowSize.height / 2 };
        if (halfSize.width == 0) halfSize.width = 1;
        if (halfSize.height == 0) halfSize.height = 1;

        vk::ImageUsageFlags blurUsageFlags = vk::ImageUsageFlagBits::eSampled |
            vk::ImageUsageFlagBits::eTransferSrc |
            vk::ImageUsageFlagBits::eTransferDst;

        UIBlurPingAttachment = std::make_unique<Grt::FColorAttachment>(
            vk::Format::eR8G8B8A8Unorm, halfSize, 1, vk::SampleCountFlagBits::e1, blurUsageFlags
        );
        UIBlurPongAttachment = std::make_unique<Grt::FColorAttachment>(
            vk::Format::eR8G8B8A8Unorm, halfSize, 1, vk::SampleCountFlagBits::e1, blurUsageFlags
        );
        UIBlurAttachment = std::make_unique<Grt::FColorAttachment>(
            vk::Format::eR8G8B8A8Unorm, halfSize, 1, vk::SampleCountFlagBits::e1, blurUsageFlags
        );


        HistoryAttachmentInfo.setImageView(*HistoryAttachment->GetImageView());
        BlackHoleAttachmentInfo.setImageView(*BlackHoleAttachment->GetImageView());
        PreBloomAttachmentInfo.setImageView(*PreBloomAttachment->GetImageView());
        GaussBlurAttachmentInfo.setImageView(*GaussBlurAttachment->GetImageView());
        // [新增] 更新 SceneColorInfo
        SceneColorAttachmentInfo.setImageView(*SceneColorAttachment->GetImageView());
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

	AssetManager->AddAsset<Art::FTexture2D>(
		"RKKV", TextureAllocationCreateInfo, "ButtonMap/rkkv0.png", vk::Format::eR8G8B8A8Unorm,
		vk::Format::eR8G8B8A8Unorm, vk::ImageCreateFlags(), false, false);
    AssetManager->AddAsset<Art::FTexture2D>(
        "stage0", TextureAllocationCreateInfo, "stage0.png", vk::Format::eR8G8B8A8Unorm,
        vk::Format::eR8G8B8A8Unorm, vk::ImageCreateFlags(), false, false);
    AssetManager->AddAsset<Art::FTexture2D>(
        "stage1", TextureAllocationCreateInfo, "stage1.png", vk::Format::eR8G8B8A8Unorm,
        vk::Format::eR8G8B8A8Unorm, vk::ImageCreateFlags(), false, false);
    AssetManager->AddAsset<Art::FTexture2D>(
        "stage2", TextureAllocationCreateInfo, "stage2.png", vk::Format::eR8G8B8A8Unorm,
        vk::Format::eR8G8B8A8Unorm, vk::ImageCreateFlags(), false, false);
    AssetManager->AddAsset<Art::FTexture2D>(
        "stage3", TextureAllocationCreateInfo, "stage3.png", vk::Format::eR8G8B8A8Unorm,
        vk::Format::eR8G8B8A8Unorm, vk::ImageCreateFlags(), false, false);
    AssetManager->AddAsset<Art::FTexture2D>(
        "stage4", TextureAllocationCreateInfo, "stage4.png", vk::Format::eR8G8B8A8Unorm,

        vk::Format::eR8G8B8A8Unorm, vk::ImageCreateFlags(), false, false);


    auto* BlackHoleShader = AssetManager->GetAsset<Art::FShader>("BlackHole");
    auto* PreBloomShader = AssetManager->GetAsset<Art::FShader>("PreBloom");
    auto* GaussBlurShader = AssetManager->GetAsset<Art::FShader>("GaussBlur");
    auto* BlendShader = AssetManager->GetAsset<Art::FShader>("Blend");
    auto* Background = AssetManager->GetAsset<Art::FTextureCube>("Background");
    auto* RKKV = AssetManager->GetAsset<Art::FTexture2D>("RKKV");
    auto* stage0 = AssetManager->GetAsset<Art::FTexture2D>("stage0");
    auto* stage1 = AssetManager->GetAsset<Art::FTexture2D>("stage1");
    auto* stage2 = AssetManager->GetAsset<Art::FTexture2D>("stage2");
    auto* stage3 = AssetManager->GetAsset<Art::FTexture2D>("stage3");
    auto* stage4 = AssetManager->GetAsset<Art::FTexture2D>("stage4");
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
        .Fields = { "InverseCamRot;", "BlackHoleRelativePosRs", "BlackHoleRelativeDiskNormal","BlackHoleRelativeDiskTangen",
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



        // [新增] 将 UI 模糊纹理注册给 ImGui
        if (_uiRenderer && UIBlurAttachment)
        {
            // 1. 注册纹理
            ImTextureID blurTexID = _uiRenderer->AddTexture(
                *FramebufferSampler, // 复用现有的线性采样器
                *UIBlurAttachment->GetImageView(),
                vk::ImageLayout::eShaderReadOnlyOptimal
            );

            // 2. 更新 UIContext
            auto& ctx = Npgs::System::UI::UIContext::Get();
            ctx.m_scene_blur_texture = blurTexID;
            ctx.m_display_size = ImVec2((float)_WindowSize.width, (float)_WindowSize.height);
        }

    };

    CreatePostDescriptors();

    _VulkanContext->RegisterAutoRemovedCallbacks(Grt::FVulkanContext::ECallbackType::kCreateSwapchain, "CreatePostDescriptor", CreatePostDescriptors);

    std::vector<std::string> BindShaders{ "BlackHole", "PreBloom", "GaussBlur", "Blend" };

    ShaderResourceManager->BindShadersToBuffers("GameArgs", BindShaders);
    ShaderResourceManager->BindShaderToBuffers("BlackHoleArgs", "BlackHole");





    ImTextureID RKKVID = _uiRenderer->AddTexture(
        *FramebufferSampler,
        *RKKV->GetImageView(),
        vk::ImageLayout::eShaderReadOnlyOptimal
    );
    stage0ID = _uiRenderer->AddTexture(
        *FramebufferSampler,
        *stage0->GetImageView(),
        vk::ImageLayout::eShaderReadOnlyOptimal
    );
    stage1ID = _uiRenderer->AddTexture(
        *FramebufferSampler,
        *stage1->GetImageView(),
        vk::ImageLayout::eShaderReadOnlyOptimal
    );    
    stage2ID = _uiRenderer->AddTexture(
        *FramebufferSampler,
        *stage2->GetImageView(),
        vk::ImageLayout::eShaderReadOnlyOptimal
    );    
    stage3ID = _uiRenderer->AddTexture(
        *FramebufferSampler,
        *stage3->GetImageView(),
        vk::ImageLayout::eShaderReadOnlyOptimal
    );    
    stage4ID = _uiRenderer->AddTexture(
        *FramebufferSampler,
        *stage4->GetImageView(),
        vk::ImageLayout::eShaderReadOnlyOptimal
    );
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

    std::array<vk::Format, 1> SceneColorFormat{ vk::Format::eR8G8B8A8Unorm };

    vk::PipelineRenderingCreateInfo BlendRenderingCreateInfo = vk::PipelineRenderingCreateInfo()
        .setColorAttachmentCount(1)
        .setColorAttachmentFormats(SceneColorFormat);

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

    // 1. 设置自定义主题 (可以在这里或者在构造函数中完成)
    auto& theme = UI::UIContext::Get().m_theme;

    

    System::UI::AppContext appContext{
    .Application = this,
    .UIRenderer = _uiRenderer.get(),
    .GameArgs = &GameArgs,
    .BlackHoleArgs = &BlackHoleArgs,
    .GameTime = &GameTime,
    .TimeRate = &TimeRate,
    .RealityTime = &RealityTime,
    .RKKVID = RKKVID,
    .stage0ID = stage0ID,
    .stage1ID = stage1ID,
    .stage2ID = stage2ID,
    .stage3ID = stage3ID,
    .stage4ID = stage4ID
    };

    m_screen_manager = std::make_unique<System::UI::ScreenManager>();

    // 4. 创建并注册屏幕
    m_screen_manager->RegisterScreen("Game", std::make_shared<System::UI::GameScreen>(appContext));
    m_screen_manager->RegisterScreen("MainMenu", std::make_shared<System::UI::MainMenuScreen>(appContext));

    // 5. 设置初始屏幕
    m_screen_manager->RequestPushScreen("MainMenu");

    // [新增] 第一次应用变更，使主菜单在第一帧就生效
    m_screen_manager->ApplyPendingChanges();

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

        auto& ui_ctx = Npgs::System::UI::UIContext::Get();
        ui_ctx.m_display_size = ImVec2((float)_WindowSize.width, (float)_WindowSize.height);

        // =========================================================================
        // [修改] 游戏主循环中的 UI 处理
        // =========================================================================
        ui_ctx.SetInputBlocked(_bIsDraggingInWorld);
        m_screen_manager->Update(_DeltaTime);
        m_screen_manager->ApplyPendingChanges();
        m_screen_manager->Draw();
        // =========================================================================

        // Render other standard ImGui windows
       // RenderDebugUI();

        _uiRenderer->EndFrame();
        // Uniform update
        // --------------
        _FreeCamera->SetFov(cfov);
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
                BlackHoleArgs.BlackHoleRelativePosRs = glm::vec4(glm::vec3(_FreeCamera->GetViewMatrix() * glm::vec4(0.0 * BlackHoleArgs.BlackHoleMassSol * kGravityConstant / pow(kSpeedOfLight, 2) * kSolarMass / kLightYearToMeter, 0.0f, -0.000f, 1.0f)) / Rs, 1.0);
                BlackHoleArgs.BlackHoleRelativeDiskNormal = (glm::mat4_cast(_FreeCamera->GetOrientation()) * glm::vec4(0.0f, 1.0f, 0.0f, 0.0f));
                BlackHoleArgs.BlackHoleRelativeDiskTangen = (glm::mat4_cast(_FreeCamera->GetOrientation()) * glm::vec4(1.0f, 0.0f, 0.0f, 0.0f));
                BlackHoleArgs.BlackHoleTime = GameTime * kSpeedOfLight / Rs / kLightYearToMeter;
                BlackHoleArgs.BlackHoleMassSol = 1.49e7f;
                BlackHoleArgs.Spin = 0.0f;
                BlackHoleArgs.Mu = 1.0f;
                BlackHoleArgs.AccretionRate = (2e-6);
                BlackHoleArgs.InterRadiusRs = 2.1;
                BlackHoleArgs.OuterRadiusRs = 12;
                BlackHoleArgs.ThinRs = 0.5;
                BlackHoleArgs.Hopper = 0.0;
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
                BlackHoleArgs.BlackHoleRelativePosRs = glm::vec4(glm::vec3(_FreeCamera->GetViewMatrix() * glm::vec4(0.0 * BlackHoleArgs.BlackHoleMassSol * kGravityConstant / pow(kSpeedOfLight, 2) * kSolarMass / kLightYearToMeter, 0.0f, -0.000f, 1.0f)) / Rs, 1.0);
                BlackHoleArgs.BlackHoleRelativeDiskNormal = (glm::mat4_cast(_FreeCamera->GetOrientation()) * glm::vec4(0.0f, 1.0f, 0.0f, 0.0f));
                BlackHoleArgs.BlackHoleRelativeDiskTangen = (glm::mat4_cast(_FreeCamera->GetOrientation()) * glm::vec4(1.0f, 0.0f, 0.0f, 0.0f));
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
            std::cout << glm::length(_FreeCamera->GetCameraVector(System::Spatial::FCamera::EVectorType::kPosition)) / Rs << std::endl;;

            
            ShaderResourceManager->UpdateEntrieBuffer(CurrentFrame, "BlackHoleArgs", BlackHoleArgs);

            _VulkanContext->SwapImage(*Semaphores_ImageAvailable[CurrentFrame]);
            std::uint32_t ImageIndex = _VulkanContext->GetCurrentImageIndex();

           // BlendAttachmentInfo.setImageView(_VulkanContext->GetSwapchainImageView(ImageIndex));

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
            // 7. Blend Pass: 渲染到 SceneColorAttachment
            {
                vk::ImageMemoryBarrier2 sceneColorInitBarrier(
                    vk::PipelineStageFlagBits2::eTopOfPipe, vk::AccessFlagBits2::eNone,
                    vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite,
                    vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal,
                    vk::QueueFamilyIgnored, vk::QueueFamilyIgnored,
                    *SceneColorAttachment->GetImage(), SubresourceRange
                );
                CurrentBuffer->pipelineBarrier2(vk::DependencyInfo().setImageMemoryBarriers(sceneColorInitBarrier));

                vk::RenderingInfo sceneRenderingInfo = vk::RenderingInfo()
                    .setRenderArea(vk::Rect2D({ 0, 0 }, _WindowSize))
                    .setLayerCount(1)
                    .setColorAttachments(SceneColorAttachmentInfo);

                CurrentBuffer->beginRendering(sceneRenderingInfo);
                CurrentBuffer->bindVertexBuffers(0, *QuadOnlyVertexBuffer.GetBuffer(), Offset);
                CurrentBuffer->bindPipeline(vk::PipelineBindPoint::eGraphics, BlendPipeline);
                CurrentBuffer->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, BlendPipelineLayout, 0, BlendShader->GetDescriptorSets(CurrentFrame), {});
                CurrentBuffer->draw(6, 1, 0, 0);
                CurrentBuffer->endRendering();
            }

            // 8. UI Background Iterative Blur Pass
            // =========================================================================
            {
                // --- 定义模糊迭代次数 ---
                // 3 次迭代会降到 1/8 分辨率再升回来，模糊效果会非常明显
                const int blurPasses = 3;

                // --- 准备 Ping-Pong 指针 ---
                Grt::FColorAttachment* ping = UIBlurPingAttachment.get();
                Grt::FColorAttachment* pong = UIBlurPongAttachment.get();

                // --- 初始屏障 ---
                // SceneColor: ColorAttach -> TransferSrc
                // Ping: Undefined -> TransferDst
                vk::ImageMemoryBarrier2 sceneToSrcBarrier(
                    vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite,
                    vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferRead,
                    vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eTransferSrcOptimal,
                    vk::QueueFamilyIgnored, vk::QueueFamilyIgnored,
                    *SceneColorAttachment->GetImage(), SubresourceRange
                );
                vk::ImageMemoryBarrier2 pingToDstBarrier(
                    vk::PipelineStageFlagBits2::eTopOfPipe, vk::AccessFlagBits2::eNone,
                    vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite,
                    vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal,
                    vk::QueueFamilyIgnored, vk::QueueFamilyIgnored,
                    *ping->GetImage(), SubresourceRange
                );
                std::array initialBarriers = { sceneToSrcBarrier, pingToDstBarrier };
                CurrentBuffer->pipelineBarrier2(vk::DependencyInfo().setImageMemoryBarriers(initialBarriers));

                // --- 第一次降采样 (Full -> Half) ---
                vk::ImageBlit blit = {};
                blit.srcSubresource = { vk::ImageAspectFlagBits::eColor, 0, 0, 1 };
                blit.srcOffsets[1] = vk::Offset3D{ (int32_t)_WindowSize.width, (int32_t)_WindowSize.height, 1 };
                blit.dstSubresource = { vk::ImageAspectFlagBits::eColor, 0, 0, 1 };
                blit.dstOffsets[1] = vk::Offset3D{ (int32_t)(_WindowSize.width / 2), (int32_t)(_WindowSize.height / 2), 1 };
                CurrentBuffer->blitImage(
                    *SceneColorAttachment->GetImage(), vk::ImageLayout::eTransferSrcOptimal,
                    *ping->GetImage(), vk::ImageLayout::eTransferDstOptimal,
                    blit, vk::Filter::eLinear
                );

                // --- 循环降采样 (Ping-Pong) ---
                for (int i = 1; i < blurPasses; ++i)
                {
                    // 屏障: ping(Dst->Src), pong(Undefined/PrevState->Dst)
                    vk::ImageMemoryBarrier2 pingToSrcBarrier(
                        vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite,
                        vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferRead,
                        vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eTransferSrcOptimal,
                        vk::QueueFamilyIgnored, vk::QueueFamilyIgnored,
                        *ping->GetImage(), SubresourceRange
                    );
                    vk::ImageMemoryBarrier2 pongToDstBarrier(
                        vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferRead, // Pong could have been a src before
                        vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite,
                        vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::eTransferDstOptimal,
                        vk::QueueFamilyIgnored, vk::QueueFamilyIgnored,
                        *pong->GetImage(), SubresourceRange
                    );
                    std::array loopBarriers = { pingToSrcBarrier, pongToDstBarrier };
                    CurrentBuffer->pipelineBarrier2(vk::DependencyInfo().setImageMemoryBarriers(loopBarriers));

                    // Blit from ping to pong
                    blit.srcOffsets[1] = vk::Offset3D{ (int32_t)(_WindowSize.width >> i), (int32_t)(_WindowSize.height >> i), 1 };
                    blit.dstOffsets[1] = vk::Offset3D{ (int32_t)(_WindowSize.width >> (i + 1)), (int32_t)(_WindowSize.height >> (i + 1)), 1 };
                    CurrentBuffer->blitImage(
                        *ping->GetImage(), vk::ImageLayout::eTransferSrcOptimal,
                        *pong->GetImage(), vk::ImageLayout::eTransferDstOptimal,
                        blit, vk::Filter::eLinear
                    );

                    // Swap for next iteration
                    std::swap(ping, pong);
                }

                // --- 循环升采样 (Ping-Pong back to Half-res) ---
                for (int i = blurPasses - 1; i > 0; --i)
                {
                    // 屏障: ping(Dst->Src), pong(PrevState->Dst)
                    vk::ImageMemoryBarrier2 pingToSrcBarrier(
                        vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite,
                        vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferRead,
                        vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eTransferSrcOptimal,
                        vk::QueueFamilyIgnored, vk::QueueFamilyIgnored,
                        *ping->GetImage(), SubresourceRange
                    );
                    vk::ImageMemoryBarrier2 pongToDstBarrier(
                        vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferRead,
                        vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite,
                        vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::eTransferDstOptimal,
                        vk::QueueFamilyIgnored, vk::QueueFamilyIgnored,
                        *pong->GetImage(), SubresourceRange
                    );
                    std::array loopBarriers = { pingToSrcBarrier, pongToDstBarrier };
                    CurrentBuffer->pipelineBarrier2(vk::DependencyInfo().setImageMemoryBarriers(loopBarriers));

                    // Blit from ping to pong
                    blit.srcOffsets[1] = vk::Offset3D{ (int32_t)(_WindowSize.width >> (i + 1)), (int32_t)(_WindowSize.height >> (i + 1)), 1 };
                    blit.dstOffsets[1] = vk::Offset3D{ (int32_t)(_WindowSize.width >> i), (int32_t)(_WindowSize.height >> i), 1 };
                    CurrentBuffer->blitImage(
                        *ping->GetImage(), vk::ImageLayout::eTransferSrcOptimal,
                        *pong->GetImage(), vk::ImageLayout::eTransferDstOptimal,
                        blit, vk::Filter::eLinear
                    );

                    // Swap for next iteration
                    std::swap(ping, pong);
                }

                // --- 最后一次 Blit 到最终的 UIBlurAttachment ---
                vk::ImageMemoryBarrier2 finalSrcBarrier(
                    vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite,
                    vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferRead,
                    vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eTransferSrcOptimal,
                    vk::QueueFamilyIgnored, vk::QueueFamilyIgnored,
                    *ping->GetImage(), SubresourceRange
                );
                vk::ImageMemoryBarrier2 finalDstBarrier(
                    vk::PipelineStageFlagBits2::eTopOfPipe, vk::AccessFlagBits2::eNone,
                    vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite,
                    vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal,
                    vk::QueueFamilyIgnored, vk::QueueFamilyIgnored,
                    *UIBlurAttachment->GetImage(), SubresourceRange
                );
                std::array finalBlitBarriers = { finalSrcBarrier, finalDstBarrier };
                CurrentBuffer->pipelineBarrier2(vk::DependencyInfo().setImageMemoryBarriers(finalBlitBarriers));

                blit.srcOffsets[1] = vk::Offset3D{ (int32_t)(_WindowSize.width / 2), (int32_t)(_WindowSize.height / 2), 1 };
                blit.dstOffsets[1] = vk::Offset3D{ (int32_t)(_WindowSize.width / 2), (int32_t)(_WindowSize.height / 2), 1 };
                CurrentBuffer->blitImage(
                    *ping->GetImage(), vk::ImageLayout::eTransferSrcOptimal,
                    *UIBlurAttachment->GetImage(), vk::ImageLayout::eTransferDstOptimal,
                    blit, vk::Filter::eLinear
                );

                // --- Final Barrier for UI Sampling ---
                vk::ImageMemoryBarrier2 uiBlurReadyBarrier(
                    vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite,
                    vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderRead,
                    vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal,
                    vk::QueueFamilyIgnored, vk::QueueFamilyIgnored,
                    *UIBlurAttachment->GetImage(), SubresourceRange
                );
                CurrentBuffer->pipelineBarrier2(vk::DependencyInfo().setImageMemoryBarriers(uiBlurReadyBarrier));
            }

            // 9. Copy Scene to Swapchain
            {
                vk::ImageMemoryBarrier2 swapchainCopyDstBarrier(
                    vk::PipelineStageFlagBits2::eTopOfPipe, vk::AccessFlagBits2::eNone,
                    vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite,
                    vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal,
                    vk::QueueFamilyIgnored, vk::QueueFamilyIgnored,
                    _VulkanContext->GetSwapchainImage(ImageIndex), SubresourceRange
                );
                CurrentBuffer->pipelineBarrier2(vk::DependencyInfo().setImageMemoryBarriers(swapchainCopyDstBarrier));

                vk::ImageCopy copyRegion = {};
                copyRegion.srcSubresource = { vk::ImageAspectFlagBits::eColor, 0, 0, 1 };
                copyRegion.dstSubresource = { vk::ImageAspectFlagBits::eColor, 0, 0, 1 };
                copyRegion.extent = vk::Extent3D{ _WindowSize.width, _WindowSize.height, 1 };

                CurrentBuffer->copyImage(
                    *SceneColorAttachment->GetImage(), vk::ImageLayout::eTransferSrcOptimal,
                    _VulkanContext->GetSwapchainImage(ImageIndex), vk::ImageLayout::eTransferDstOptimal,
                    copyRegion
                );
            }

            // 10. UI Render Pass (on top of Swapchain)
            {
                vk::ImageMemoryBarrier2 swapchainUIBarrier(
                    vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite,
                    vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite,
                    vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eColorAttachmentOptimal,
                    vk::QueueFamilyIgnored, vk::QueueFamilyIgnored,
                    _VulkanContext->GetSwapchainImage(ImageIndex), SubresourceRange
                );
                CurrentBuffer->pipelineBarrier2(vk::DependencyInfo().setImageMemoryBarriers(swapchainUIBarrier));

                vk::RenderingAttachmentInfo uiAttachmentInfo = vk::RenderingAttachmentInfo()
                    .setImageView(_VulkanContext->GetSwapchainImageView(ImageIndex))
                    .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
                    .setLoadOp(vk::AttachmentLoadOp::eLoad)
                    .setStoreOp(vk::AttachmentStoreOp::eStore);

                vk::RenderingInfo uiRenderingInfo = vk::RenderingInfo()
                    .setRenderArea(vk::Rect2D({ 0, 0 }, _WindowSize))
                    .setLayerCount(1)
                    .setColorAttachments(uiAttachmentInfo);

                CurrentBuffer->beginRendering(uiRenderingInfo);
                _uiRenderer->Render(*CurrentBuffer);
                CurrentBuffer->endRendering();
            }

            // 11. Final Present Barrier
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

            // =========================================================================
            // [新代码块结束]
            // =========================================================================

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

    _FreeCamera = std::make_unique<SysSpa::FCamera>(glm::vec3(0.0f, 0.0f, 0.0f), 0.2, 2.5, cfov);
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

            _bIsDraggingInWorld = true;
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
            _bIsDraggingInWorld = false;

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
    else if (_bLeftMousePressedInWorld && _bIsDraggingInWorld && s_FrameCooldown == 0)
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
        ImGui::Text("Camera Position: (%.13f, %.13f, %.13f)",
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
