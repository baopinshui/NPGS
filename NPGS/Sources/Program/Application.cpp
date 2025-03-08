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
{}

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
        .setStoreOp(vk::AttachmentStoreOp::eStore);
    };
    vk::RenderingAttachmentInfo GaussBlurAttachmentInfo = vk::RenderingAttachmentInfo()
    Art::FShader::FResourceInfo BlendResourceInfo
    {
        .setStoreOp(vk::AttachmentStoreOp::eStore);
        { { 0, 0, false } }
    vk::RenderingAttachmentInfo BlendAttachmentInfo = vk::RenderingAttachmentInfo()
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eStore);
    std::vector<std::string> BlendShaderFiles({ "ScreenQuad.vert.spv", "ColorBlend.frag.spv" });
    AssetManager->AddAsset<Art::FShader>("PreBloom", PreBloomShaderFiles, BloomResourceInfo);
    AssetManager->AddAsset<Art::FShader>("GaussBlur", GaussBlurShaderFiles, BloomResourceInfo);
    AssetManager->AddAsset<Art::FShader>("Blend", BlendShaderFiles, BlendResourceInfo);
    auto* BlackHoleShader = AssetManager->GetAsset<Art::FShader>("BlackHole");
        HistoryAttachment = std::make_unique<Grt::FColorAttachment>(
            vk::Format::eR16G16B16A16Sfloat, _WindowSize, 1, vk::SampleCountFlagBits::e1,
            vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst);
    auto* BlendShader = AssetManager->GetAsset<Art::FShader>("Blend");
        BlackHoleAttachment = std::make_unique<Grt::FColorAttachment>(
            vk::Format::eR16G16B16A16Sfloat, _WindowSize, 1, vk::SampleCountFlagBits::e1,
            vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage |
            vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst);
        .Name = "GameArgs",
        PreBloomAttachment = std::make_unique<Grt::FColorAttachment>(
            vk::Format::eR16G16B16A16Sfloat, _WindowSize, 1, vk::SampleCountFlagBits::e1,
            vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage |
            vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst);
        .Binding = 0,
        GaussBlurAttachment = std::make_unique<Grt::FColorAttachment>(
            vk::Format::eR16G16B16A16Sfloat, _WindowSize, 1, vk::SampleCountFlagBits::e1,
            vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage |
            vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst);
    Grt::FShaderResourceManager::FUniformBufferCreateInfo BlackHoleArgsCreateInfo
        HistoryAttachmentInfo.setImageView(*HistoryAttachment->GetImageView());
        BlackHoleAttachmentInfo.setImageView(*BlackHoleAttachment->GetImageView());
        PreBloomAttachmentInfo.setImageView(*PreBloomAttachment->GetImageView());
        GaussBlurAttachmentInfo.setImageView(*GaussBlurAttachment->GetImageView());
        .Set = 0,
        .Binding = 1,
        .Usage = vk::DescriptorType::eUniformBuffer
    };

    auto ShaderResourceManager = Grt::FShaderResourceManager::GetInstance();
    ShaderResourceManager->CreateBuffers<FGameArgs>(GameArgsCreateInfo);
    ShaderResourceManager->CreateBuffers<FBlackHoleArgs>(BlackHoleArgsCreateInfo);

    _VulkanContext->RegisterAutoRemovedCallbacks(Grt::FVulkanContext::ECallbackType::kCreateSwapchain, "CreateFramebuffers", CreateFramebuffers);
    _VulkanContext->RegisterAutoRemovedCallbacks(Grt::FVulkanContext::ECallbackType::kDestroySwapchain, "DestroyFramebuffers", DestroyFramebuffers);

    Grt::FVulkanSampler Sampler(SamplerCreateInfo);

    SamplerCreateInfo
        .setMagFilter(vk::Filter::eLinear)
    Art::FShader::FResourceInfo BlackHoleResourceInfo
        .setMipmapMode(vk::SamplerMipmapMode::eNearest);

        vk::DescriptorImageInfo PreBloomImageInfoForSample(
            *FramebufferSampler, *PreBloomAttachment->GetImageView(), vk::ImageLayout::eShaderReadOnlyOptimal);
        vk::DescriptorImageInfo PreBloomImageInfoForStore(
            *FramebufferSampler, *PreBloomAttachment->GetImageView(), vk::ImageLayout::eGeneral);
        vk::DescriptorImageInfo GaussBlurImageInfoForSample(
    Art::FShader::FResourceInfo BloomResourceInfo
        vk::DescriptorImageInfo GaussBlurImageInfoForStore(
        {}, {},

        ImageInfos.clear();
        ImageInfos.push_back(BlackHoleImageInfo);
    Art::FShader::FResourceInfo BlendResourceInfo
        ImageInfos.clear();
        { { 0, sizeof(FQuadOnlyVertex), false } },
        { { 0, 0, offsetof(FQuadOnlyVertex, Position) } },

        ImageInfos.clear();
        ImageInfos.push_back(PreBloomImageInfoForSample);
    std::vector<std::string> BlackHoleShaderFiles({ "ScreenQuad.vert.spv", "BlackHole.frag.spv" });
    std::vector<std::string> PreBloomShaderFiles({ "PreBloom.comp.spv" });
    std::vector<std::string> GaussBlurShaderFiles({ "GaussBlur.comp.spv" });
    std::vector<std::string> BlendShaderFiles({ "ScreenQuad.vert.spv", "ColorBlend.frag.spv" });
        BlendShader->WriteSharedDescriptors(1, 0, vk::DescriptorType::eCombinedImageSampler, ImageInfos);
    };

    CreatePostDescriptors();

    _VulkanContext->RegisterAutoRemovedCallbacks(Grt::FVulkanContext::ECallbackType::kCreateSwapchain, "CreatePostDescriptor", CreatePostDescriptors);

    std::vector<std::string> BindShaders{ "BlackHole", "PreBloom", "GaussBlur", "Blend" };

    ShaderResourceManager->BindShadersToBuffers("GameArgs", BindShaders);
    ShaderResourceManager->BindShaderToBuffers("BlackHoleArgs", "BlackHole");

#include "Vertices.inc"
        .Name    = "GameArgs",
        .Fields  = { "Resolution", "FovRadians", "Time", "TimeDelta", "TimeRate" },
    QuadOnlyVertexBuffer.CopyData(QuadOnlyVertices);

    auto BlackHoleShaderStageCreateInfos = BlackHoleShader->CreateShaderStageCreateInfo();

    auto* PipelineManager = Grt::FPipelineManager::GetInstance();
    Grt::FShaderResourceManager::FUniformBufferCreateInfo BlackHoleArgsCreateInfo
    vk::PipelineColorBlendAttachmentState ColorBlendAttachmentState = vk::PipelineColorBlendAttachmentState()
        .Name    = "BlackHoleArgs",
        .Fields  = { "InverseCamRot;","WorldUpView", "BlackHoleRelativePos", "BlackHoleRelativeDiskNormal",
                     "BlackHoleMassSol", "Spin", "Mu", "AccretionRate", "InterRadiusLy", "OuterRadiusLy" ,"BlendWeight"},
    std::array<vk::Format, 1> ColorFormat{ vk::Format::eR16G16B16A16Sfloat };

    vk::PipelineRenderingCreateInfo BlackHoleRenderingCreateInfo = vk::PipelineRenderingCreateInfo()
        .setColorAttachmentCount(1)
        .setColorAttachmentFormats(ColorFormat);
        static_cast<float>(_WindowSize.width), -static_cast<float>(_WindowSize.height),
    ShaderResourceManager->CreateBuffers<FGameArgs>(GameArgsCreateInfo);
    ShaderResourceManager->CreateBuffers<FBlackHoleArgs>(BlackHoleArgsCreateInfo);

    // Create graphics pipeline
    // ------------------------
    vk::PipelineRenderingCreateInfo BlendRenderingCreateInfo = vk::PipelineRenderingCreateInfo()

    vk::Pipeline GaussBlurPipeline;
    vk::Pipeline BlendPipeline;

    auto GetPipelines = [&]() -> void
        .setMagFilter(vk::Filter::eLinear)
        .setMinFilter(vk::Filter::eLinear)
        .setMipmapMode(vk::SamplerMipmapMode::eNearest);
        BlendPipeline = PipelineManager->GetPipeline("BlendPipeline");
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
    auto GaussBlurPipelineLayout = PipelineManager->GetPipelineLayout("GaussBlurPipeline");
        ImageInfos.push_back(HistoryFrameImageInfo);
        BlackHoleShader->WriteSharedDescriptors(1, 0, vk::DescriptorType::eSampledImage, ImageInfos);
    std::vector<Grt::FVulkanCommandBuffer> BlackHoleCommandBuffers(Config::Graphics::kMaxFrameInFlight);
        ImageInfos.clear();
        ImageInfos.push_back(Background->CreateDescriptorImageInfo(Sampler));
        BlackHoleShader->WriteSharedDescriptors(1, 1, vk::DescriptorType::eCombinedImageSampler, ImageInfos);

        ImageInfos.clear();
        ImageInfos.push_back(BlackHoleImageInfo);
        PreBloomShader->WriteSharedDescriptors(1, 0, vk::DescriptorType::eCombinedImageSampler, ImageInfos);
        ImageInfos.clear();
        ImageInfos.push_back(PreBloomImageInfoForStore);
        PreBloomShader->WriteSharedDescriptors(1, 1, vk::DescriptorType::eStorageImage, ImageInfos);
    _VulkanContext->GetGraphicsCommandPool().AllocateBuffers(vk::CommandBufferLevel::ePrimary, BlendCommandBuffers);
    auto InitHistoryFrame = [&]() -> void
        ImageInfos.push_back(PreBloomImageInfoForSample);
        GaussBlurShader->WriteSharedDescriptors(1, 0, vk::DescriptorType::eCombinedImageSampler, ImageInfos);
        ImageInfos.clear();
        ImageInfos.push_back(GaussBlurImageInfoForStore);
        GaussBlurShader->WriteSharedDescriptors(1, 1, vk::DescriptorType::eStorageImage, ImageInfos);
            vk::AccessFlagBits2::eNone,
            vk::PipelineStageFlagBits2::eFragmentShader,
        ImageInfos.push_back(BlackHoleImageInfo);
        ImageInfos.push_back(GaussBlurImageInfoForSample);
        BlendShader->WriteSharedDescriptors(1, 0, vk::DescriptorType::eCombinedImageSampler, ImageInfos);
            vk::ImageLayout::eShaderReadOnlyOptimal,
            vk::QueueFamilyIgnored,
            vk::QueueFamilyIgnored,
            *HistoryAttachment->GetImage(),
    _VulkanContext->RegisterAutoRemovedCallbacks(Grt::FVulkanContext::ECallbackType::kCreateSwapchain, "CreatePostDescriptor", CreatePostDescriptors);
        vk::DependencyInfo InitialDependencyInfo = vk::DependencyInfo()
    std::vector<std::string> BindShaders{ "BlackHole", "PreBloom", "GaussBlur", "Blend" };

    ShaderResourceManager->BindShadersToBuffers("GameArgs", BindShaders);
    ShaderResourceManager->BindShaderToBuffers("BlackHoleArgs", "BlackHole");

#include "Vertices.inc"
    glm::mat4x4 lastdir(0.0f);
    Grt::FDeviceLocalBuffer QuadOnlyVertexBuffer(QuadOnlyVertices.size() * sizeof(FQuadOnlyVertex), vk::BufferUsageFlagBits::eVertexBuffer);
    QuadOnlyVertexBuffer.CopyData(QuadOnlyVertices);
        {
    auto BlackHoleShaderStageCreateInfos = BlackHoleShader->CreateShaderStageCreateInfo();
        // Uniform update
        auto& CurrentBuffer = BlackHoleCommandBuffers[CurrentFrame];
        CurrentBuffer.Begin(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

        vk::ImageMemoryBarrier2 InitBlackHoleBarrier(
            vk::PipelineStageFlagBits2::eTopOfPipe,
            vk::AccessFlagBits2::eNone,
    std::array<vk::Format, 1> ColorFormat{ vk::Format::eR16G16B16A16Sfloat };

    vk::PipelineRenderingCreateInfo BlackHoleRenderingCreateInfo = vk::PipelineRenderingCreateInfo()
            vk::AccessFlagBits2::eColorAttachmentWrite,
        .setColorAttachmentFormats(ColorFormat);
            vk::QueueFamilyIgnored,
    Grt::FGraphicsPipelineCreateInfoPack BlackHoleCreateInfoPack;
    BlackHoleCreateInfoPack.GraphicsPipelineCreateInfo.setPNext(&BlackHoleRenderingCreateInfo);
    BlackHoleCreateInfoPack.InputAssemblyStateCreateInfo.setTopology(vk::PrimitiveTopology::eTriangleList);
    BlackHoleCreateInfoPack.ColorBlendAttachmentStates.emplace_back(ColorBlendAttachmentState);
            .setDependencyFlags(vk::DependencyFlagBits::eByRegion)
    BlackHoleCreateInfoPack.Viewports.emplace_back(0.0f, static_cast<float>(_WindowSize.height),
                                                   static_cast<float>(_WindowSize.width), -static_cast<float>(_WindowSize.height),
                                                   0.0f, 1.0f);
    BlackHoleCreateInfoPack.Scissors.emplace_back(vk::Offset2D(), _WindowSize);
        vk::RenderingInfo BlackHoleRenderingInfo = vk::RenderingInfo()
    PipelineManager->CreatePipeline("BlackHolePipeline", "BlackHole", BlackHoleCreateInfoPack);
        CurrentBuffer->bindPipeline(vk::PipelineBindPoint::eGraphics, BlackHolePipeline);
    vk::PipelineRenderingCreateInfo BlendRenderingCreateInfo = vk::PipelineRenderingCreateInfo()
            vk::PipelineStageFlagBits2::eTransfer,
            vk::AccessFlagBits2::eTransferRead,
            vk::ImageLayout::eColorAttachmentOptimal,
    BlackHoleCreateInfoPack.GraphicsPipelineCreateInfo.setPNext(&BlendRenderingCreateInfo);
    PipelineManager->CreatePipeline("BlendPipeline", "Blend", BlackHoleCreateInfoPack);
            vk::QueueFamilyIgnored,
    PipelineManager->CreatePipeline("PreBloomPipeline",  "PreBloom");
    PipelineManager->CreatePipeline("GaussBlurPipeline", "GaussBlur");

    vk::Pipeline BlackHolePipeline;
    vk::Pipeline PreBloomPipeline;
    vk::Pipeline GaussBlurPipeline;
    vk::Pipeline BlendPipeline;
            vk::PipelineStageFlagBits2::eFragmentShader,
        vk::ImageCopy HistoryCopyRegion = vk::ImageCopy()
            .setSrcSubresource(vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1))
        BlackHolePipeline = PipelineManager->GetPipeline("BlackHolePipeline");
        PreBloomPipeline  = PipelineManager->GetPipeline("PreBloomPipeline");
        GaussBlurPipeline = PipelineManager->GetPipeline("GaussBlurPipeline");
        BlendPipeline     = PipelineManager->GetPipeline("BlendPipeline");
            *HistoryAttachment->GetImage(), vk::ImageLayout::eTransferDstOptimal, HistoryCopyRegion);

        vk::ImageMemoryBarrier2 PostCopySrcBarrier(
            vk::PipelineStageFlagBits2::eTransfer,
    _VulkanContext->RegisterAutoRemovedCallbacks(Grt::FVulkanContext::ECallbackType::kCreateSwapchain, "GetPipelines", GetPipelines);
            vk::AccessFlagBits2::eShaderRead,
    auto BlackHolePipelineLayout = PipelineManager->GetPipelineLayout("BlackHolePipeline");
    auto PreBloomPipelineLayout  = PipelineManager->GetPipelineLayout("PreBloomPipeline");
    auto GaussBlurPipelineLayout = PipelineManager->GetPipelineLayout("GaussBlurPipeline");
    auto BlendPipelineLayout     = PipelineManager->GetPipelineLayout("BlendPipeline");
            SubresourceRange);

        vk::ImageMemoryBarrier2 PostCopyDstBarrier(
            vk::PipelineStageFlagBits2::eTransfer,
            vk::PipelineStageFlagBits2::eFragmentShader,
            vk::AccessFlagBits2::eShaderRead,
            vk::ImageLayout::eTransferDstOptimal,
            vk::ImageLayout::eShaderReadOnlyOptimal,
            vk::QueueFamilyIgnored,
            vk::QueueFamilyIgnored,
            *HistoryAttachment->GetImage(),
    std::vector<Grt::FVulkanCommandBuffer> BlackHoleCommandBuffers(Config::Graphics::kMaxFrameInFlight);
    std::vector<Grt::FVulkanCommandBuffer> PreBloomCommandBuffers(Config::Graphics::kMaxFrameInFlight);
    std::vector<Grt::FVulkanCommandBuffer> GaussBlurCommandBuffers(Config::Graphics::kMaxFrameInFlight);
    std::vector<Grt::FVulkanCommandBuffer> BlendCommandBuffers(Config::Graphics::kMaxFrameInFlight);
    _VulkanContext->GetGraphicsCommandPool().AllocateBuffers(vk::CommandBufferLevel::ePrimary, BlackHoleCommandBuffers);
    _VulkanContext->GetGraphicsCommandPool().AllocateBuffers(vk::CommandBufferLevel::ePrimary, PreBloomCommandBuffers);
    _VulkanContext->GetGraphicsCommandPool().AllocateBuffers(vk::CommandBufferLevel::ePrimary, GaussBlurCommandBuffers);
    _VulkanContext->GetGraphicsCommandPool().AllocateBuffers(vk::CommandBufferLevel::ePrimary, BlendCommandBuffers);

    vk::DeviceSize Offset       = 0;
    std::uint32_t  CurrentFrame = 0;
    glm::vec4      WorldUp(0.0f, 1.0f, 0.0f, 1.0f);
            .setImageMemoryBarriers(PostCopyBarriers);
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
        //// Record PreBloom rendering commands
    _VulkanContext->RegisterAutoRemovedCallbacks(Grt::FVulkanContext::ECallbackType::kCreateSwapchain, "InitHistoryFrame", InitHistoryFrame);
    glm::vec4    LastBlackHoleRelativePos(0.0f, 0.0f, 0.0f,1.0f);
    glm::mat4x4 lastdir(0.0f);
        //CurrentBuffer.Begin(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

        vk::ImageMemoryBarrier2 InitPreBloomBarrier(
            vk::PipelineStageFlagBits2::eTopOfPipe,
            vk::AccessFlagBits2::eNone,
            vk::PipelineStageFlagBits2::eComputeShader,
            vk::AccessFlagBits2::eShaderWrite,
            .setImageMemoryBarriers(InitPreBloomBarrier);

        CurrentBuffer->pipelineBarrier2(PreBloomInitialDependencyInfo);

        GameArgs.Resolution = glm::vec2(_WindowSize.width, _WindowSize.height);
        GameArgs.FovRadians = glm::radians(_FreeCamera->GetCameraZoom());
        GameArgs.Time = static_cast<float>(glfwGetTime());
        GameArgs.TimeDelta = static_cast<float>(_DeltaTime);
        GameArgs.TimeRate = 300.0f;
        LastBlackHoleRelativePos = BlackHoleArgs.BlackHoleRelativePos;
        lastdir = BlackHoleArgs.InverseCamRot;
        ShaderResourceManager->UpdateEntrieBuffer(CurrentFrame, "GameArgs", GameArgs);
        BlackHoleArgs.InverseCamRot = glm::mat4_cast(glm::conjugate(_FreeCamera->GetOrientation()));
        BlackHoleArgs.WorldUpView = (glm::mat4_cast(_FreeCamera->GetOrientation()) * WorldUp);
        BlackHoleArgs.BlackHoleRelativePos = (_FreeCamera->GetViewMatrix() * glm::vec4(0.0 * BlackHoleArgs.BlackHoleMassSol * kGravityConstant / pow(kSpeedOfLight, 2) * kSolarMass / kLightYearToMeter, 0.0f, -0.000f, 1.0f));
        BlackHoleArgs.BlackHoleRelativeDiskNormal = (glm::mat4_cast(_FreeCamera->GetOrientation()) * glm::vec4(0.0f, 1.0f, 0.00001f, 1.0f));
        BlackHoleArgs.BlackHoleMassSol            = 1.49e7f;
        BlackHoleArgs.Spin                        = 0.0f;
        BlackHoleArgs.Mu                          = 1.0f;
        BlackHoleArgs.AccretionRate               = 5e15f;
        BlackHoleArgs.InterRadiusLy               = 9.7756e-6f;
        BlackHoleArgs.OuterRadiusLy               = 5.586e-5f;
        float Rs= 2.0 * BlackHoleArgs.BlackHoleMassSol * kGravityConstant / pow(kSpeedOfLight, 2) * kSolarMass / kLightYearToMeter;
        BlackHoleArgs.BlendWeight                 = (1.0 - pow(0.5, (_DeltaTime) / std::max(std::min((0.131 * 36.0 / (GameArgs.TimeRate) * (Rs/0.00000465)), 0.3), 0.02)));
        if (glm::length(glm::vec3(LastBlackHoleRelativePos - BlackHoleArgs.BlackHoleRelativePos))> glm::length(glm::vec3(LastBlackHoleRelativePos))*0.01* _DeltaTime || glm::determinant(glm::mat3x3(lastdir - BlackHoleArgs.InverseCamRot)) > 1e-14 * _DeltaTime) { BlackHoleArgs.BlendWeight = 1.0f; }

        if (int(glfwGetTime()) < 1)
        {
            _FreeCamera->SetTargetOrbitAxis(glm::vec3(0., 1., 0.)); _FreeCamera->SetTargetOrbitCenter(glm::vec3(0, 0, 0));
        }//else{ _FreeCamera->SetTargetOrbitAxis(glm::vec3(0., -1., -0.)); _FreeCamera->SetTargetOrbitCenter(glm::vec3(0.,0.0*5.586e-5f, 0));
       // }
        ShaderResourceManager->UpdateEntrieBuffer(CurrentFrame, "BlackHoleArgs", BlackHoleArgs);
            .setDependencyFlags(vk::DependencyFlagBits::eByRegion)
            .setImageMemoryBarriers(FirstBlurBarrier);

        CurrentBuffer->pipelineBarrier2(FirstBlurDependencyInfo);
        BlendAttachmentInfo.setImageView(_VulkanContext->GetSwapchainImageView(ImageIndex));

        std::uint32_t WorkgroundX = (_WindowSize.width  + 9) / 10;
        std::uint32_t WorkgroundY = (_WindowSize.height + 9) / 10;

        // Record BlackHole rendering commands
        // -----------------------------------
        auto& CurrentBuffer = BlackHoleCommandBuffers[CurrentFrame];
        //// Record GaussBlur rendering commands
        //// -----------------------------------
        vk::ImageMemoryBarrier2 InitBlackHoleBarrier(
        //CurrentBuffer.Begin(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

        vk::ImageMemoryBarrier2 InitGaussBlurBarrier(
            vk::PipelineStageFlagBits2::eTopOfPipe,
            vk::AccessFlagBits2::eNone,
            vk::PipelineStageFlagBits2::eComputeShader,
            vk::AccessFlagBits2::eShaderWrite,
            vk::ImageLayout::eUndefined,
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
            *GaussBlurAttachment->GetImage(),
        vk::ImageMemoryBarrier2 PreCopySrcBarrier(
            .setDependencyFlags(vk::DependencyFlagBits::eByRegion)
            .setImageMemoryBarriers(InitGaussBlurBarrier);
            vk::PipelineStageFlagBits2::eTransfer,
            vk::AccessFlagBits2::eTransferRead,
        CurrentBuffer->pipelineBarrier2(GaussBlurInitialDependencyInfo);

        vk::Bool32 bHorizontal = vk::True;
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
            sizeof(vk::Bool32), &bHorizontal);
        vk::ImageMemoryBarrier2 InitPreBloomBarrier(
        CurrentBuffer->bindDescriptorSets(vk::PipelineBindPoint::eCompute, GaussBlurPipelineLayout, 0,
            GaussBlurShader->GetDescriptorSets(CurrentFrame), {});
            vk::PipelineStageFlagBits2::eComputeShader,
            vk::AccessFlagBits2::eShaderWrite,

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
            vk::PipelineStageFlagBits2::eComputeShader,
            vk::AccessFlagBits2::eShaderWrite,
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
            vk::ImageLayout::eTransferSrcOptimal,
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
            vk::QueueFamilyIgnored,
        vk::DependencyInfo GaussBlurInitialDependencyInfo = vk::DependencyInfo()
            SubresourceRange);
            .setImageMemoryBarriers(InitGaussBlurBarrier);

        CurrentBuffer->pipelineBarrier2(GaussBlurInitialDependencyInfo);

        vk::Bool32 bHorizontal = vk::True;
        vk::ImageMemoryBarrier2 CopybackDstBarrier(
        CurrentBuffer->bindPipeline(vk::PipelineBindPoint::eCompute, GaussBlurPipeline);
        CurrentBuffer->pushConstants(GaussBlurPipelineLayout, vk::ShaderStageFlagBits::eCompute, 0,
                                     sizeof(vk::Bool32), &bHorizontal);
            vk::AccessFlagBits2::eShaderRead,
        CurrentBuffer->bindDescriptorSets(vk::PipelineBindPoint::eCompute, GaussBlurPipelineLayout, 0,
                                          GaussBlurShader->GetDescriptorSets(CurrentFrame), {});
            vk::ImageLayout::eShaderReadOnlyOptimal,
        CurrentBuffer->dispatch(WorkgroundX, WorkgroundY, 1);
            SubresourceRange);
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
            .setDstSubresource(vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1))
        vk::ImageMemoryBarrier2 CopybackDstBarrier(
            vk::PipelineStageFlagBits2::eComputeShader,

            vk::PipelineStageFlagBits2::eTransfer,
            vk::AccessFlagBits2::eTransferWrite,
            vk::PipelineStageFlagBits2::eTransfer,
            vk::AccessFlagBits2::eTransferWrite,
            vk::PipelineStageFlagBits2::eComputeShader,
            *PreBloomAttachment->GetImage(),
            SubresourceRange);
            vk::QueueFamilyIgnored,
        std::array CopybackBarriers{ CopybackSrcBarrier, CopybackDstBarrier };
        vk::DependencyInfo CopybackDependencyInfo = vk::DependencyInfo()
            *PreBloomAttachment->GetImage(),
            .setImageMemoryBarriers(CopybackBarriers);

        CurrentBuffer->pipelineBarrier2(CopybackDependencyInfo);

        vk::ImageCopy CopybackRegion = vk::ImageCopy()
            .setSrcSubresource(vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1))
            .setDstSubresource(vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1))
            .setExtent(vk::Extent3D(_WindowSize.width, _WindowSize.height, 1));
            vk::PipelineStageFlagBits2::eTransfer,
        CurrentBuffer->copyImage(*GaussBlurAttachment->GetImage(), vk::ImageLayout::eTransferSrcOptimal,
                                 *PreBloomAttachment->GetImage(), vk::ImageLayout::eTransferDstOptimal, CopybackRegion);
            vk::AccessFlagBits2::eShaderWrite,
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
        std::array RestoreBarriers{ ResampleBarrier, RewriteBarrier };
        vk::DependencyInfo RerenderDependencyInfo = vk::DependencyInfo()
            .setDependencyFlags(vk::DependencyFlagBits::eByRegion)
            .setImageMemoryBarriers(RestoreBarriers);
        bHorizontal = vk::False;
        CurrentBuffer->pipelineBarrier2(RerenderDependencyInfo);

        bHorizontal = vk::False;
            vk::AccessFlagBits2::eShaderWrite,
        CurrentBuffer->pushConstants(GaussBlurPipelineLayout, vk::ShaderStageFlagBits::eCompute, 0,
                                     sizeof(vk::Bool32), &bHorizontal);
            vk::ImageLayout::eShaderReadOnlyOptimal,
        CurrentBuffer->bindDescriptorSets(vk::PipelineBindPoint::eCompute, GaussBlurPipelineLayout, 0,
                                          GaussBlurShader->GetDescriptorSets(CurrentFrame), {});
            .setImageMemoryBarriers(BlendSampleBarrier);
        CurrentBuffer->dispatch(WorkgroundX, WorkgroundY, 1);
        //CurrentBuffer.End();
        vk::ImageMemoryBarrier2 BlendSampleBarrier(
            vk::PipelineStageFlagBits2::eComputeShader,
            vk::AccessFlagBits2::eShaderWrite,
        //// Record Blend rendering commands
        //// ------------------------------
            vk::ImageLayout::eGeneral,
        //CurrentBuffer.Begin(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

        vk::ImageMemoryBarrier2 InitSwapchainBarrier(
            *GaussBlurAttachment->GetImage(),
            SubresourceRange);
            vk::AccessFlagBits2::eColorAttachmentWrite,
        vk::DependencyInfo BlendSampleDepencencyInfo = vk::DependencyInfo()
            vk::ImageLayout::eColorAttachmentOptimal,
            .setImageMemoryBarriers(BlendSampleBarrier);

        CurrentBuffer->pipelineBarrier2(BlendSampleDepencencyInfo);
        //CurrentBuffer.End();

        //_VulkanContext->ExecuteComputeCommands(CurrentBuffer);

        //// Record Blend rendering commands
        //// ------------------------------
        //CurrentBuffer = BlendCommandBuffers[CurrentFrame];
        //CurrentBuffer.Begin(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
            vk::QueueFamilyIgnored,
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
            SubresourceRange);
        vk::DependencyInfo SwapchainInitialDependencyInfo = vk::DependencyInfo()
            .setDependencyFlags(vk::DependencyFlagBits::eByRegion)
            .setImageMemoryBarriers(InitSwapchainBarrier);

        CurrentBuffer->pipelineBarrier2(SwapchainInitialDependencyInfo);

        vk::RenderingInfo BlendRenderingInfo = vk::RenderingInfo()
            .setRenderArea(vk::Rect2D({ 0, 0 }, _WindowSize))
            .setLayerCount(1)
            .setColorAttachments(BlendAttachmentInfo);
            .setColorAttachments(BlendAttachmentInfo);
        CurrentBuffer->beginRendering(BlendRenderingInfo);
        CurrentBuffer->bindVertexBuffers(0, *QuadOnlyVertexBuffer.GetBuffer(), Offset);
        CurrentBuffer->bindPipeline(vk::PipelineBindPoint::eGraphics, BlendPipeline);
        CurrentBuffer->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, BlendPipelineLayout, 0, BlendShader->GetDescriptorSets(CurrentFrame), {});
        CurrentBuffer->bindVertexBuffers(0, *QuadOnlyVertexBuffer.GetBuffer(), Offset);
        CurrentBuffer->bindPipeline(vk::PipelineBindPoint::eGraphics, BlendPipeline);
        CurrentBuffer->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, BlendPipelineLayout, 0, BlendShader->GetDescriptorSets(CurrentFrame), {});
        CurrentBuffer->draw(6, 1, 0, 0);
        CurrentBuffer->endRendering();

        vk::ImageMemoryBarrier2 PresentBarrier(
            vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            vk::AccessFlagBits2::eColorAttachmentWrite,
            vk::PipelineStageFlagBits2::eBottomOfPipe,
            vk::AccessFlagBits2::eNone,
            vk::ImageLayout::eColorAttachmentOptimal,
            vk::ImageLayout::ePresentSrcKHR,
            SubresourceRange);
            _VulkanContext->GetSwapchainImage(ImageIndex),
        vk::DependencyInfo PresentDependencyInfo = vk::DependencyInfo()

        vk::DependencyInfo PresentDependencyInfo = vk::DependencyInfo()
            .setDependencyFlags(vk::DependencyFlagBits::eByRegion)
        CurrentBuffer->pipelineBarrier2(PresentDependencyInfo);
        CurrentBuffer->pipelineBarrier2(PresentDependencyInfo);
        CurrentBuffer.End();
        _VulkanContext->SubmitCommandBufferToGraphics(
            *CurrentBuffer, *Semaphores_ImageAvailable[CurrentFrame],
            *Semaphores_RenderFinished[CurrentFrame], *InFlightFences[CurrentFrame]);
            *CurrentBuffer, *Semaphores_ImageAvailable[CurrentFrame],
            *Semaphores_RenderFinished[CurrentFrame], *InFlightFences[CurrentFrame]);
        _VulkanContext->PresentImage(*Semaphores_RenderFinished[CurrentFrame]);

        CurrentFrame = (CurrentFrame + 1) % Config::Graphics::kMaxFrameInFlight;

        ShowTitleFps();
        update();
        glfwPollEvents();
        ShowTitleFps();
        update();
    _VulkanContext->WaitIdle();
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

    _FreeCamera = std::make_unique<SysSpa::FCamera>(glm::vec3(0.0f, 0.0f, 0.0f), 0.05, 2.5, 60.0);
    _FreeCamera->SetCameraMode(true);
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
    static double CurrentTime = 0.0;
    static double PreviousTime = glfwGetTime();
    static double LastFrameTime = 0.0;
    static int    FrameCount = 0;

    CurrentTime = glfwGetTime();
    _DeltaTime = CurrentTime - LastFrameTime;
    LastFrameTime = CurrentTime;
    ++FrameCount;
    if (CurrentTime - PreviousTime >= 1.0)
    {
        glfwSetWindowTitle(_Window, (std::string(_WindowTitle) + " " + std::to_string(FrameCount)).c_str());
        FrameCount = 0;
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
        //_FreeCamera->SetFov(glm::degrees(2 * atan(tan(0.5 * glm::radians(_FreeCamera->GetFov())) * pow(2.0f, _DeltaTime))));
        _FreeCamera->ProcessKeyboard(SysSpa::FCamera::EMovement::kForward, _DeltaTime);
    if (glfwGetKey(_Window, GLFW_KEY_S) == GLFW_PRESS)
        // _FreeCamera->SetFov(glm::degrees(2 * atan(tan(0.5 * glm::radians(_FreeCamera->GetFov())) * pow(2.0f, -_DeltaTime))));
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
    if (glfwGetKey(_Window, GLFW_KEY_T) == GLFW_PRESS)
        _FreeCamera->ProcessModeChange();
}

