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
#include <fstream>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp> 

FGameArgs GameArgs{};
FBlackHoleArgs BlackHoleArgs{};
FMatrices Matrices;
FLightMaterial LightMaterial;
float cfov = 60.0f;
float camsmth = 30.0f;
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
    // [新增] 预计算阶段的附件
    std::unique_ptr<Grt::FColorAttachment> DistortionAttachment; // 对应 OutDistortionFlag (Set 1, Binding 3)
    std::unique_ptr<Grt::FColorAttachment> VolumetricAttachment; // 对应 OutVolumetric (Set 1, Binding 4)
    // [修改] BlackHoleAttachment 现在作为合成后的输出
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


    vk::RenderingAttachmentInfo DistortionAttachmentInfo = vk::RenderingAttachmentInfo()
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eStore)
        .setClearValue(vk::ClearValue(vk::ClearColorValue(std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f})));

    // Prepass 输出 1
    vk::RenderingAttachmentInfo VolumetricAttachmentInfo = vk::RenderingAttachmentInfo()
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eStore)
        .setClearValue(vk::ClearValue(vk::ClearColorValue(std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f})));

    // Composite 输出
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

    vk::Extent2D HalfWindowSize = { _WindowSize.width / 2, _WindowSize.height / 2 };
    if (HalfWindowSize.width == 0) HalfWindowSize.width = 1;
    if (HalfWindowSize.height == 0) HalfWindowSize.height = 1;

    auto CreateFramebuffers = [&]() -> void
    {
        _VulkanContext->WaitIdle();

        vk::Extent2D HalfWindowSize = { _WindowSize.width / 2, _WindowSize.height / 2 };
        if (HalfWindowSize.width == 0) HalfWindowSize.width = 1;
        if (HalfWindowSize.height == 0) HalfWindowSize.height = 1;

        // 1. History (保持全分辨率)
        HistoryAttachment = std::make_unique<Grt::FColorAttachment>(
            vk::Format::eR16G16B16A16Sfloat, _WindowSize, 1, vk::SampleCountFlagBits::e1,
            vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst);

        // 2. Prepass Attachments (修改为 HalfWindowSize)
        // Distortion: 即使是半分辨率，RG32F 也能保证向量精度
        DistortionAttachment = std::make_unique<Grt::FColorAttachment>(
            vk::Format::eR32G32B32A32Sfloat, HalfWindowSize, 1, vk::SampleCountFlagBits::e1, // <--- 使用 HalfWindowSize
            vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled);

        // Volumetric: 半分辨率
        VolumetricAttachment = std::make_unique<Grt::FColorAttachment>(
            vk::Format::eR16G16B16A16Sfloat, HalfWindowSize, 1, vk::SampleCountFlagBits::e1, // <--- 使用 HalfWindowSize
            vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled);

        // 3. Composite Output (保持全分辨率)
        BlackHoleAttachment = std::make_unique<Grt::FColorAttachment>(
            vk::Format::eR16G16B16A16Sfloat, _WindowSize, 1, vk::SampleCountFlagBits::e1,
            vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage |
            vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eColorAttachment);

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
        DistortionAttachmentInfo.setImageView(*DistortionAttachment->GetImageView()); // New
        VolumetricAttachmentInfo.setImageView(*VolumetricAttachment->GetImageView()); // New
        BlackHoleAttachmentInfo.setImageView(*BlackHoleAttachment->GetImageView());
        PreBloomAttachmentInfo.setImageView(*PreBloomAttachment->GetImageView());
        GaussBlurAttachmentInfo.setImageView(*GaussBlurAttachment->GetImageView());
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

    Art::FShader::FResourceInfo QuadResourceInfo
    {
        { { 0, sizeof(FQuadOnlyVertex), false } },
        { { 0, 0, offsetof(FQuadOnlyVertex, Position) } },
        {
            { 0, 0, false },
            { 0, 1, false }
        }
    };

    Art::FShader::FResourceInfo ComputeResourceInfo
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
    std::vector<std::string> PrepassShaderFiles({ "ScreenQuad.vert.spv", "BlackHole_prepass.frag.spv" });
    std::vector<std::string> CompositeShaderFiles({ "ScreenQuad.vert.spv", "BlackHole_composite.frag.spv" });
    std::vector<std::string> PreBloomShaderFiles({ "PreBloom.comp.spv" });
    std::vector<std::string> GaussBlurShaderFiles({ "GaussBlur.comp.spv" });
    std::vector<std::string> BlendShaderFiles({ "ScreenQuad.vert.spv", "ColorBlend.frag.spv" });

    VmaAllocationCreateInfo TextureAllocationCreateInfo
    {
        .flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        .usage = VMA_MEMORY_USAGE_GPU_ONLY
    };
    AssetManager->AddAsset<Art::FShader>("BlackHolePrepass", PrepassShaderFiles, QuadResourceInfo);
    AssetManager->AddAsset<Art::FShader>("BlackHoleComposite", CompositeShaderFiles, QuadResourceInfo);

    AssetManager->AddAsset<Art::FShader>("PreBloom", PreBloomShaderFiles, ComputeResourceInfo);
    AssetManager->AddAsset<Art::FShader>("GaussBlur", GaussBlurShaderFiles, ComputeResourceInfo);
    AssetManager->AddAsset<Art::FShader>("Blend", BlendShaderFiles, BlendResourceInfo);
    AssetManager->AddAsset<Art::FTextureCube>(
        "Background0", TextureAllocationCreateInfo, "Universe0Skybox", vk::Format::eR8G8B8A8Unorm, vk::Format::eR8G8B8A8Unorm,
        vk::ImageCreateFlagBits::eMutableFormat, true, false);
    AssetManager->AddAsset<Art::FTextureCube>(
        "Antiground0", TextureAllocationCreateInfo, "Antiverse0Skybox", vk::Format::eR8G8B8A8Unorm, vk::Format::eR8G8B8A8Unorm,
        vk::ImageCreateFlagBits::eMutableFormat, true, false);
    AssetManager->AddAsset<Art::FTextureCube>(
        "Background1", TextureAllocationCreateInfo, "Universe1Skybox", vk::Format::eR8G8B8A8Unorm, vk::Format::eR8G8B8A8Unorm,
        vk::ImageCreateFlagBits::eMutableFormat, true, false);
    AssetManager->AddAsset<Art::FTextureCube>(
        "Antiground1", TextureAllocationCreateInfo, "Antiverse1Skybox", vk::Format::eR8G8B8A8Unorm, vk::Format::eR8G8B8A8Unorm,
        vk::ImageCreateFlagBits::eMutableFormat, true, false);
    AssetManager->AddAsset<Art::FTextureCube>(
        "Background2", TextureAllocationCreateInfo, "Universe2Skybox", vk::Format::eR8G8B8A8Unorm, vk::Format::eR8G8B8A8Unorm,
        vk::ImageCreateFlagBits::eMutableFormat, true, false);
    AssetManager->AddAsset<Art::FTextureCube>(
        "Antiground2", TextureAllocationCreateInfo, "Antiverse2Skybox", vk::Format::eR8G8B8A8Unorm, vk::Format::eR8G8B8A8Unorm,
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


    auto* PrepassShader = AssetManager->GetAsset<Art::FShader>("BlackHolePrepass");
    auto* CompositeShader = AssetManager->GetAsset<Art::FShader>("BlackHoleComposite");
    auto* PreBloomShader = AssetManager->GetAsset<Art::FShader>("PreBloom");
    auto* GaussBlurShader = AssetManager->GetAsset<Art::FShader>("GaussBlur");
    auto* BlendShader = AssetManager->GetAsset<Art::FShader>("Blend");
    auto* Background0 = AssetManager->GetAsset<Art::FTextureCube>("Background0");
    auto* Antiground0 = AssetManager->GetAsset<Art::FTextureCube>("Antiground0");
    auto* Background1 = AssetManager->GetAsset<Art::FTextureCube>("Background1");
    auto* Antiground1 = AssetManager->GetAsset<Art::FTextureCube>("Antiground1");
    auto* Background2 = AssetManager->GetAsset<Art::FTextureCube>("Background2");
    auto* Antiground2 = AssetManager->GetAsset<Art::FTextureCube>("Antiground2");
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
    Grt::FShaderResourceManager::FUniformBufferCreateInfo PrepassGameArgsCreateInfo = GameArgsCreateInfo;
    PrepassGameArgsCreateInfo.Name = "GameArgsPrepass";

    Grt::FShaderResourceManager::FUniformBufferCreateInfo BlackHoleArgsCreateInfo
    {
        .Name = "BlackHoleArgs",
        .Fields = { "InverseCamRot;", "BlackHoleRelativePosRs", "BlackHoleRelativeDiskNormal","BlackHoleRelativeDiskTangen","CameraVelocity","DEBUG","Whitehole","InWhichUniverse","Grid","EnableHearHaze","ObserverMode","UniverseSign",
                     "BlackHoleTime","BlackHoleMassSol", "Spin","Q", "Mu", "AccretionRate","BackShiftMax", "InterRadiusRs", "OuterRadiusRs","ThinRs","Hopper", "Brightmut","Darkmut","Reddening","Saturation"
                     , "BlackbodyIntensityExponent","RedShiftColorExponent","RedShiftIntensityExponent","HeatHaze","BackgroundBrightmut","PhotonRingBoost","PhotonRingColorTempBoost","BoostRot","JetRedShiftIntensityExponent","JetBrightmut","JetSaturation","JetShiftMax","BlendWeight"},
        .Set = 0,                                                                                          
        .Binding = 1,
        .Usage = vk::DescriptorType::eUniformBuffer
    };

    auto ShaderResourceManager = Grt::FShaderResourceManager::GetInstance();
    ShaderResourceManager->CreateBuffers<FGameArgs>(GameArgsCreateInfo);
    ShaderResourceManager->CreateBuffers<FGameArgs>(PrepassGameArgsCreateInfo);
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
    SamplerCreateInfo.setMagFilter(vk::Filter::eNearest).setMinFilter(vk::Filter::eNearest);
    Grt::FVulkanSampler PointSampler(SamplerCreateInfo);

    auto CreatePostDescriptors = [&]() -> void
    {
        ImageInfos.clear();
        ImageInfos.push_back(Background0->CreateDescriptorImageInfo(Sampler));
        PrepassShader->WriteSharedDescriptors(1, 1, vk::DescriptorType::eCombinedImageSampler, ImageInfos);

        ImageInfos.clear();
        ImageInfos.push_back(Antiground0->CreateDescriptorImageInfo(Sampler));
        PrepassShader->WriteSharedDescriptors(1, 2, vk::DescriptorType::eCombinedImageSampler, ImageInfos);


        ImageInfos.clear();
        ImageInfos.push_back(Background1->CreateDescriptorImageInfo(Sampler));
        PrepassShader->WriteSharedDescriptors(1, 3, vk::DescriptorType::eCombinedImageSampler, ImageInfos);

        ImageInfos.clear();
        ImageInfos.push_back(Antiground1->CreateDescriptorImageInfo(Sampler));
        PrepassShader->WriteSharedDescriptors(1, 4, vk::DescriptorType::eCombinedImageSampler, ImageInfos);


        ImageInfos.clear();
        ImageInfos.push_back(Background2->CreateDescriptorImageInfo(Sampler));
        PrepassShader->WriteSharedDescriptors(1, 5, vk::DescriptorType::eCombinedImageSampler, ImageInfos);

        ImageInfos.clear();
        ImageInfos.push_back(Antiground2->CreateDescriptorImageInfo(Sampler));
        PrepassShader->WriteSharedDescriptors(1, 6, vk::DescriptorType::eCombinedImageSampler, ImageInfos);

        // 2. Composite Descriptors (Set 1)
        // ----------------------------------------------------
        // Binding 0: History Texture
        ImageInfos.clear();
        vk::DescriptorImageInfo HistoryFrameImageInfo(nullptr, *HistoryAttachment->GetImageView(), vk::ImageLayout::eShaderReadOnlyOptimal);
        ImageInfos.push_back(HistoryFrameImageInfo);
        CompositeShader->WriteSharedDescriptors(1, 0, vk::DescriptorType::eSampledImage, ImageInfos);

        ImageInfos.clear();
        ImageInfos.push_back(Background0->CreateDescriptorImageInfo(Sampler));
        CompositeShader->WriteSharedDescriptors(1, 1, vk::DescriptorType::eCombinedImageSampler, ImageInfos);

        ImageInfos.clear();
        ImageInfos.push_back(Antiground0->CreateDescriptorImageInfo(Sampler));
        CompositeShader->WriteSharedDescriptors(1, 2, vk::DescriptorType::eCombinedImageSampler, ImageInfos);


        ImageInfos.clear();
        ImageInfos.push_back(Background1->CreateDescriptorImageInfo(Sampler));
        CompositeShader->WriteSharedDescriptors(1, 3, vk::DescriptorType::eCombinedImageSampler, ImageInfos);

        ImageInfos.clear();
        ImageInfos.push_back(Antiground1->CreateDescriptorImageInfo(Sampler));
        CompositeShader->WriteSharedDescriptors(1, 4, vk::DescriptorType::eCombinedImageSampler, ImageInfos);


        ImageInfos.clear();
        ImageInfos.push_back(Background2->CreateDescriptorImageInfo(Sampler));
        CompositeShader->WriteSharedDescriptors(1, 5, vk::DescriptorType::eCombinedImageSampler, ImageInfos);

        ImageInfos.clear();
        ImageInfos.push_back(Antiground2->CreateDescriptorImageInfo(Sampler));
        CompositeShader->WriteSharedDescriptors(1, 6, vk::DescriptorType::eCombinedImageSampler, ImageInfos);




        ImageInfos.clear();
        vk::DescriptorImageInfo DistortionImageInfo(*PointSampler, *DistortionAttachment->GetImageView(), vk::ImageLayout::eShaderReadOnlyOptimal);
        ImageInfos.push_back(DistortionImageInfo);
        CompositeShader->WriteSharedDescriptors(1, 7, vk::DescriptorType::eCombinedImageSampler, ImageInfos);

        ImageInfos.clear();
        vk::DescriptorImageInfo VolumetricImageInfo(*FramebufferSampler, *VolumetricAttachment->GetImageView(), vk::ImageLayout::eShaderReadOnlyOptimal);
        ImageInfos.push_back(VolumetricImageInfo);
        CompositeShader->WriteSharedDescriptors(1, 8, vk::DescriptorType::eCombinedImageSampler, ImageInfos);

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

    // 绑定 UBO
    std::vector<std::string> BindShaders{ "BlackHoleComposite", "PreBloom", "GaussBlur", "Blend" };
    ShaderResourceManager->BindShadersToBuffers("GameArgs", BindShaders);
    ShaderResourceManager->BindShaderToBuffers("GameArgsPrepass", "BlackHolePrepass");
    // 注意：只给 Prepass 绑定 BlackHoleArgs 也可以，但 Composite 可能需要一些参数（如 BlendWeight），所以都绑定
    ShaderResourceManager->BindShaderToBuffers("BlackHoleArgs", "BlackHolePrepass");
    ShaderResourceManager->BindShaderToBuffers("BlackHoleArgs", "BlackHoleComposite");





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

    // =========================================================================
    // [创建] Pipelines
    // =========================================================================
    auto* PipelineManager = Grt::FPipelineManager::GetInstance();
    vk::PipelineColorBlendAttachmentState ColorBlendAttachmentState = vk::PipelineColorBlendAttachmentState()
        .setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);

    // 1. Prepass Pipeline (2 Outputs)
    std::array<vk::Format, 2> PrepassFormats{
        vk::Format::eR32G32B32A32Sfloat, // Distortion
        vk::Format::eR16G16B16A16Sfloat  // Volumetric
    };
    vk::PipelineRenderingCreateInfo PrepassRenderingCreateInfo = vk::PipelineRenderingCreateInfo()
        .setColorAttachmentCount(2)
        .setColorAttachmentFormats(PrepassFormats);

    Grt::FGraphicsPipelineCreateInfoPack PrepassCreateInfoPack;
    PrepassCreateInfoPack.GraphicsPipelineCreateInfo.setPNext(&PrepassRenderingCreateInfo);
    PrepassCreateInfoPack.InputAssemblyStateCreateInfo.setTopology(vk::PrimitiveTopology::eTriangleList);
    PrepassCreateInfoPack.ColorBlendAttachmentStates = { ColorBlendAttachmentState, ColorBlendAttachmentState }; // 两个附件都需要 Blend State (虽然通常 Disable Blend)
    PrepassCreateInfoPack.Viewports.emplace_back(
        0.0f, static_cast<float>(HalfWindowSize.height), static_cast<float>(HalfWindowSize.width),
        -static_cast<float>(HalfWindowSize.height), 0.0f, 1.0f
    );
    PrepassCreateInfoPack.Scissors.emplace_back(vk::Offset2D(), HalfWindowSize);

    PipelineManager->CreateGraphicsPipeline("BlackHolePrepassPipeline", "BlackHolePrepass", PrepassCreateInfoPack);

    // 2. Composite Pipeline (1 Output)
    std::array<vk::Format, 1> CompositeFormat{ vk::Format::eR16G16B16A16Sfloat };
    vk::PipelineRenderingCreateInfo CompositeRenderingCreateInfo = vk::PipelineRenderingCreateInfo()
        .setColorAttachmentCount(1)
        .setColorAttachmentFormats(CompositeFormat);

    Grt::FGraphicsPipelineCreateInfoPack CompositeCreateInfoPack;
    CompositeCreateInfoPack.GraphicsPipelineCreateInfo.setPNext(&CompositeRenderingCreateInfo);
    CompositeCreateInfoPack.InputAssemblyStateCreateInfo.setTopology(vk::PrimitiveTopology::eTriangleList);
    CompositeCreateInfoPack.ColorBlendAttachmentStates.emplace_back(ColorBlendAttachmentState);
    CompositeCreateInfoPack.Viewports.emplace_back(0.0f, static_cast<float>(_WindowSize.height), static_cast<float>(_WindowSize.width), -static_cast<float>(_WindowSize.height), 0.0f, 1.0f);
    CompositeCreateInfoPack.Scissors.emplace_back(vk::Offset2D(), _WindowSize);

    PipelineManager->CreateGraphicsPipeline("BlackHoleCompositePipeline", "BlackHoleComposite", CompositeCreateInfoPack);

    // 3. Other Pipelines
    std::array<vk::Format, 1> SceneColorFormat{ vk::Format::eR8G8B8A8Unorm };
    vk::PipelineRenderingCreateInfo BlendRenderingCreateInfo = vk::PipelineRenderingCreateInfo().setColorAttachmentCount(1).setColorAttachmentFormats(SceneColorFormat);
    Grt::FGraphicsPipelineCreateInfoPack BlendCreateInfoPack = CompositeCreateInfoPack; // 复用配置
    BlendCreateInfoPack.GraphicsPipelineCreateInfo.setPNext(&BlendRenderingCreateInfo);

    PipelineManager->CreateGraphicsPipeline("BlendPipeline", "Blend", BlendCreateInfoPack);
    PipelineManager->CreateComputePipeline("PreBloomPipeline", "PreBloom");
    PipelineManager->CreateComputePipeline("GaussBlurPipeline", "GaussBlur");

    vk::Pipeline PrepassPipeline;
    vk::Pipeline CompositePipeline;
    vk::Pipeline PreBloomPipeline;
    vk::Pipeline GaussBlurPipeline;
    vk::Pipeline BlendPipeline;

    auto GetPipelines = [&]() -> void
    {
        PrepassPipeline = PipelineManager->GetPipeline("BlackHolePrepassPipeline");
        CompositePipeline = PipelineManager->GetPipeline("BlackHoleCompositePipeline");
        PreBloomPipeline = PipelineManager->GetPipeline("PreBloomPipeline");
        GaussBlurPipeline = PipelineManager->GetPipeline("GaussBlurPipeline");
        BlendPipeline = PipelineManager->GetPipeline("BlendPipeline");
    };

    GetPipelines();
    _VulkanContext->RegisterAutoRemovedCallbacks(Grt::FVulkanContext::ECallbackType::kCreateSwapchain, "GetPipelines", GetPipelines);

    auto PrepassPipelineLayout = PipelineManager->GetPipelineLayout("BlackHolePrepassPipeline");
    auto CompositePipelineLayout = PipelineManager->GetPipelineLayout("BlackHoleCompositePipeline");
    auto PreBloomPipelineLayout = PipelineManager->GetPipelineLayout("PreBloomPipeline");
    auto GaussBlurPipelineLayout = PipelineManager->GetPipelineLayout("GaussBlurPipeline");
    auto BlendPipelineLayout = PipelineManager->GetPipelineLayout("BlendPipeline");

    // Fences & Command Buffers
    std::vector<Grt::FVulkanFence> InFlightFences;
    std::vector<Grt::FVulkanSemaphore> Semaphores_ImageAvailable;
    std::vector<Grt::FVulkanSemaphore> Semaphores_RenderFinished;
    for (std::size_t i = 0; i != Config::Graphics::kMaxFrameInFlight; ++i)
    {
        InFlightFences.emplace_back(vk::FenceCreateFlagBits::eSignaled);
        Semaphores_ImageAvailable.emplace_back(vk::SemaphoreCreateFlags());
        Semaphores_RenderFinished.emplace_back(vk::SemaphoreCreateFlags());
    }

    std::vector<Grt::FVulkanCommandBuffer> GraphicsCommandBuffers(Config::Graphics::kMaxFrameInFlight);
    _VulkanContext->GetGraphicsCommandPool().AllocateBuffers(vk::CommandBufferLevel::ePrimary, GraphicsCommandBuffers);

    vk::DeviceSize Offset = 0;
    std::uint32_t  CurrentFrame = 0;
    vk::ImageSubresourceRange SubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);

    // Init History Frame (保持不变)
    auto InitHistoryFrame = [&]() -> void
    {
        vk::ImageMemoryBarrier2 InitHistoryBarrier(vk::PipelineStageFlagBits2::eTopOfPipe, vk::AccessFlagBits2::eNone, vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderRead, vk::ImageLayout::eUndefined, vk::ImageLayout::eShaderReadOnlyOptimal, vk::QueueFamilyIgnored, vk::QueueFamilyIgnored, *HistoryAttachment->GetImage(), SubresourceRange);
        vk::DependencyInfo InitialDependencyInfo = vk::DependencyInfo().setDependencyFlags(vk::DependencyFlagBits::eByRegion).setImageMemoryBarriers(InitHistoryBarrier);
        auto& CommandBuffer = _VulkanContext->GetTransferCommandBuffer();
        CommandBuffer.Begin();
        CommandBuffer->pipelineBarrier2(InitialDependencyInfo);
        CommandBuffer.End();
        _VulkanContext->ExecuteGraphicsCommands(CommandBuffer);
    };
    InitHistoryFrame();
    _VulkanContext->RegisterAutoRemovedCallbacks(Grt::FVulkanContext::ECallbackType::kCreateSwapchain, "InitHistoryFrame", InitHistoryFrame);

    // UI Init (保持不变)
    System::UI::AppContext appContext{ .Application = this, .UIRenderer = _uiRenderer.get(), .GameArgs = &GameArgs, .BlackHoleArgs = &BlackHoleArgs, .GameTime = &GameTime, .TimeRate = &TimeRate, .RealityTime = &RealityTime, .RKKVID = RKKVID, .stage0ID = stage0ID, .stage1ID = stage1ID, .stage2ID = stage2ID, .stage3ID = stage3ID, .stage4ID = stage4ID };
    m_screen_manager = std::make_unique<System::UI::ScreenManager>();
    m_screen_manager->RegisterScreen("Game", std::make_shared<System::UI::GameScreen>(appContext));
    m_screen_manager->RegisterScreen("MainMenu", std::make_shared<System::UI::MainMenuScreen>(appContext));
    m_screen_manager->RequestPushScreen("MainMenu");
    m_screen_manager->ApplyPendingChanges();

    glm::vec4 LastBlackHoleRelativePos(0.0f, 0.0f, 0.0f, 1.0f);
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
        RenderDebugUI();

        _uiRenderer->EndFrame();
        // Uniform update
        // --------------
        _FreeCamera->SetFov(cfov);


        {
            float Rs = 2.0 * abs(BlackHoleArgs.BlackHoleMassSol) * kGravityConstant / pow(kSpeedOfLight, 2) * kSolarMass / kLightYearToMeter;
            if (FrameCount <= 10)
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
                FGameArgs PrepassArgs = GameArgs;
                PrepassArgs.Resolution = GameArgs.Resolution * 0.5f; // <--- 关键：分辨率减半
                ShaderResourceManager->UpdateEntrieBuffer(CurrentFrame, "GameArgsPrepass", PrepassArgs);
                BlackHoleArgs.InverseCamRot = glm::mat4_cast(glm::conjugate(_FreeCamera->GetOrientation()));
                BlackHoleArgs.BlackHoleRelativePosRs = glm::vec4(glm::vec3(_FreeCamera->GetViewMatrix() * glm::vec4(0.0 * BlackHoleArgs.BlackHoleMassSol * kGravityConstant / pow(kSpeedOfLight, 2) * kSolarMass / kLightYearToMeter, 0.0f, -0.000f, 1.0f)) / Rs, 1.0);
                BlackHoleArgs.BlackHoleRelativeDiskNormal = (glm::mat4_cast(_FreeCamera->GetOrientation()) * glm::vec4(0.0f, 1.0f, 0.0f, 0.0f));
                BlackHoleArgs.BlackHoleRelativeDiskTangen = (glm::mat4_cast(_FreeCamera->GetOrientation()) * glm::vec4(1.0f, 0.0f, 0.0f, 0.0f));
                BlackHoleArgs.CameraVelocity = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
                BlackHoleArgs.DEBUG = 0;
				BlackHoleArgs.Whitehole = 0;
				BlackHoleArgs.InWhichUniverse = 0;
                BlackHoleArgs.Grid = 0;
				BlackHoleArgs.EnableHearHaze = 1;
                BlackHoleArgs.ObserverMode = 0;
                BlackHoleArgs.UniverseSign = 1.0;
                BlackHoleArgs.BlackHoleTime = GameTime * kSpeedOfLight / Rs / kLightYearToMeter;
                BlackHoleArgs.BlackHoleMassSol = 1.49e7f;
                BlackHoleArgs.Spin = 0.7f;
                BlackHoleArgs.Q = 0.7f;
                BlackHoleArgs.Mu = 1.0f;
                BlackHoleArgs.AccretionRate = (5e-4);
				BlackHoleArgs.BackShiftMax = 1.02f;
                BlackHoleArgs.InterRadiusRs = 1.5;
                BlackHoleArgs.OuterRadiusRs = 25;
                BlackHoleArgs.ThinRs = 0.75;
                BlackHoleArgs.Hopper = 0.24;
                BlackHoleArgs.Brightmut = 1.0;
                BlackHoleArgs.Darkmut = 0.5;
                BlackHoleArgs.Reddening = 0.3;
                BlackHoleArgs.Saturation = 0.5;
                BlackHoleArgs.BlackbodyIntensityExponent = 0.5;
                BlackHoleArgs.RedShiftColorExponent = 3.0;
                BlackHoleArgs.RedShiftIntensityExponent = 4.0;
				BlackHoleArgs.HeatHaze = 1.0;
				BlackHoleArgs.BackgroundBrightmut = 1.0;
				BlackHoleArgs.PhotonRingBoost = 0.0;
				BlackHoleArgs.PhotonRingColorTempBoost = 2.0;
				BlackHoleArgs.BoostRot = 0.75;
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
                FGameArgs PrepassArgs = GameArgs;
                PrepassArgs.Resolution = GameArgs.Resolution * 0.5f; // <--- 关键：分辨率减半
                ShaderResourceManager->UpdateEntrieBuffer(CurrentFrame, "GameArgsPrepass", PrepassArgs);
                BlackHoleArgs.BlackHoleTime = GameTime * kSpeedOfLight / Rs / kLightYearToMeter;
                BlackHoleArgs.InverseCamRot = glm::mat4_cast(glm::conjugate(_FreeCamera->GetOrientation()));
                BlackHoleArgs.BlackHoleRelativePosRs = glm::vec4(glm::vec3(_FreeCamera->GetViewMatrix() * glm::vec4(0.0f, 0.0f, -0.000f, 1.0f)) / Rs, 1.0);
                BlackHoleArgs.BlackHoleRelativeDiskNormal = (glm::mat4_cast(_FreeCamera->GetOrientation()) * glm::vec4(0.0f, 1.0f, 0.0f, 0.0f));
                BlackHoleArgs.BlackHoleRelativeDiskTangen = (glm::mat4_cast(_FreeCamera->GetOrientation()) * glm::vec4(1.0f, 0.0f, 0.0f, 0.0f));




                // 修正后的代码
                glm::vec3 posDiff = _FreeCamera->GetCameraVector(System::Spatial::FCamera::EVectorType::kPosition) - LastCameraWorldPos;

                // 计算真实的无量纲物理速度 (v / c)
                // posDiff 的单位是光年，转成米后除以 (时间 * 光速)
                glm::vec3 currentDimVelocity = posDiff * (kLightYearToMeter / float(_LastDeltaTime * TimeRate * kSpeedOfLight));

                // 平滑过渡
                BlackHoleArgs.CameraVelocity += float(1.0 - exp(-_DeltaTime / 0.1)) * (glm::vec4(currentDimVelocity, 0.0f) - BlackHoleArgs.CameraVelocity);
                if (glm::any(glm::isnan(BlackHoleArgs.CameraVelocity)) || glm::any(glm::isinf(BlackHoleArgs.CameraVelocity)))
				{
					BlackHoleArgs.CameraVelocity = glm::vec4(0.0f);
				}
            }

            Rs = 2.0 * abs(BlackHoleArgs.BlackHoleMassSol) * kGravityConstant / pow(kSpeedOfLight, 2) * kSolarMass / kLightYearToMeter;
            BlackHoleArgs.BlendWeight = (1.0 - pow(0.5, (_DeltaTime) / std::max(std::min((0.131 * 36.0 / (GameArgs.TimeRate) * (Rs / 0.00000465)), 0.5), 0.06)));
            if (!(abs(glm::quat((lastdir - BlackHoleArgs.InverseCamRot)).w - 0.5) < 0.001 * _DeltaTime || abs(glm::quat((lastdir - BlackHoleArgs.InverseCamRot)).w - 0.0) < 0.001 * _DeltaTime) ||
                glm::length(glm::vec3(LastBlackHoleRelativePos - BlackHoleArgs.BlackHoleRelativePosRs)) > (glm::length(glm::vec3(LastBlackHoleRelativePos)) - 1.0) * 0.006 * _DeltaTime  || glm::length(BlackHoleArgs.CameraVelocity)>0.0001)
            {
                BlackHoleArgs.BlendWeight = 1.0f;
            }
            if (int(glfwGetTime()) < 1)
            {
                _FreeCamera->SetTargetOrbitAxis(glm::vec3(0., 1., 0.)); _FreeCamera->SetTargetOrbitCenter(glm::vec3(0, 0, 0));
            }//else{ _FreeCamera->SetTargetOrbitAxis(glm::vec3(0., -1., -0.)); _FreeCamera->SetTargetOrbitCenter(glm::vec3(0.,0.0*5.586e-5f, 0));
           // }
           // _FreeCamera->ProcessMouseMovement(10, 0);
            glm::vec3 pos = _FreeCamera->GetCameraVector(System::Spatial::FCamera::EVectorType::kPosition);
            std::cout << pos.x / Rs << "   " << pos.y / Rs << "   " << pos.z / Rs << std::endl;
            // 2. 物理参数准备
            float M = 0.5f * Rs;
            float a = BlackHoleArgs.Spin * M;      // 物理自旋 a
            float Q_phys = BlackHoleArgs.Q * M;    // 物理电荷 Q (注意量纲跟随M)
            float a2 = a * a;
            float Q2 = Q_phys * Q_phys;

            // 获取当前帧相机世界坐标及 UniverseSign 更新逻辑
            glm::vec3 currentPos = _FreeCamera->GetCameraVector(System::Spatial::FCamera::EVectorType::kPosition);
            if (LastCameraWorldPos.y * currentPos.y <= 0.0f && FrameCount > 1)
            {
                float denom = LastCameraWorldPos.y - currentPos.y;
                if (std::abs(denom) >0)
                {
                    float t = LastCameraWorldPos.y / denom;
                    float intersectX = LastCameraWorldPos.x + t * (currentPos.x - LastCameraWorldPos.x);
                    float intersectZ = LastCameraWorldPos.z + t * (currentPos.z - LastCameraWorldPos.z);
                    float rho2 = intersectX * intersectX + intersectZ * intersectZ;
                    // 奇环判定仅与自旋 a 有关，与 Q 无关
                    if (rho2 < a2)
                    {
                        BlackHoleArgs.UniverseSign *= -1.0f;
                    }
                }
            }
            LastCameraWorldPos = currentPos;

            // 3. 从 KS 坐标求解 BL 半径 r
            // 坐标变换方程中的共焦椭球结构主要由 a 决定，Q 仅改变度规分量系数
            float x2 = pos.x * pos.x;
            float y2 = pos.y * pos.y;
            float z2 = pos.z * pos.z;
            float R2 = x2 + y2 + z2;

            float b = R2 - a2;
            float c = a2 * y2;
            float r2 = 0.5f * (b + std::sqrt(b * b + 4.0f * c));
            float r = std::sqrt(r2) * BlackHoleArgs.UniverseSign;

            // 4. 计算 Kerr-Newman 度规函数 f
            // g_tt = -1 + f. 在 KN 度规中 f = (2Mr - Q^2) / Sigma
            // 原代码分母计算的是 Sigma * r^2 (即 r^4 + a^2*y^2)，所以分子也要乘 r^2
            float sigma_times_r2 = r2 * r2 + a2 * y2;
            float f = ((2.0f * M * r - Q2) * r2) / sigma_times_r2;

            // ... (前置代码保持不变: M, a, Q_phys, r, f 等计算) ...

             // 5. 计算视界半径
            float delta_discriminant = M * M - a2 - Q2;
            float horizon_outer = 0.0f;
            float horizon_inner = 0.0f;
            bool isNakedSingularity = delta_discriminant < 0.0f;

            if (!isNakedSingularity)
            {
                float sqrt_delta = std::sqrt(delta_discriminant);
                horizon_outer = M + sqrt_delta;
                horizon_inner = M - sqrt_delta;
            }
            static float s_last_r = r; // 记录上一帧的 BL 半径 r
            // 仅在内视界存在且观测模式为 2 时执行
            if (!isNakedSingularity && BlackHoleArgs.Whitehole == 1 )
            {
                // 若上一帧还在内视界之外，当前帧进入了内视界（向内穿过）
                if (s_last_r > horizon_inner && r <= horizon_inner&& BlackHoleArgs.UniverseSign==1.0f)
                {
                    // 在 0 和 1 之间翻转 InWhichUniverse
                    BlackHoleArgs.InWhichUniverse = (BlackHoleArgs.InWhichUniverse +1)%3;
                }
            }

            s_last_r = r; // 逻辑判断完后更新 s_last_r 供下一帧使用
            // 6. 区域判定 & 观测者限制警告
            std::string locationStatus = "";
            std::string warningMsg = "";

            // --- 判定禁止静态观者 ---
            // 1. 能层内: g_tt > 0 => f > 1.0
            // 2. 视界间: Delta < 0，r 变为时间坐标，无法保持静止
            bool isBetweenHorizons = (!isNakedSingularity && r < horizon_outer && r > horizon_inner);
            if (f > 1.0f || isBetweenHorizons)
            {
                warningMsg += " [禁止静态观者]";
            }

            // --- 判定禁止自由落体观者 (从无穷远静止下落 E=1) ---
            // 在 KN 度规中，当 2Mr < Q^2 时，引力项被电荷排斥项抵消
            // 导致从无穷远静止下落的观测者在此半径前速度归零并被弹回
            if (2.0f * M * r < Q2)
            {
                warningMsg += " [禁止自由落体]";
            }

            // --- 位置描述文本 ---
            if (r < 0.0f)
            {
                locationStatus = "Antiverse (r < 0)";
            }
            else if (isNakedSingularity)
            {
                locationStatus = "裸奇点周边";
            }
            else
            {
                if (r > horizon_outer)
                {
                    locationStatus = (f > 1.0f) ? "外能层内" : "外静界外";
                }
                else if (r > horizon_inner)
                {
                    locationStatus = "内外视界间";
                }
                else
                {
                    // 内视界内部
                    // 检查是否进入了"排斥力"占主导的区域
                    if (2.0f * M * r < Q2)
                        locationStatus = "重力排斥区 (Repulsive Gravity)";
                    else if (f > 1.0f)
                        locationStatus = "内能层内";
                    else
                        locationStatus = "内静界内";
                }
            }

            std::cout << "r/Rs: " << r / Rs
                << " | Q*: " << BlackHoleArgs.Q
                << " | " << locationStatus
                << warningMsg // 追加警告信息
                << std::endl;



            ShaderResourceManager->UpdateEntrieBuffer(CurrentFrame, "BlackHoleArgs", BlackHoleArgs);

            _VulkanContext->SwapImage(*Semaphores_ImageAvailable[CurrentFrame]);
            std::uint32_t ImageIndex = _VulkanContext->GetCurrentImageIndex();

           // BlendAttachmentInfo.setImageView(_VulkanContext->GetSwapchainImageView(ImageIndex));

            std::uint32_t WorkgroundX = (_WindowSize.width + 9) / 10;
            std::uint32_t WorkgroundY = (_WindowSize.height + 9) / 10;

            // Record BlackHole rendering commands
            // -----------------------------------
            auto& CurrentBuffer = GraphicsCommandBuffers[CurrentFrame];
            CurrentBuffer.Begin(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);


            vk::Extent2D CurrentHalfSize = { _WindowSize.width / 2, _WindowSize.height / 2 };
            if (CurrentHalfSize.width == 0) CurrentHalfSize.width = 1;
            if (CurrentHalfSize.height == 0) CurrentHalfSize.height = 1;

            // 定义视口和裁剪
            vk::Viewport PrepassViewport(
                0.0f, static_cast<float>(CurrentHalfSize.height),
                static_cast<float>(CurrentHalfSize.width), -static_cast<float>(CurrentHalfSize.height),
                0.0f, 1.0f
            );
            vk::Rect2D PrepassScissor({ 0, 0 }, CurrentHalfSize);



            {
                vk::ImageMemoryBarrier2 b1(vk::PipelineStageFlagBits2::eTopOfPipe, vk::AccessFlagBits2::eNone, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal, vk::QueueFamilyIgnored, vk::QueueFamilyIgnored, *DistortionAttachment->GetImage(), SubresourceRange);
                vk::ImageMemoryBarrier2 b2(vk::PipelineStageFlagBits2::eTopOfPipe, vk::AccessFlagBits2::eNone, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal, vk::QueueFamilyIgnored, vk::QueueFamilyIgnored, *VolumetricAttachment->GetImage(), SubresourceRange);
                std::array barriers = { b1, b2 };
                CurrentBuffer->pipelineBarrier2(vk::DependencyInfo().setDependencyFlags(vk::DependencyFlagBits::eByRegion).setImageMemoryBarriers(barriers));
            }

            std::array<vk::RenderingAttachmentInfo, 2> PrepassAttachments = {
                            DistortionAttachmentInfo,
                            VolumetricAttachmentInfo
            };

            vk::RenderingInfo PrepassRenderingInfo = vk::RenderingInfo()
                .setRenderArea(vk::Rect2D({ 0, 0 }, CurrentHalfSize))
                .setLayerCount(1)
                .setColorAttachments(PrepassAttachments); // 或者使用 

            CurrentBuffer->beginRendering(PrepassRenderingInfo);
            CurrentBuffer->bindVertexBuffers(0, *QuadOnlyVertexBuffer.GetBuffer(), Offset);
            CurrentBuffer->bindPipeline(vk::PipelineBindPoint::eGraphics, PrepassPipeline);
            CurrentBuffer->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, PrepassPipelineLayout, 0, PrepassShader->GetDescriptorSets(CurrentFrame), {});
            CurrentBuffer->draw(6, 1, 0, 0);
            CurrentBuffer->endRendering();

            // =====================================================================
            // PASS 2: Composite (Render to BlackHoleAttachment, Reading Prepass)
            // =====================================================================

            // 1. Barrier: Transition Prepass Attachments to ShaderReadOnly
            //    Barrier: Transition Composite Output (BlackHole) to ColorAttachmentOptimal
            {
                // Distortion & Volumetric: Write -> Read
                vk::ImageMemoryBarrier2 b1(vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite, vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderRead, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, vk::QueueFamilyIgnored, vk::QueueFamilyIgnored, *DistortionAttachment->GetImage(), SubresourceRange);
                vk::ImageMemoryBarrier2 b2(vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite, vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderRead, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, vk::QueueFamilyIgnored, vk::QueueFamilyIgnored, *VolumetricAttachment->GetImage(), SubresourceRange);
                // BlackHole: Undef -> Write
                vk::ImageMemoryBarrier2 b3(vk::PipelineStageFlagBits2::eTopOfPipe, vk::AccessFlagBits2::eNone, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal, vk::QueueFamilyIgnored, vk::QueueFamilyIgnored, *BlackHoleAttachment->GetImage(), SubresourceRange);

                std::array barriers = { b1, b2, b3 };
                CurrentBuffer->pipelineBarrier2(vk::DependencyInfo().setDependencyFlags(vk::DependencyFlagBits::eByRegion).setImageMemoryBarriers(barriers));
            }

            vk::RenderingInfo CompositeRenderingInfo = vk::RenderingInfo()
                .setRenderArea(vk::Rect2D({ 0, 0 }, _WindowSize))
                .setLayerCount(1)
                .setColorAttachments(BlackHoleAttachmentInfo);

            CurrentBuffer->beginRendering(CompositeRenderingInfo);
            CurrentBuffer->bindVertexBuffers(0, *QuadOnlyVertexBuffer.GetBuffer(), Offset);
            CurrentBuffer->bindPipeline(vk::PipelineBindPoint::eGraphics, CompositePipeline);
            // 注意：Composite Shader 的 Descriptor Set 包含了 Prepass 的结果
            CurrentBuffer->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, CompositePipelineLayout, 0, CompositeShader->GetDescriptorSets(CurrentFrame), {});
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
    // 注意：全屏模式下透明缓冲通常无效或会导致兼容性问题，根据需求保留
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, true); 
    GLFWmonitor* PrimaryMonitor = nullptr;
    
    if (_bEnableFullscreen)
    {
        // 获取主显示器
        PrimaryMonitor = glfwGetPrimaryMonitor();
        

        const GLFWvidmode* mode = glfwGetVideoMode(PrimaryMonitor);
        _WindowSize.width = mode->width;
        _WindowSize.height = mode->height;
        
    }
    // 将 PrimaryMonitor 传给第四个参数
    _Window = glfwCreateWindow(_WindowSize.width, _WindowSize.height, _WindowTitle.c_str(), PrimaryMonitor, nullptr);
    
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
    _FreeCamera->SetRotationSmoothCoefficient(camsmth);
    _FreeCamera->ProcessTimeEvolution(_DeltaTime);

    CurrentTime = glfwGetTime();
    _LastDeltaTime = _DeltaTime;
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
void FApplication::DumpArgsToJson(const std::string& filepath)
{

    std::ofstream out(filepath);
    if (!out.is_open())
    {
        std::cout << "Failed to open file for dumping args!" << std::endl;
        return;
    }

    out << "{\n";

    // ==========================================
    // 导出 FGameArgs
    // ==========================================
    out << "  \"FGameArgs\": {\n";

    // 宏定义：方便打印标量和 glm 变量（用完即抛）
#define DUMP_G(var) out << "    \"" #var "\": " << GameArgs.var << ",\n"
#define DUMP_G_GLM(var) out << "    \"" #var "\": \"" << glm::to_string(glm::vec2(GameArgs.var)) << "\",\n"

    DUMP_G_GLM(Resolution);
    DUMP_G(FovRadians);
    DUMP_G(Time);
    DUMP_G(GameTime);
    DUMP_G(TimeDelta);
    DUMP_G(TimeRate);

#undef DUMP_G
#undef DUMP_G_GLM
    out << "    \"__end__\": 0\n"; // 防止最后一个逗号导致 JSON 解析报错
    out << "  },\n";

    // ==========================================
    // 导出 FBlackHoleArgs
    // ==========================================
    out << "  \"FBlackHoleArgs\": {\n";

#define DUMP_B(var) out << "    \"" #var "\": " << BlackHoleArgs.var << ",\n"
#define DUMP_B_GLM(var) out << "    \"" #var "\": \"" << glm::to_string(BlackHoleArgs.var) << "\",\n"

    // 向量与矩阵
    DUMP_B_GLM(InverseCamRot);
    DUMP_B_GLM(BlackHoleRelativePosRs);
    DUMP_B_GLM(BlackHoleRelativeDiskNormal);
    DUMP_B_GLM(BlackHoleRelativeDiskTangen);
    DUMP_B_GLM(CameraVelocity);

    // 标量
    DUMP_B(DEBUG);
    DUMP_B(Whitehole);
    DUMP_B(InWhichUniverse);
    DUMP_B(Grid);
    DUMP_B(EnableHearHaze);
    DUMP_B(ObserverMode);
    DUMP_B(UniverseSign);
    DUMP_B(BlackHoleTime);
    DUMP_B(BlackHoleMassSol);
    DUMP_B(Spin);
    DUMP_B(Q);
    DUMP_B(Mu);
    DUMP_B(AccretionRate);
    DUMP_B(InterRadiusRs);
    DUMP_B(OuterRadiusRs);
    DUMP_B(ThinRs);
    DUMP_B(Hopper);
    DUMP_B(Brightmut);
    DUMP_B(Darkmut);
    DUMP_B(Reddening);
    DUMP_B(Saturation);
    DUMP_B(BlackbodyIntensityExponent);
    DUMP_B(RedShiftColorExponent);
    DUMP_B(RedShiftIntensityExponent);
    DUMP_B(HeatHaze);
    DUMP_B(BackgroundBrightmut);
    DUMP_B(PhotonRingBoost);
    DUMP_B(PhotonRingColorTempBoost);
    DUMP_B(BoostRot);
    DUMP_B(JetRedShiftIntensityExponent);
    DUMP_B(JetBrightmut);
    DUMP_B(JetSaturation);
    DUMP_B(JetShiftMax);
    DUMP_B(BlendWeight);

#undef DUMP_B
#undef DUMP_B_GLM
    out << "    \"__end__\": 0\n";
    out << "  }\n";

    out << "}\n";
    out.close();

    std::cout << "Successfully dumped shader parameters to: " << filepath << std::endl;
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
    // 2. 获取当前鼠标位置
    // -----------------------------------------------------------
    double currX, currY;
    glfwGetCursorPos(_Window, &currX, &currY);

    // -----------------------------------------------------------
    // 3. 处理中键：平滑归零摆头 (新增)
    // -----------------------------------------------------------
    bool isMiddleDown = glfwGetMouseButton(_Window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS;
    if (isMiddleDown && !bMouseBlocked)
    {
        _FreeCamera->ResetSway();
    }

    // -----------------------------------------------------------
    // 4. 处理左键：绕轨道旋转中心 (原有逻辑)
    // -----------------------------------------------------------
    bool isLeftDown = glfwGetMouseButton(_Window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    static bool wasLeftDown = false; // 上一帧状态

    static int s_LeftFrameCooldown = 0;
    if (s_LeftFrameCooldown > 0)
    {
        s_LeftFrameCooldown--;
    }

    // [Case A]: 刚按下左键 (Rising Edge)
    if (isLeftDown && !wasLeftDown)
    {
        if (!bMouseBlocked && s_LeftFrameCooldown == 0)
        {
            _bLeftMousePressedInWorld = true;
            _DragStartX = currX;
            _DragStartY = currY;

            _bIsDraggingInWorld = true;
            glfwSetInputMode(_Window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
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

            // 仅当右键没有在拖动时才恢复鼠标显示，防止左右键冲突
            if (!_bIsDraggingRightInWorld)
            {
                glfwSetInputMode(_Window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            }

            glfwSetCursorPos(_Window, _DragStartX, _DragStartY);
            io.MousePos = ImVec2((float)_DragStartX, (float)_DragStartY);

            s_LeftFrameCooldown = 3;
        }
    }
    // [Case C]: 持续按住并拖动
    else if (_bLeftMousePressedInWorld && _bIsDraggingInWorld && s_LeftFrameCooldown == 0)
    {
        if (_bFirstMouse)
        {
            _LastX = currX;
            _LastY = currY;
            _bFirstMouse = false;
        }
        else
        {
            double deltaX = currX - _LastX;
            double deltaY = currY - _LastY;

            _LastX = currX;
            _LastY = currY;

            if (deltaX != 0.0 || deltaY != 0.0)
            {
                _FreeCamera->ProcessMouseMovement(deltaX, deltaY);
            }
        }
    }

    // -----------------------------------------------------------
    // 5. 处理右键：摄像机摆头 (新增逻辑，与左键高度对称)
    // -----------------------------------------------------------
    bool isRightDown = glfwGetMouseButton(_Window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
    static bool wasRightDown = false; // 上一帧状态

    static int s_RightFrameCooldown = 0;
    if (s_RightFrameCooldown > 0)
    {
        s_RightFrameCooldown--;
    }

    // [Case A]: 刚按下右键 (Rising Edge)
    if (isRightDown && !wasRightDown)
    {
        if (!bMouseBlocked && s_RightFrameCooldown == 0)
        {
            _bRightMousePressedInWorld = true;
            _DragRightStartX = currX;
            _DragRightStartY = currY;

            _bIsDraggingRightInWorld = true;
            glfwSetInputMode(_Window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            _bFirstMouseRight = true;
        }
    }
    // [Case B]: 刚松开右键 (Falling Edge)
    else if (!isRightDown && wasRightDown)
    {
        if (_bRightMousePressedInWorld)
        {
            _bRightMousePressedInWorld = false;
            _bIsDraggingRightInWorld = false;

            // 仅当左键没有在拖动时才恢复鼠标显示
            if (!_bIsDraggingInWorld)
            {
                glfwSetInputMode(_Window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            }

            glfwSetCursorPos(_Window, _DragRightStartX, _DragRightStartY);
            io.MousePos = ImVec2((float)_DragRightStartX, (float)_DragRightStartY);

            s_RightFrameCooldown = 3;
        }
    }
    // [Case C]: 右键持续按住并拖动
    else if (_bRightMousePressedInWorld && _bIsDraggingRightInWorld && s_RightFrameCooldown == 0)
    {
        if (_bFirstMouseRight)
        {
            _LastRightX = currX;
            _LastRightY = currY;
            _bFirstMouseRight = false;
        }
        else
        {
            double deltaX = currX - _LastRightX;
            double deltaY = currY - _LastRightY;

            _LastRightX = currX;
            _LastRightY = currY;

            if (deltaX != 0.0 || deltaY != 0.0)
            {
                // 调用新增的摆头处理函数
                _FreeCamera->ProcessSwayMovement(deltaX, deltaY);
            }
        }
    }

    // 更新鼠标按键历史状态
    wasLeftDown = isLeftDown;
    wasRightDown = isRightDown;

    // -----------------------------------------------------------
    // 6. 处理滚轮缩放 (消费 Buffer)
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
    // 7. 处理键盘移动
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


        static bool wasPDown = false;
        bool isPDown = glfwGetKey(_Window, GLFW_KEY_P) == GLFW_PRESS;
        if (isPDown && !wasPDown)
        {
            std::string filename = "C:/Users/bcy00/Desktop/python乱七八糟/ShaderArgs_" + std::to_string(FrameCount) + ".json";
            DumpArgsToJson(filename);
        }
        wasPDown = isPDown;


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
#include <vector>
#include <cmath>
// 确保引入了 ImGui
// #include "imgui.h"
#include "imgui.h"
#include <cmath>
#include <vector>
void FApplication::RenderDebugUI()
{
    // 如果你已经有全局的 ImGui::Begin()，请确保这里的名字不冲突
    ImGui::Begin("Black Hole Topology Map", nullptr, ImGuiWindowFlags_NoScrollbar);

    // 1. 获取基础物理参数与坐标 (按 Rs 归一化)
    float Rs = 2.0f * std::abs(BlackHoleArgs.BlackHoleMassSol) * kGravityConstant / std::pow(kSpeedOfLight, 2) * kSolarMass / kLightYearToMeter;
    if (Rs < 1e-6f) Rs = 1.0f; // 防御性除零

    float M = 0.5f; // 以 Rs 为单位，M 恒为 0.5
    float a = BlackHoleArgs.Spin * M;
    float Q = BlackHoleArgs.Q * M;
    float a2 = a * a;
    float Q2 = Q * Q;

    // 获取相机位置和朝向，并转化到 Rs 单位空间
    glm::vec3 camPos = _FreeCamera->GetCameraVector(System::Spatial::FCamera::EVectorType::kPosition) / Rs;
    glm::vec3 camDir = _FreeCamera->GetCameraVector(System::Spatial::FCamera::EVectorType::kFront);

    // 判断所在宇宙状态与裸奇点
    bool isAntiverse = (BlackHoleArgs.UniverseSign < 0.0f);
    float delta_discriminant = M * M - a2 - Q2;
    bool isNakedSingularity = (delta_discriminant < 0.0f);
    float r_outer = isNakedSingularity ? 0.0f : M + std::sqrt(delta_discriminant);
    float r_inner = isNakedSingularity ? 0.0f : M - std::sqrt(delta_discriminant);

    // 获取画布信息
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 canvas_p0 = ImGui::GetCursorScreenPos();
    ImVec2 canvas_sz = ImGui::GetContentRegionAvail();
    if (canvas_sz.x < 100.0f) canvas_sz.x = 100.0f;
    if (canvas_sz.y < 50.0f) canvas_sz.y = 50.0f;

    // 2. 绘制背景颜色
    ImU32 bgColor = isAntiverse ? IM_COL32(40, 10, 15, 255) : IM_COL32(15, 20, 30, 255);
    draw_list->AddRectFilled(canvas_p0, ImVec2(canvas_p0.x + canvas_sz.x, canvas_p0.y + canvas_sz.y), bgColor);

    // 划分为左右两个视图
    float halfWidth = canvas_sz.x * 0.5f;
    ImVec2 centerSide = ImVec2(canvas_p0.x + halfWidth * 0.5f, canvas_p0.y + canvas_sz.y * 0.5f);
    ImVec2 centerTop = ImVec2(canvas_p0.x + halfWidth * 1.5f, canvas_p0.y + canvas_sz.y * 0.5f);

    // 动态缩放逻辑
    float camDist = glm::length(camPos);
    float displayRadius = std::max(1.5f, camDist * 1.2f);
    float scale = std::min(halfWidth, canvas_sz.y) / (2.0f * displayRadius);

    // 坐标转换 Lambda 工具
    auto ToSideScreen = [&](float x, float y) -> ImVec2 { return ImVec2(centerSide.x + x * scale, centerSide.y - y * scale); };
    auto ToTopScreen = [&](float x, float z) -> ImVec2 { return ImVec2(centerTop.x + x * scale, centerTop.y - z * scale); };

    // 3. 绘制辅助网格和标题
    ImU32 axisColor = IM_COL32(100, 100, 100, 150);
    draw_list->AddLine(ImVec2(canvas_p0.x, centerSide.y), ImVec2(canvas_p0.x + halfWidth, centerSide.y), axisColor, 1.0f);
    draw_list->AddLine(ImVec2(centerSide.x, canvas_p0.y), ImVec2(centerSide.x, canvas_p0.y + canvas_sz.y), axisColor, 1.0f);
    draw_list->AddLine(ImVec2(canvas_p0.x + halfWidth, centerTop.y), ImVec2(canvas_p0.x + canvas_sz.x, centerTop.y), axisColor, 1.0f);
    draw_list->AddLine(ImVec2(centerTop.x, canvas_p0.y), ImVec2(centerTop.x, canvas_p0.y + canvas_sz.y), axisColor, 1.0f);
    draw_list->AddLine(ImVec2(canvas_p0.x + halfWidth, canvas_p0.y), ImVec2(canvas_p0.x + halfWidth, canvas_p0.y + canvas_sz.y), IM_COL32(200, 200, 200, 255), 2.0f);

    draw_list->AddText(ImVec2(canvas_p0.x + 10, canvas_p0.y + 10), IM_COL32(255, 255, 255, 255), "Meridian (Y-X) Plane");
    draw_list->AddText(ImVec2(canvas_p0.x + halfWidth + 10, canvas_p0.y + 10), IM_COL32(255, 255, 255, 255), "Top-Down (X-(-Z)) Plane");
    std::string univText = !isAntiverse ? "Status: r > 0 (Universe)" : "Status: r < 0 (Antiverse)";
    draw_list->AddText(ImVec2(canvas_p0.x + 10, canvas_p0.y + 30), !isAntiverse ? IM_COL32(200, 255, 200, 255) : IM_COL32(255, 150, 150, 255), univText.c_str());

    // === 核心修改点 1: 根据是否处于反宇宙，调整线条透明度 ===
    ImU32 alphaStandard = isAntiverse ? 25 : 255;  // 正常宇宙边界在反宇宙中极其淡化
    ImU32 alphaErgo = isAntiverse ? 15 : 200;
    ImU32 alphaCTC = isAntiverse ? 255 : 25;  // CTC 边界在反宇宙中高亮，在正常宇宙中极其淡化

    ImU32 colOuterHorizon = IM_COL32(255, 100, 100, alphaStandard);
    ImU32 colInnerHorizon = IM_COL32(100, 150, 255, alphaStandard);
    ImU32 colErgosphere = IM_COL32(150, 255, 150, alphaErgo);
    ImU32 colInnerErgo = IM_COL32(255, 200, 50, alphaErgo);
    ImU32 colCTC = IM_COL32(200, 100, 255, alphaCTC); // 紫色代表时光机区域
    ImU32 colSingularity = IM_COL32(255, 0, 255, 255);        // 奇环作为连通点，恒定不褪色

    // === 核心修改点 2: 求解 CTC (Closed Timelike Curves) 边界的 Lambda ===
    // 求解 g_phi_phi = 0 在 r < 0 侧的根。方程为: (r^2+a^2)(r^2+a^2 cos^2T) + a^2 sin^2T (2Mr - Q^2) = 0
    auto GetCTCRoots = [&](float cosT, float sinT, float& r_out, float& r_in) -> bool
    {
        auto G = [&](float r)
        {
            float r2 = r * r;
            return (r2 + a2) * (r2 + a2 * cosT * cosT) + a2 * sinT * sinT * (2.0f * M * r - Q2);
        };
        float r_start = 0.0f, r_end = -10.0f; // 扫描 Antiverse 区间
        int steps = 200;
        float dr = (r_end - r_start) / steps;
        std::vector<float> roots;
        float prev_G = G(r_start);

        if (std::abs(prev_G) < 1e-5f)
        { // 特殊处理赤道且 Q=0 时 r=0 是根的情况
            roots.push_back(0.0f);
            prev_G = G(r_start + dr * 0.1f);
        }
        for (int k = 1; k <= steps; ++k)
        {
            float r = r_start + k * dr;
            float curr_G = G(r);
            if (curr_G * prev_G < 0.0f)
            {
                roots.push_back(r - dr * curr_G / (curr_G - prev_G)); // 线性插值求根
            }
            prev_G = curr_G;
        }
        if (roots.size() >= 2)
        {
            r_out = roots[0]; r_in = roots.back();
            return true;
        }
        else if (roots.size() == 1)
        {
            r_out = 0.0f; r_in = roots[0];
            return true;
        }
        return false;
    };

    // 4. 计算和绘制几何边界
    const int segments = 511;
    std::vector<ImVec2> sideOutPts, sideInPts;
    ImVec2 prev_ergo_out, prev_ergo_in, prev_ctc_out, prev_ctc_in;
    bool prev_ergo_valid = false, prev_ctc_valid = false;

    for (int i = 0; i <= segments; ++i)
    {
        float theta = (static_cast<float>(i) / segments) * 2.0f * 3.14159265f;
        float cosTheta = std::cos(theta);
        float sinTheta = std::sin(theta);

        // --- 绘制正常宇宙下的静界 (Ergosphere) ---
        float ergo_discriminant = M * M - a2 * cosTheta * cosTheta - Q2;
        bool ergo_valid = (ergo_discriminant >= 0.0f);
        if (ergo_valid)
        {
            float sqrt_disc = std::sqrt(ergo_discriminant);
            float r_ergo_out = M + sqrt_disc, r_ergo_in = M - sqrt_disc;
            ImVec2 pt_ergo_out = ToSideScreen(std::sqrt(r_ergo_out * r_ergo_out + a2) * sinTheta, r_ergo_out * cosTheta);
            ImVec2 pt_ergo_in = ToSideScreen(std::sqrt(r_ergo_in * r_ergo_in + a2) * sinTheta, r_ergo_in * cosTheta);

            if (prev_ergo_valid)
            {
                draw_list->AddLine(prev_ergo_out, pt_ergo_out, colErgosphere, 1.5f);
                draw_list->AddLine(prev_ergo_in, pt_ergo_in, colInnerErgo, 1.5f);
            }
            else if (i > 0)
            {
                draw_list->AddLine(pt_ergo_out, pt_ergo_in, colErgosphere, 1.5f);
            }
            prev_ergo_out = pt_ergo_out; prev_ergo_in = pt_ergo_in;
        }
        else if (prev_ergo_valid) draw_list->AddLine(prev_ergo_out, prev_ergo_in, colErgosphere, 1.5f);
        prev_ergo_valid = ergo_valid;

        // --- 绘制反宇宙中的 CTC 边界 ---
        float r_ctc_out, r_ctc_in;
        bool ctc_valid = GetCTCRoots(cosTheta, sinTheta, r_ctc_out, r_ctc_in);
        if (ctc_valid)
        {
            // 对于 r < 0, 映射到伪笛卡尔坐标系的公式同样是 Rho = sqrt(r^2+a^2)*sin(t), Z = r*cos(t)
            ImVec2 pt_ctc_out = ToSideScreen(std::sqrt(r_ctc_out * r_ctc_out + a2) * sinTheta, r_ctc_out * cosTheta);
            ImVec2 pt_ctc_in = ToSideScreen(std::sqrt(r_ctc_in * r_ctc_in + a2) * sinTheta, r_ctc_in * cosTheta);

            if (prev_ctc_valid)
            {
                draw_list->AddLine(prev_ctc_out, pt_ctc_out, colCTC, 1.5f);
                draw_list->AddLine(prev_ctc_in, pt_ctc_in, colCTC, 1.5f);
            }
            else if (i > 0)
            {
                draw_list->AddLine(pt_ctc_out, pt_ctc_in, colCTC, 1.5f);
            }
            prev_ctc_out = pt_ctc_out; prev_ctc_in = pt_ctc_in;
        }
        else if (prev_ctc_valid) draw_list->AddLine(prev_ctc_out, prev_ctc_in, colCTC, 1.5f);
        prev_ctc_valid = ctc_valid;

        // --- 俯视图绘制 (赤道面) ---
        if (i == 0)
        {
            float eq_disc = M * M - Q2;
            if (eq_disc >= 0.0f)
            {
                draw_list->AddCircle(centerTop, std::sqrt(std::pow(M + std::sqrt(eq_disc), 2) + a2) * scale, colErgosphere, 64, 1.5f);
                draw_list->AddCircle(centerTop, std::sqrt(std::pow(M - std::sqrt(eq_disc), 2) + a2) * scale, colInnerErgo, 64, 1.5f);
            }
            if (ctc_valid)
            {
                draw_list->AddCircle(centerTop, std::sqrt(r_ctc_out * r_ctc_out + a2) * scale, colCTC, 64, 1.5f);
                draw_list->AddCircle(centerTop, std::sqrt(r_ctc_in * r_ctc_in + a2) * scale, colCTC, 64, 1.5f);
            }
        }

        // --- 绘制正常宇宙视界 ---
        if (!isNakedSingularity)
        {
            sideOutPts.push_back(ToSideScreen(std::sqrt(r_outer * r_outer + a2) * sinTheta, r_outer * cosTheta));
            sideInPts.push_back(ToSideScreen(std::sqrt(r_inner * r_inner + a2) * sinTheta, r_inner * cosTheta));
            if (i == 0)
            {
                draw_list->AddCircle(centerTop, std::sqrt(r_outer * r_outer + a2) * scale, colOuterHorizon, 64, 2.0f);
                draw_list->AddCircle(centerTop, std::sqrt(r_inner * r_inner + a2) * scale, colInnerHorizon, 64, 2.0f);
            }
        }
    }

    if (!isNakedSingularity)
    {
        draw_list->AddPolyline(sideOutPts.data(), sideOutPts.size(), colOuterHorizon, ImDrawFlags_Closed, 2.0f);
        draw_list->AddPolyline(sideInPts.data(), sideInPts.size(), colInnerHorizon, ImDrawFlags_Closed, 2.0f);
    }

    // 5. 绘制奇环 (a) (永远完全不透明显示，作为两个宇宙的连接点)
    draw_list->AddCircleFilled(ToSideScreen(std::abs(a), 0.0f), 4.0f, colSingularity);
    draw_list->AddCircleFilled(ToSideScreen(-std::abs(a), 0.0f), 4.0f, colSingularity);
    draw_list->AddCircle(centerTop, std::abs(a) * scale, colSingularity, 64, 2.0f);

    // 6. 绘制相机位置和朝向
    ImU32 camColor = IM_COL32(255, 255, 0, 255);
    float rho_cam = std::sqrt(camPos.x * camPos.x + camPos.z * camPos.z);
    ImVec2 camSidePos = ToSideScreen(rho_cam, camPos.y);

    float drho = (rho_cam > 1e-6f) ? ((camPos.x * camDir.x + camPos.z * camDir.z) / rho_cam) : std::sqrt(camDir.x * camDir.x + camDir.z * camDir.z);

    float camLineLen = std::max(1.5f, camDist * 0.15f);
    ImVec2 camSideDir = ToSideScreen(rho_cam + drho * camLineLen, camPos.y + camDir.y * camLineLen);
    ImVec2 camTopPos = ToTopScreen(camPos.x, -camPos.z);
    ImVec2 camTopDir = ToTopScreen(camPos.x + camDir.x * camLineLen, -camPos.z - camDir.z * camLineLen);

    auto IsInCanvas = [&](ImVec2 p)
    {
        return p.x >= canvas_p0.x && p.x <= canvas_p0.x + canvas_sz.x &&
            p.y >= canvas_p0.y && p.y <= canvas_p0.y + canvas_sz.y;
    };

    if (IsInCanvas(camSidePos))
    {
        draw_list->AddLine(camSidePos, camSideDir, camColor, 2.0f);
        draw_list->AddCircleFilled(camSidePos, 5.0f, camColor);
    }
    if (IsInCanvas(camTopPos))
    {
        draw_list->AddLine(camTopPos, camTopDir, camColor, 2.0f);
        draw_list->AddCircleFilled(camTopPos, 5.0f, camColor);
    }

    ImGui::End();
}
_NPGS_END