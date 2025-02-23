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
#include "DataStructures.h"

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
            vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst);

        PreBloomAttachment = std::make_unique<Grt::FColorAttachment>(
            vk::Format::eR16G16B16A16Sfloat, _WindowSize, 1, vk::SampleCountFlagBits::e1,
            vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst);

        GaussBlurAttachment = std::make_unique<Grt::FColorAttachment>(
            vk::Format::eR16G16B16A16Sfloat, _WindowSize, 1, vk::SampleCountFlagBits::e1,
            vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst);

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
        {
            { 0, sizeof(FQuadOnlyVertex), false }
        },
        {
            { 0, 0, offsetof(FQuadOnlyVertex, Position) }
        },
        {
            { 0, 0, false },
            { 0, 1, false }
        }
    };

    Art::FShader::FResourceInfo BloomResourceInfo
    {
        {
            { 0, sizeof(FQuadOnlyVertex), false }
        },
        {
            { 0, 0, offsetof(FQuadOnlyVertex, Position) }
        },
        {
            { 0, 0, false }
        },
        {
            { vk::ShaderStageFlagBits::eFragment, { "ibHorizontal" } }
        }
    };

    std::vector<std::string> BlackHoleShaderFiles({ "ScreenQuad.vert.spv", "BlackHole.frag.spv" });
    std::vector<std::string> PreBloomShaderFiles({ "ScreenQuad.vert.spv", "PreBloom.frag.spv" });
    std::vector<std::string> GaussBlurShaderFiles({ "ScreenQuad.vert.spv", "GaussBlur.frag.spv" });
    std::vector<std::string> BlendShaderFiles({ "ScreenQuad.vert.spv", "ColorBlend.frag.spv" });

    AssetManager->AddAsset<Art::FShader>("BlackHole", BlackHoleShaderFiles, BlackHoleResourceInfo);
    AssetManager->AddAsset<Art::FShader>("PreBloom",  PreBloomShaderFiles,  BloomResourceInfo);
    AssetManager->AddAsset<Art::FShader>("GaussBlur", GaussBlurShaderFiles, BloomResourceInfo);
    AssetManager->AddAsset<Art::FShader>("Blend",     BlendShaderFiles,     BloomResourceInfo);

    auto* BlackHoleShader = AssetManager->GetAsset<Art::FShader>("BlackHole");
    auto* PreBloomShader  = AssetManager->GetAsset<Art::FShader>("PreBloom");
    auto* GaussBlurShader = AssetManager->GetAsset<Art::FShader>("GaussBlur");
    auto* BlendShader     = AssetManager->GetAsset<Art::FShader>("Blend");

    Grt::FShaderResourceManager::FUniformBufferCreateInfo GameArgsCreateInfo
    {
        .Name    = "GameArgs",
        .Fields  = { "Resolution", "FovRadians", "Time", "TimeDelta", "TimeRate" },
        .Set     = 0,
        .Binding = 0,
        .Usage   = vk::DescriptorType::eUniformBuffer
    };

    Grt::FShaderResourceManager::FUniformBufferCreateInfo BlackHoleArgsCreateInfo
    {
        .Name    = "BlackHoleArgs",
        .Fields  = { "WorldUpView", "BlackHoleRelativePos", "BlackHoleRelativeDiskNormal",
                     "BlackHoleMassSol", "Spin", "Mu", "AccretionRate", "InterRadiusLy", "OuterRadiusLy" },
        .Set     = 0,
        .Binding = 1,
        .Usage   = vk::DescriptorType::eUniformBuffer
    };

    auto ShaderResourceManager = Grt::FShaderResourceManager::GetInstance();
    ShaderResourceManager->CreateBuffers<FGameArgs>(GameArgsCreateInfo);
    ShaderResourceManager->CreateBuffers<FBlackHoleArgs>(BlackHoleArgsCreateInfo);

    // Create graphics pipeline
    // ------------------------
    vk::SamplerCreateInfo SamplerCreateInfo = Art::FTextureBase::CreateDefaultSamplerCreateInfo();
    std::vector<vk::DescriptorImageInfo> ImageInfos;

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
        vk::DescriptorImageInfo PreBloomImageInfo(
            *FramebufferSampler, *PreBloomAttachment->GetImageView(), vk::ImageLayout::eShaderReadOnlyOptimal);
        vk::DescriptorImageInfo GaussBlurImageInfo(
            *FramebufferSampler, *GaussBlurAttachment->GetImageView(), vk::ImageLayout::eShaderReadOnlyOptimal);

        ImageInfos.push_back(HistoryFrameImageInfo);
        BlackHoleShader->WriteSharedDescriptors(1, 0, vk::DescriptorType::eSampledImage, ImageInfos);

        ImageInfos.clear();
        ImageInfos.push_back(BlackHoleImageInfo);
        PreBloomShader->WriteSharedDescriptors(1, 0, vk::DescriptorType::eCombinedImageSampler, ImageInfos);

        ImageInfos.clear();
        ImageInfos.push_back(PreBloomImageInfo);
        GaussBlurShader->WriteSharedDescriptors(1, 0, vk::DescriptorType::eCombinedImageSampler, ImageInfos);

        ImageInfos.clear();
        ImageInfos.push_back(BlackHoleImageInfo);
        ImageInfos.push_back(GaussBlurImageInfo);
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

    BlackHoleCreateInfoPack.Viewports.emplace_back(0.0f, static_cast<float>(_WindowSize.height),
                                                   static_cast<float>(_WindowSize.width), -static_cast<float>(_WindowSize.height),
                                                   0.0f, 1.0f);
    BlackHoleCreateInfoPack.Scissors.emplace_back(vk::Offset2D(), _WindowSize);

    PipelineManager->CreatePipeline("BlackHolePipeline", "BlackHole", BlackHoleCreateInfoPack);
    PipelineManager->CreatePipeline("PreBloomPipeline",  "PreBloom",  BlackHoleCreateInfoPack);
    PipelineManager->CreatePipeline("GaussBlurPipeline", "GaussBlur", BlackHoleCreateInfoPack);


    vk::PipelineRenderingCreateInfo BlendRenderingCreateInfo = vk::PipelineRenderingCreateInfo()
        .setColorAttachmentCount(1)
        .setColorAttachmentFormats(_VulkanContext->GetSwapchainCreateInfo().imageFormat);

    BlackHoleCreateInfoPack.GraphicsPipelineCreateInfo.setPNext(&BlendRenderingCreateInfo);
    PipelineManager->CreatePipeline("BlendPipeline", "Blend", BlackHoleCreateInfoPack);

    vk::Pipeline BlackHolePipeline;
    vk::Pipeline PreBloomPipeline;
    vk::Pipeline GaussBlurPipeline;
    vk::Pipeline BlendPipeline;

    auto GetPipelines = [&]() -> void
    {
        BlackHolePipeline = PipelineManager->GetPipeline("BlackHolePipeline");
        PreBloomPipeline  = PipelineManager->GetPipeline("PreBloomPipeline");
        GaussBlurPipeline = PipelineManager->GetPipeline("GaussBlurPipeline");
        BlendPipeline     = PipelineManager->GetPipeline("BlendPipeline");
    };

    GetPipelines();

    _VulkanContext->RegisterAutoRemovedCallbacks(Grt::FVulkanContext::ECallbackType::kCreateSwapchain, "GetPipelines", GetPipelines);

    auto BlackHolePipelineLayout = PipelineManager->GetPipelineLayout("BlackHolePipeline");
    auto PreBloomPipelineLayout  = PipelineManager->GetPipelineLayout("PreBloomPipeline");
    auto GaussBlurPipelineLayout = PipelineManager->GetPipelineLayout("GaussBlurPipeline");
    auto BlendPipelineLayout     = PipelineManager->GetPipelineLayout("BlendPipeline");

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

    vk::DeviceSize Offset       = 0;
    std::uint32_t  CurrentFrame = 0;
    glm::vec4      WorldUp(0.0f, 1.0f, 0.0f, 1.0f);

    vk::ImageSubresourceRange SubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);

    auto InitHistoryFrame = [&]() -> void
    {
        vk::ImageMemoryBarrier2 InitHistoryBarrier(vk::PipelineStageFlagBits2::eTopOfPipe,
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

        auto& CommandBuffer = CommandBuffers[0];
        CommandBuffer.Begin();
        CommandBuffer->pipelineBarrier2(InitialDependencyInfo);
        CommandBuffer.End();
        _VulkanContext->ExecuteGraphicsCommands(CommandBuffer);
    };

    InitHistoryFrame();

    _VulkanContext->RegisterAutoRemovedCallbacks(Grt::FVulkanContext::ECallbackType::kCreateSwapchain, "InitHistoryFrame", InitHistoryFrame);

    while (!glfwWindowShouldClose(_Window))
    {
        while (glfwGetWindowAttrib(_Window, GLFW_ICONIFIED))
        {
            glfwWaitEvents();
        }

        InFlightFences[CurrentFrame].WaitAndReset();

        // Uniform update
        // --------------
        GameArgs.Resolution = glm::vec2(_WindowSize.width, _WindowSize.height);
        GameArgs.FovRadians = glm::radians(_FreeCamera->GetCameraZoom());
        GameArgs.Time       = static_cast<float>(glfwGetTime());
        GameArgs.TimeDelta  = static_cast<float>(_DeltaTime);
        GameArgs.TimeRate   = 30.0f;

        ShaderResourceManager->UpdateEntrieBuffer(CurrentFrame, "GameArgs", GameArgs);

        BlackHoleArgs.WorldUpView                 = glm::vec3(glm::mat4_cast(_FreeCamera->GetOrientation()) * WorldUp);
        BlackHoleArgs.BlackHoleRelativePos        = glm::vec3(0.0f, 0.0f, -0.0001f);
        BlackHoleArgs.BlackHoleRelativeDiskNormal = glm::vec3(glm::mat4_cast(_FreeCamera->GetOrientation()) * glm::vec4(0.2f, 1.0f, 0.5f, 1.0f));
        BlackHoleArgs.BlackHoleMassSol            = 1.49e7f;
        BlackHoleArgs.Spin                        = 0.0f;
        BlackHoleArgs.Mu                          = 1.0f;
        BlackHoleArgs.AccretionRate               = 5e15f;
        BlackHoleArgs.InterRadiusLy               = 9.7756e-6f;
        BlackHoleArgs.OuterRadiusLy               = 5.586e-5f;

        ShaderResourceManager->UpdateEntrieBuffer(CurrentFrame, "BlackHoleArgs", BlackHoleArgs);

        _VulkanContext->SwapImage(*Semaphores_ImageAvailable[CurrentFrame]);
        std::uint32_t ImageIndex = _VulkanContext->GetCurrentImageIndex();

        BlendAttachmentInfo.setImageView(_VulkanContext->GetSwapchainImageView(ImageIndex));

        // Record commands
        // ---------------
        auto& CurrentBuffer = CommandBuffers[CurrentFrame];
        CurrentBuffer.Begin(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

        vk::ImageMemoryBarrier2 InitSwapchainBarrier(vk::PipelineStageFlagBits2::eTopOfPipe,
                                                     vk::AccessFlagBits2::eNone,
                                                     vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                                                     vk::AccessFlagBits2::eColorAttachmentWrite,
                                                     vk::ImageLayout::eUndefined,
                                                     vk::ImageLayout::eColorAttachmentOptimal,
                                                     vk::QueueFamilyIgnored,
                                                     vk::QueueFamilyIgnored,
                                                     _VulkanContext->GetSwapchainImage(ImageIndex),
                                                     SubresourceRange);

        vk::ImageMemoryBarrier2 InitBlackHoleBarrier(vk::PipelineStageFlagBits2::eTopOfPipe,
                                                     vk::AccessFlagBits2::eNone,
                                                     vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                                                     vk::AccessFlagBits2::eColorAttachmentWrite,
                                                     vk::ImageLayout::eUndefined,
                                                     vk::ImageLayout::eColorAttachmentOptimal,
                                                     vk::QueueFamilyIgnored,
                                                     vk::QueueFamilyIgnored,
                                                     *BlackHoleAttachment->GetImage(),
                                                     SubresourceRange);

        vk::ImageMemoryBarrier2 InitPreBloomBarrier(vk::PipelineStageFlagBits2::eTopOfPipe,
                                                     vk::AccessFlagBits2::eNone,
                                                     vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                                                     vk::AccessFlagBits2::eColorAttachmentWrite,
                                                     vk::ImageLayout::eUndefined,
                                                     vk::ImageLayout::eColorAttachmentOptimal,
                                                     vk::QueueFamilyIgnored,
                                                     vk::QueueFamilyIgnored,
                                                     *PreBloomAttachment->GetImage(),
                                                     SubresourceRange);

        vk::ImageMemoryBarrier2 InitGaussBlurBarrier(vk::PipelineStageFlagBits2::eTopOfPipe,
                                                     vk::AccessFlagBits2::eNone,
                                                     vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                                                     vk::AccessFlagBits2::eColorAttachmentWrite,
                                                     vk::ImageLayout::eUndefined,
                                                     vk::ImageLayout::eColorAttachmentOptimal,
                                                     vk::QueueFamilyIgnored,
                                                     vk::QueueFamilyIgnored,
                                                     *GaussBlurAttachment->GetImage(),
                                                     SubresourceRange);

        std::array InitialBarriers{ InitSwapchainBarrier, InitBlackHoleBarrier,
                                    InitPreBloomBarrier, InitGaussBlurBarrier };
        vk::DependencyInfo InitialDependencyInfo = vk::DependencyInfo()
            .setDependencyFlags(vk::DependencyFlagBits::eByRegion)
            .setImageMemoryBarriers(InitialBarriers);

        CurrentBuffer->pipelineBarrier2(InitialDependencyInfo);

        vk::RenderingInfo BlackHoleRenderingInfo = vk::RenderingInfo()
            .setRenderArea(vk::Rect2D({ 0, 0 }, _WindowSize))
            .setLayerCount(1)
            .setColorAttachments(BlackHoleAttachmentInfo);

        CurrentBuffer->beginRendering(BlackHoleRenderingInfo);
        CurrentBuffer->bindVertexBuffers(0, *QuadOnlyVertexBuffer.GetBuffer(), Offset);
        CurrentBuffer->bindPipeline(vk::PipelineBindPoint::eGraphics, BlackHolePipeline);
        CurrentBuffer->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, BlackHolePipelineLayout, 0, BlackHoleShader->GetDescriptorSets(CurrentFrame), {});
        CurrentBuffer->draw(6, 1, 0, 0);
        CurrentBuffer->endRendering();

        vk::ImageMemoryBarrier2 PreCopySrcBarrier(vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                                                  vk::AccessFlagBits2::eColorAttachmentWrite,
                                                  vk::PipelineStageFlagBits2::eTransfer,
                                                  vk::AccessFlagBits2::eTransferRead,
                                                  vk::ImageLayout::eColorAttachmentOptimal,
                                                  vk::ImageLayout::eTransferSrcOptimal,
                                                  vk::QueueFamilyIgnored,
                                                  vk::QueueFamilyIgnored,
                                                  *BlackHoleAttachment->GetImage(),
                                                  SubresourceRange);

        vk::ImageMemoryBarrier2 PreCopyDstBarrier(vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                                                  vk::AccessFlagBits2::eColorAttachmentWrite,
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

        vk::ImageMemoryBarrier2 PostCopySrcBarrier(vk::PipelineStageFlagBits2::eTransfer,
                                                   vk::AccessFlagBits2::eTransferRead,
                                                   vk::PipelineStageFlagBits2::eFragmentShader,
                                                   vk::AccessFlagBits2::eShaderRead,
                                                   vk::ImageLayout::eTransferSrcOptimal,
                                                   vk::ImageLayout::eShaderReadOnlyOptimal,
                                                   vk::QueueFamilyIgnored,
                                                   vk::QueueFamilyIgnored,
                                                   *BlackHoleAttachment->GetImage(),
                                                   SubresourceRange);

        vk::ImageMemoryBarrier2 PostCopyDstBarrier(vk::PipelineStageFlagBits2::eTransfer,
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

        vk::RenderingInfo PreBloomRenderingInfo = vk::RenderingInfo()
            .setRenderArea(vk::Rect2D({ 0, 0 }, _WindowSize))
            .setLayerCount(1)
            .setColorAttachments(PreBloomAttachmentInfo);

        CurrentBuffer->beginRendering(PreBloomRenderingInfo);
        CurrentBuffer->bindVertexBuffers(0, *QuadOnlyVertexBuffer.GetBuffer(), Offset);
        CurrentBuffer->bindPipeline(vk::PipelineBindPoint::eGraphics, PreBloomPipeline);
        CurrentBuffer->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, PreBloomPipelineLayout, 0, PreBloomShader->GetDescriptorSets(CurrentFrame), {});
        CurrentBuffer->draw(6, 1, 0, 0);
        CurrentBuffer->endRendering();

        vk::ImageMemoryBarrier2 FirstBlurBarrier(vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                                                 vk::AccessFlagBits2::eColorAttachmentWrite,
                                                 vk::PipelineStageFlagBits2::eFragmentShader,
                                                 vk::AccessFlagBits2::eShaderRead,
                                                 vk::ImageLayout::eColorAttachmentOptimal,
                                                 vk::ImageLayout::eShaderReadOnlyOptimal,
                                                 vk::QueueFamilyIgnored,
                                                 vk::QueueFamilyIgnored,
                                                 *PreBloomAttachment->GetImage(),
                                                 SubresourceRange);

        vk::DependencyInfo PreBlurDependencyInfo = vk::DependencyInfo()
            .setDependencyFlags(vk::DependencyFlagBits::eByRegion)
            .setImageMemoryBarriers(FirstBlurBarrier);

        CurrentBuffer->pipelineBarrier2(PreBlurDependencyInfo);

        vk::RenderingInfo GaussBlurRenderingInfo = vk::RenderingInfo()
            .setRenderArea(vk::Rect2D({ 0, 0 }, _WindowSize))
            .setLayerCount(1)
            .setColorAttachments(GaussBlurAttachmentInfo);

        vk::Bool32 bHorizontal = vk::True;

        CurrentBuffer->beginRendering(GaussBlurRenderingInfo);
        CurrentBuffer->bindVertexBuffers(0, *QuadOnlyVertexBuffer.GetBuffer(), Offset);
        CurrentBuffer->bindPipeline(vk::PipelineBindPoint::eGraphics, GaussBlurPipeline);
        CurrentBuffer->pushConstants(GaussBlurPipelineLayout, vk::ShaderStageFlagBits::eFragment, 0, sizeof(vk::Bool32), &bHorizontal);
        CurrentBuffer->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, GaussBlurPipelineLayout, 0, GaussBlurShader->GetDescriptorSets(CurrentFrame), {});
        CurrentBuffer->draw(6, 1, 0, 0);
        CurrentBuffer->endRendering();

        vk::ImageMemoryBarrier2 CopybackSrcBarrier(vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                                                   vk::AccessFlagBits2::eColorAttachmentWrite,
                                                   vk::PipelineStageFlagBits2::eTransfer,
                                                   vk::AccessFlagBits2::eTransferRead,
                                                   vk::ImageLayout::eColorAttachmentOptimal,
                                                   vk::ImageLayout::eTransferSrcOptimal,
                                                   vk::QueueFamilyIgnored,
                                                   vk::QueueFamilyIgnored,
                                                   *GaussBlurAttachment->GetImage(),
                                                   SubresourceRange);

        vk::ImageMemoryBarrier2 CopybackDstBarrier(vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                                                   vk::AccessFlagBits2::eColorAttachmentWrite,
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

        vk::ImageCopy BlendCopyRegion = vk::ImageCopy()
            .setSrcSubresource(vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1))
            .setDstSubresource(vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1))
            .setExtent(vk::Extent3D(_WindowSize.width, _WindowSize.height, 1));

        CurrentBuffer->copyImage(*GaussBlurAttachment->GetImage(), vk::ImageLayout::eTransferSrcOptimal,
                                 *PreBloomAttachment->GetImage(), vk::ImageLayout::eTransferDstOptimal, BlendCopyRegion);

        vk::ImageMemoryBarrier2 ResampleBarrier(vk::PipelineStageFlagBits2::eTransfer,
                                                vk::AccessFlagBits2::eTransferWrite,
                                                vk::PipelineStageFlagBits2::eFragmentShader,
                                                vk::AccessFlagBits2::eShaderRead,
                                                vk::ImageLayout::eTransferDstOptimal,
                                                vk::ImageLayout::eShaderReadOnlyOptimal,
                                                vk::QueueFamilyIgnored,
                                                vk::QueueFamilyIgnored,
                                                *PreBloomAttachment->GetImage(),
                                                SubresourceRange);

        vk::ImageMemoryBarrier2 RewriteBarrier(vk::PipelineStageFlagBits2::eTransfer,
                                               vk::AccessFlagBits2::eTransferRead,
                                               vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                                               vk::AccessFlagBits2::eColorAttachmentWrite,
                                               vk::ImageLayout::eTransferSrcOptimal,
                                               vk::ImageLayout::eColorAttachmentOptimal,
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

        CurrentBuffer->beginRendering(GaussBlurRenderingInfo);
        CurrentBuffer->bindVertexBuffers(0, *QuadOnlyVertexBuffer.GetBuffer(), Offset);
        CurrentBuffer->bindPipeline(vk::PipelineBindPoint::eGraphics, GaussBlurPipeline);
        CurrentBuffer->pushConstants(GaussBlurPipelineLayout, vk::ShaderStageFlagBits::eFragment, 0, sizeof(vk::Bool32), &bHorizontal);
        CurrentBuffer->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, GaussBlurPipelineLayout, 0, GaussBlurShader->GetDescriptorSets(CurrentFrame), {});
        CurrentBuffer->draw(6, 1, 0, 0);
        CurrentBuffer->endRendering();

        vk::ImageMemoryBarrier2 BlendSampleBarrier(vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                                                   vk::AccessFlagBits2::eColorAttachmentWrite,
                                                   vk::PipelineStageFlagBits2::eFragmentShader,
                                                   vk::AccessFlagBits2::eShaderRead,
                                                   vk::ImageLayout::eColorAttachmentOptimal,
                                                   vk::ImageLayout::eShaderReadOnlyOptimal,
                                                   vk::QueueFamilyIgnored,
                                                   vk::QueueFamilyIgnored,
                                                   *GaussBlurAttachment->GetImage(),
                                                   SubresourceRange);

        vk::DependencyInfo BlendSampleDepencencyInfo = vk::DependencyInfo()
            .setDependencyFlags(vk::DependencyFlagBits::eByRegion)
            .setImageMemoryBarriers(BlendSampleBarrier);

        CurrentBuffer->pipelineBarrier2(BlendSampleDepencencyInfo);

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

        vk::ImageMemoryBarrier2 PresentBarrier(vk::PipelineStageFlagBits2::eColorAttachmentOutput,
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