void FApplication::FramebufferSizeCallback(GLFWwindow* Window, int Width, int Height)
{
    auto* App = reinterpret_cast<FApplication*>(glfwGetWindowUserPointer(Window));

    if (Width == 0 || Height == 0)
    {
        return;
    }

    App->_WindowSize.width = Width;
    App->_WindowSize.height = Height;
    App->_VulkanContext->WaitIdle();
    App->_VulkanContext->RecreateSwapchain();
}
void FApplication::update()
{
    _FreeCamera->ProcessTimeEvolution(_DeltaTime);
}
void FApplication::CursorPosCallback(GLFWwindow* Window, double PosX, double PosY)
{
    auto* App = static_cast<FApplication*>(glfwGetWindowUserPointer(Window));

    if (App->_bFirstMouse)
    {
        App->_LastX = PosX;
        App->_LastY = PosY;
        App->_bFirstMouse = false;
    }

    double OffsetX = PosX - App->_LastX;
    double OffsetY = App->_LastY - PosY;
    App->_LastX = PosX;
    App->_LastY = PosY;
    App->_FreeCamera->ProcessMouseMovement(OffsetX, OffsetY);

    //   std::cout << OffsetX / App->_DeltaTime << "    " << OffsetY / App->_DeltaTime << "\n";
}

void FApplication::ScrollCallback(GLFWwindow* Window, double OffsetX, double OffsetY)
{
    auto* App = static_cast<FApplication*>(glfwGetWindowUserPointer(Window));
    App->_FreeCamera->ProcessMouseScroll(OffsetY);
}

_NPGS_END
