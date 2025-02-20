#include "Application.h"

#include <cmath>
#include <cstddef>
#include <cstdint>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_aligned.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Engine/Core/Base/Config/EngineConfig.h"
#include "Engine/Core/Runtime/AssetLoaders/AssetManager.h"
#include "Engine/Core/Runtime/AssetLoaders/Shader.h"
#include "Engine/Core/Runtime/AssetLoaders/Texture.h"
#include "Engine/Core/Runtime/Graphics/Renderers/PipelineManager.h"
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
    std::unique_ptr<Runtime::Graphics::FColorAttachment>        BlackHoleAttachment;
    std::unique_ptr<Runtime::Graphics::FColorAttachment>        ColorAttachment;
    std::unique_ptr<Runtime::Graphics::FColorAttachment>        ColorAttachment2;
    std::unique_ptr<Runtime::Graphics::FColorAttachment>        PostColorAttachment;
    std::unique_ptr<Runtime::Graphics::FColorAttachment>        PostColorAttachment2;
    std::unique_ptr<Runtime::Graphics::FDepthStencilAttachment> DepthStencilAttachment;

    vk::RenderingAttachmentInfo ColorAttachmentInfo = vk::RenderingAttachmentInfo()
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setResolveMode(vk::ResolveModeFlagBits::eAverage)
        .setResolveImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eStore)
        .setClearValue(vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f));

    vk::RenderingAttachmentInfo ColorAttachmentInfo2 = vk::RenderingAttachmentInfo()
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setResolveMode(vk::ResolveModeFlagBits::eAverage)
        .setResolveImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eStore)
        .setClearValue(vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f));

    vk::RenderingAttachmentInfo DepthStencilAttachmentInfo = vk::RenderingAttachmentInfo()
        .setImageLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eDontCare)
        .setClearValue(vk::ClearDepthStencilValue(1.0f, 0));

    vk::RenderingAttachmentInfo PostColorAttachmentInfo = vk::RenderingAttachmentInfo()
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eStore)
        .setClearValue(vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f));

    vk::RenderingAttachmentInfo PostColorAttachmentInfo2 = vk::RenderingAttachmentInfo()
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eStore)
        .setClearValue(vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f));

    vk::RenderingAttachmentInfo FinalOutputAttachmentInfo = vk::RenderingAttachmentInfo()
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eDontCare)
        .setStoreOp(vk::AttachmentStoreOp::eStore);

    vk::RenderingAttachmentInfo BlackHoleAttachmentInfo = vk::RenderingAttachmentInfo()
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setResolveMode(vk::ResolveModeFlagBits::eAverage)
        .setResolveImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eStore);

    std::array ColorAttachmentInfos{ ColorAttachmentInfo, ColorAttachmentInfo2 };

    auto CreateFramebuffers = [&]() -> void
    {
        _VulkanContext->WaitIdle();

        ColorAttachment = std::make_unique<Grt::FColorAttachment>(
            _VulkanContext->GetSwapchainCreateInfo().imageFormat, _WindowSize, 1, vk::SampleCountFlagBits::e8);

        ColorAttachment2 = std::make_unique<Grt::FColorAttachment>(
            _VulkanContext->GetSwapchainCreateInfo().imageFormat, _WindowSize, 1, vk::SampleCountFlagBits::e8);

        PostColorAttachment = std::make_unique<Grt::FColorAttachment>(
            _VulkanContext->GetSwapchainCreateInfo().imageFormat, _WindowSize, 1, vk::SampleCountFlagBits::e1,
            vk::ImageUsageFlagBits::eSampled);

        PostColorAttachment2 = std::make_unique<Grt::FColorAttachment>(
            _VulkanContext->GetSwapchainCreateInfo().imageFormat, _WindowSize, 1, vk::SampleCountFlagBits::e1,
            vk::ImageUsageFlagBits::eSampled);

        DepthStencilAttachment = std::make_unique<Grt::FDepthStencilAttachment>(
            vk::Format::eD32SfloatS8Uint, _WindowSize, 1, vk::SampleCountFlagBits::e8);

        BlackHoleAttachment = std::make_unique<Grt::FColorAttachment>(
            _VulkanContext->GetSwapchainCreateInfo().imageFormat, _WindowSize, 1, vk::SampleCountFlagBits::e1);

        ColorAttachmentInfos[0].setImageView(*ColorAttachment->GetImageView())
                               .setResolveImageView(*PostColorAttachment->GetImageView());
        ColorAttachmentInfos[1].setImageView(*ColorAttachment2->GetImageView())
                               .setResolveImageView(*PostColorAttachment2->GetImageView());
        DepthStencilAttachmentInfo.setImageView(*DepthStencilAttachment->GetImageView());
        PostColorAttachmentInfo.setImageView(*PostColorAttachment->GetImageView());
        PostColorAttachmentInfo2.setImageView(*PostColorAttachment2->GetImageView());
        BlackHoleAttachmentInfo.setImageView(*BlackHoleAttachment->GetImageView());
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
    struct FVertex
    {
        glm::vec3 Position;
        glm::vec3 Normal;
        glm::vec2 TexCoord;
    };

    struct FQuadVertex
    {
        glm::vec2 Position;
        glm::vec2 TexCoord;
    };

    struct FQuadOnlyVertex
    {
        glm::vec2 Position;
    };

    auto* AssetManager = Art::FAssetManager::GetInstance();

    Art::FShader::FResourceInfo ResourceInfo
    {
        {
            { 0, sizeof(FVertex), false }
        },
        {
            { 0, 0, offsetof(FVertex, Position) },
            { 0, 1, offsetof(FVertex, Normal) },
            { 0, 2, offsetof(FVertex, TexCoord) }
        },
        {
            { 0, 0, false },
            { 0, 1, false }
        },
        {
            { vk::ShaderStageFlagBits::eVertex, { "iModel" } }
        }
    };

    Art::FShader::FResourceInfo PostResourceInfo
    {
        {
            { 0, sizeof(FQuadVertex), false }
        },
        {
            { 0, 0, offsetof(FQuadVertex, Position) },
            { 0, 1, offsetof(FQuadVertex, TexCoord) }
        }
    };

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

    std::vector<std::string> BlackHoleShaderFiles({ "BlackHole.vert.spv", "BlackHole.frag.spv" });
    std::vector<std::string> BasicLightingShaderFiles({ "BasicLighting.vert.spv", "BasicLighting.frag.spv" });
    std::vector<std::string> LampShaderFiles({ "BasicLighting.vert.spv", "BasicLighting_Lamp.frag.spv" });
    std::vector<std::string> PostShaderFiles({ "PostProcess.vert.spv", "PostProcess.frag.spv" });
    AssetManager->AddAsset<Art::FShader>("BlackHole", BlackHoleShaderFiles, BlackHoleResourceInfo);
    AssetManager->AddAsset<Art::FShader>("BasicLightingShader", BasicLightingShaderFiles, ResourceInfo);
    AssetManager->AddAsset<Art::FShader>("LampShader", LampShaderFiles, ResourceInfo);
    AssetManager->AddAsset<Art::FShader>("PostShader", PostShaderFiles, PostResourceInfo);
    AssetManager->AddAsset<Art::FTexture2D>("ContainerDiffuse", "ContainerDiffuse.png", vk::Format::eR8G8B8A8Unorm, vk::Format::eR8G8B8A8Unorm, vk::ImageCreateFlagBits::eMutableFormat, true);
    AssetManager->AddAsset<Art::FTexture2D>("ContainerSpecular", "ContainerSpecular.png", vk::Format::eR8G8B8A8Unorm, vk::Format::eR8G8B8A8Unorm, vk::ImageCreateFlagBits::eMutableFormat, true);

    auto* BlackHoleShader     = AssetManager->GetAsset<Art::FShader>("BlackHole");
    auto* BasicLightingShader = AssetManager->GetAsset<Art::FShader>("BasicLightingShader");
    auto* LampShader          = AssetManager->GetAsset<Art::FShader>("LampShader");
    auto* PostShader          = AssetManager->GetAsset<Art::FShader>("PostShader");
    auto* ContainerDiffuse    = AssetManager->GetAsset<Art::FTexture2D>("ContainerDiffuse");
    auto* ContainerSpecular   = AssetManager->GetAsset<Art::FTexture2D>("ContainerSpecular");

    struct FBlackHolePos
    {
        glm::aligned_vec3 WorldUpView;
        glm::aligned_vec3 BlackHoleRelativePos;
        glm::aligned_vec3 BlackHoleRelativeDiskNormal;
    } BlackHolePos;

    Grt::FShaderResourceManager::FUniformBufferCreateInfo BlackHolePosCreateInfo
    {
        .Name    = "BlackHolePos",
        .Fields  = { "WorldUpView", "BlackHoleRelativePos", "BlackHoleRelativeDiskNormal" },
        .Set     = 0,
        .Binding = 0,
        .Usage   = vk::DescriptorType::eUniformBuffer
    };

    struct FBlackHoleArgs
    {
        glm::aligned_vec2 Resolution;
        float FovRadians;
        float Time;
        float TimeRate;
        float BlackHoleMassSol;
        float Spin;
        float Mu;
        float AccretionRate;
        float InterRadiusLy;
        float OuterRadiusLy;
    } BlackHoleArgs;

    Grt::FShaderResourceManager::FUniformBufferCreateInfo BlackHoleArgsCreateInfo
    {
        .Name    = "BlackHoleArgs",
        .Fields  = { "Resolution", "FovRadians", "Time", "TimeRate", "BlackHoleMassSol", "Spin", "Mu", "AccretionRate", "InterRadiusLy", "OuterRadiusLy" },
        .Set     = 0,
        .Binding = 1,
        .Usage   = vk::DescriptorType::eUniformBuffer
    };

    struct FMatrices
    {
        glm::aligned_mat4x4 View{ glm::mat4x4(1.0f) };
        glm::aligned_mat4x4 Projection{ glm::mat4x4(1.0f) };
        glm::aligned_mat3x3 NormalMatrix{ glm::mat3x3(1.0f) };
    } Matrices;

    Grt::FShaderResourceManager::FUniformBufferCreateInfo MatricesCreateInfo
    {
        .Name    = "Matrices",
        .Fields  = { "View", "Projection", "NormalMatrix" },
        .Set     = 0,
        .Binding = 0,
        .Usage   = vk::DescriptorType::eUniformBuffer
    };

    struct FMaterial
    {
        alignas(16) float Shininess;
    };

    struct FLight
    {
        glm::aligned_vec3 Position;
        glm::aligned_vec3 Ambient;
        glm::aligned_vec3 Diffuse;
        glm::aligned_vec3 Specular;
    };

    struct FLightMaterial
    {
        FMaterial         Material;
        FLight            Light;
        glm::aligned_vec3 ViewPos;
    } LightMaterial;

    Grt::FShaderResourceManager::FUniformBufferCreateInfo LightMaterialCreateInfo
    {
        .Name    = "LightMaterial",
        .Fields  = { "Material", "Light", "ViewPos" },
        .Set     = 0,
        .Binding = 1,
        .Usage   = vk::DescriptorType::eUniformBuffer
    };

    auto ShaderResourceManager = Grt::FShaderResourceManager::GetInstance();
    ShaderResourceManager->CreateBuffer<FBlackHolePos>(BlackHolePosCreateInfo);
    ShaderResourceManager->CreateBuffer<FBlackHoleArgs>(BlackHoleArgsCreateInfo);
    ShaderResourceManager->CreateBuffer<FMatrices>(MatricesCreateInfo);
    ShaderResourceManager->CreateBuffer<FLightMaterial>(LightMaterialCreateInfo);

    // Create graphics pipeline
    // ------------------------
    vk::SamplerCreateInfo SamplerCreateInfo = Art::FTextureBase::CreateDefaultSamplerCreateInfo();
    Grt::FVulkanSampler Sampler(SamplerCreateInfo);

    vk::DescriptorImageInfo SamplerInfo(*Sampler);
    BasicLightingShader->WriteSharedDescriptors<vk::DescriptorImageInfo>(1, 0, vk::DescriptorType::eSampler, { SamplerInfo });

    std::vector<vk::DescriptorImageInfo> ImageInfos;
    ImageInfos.push_back(ContainerDiffuse->CreateDescriptorImageInfo(nullptr));
    ImageInfos.push_back(ContainerSpecular->CreateDescriptorImageInfo(nullptr));
    BasicLightingShader->WriteSharedDescriptors(1, 1, vk::DescriptorType::eSampledImage, ImageInfos);

    SamplerCreateInfo
        .setMipmapMode(vk::SamplerMipmapMode::eNearest)
        .setAddressModeU(vk::SamplerAddressMode::eClampToEdge)
        .setAddressModeV(vk::SamplerAddressMode::eClampToEdge)
        .setAddressModeW(vk::SamplerAddressMode::eClampToEdge)
        .setAnisotropyEnable(vk::False)
        .setMinLod(0.0f)
        .setMaxLod(0.0f)
        .setBorderColor(vk::BorderColor::eFloatTransparentBlack);

    Grt::FVulkanSampler FramebufferSampler(SamplerCreateInfo);

    auto CreatePostDescriptors = [&]() -> void
    {
        ImageInfos.clear();

        vk::DescriptorImageInfo FramebufferImageInfo(*FramebufferSampler, *PostColorAttachment->GetImageView(), vk::ImageLayout::eShaderReadOnlyOptimal);
        vk::DescriptorImageInfo FramebufferImageInfo2(*FramebufferSampler, *PostColorAttachment2->GetImageView(), vk::ImageLayout::eShaderReadOnlyOptimal);

        ImageInfos.push_back(FramebufferImageInfo);
        ImageInfos.push_back(FramebufferImageInfo2);

        PostShader->WriteSharedDescriptors(0, 0, vk::DescriptorType::eCombinedImageSampler, ImageInfos);
    };

    CreatePostDescriptors();

    _VulkanContext->RegisterAutoRemovedCallbacks(Grt::FVulkanContext::ECallbackType::kCreateSwapchain, "CreatePostDescriptor", CreatePostDescriptors);

    ShaderResourceManager->BindShaderToBuffers("BlackHolePos", "BlackHole");
    ShaderResourceManager->BindShaderToBuffers("BlackHoleArgs", "BlackHole");
    ShaderResourceManager->BindShaderToBuffers("Matrices", "BasicLightingShader");
    ShaderResourceManager->BindShaderToBuffers("Matrices", "LampShader");
    ShaderResourceManager->BindShaderToBuffers("LightMaterial", "BasicLightingShader");

#include "Vertices.inc"

    Grt::FDeviceLocalBuffer VertexBuffer(CubeVertices.size() * sizeof(FVertex), vk::BufferUsageFlagBits::eVertexBuffer);
    VertexBuffer.CopyData(CubeVertices);
    Grt::FDeviceLocalBuffer QuadVertexBuffer(QuadVertices.size() * sizeof(FQuadVertex), vk::BufferUsageFlagBits::eVertexBuffer);
    QuadVertexBuffer.CopyData(QuadVertices);
    Grt::FDeviceLocalBuffer QuadOnlyVertexBuffer(QuadOnlyVertices.size() * sizeof(FQuadOnlyVertex), vk::BufferUsageFlagBits::eVertexBuffer);
    QuadOnlyVertexBuffer.CopyData(QuadOnlyVertices);

    auto BlackHoleShaderStageCreateInfos = BlackHoleShader->CreateShaderStageCreateInfo();
    auto BasicLightingShaderStageCreateInfos = BasicLightingShader->CreateShaderStageCreateInfo();
    auto LampShaderStageCreateInfos = LampShader->CreateShaderStageCreateInfo();
    auto PostShaderStageCreateInfos = PostShader->CreateShaderStageCreateInfo();

    auto* PipelineManager = Grt::FPipelineManager::GetInstance();

    vk::PipelineColorBlendAttachmentState ColorBlendAttachmentState = vk::PipelineColorBlendAttachmentState()
        .setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                           vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);

    vk::PipelineRenderingCreateInfo BlackHoleRenderingCreateInfo = vk::PipelineRenderingCreateInfo()
        .setColorAttachmentCount(1)
        .setColorAttachmentFormats(_VulkanContext->GetSwapchainCreateInfo().imageFormat);

    Grt::FGraphicsPipelineCreateInfoPack BlackHoleCreateInfoPack;
    BlackHoleCreateInfoPack.GraphicsPipelineCreateInfo.setPNext(&BlackHoleRenderingCreateInfo);
    BlackHoleCreateInfoPack.InputAssemblyStateCreateInfo.setTopology(vk::PrimitiveTopology::eTriangleList);
    BlackHoleCreateInfoPack.ColorBlendAttachmentStates.emplace_back(ColorBlendAttachmentState);

    BlackHoleCreateInfoPack.Viewports.emplace_back(0.0f, static_cast<float>(_WindowSize.height),
                                                   static_cast<float>(_WindowSize.width), -static_cast<float>(_WindowSize.height),
                                                   0.0f, 1.0f);
    BlackHoleCreateInfoPack.Scissors.emplace_back(vk::Offset2D(), _WindowSize);

    PipelineManager->CreatePipeline("BlackHolePipeline", "BlackHole", BlackHoleCreateInfoPack);

    std::array ColorFormats
    {
        _VulkanContext->GetSwapchainCreateInfo().imageFormat,
        _VulkanContext->GetSwapchainCreateInfo().imageFormat
    };

    vk::PipelineRenderingCreateInfo PipelineRenderingCreateInfo = vk::PipelineRenderingCreateInfo()
        .setColorAttachmentCount(2)
        .setColorAttachmentFormats(ColorFormats)
        .setDepthAttachmentFormat(vk::Format::eD32SfloatS8Uint);

    Grt::FGraphicsPipelineCreateInfoPack CreateInfoPack;
    CreateInfoPack.GraphicsPipelineCreateInfo.setPNext(&PipelineRenderingCreateInfo);
    CreateInfoPack.InputAssemblyStateCreateInfo.setTopology(vk::PrimitiveTopology::eTriangleList);

    CreateInfoPack.MultisampleStateCreateInfo
        .setRasterizationSamples(vk::SampleCountFlagBits::e8)
        .setSampleShadingEnable(vk::True)
        .setMinSampleShading(1.0f);

    CreateInfoPack.DepthStencilStateCreateInfo
        .setDepthTestEnable(vk::True)
        .setDepthWriteEnable(vk::True)
        .setDepthCompareOp(vk::CompareOp::eLess)
        .setDepthBoundsTestEnable(vk::False)
        .setStencilTestEnable(vk::False);

    CreateInfoPack.ColorBlendAttachmentStates.emplace_back(ColorBlendAttachmentState);
    CreateInfoPack.ColorBlendAttachmentStates.emplace_back(ColorBlendAttachmentState);

    CreateInfoPack.Viewports.emplace_back(0.0f, static_cast<float>(_WindowSize.height),
                                          static_cast<float>(_WindowSize.width), -static_cast<float>(_WindowSize.height),
                                          0.0f, 1.0f);
    CreateInfoPack.Scissors.emplace_back(vk::Offset2D(), _WindowSize);

    PipelineManager->CreatePipeline("ContainerPipeline", "BasicLightingShader", CreateInfoPack);
    PipelineManager->CreatePipeline("LampPipeline", "LampShader", CreateInfoPack);

    vk::PipelineRenderingCreateInfo PipelineRenderingCreateInfo2 = vk::PipelineRenderingCreateInfo()
        .setColorAttachmentCount(1)
        .setColorAttachmentFormats(_VulkanContext->GetSwapchainCreateInfo().imageFormat);

    CreateInfoPack.GraphicsPipelineCreateInfo.setPNext(&PipelineRenderingCreateInfo2);

    CreateInfoPack.MultisampleStateCreateInfo
        .setRasterizationSamples(vk::SampleCountFlagBits::e1)
        .setSampleShadingEnable(vk::False)
        .setMinSampleShading(0.0f);

    CreateInfoPack.ColorBlendAttachmentStates.pop_back();

    PipelineManager->CreatePipeline("PostPipeline", "PostShader", CreateInfoPack);

    vk::Pipeline BlackHolePipeline;
    vk::Pipeline ContainerPipeline;
    vk::Pipeline LampPipeline;
    vk::Pipeline PostPipeline;

    auto GetPipelines = [&]() -> void
    {
        BlackHolePipeline = PipelineManager->GetPipeline("BlackHolePipeline");
        ContainerPipeline = PipelineManager->GetPipeline("ContainerPipeline");
        LampPipeline      = PipelineManager->GetPipeline("LampPipeline");
        PostPipeline      = PipelineManager->GetPipeline("PostPipeline");
    };

    GetPipelines();

    _VulkanContext->RegisterAutoRemovedCallbacks(Grt::FVulkanContext::ECallbackType::kCreateSwapchain, "GetPipelines", GetPipelines);

    auto BlackHolePipelineLayout = PipelineManager->GetPipelineLayout("BlackHolePipeline");
    auto ContainerPipelineLayout = PipelineManager->GetPipelineLayout("ContainerPipeline");
    auto LampPipelineLayout      = PipelineManager->GetPipelineLayout("LampPipeline");
    auto PostPipelineLayout      = PipelineManager->GetPipelineLayout("PostPipeline");

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

    glm::vec3 LightPos(1.2f, 1.0f, 2.0f);
    glm::vec4 WorldUp(0.0f, 1.0f, 0.0f, 1.0f);

    while (!glfwWindowShouldClose(_Window))
    {
        while (glfwGetWindowAttrib(_Window, GLFW_ICONIFIED))
        {
            glfwWaitEvents();
        }

        InFlightFences[CurrentFrame].WaitAndReset();

        // Uniform update
        // --------------
        //glm::mat4x4 Model(1.0f);
        //Matrices.View         = _FreeCamera->GetViewMatrix();
        //Matrices.Projection   = _FreeCamera->GetProjectionMatrix(static_cast<float>(_WindowSize.width) / static_cast<float>(_WindowSize.height), 0.1f);
        //Matrices.NormalMatrix = glm::mat3x3(glm::transpose(glm::inverse(Model)));

        //ShaderResourceManager->UpdateEntrieBuffer(CurrentFrame, "Matrices", Matrices);

        //LightMaterial.Material.Shininess = 64.0f;
        //LightMaterial.Light.Position     = LightPos;
        //LightMaterial.Light.Ambient      = glm::vec3(0.1f);
        //LightMaterial.Light.Diffuse      = glm::vec3(1.0f);
        //LightMaterial.Light.Specular     = glm::vec3(1.0f);
        //LightMaterial.ViewPos            = _FreeCamera->GetCameraVector(SysSpa::FCamera::EVectorType::kPosition);

        //ShaderResourceManager->UpdateEntrieBuffer(CurrentFrame, "LightMaterial", LightMaterial);

        BlackHolePos.WorldUpView                 = glm::vec3(glm::mat4_cast(_FreeCamera->GetOrientation()) * WorldUp);
        BlackHolePos.BlackHoleRelativePos        = glm::vec3(0.0f, 0.0f, -0.0001f);
        BlackHolePos.BlackHoleRelativeDiskNormal = glm::vec3(0.2f, 1.0f, 0.5f);

        ShaderResourceManager->UpdateEntrieBuffer(CurrentFrame, "BlackHolePos", BlackHolePos);

        BlackHoleArgs.Resolution       = glm::vec2(_WindowSize.width, _WindowSize.height);
        BlackHoleArgs.FovRadians       = glm::radians(_FreeCamera->GetCameraZoom());
        BlackHoleArgs.Time             = static_cast<float>(glfwGetTime());
        BlackHoleArgs.TimeRate         = 30.0f;
        BlackHoleArgs.BlackHoleMassSol = 1.49e7f;
        BlackHoleArgs.Spin             = 0.0f;
        BlackHoleArgs.Mu               = 1.0f;
        BlackHoleArgs.AccretionRate    = 5e15f;
        BlackHoleArgs.InterRadiusLy    = 9.7756e-6f;
        BlackHoleArgs.OuterRadiusLy    = 5.586e-5f;

        ShaderResourceManager->UpdateEntrieBuffer(CurrentFrame, "BlackHoleArgs", BlackHoleArgs);

        _VulkanContext->SwapImage(*Semaphores_ImageAvailable[CurrentFrame]);
        std::uint32_t ImageIndex = _VulkanContext->GetCurrentImageIndex();

        BlackHoleAttachmentInfo.setResolveImageView(_VulkanContext->GetSwapchainImageView(ImageIndex));

        // Record commands
        // ---------------
        auto& CurrentBuffer = CommandBuffers[CurrentFrame];
        CurrentBuffer.Begin(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

        vk::ImageSubresourceRange SubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);
        vk::ImageMemoryBarrier2 InitialSwapchainTransitionBarrier(vk::PipelineStageFlagBits2::eTopOfPipe,
                                                                  vk::AccessFlagBits2::eNone,
                                                                  vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                                                                  vk::AccessFlagBits2::eColorAttachmentWrite,
                                                                  vk::ImageLayout::eUndefined,
                                                                  vk::ImageLayout::eColorAttachmentOptimal,
                                                                  vk::QueueFamilyIgnored,
                                                                  vk::QueueFamilyIgnored,
                                                                  _VulkanContext->GetSwapchainImage(ImageIndex),
                                                                  SubresourceRange);
        //vk::ImageMemoryBarrier2 InitialSwapchainTransitionBarrier(vk::PipelineStageFlagBits2::eTopOfPipe,
        //                                                          vk::AccessFlagBits2::eNone,
        //                                                          vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        //                                                          vk::AccessFlagBits2::eColorAttachmentWrite,
        //                                                          vk::ImageLayout::eUndefined,
        //                                                          vk::ImageLayout::eColorAttachmentOptimal,
        //                                                          vk::QueueFamilyIgnored,
        //                                                          vk::QueueFamilyIgnored,
        //                                                          _VulkanContext->GetSwapchainImage(ImageIndex),
        //                                                          SubresourceRange);

        //vk::ImageMemoryBarrier2 PostRestoreTransitionBarrier(vk::PipelineStageFlagBits2::eTopOfPipe,
        //                                                     vk::AccessFlagBits2::eNone,
        //                                                     vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        //                                                     vk::AccessFlagBits2::eColorAttachmentWrite,
        //                                                     vk::ImageLayout::eUndefined,
        //                                                     vk::ImageLayout::eColorAttachmentOptimal,
        //                                                     vk::QueueFamilyIgnored,
        //                                                     vk::QueueFamilyIgnored,
        //                                                     *PostColorAttachment->GetImage(),
        //                                                     SubresourceRange);

        //vk::ImageMemoryBarrier2 PostRestoreTransitionBarrier2(vk::PipelineStageFlagBits2::eTopOfPipe,
        //                                                      vk::AccessFlagBits2::eNone,
        //                                                      vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        //                                                      vk::AccessFlagBits2::eColorAttachmentWrite,
        //                                                      vk::ImageLayout::eUndefined,
        //                                                      vk::ImageLayout::eColorAttachmentOptimal,
        //                                                      vk::QueueFamilyIgnored,
        //                                                      vk::QueueFamilyIgnored,
        //                                                      *PostColorAttachment2->GetImage(),
        //                                                      SubresourceRange);

        //std::array InitialTransitionBarriers{ InitialSwapchainTransitionBarrier, PostRestoreTransitionBarrier, PostRestoreTransitionBarrier2 };

        //vk::DependencyInfo InitialDependencyInfo = vk::DependencyInfo()
        //    .setDependencyFlags(vk::DependencyFlagBits::eByRegion)
        //    .setImageMemoryBarriers(InitialTransitionBarriers);

        //CurrentBuffer->pipelineBarrier2(InitialDependencyInfo);

        vk::DependencyInfo InitialDependencyInfo = vk::DependencyInfo()
            .setDependencyFlags(vk::DependencyFlagBits::eByRegion)
            .setImageMemoryBarriers(InitialSwapchainTransitionBarrier);

        CurrentBuffer->pipelineBarrier2(InitialDependencyInfo);

        //vk::RenderingInfo RenderingInfo = vk::RenderingInfo()
        //    .setRenderArea(vk::Rect2D({ 0, 0 }, _WindowSize))
        //    .setLayerCount(1)
        //    .setColorAttachments(ColorAttachmentInfos)
        //    .setPDepthAttachment(&DepthStencilAttachmentInfo);

        vk::RenderingInfo RenderingInfo = vk::RenderingInfo()
            .setRenderArea(vk::Rect2D({ 0, 0 }, _WindowSize))
            .setLayerCount(1)
            .setColorAttachments(BlackHoleAttachmentInfo);

        CurrentBuffer->beginRendering(RenderingInfo);
        CurrentBuffer->bindVertexBuffers(0, *QuadOnlyVertexBuffer.GetBuffer(), Offset);
        CurrentBuffer->bindPipeline(vk::PipelineBindPoint::eGraphics, BlackHolePipeline);
        CurrentBuffer->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, BlackHolePipelineLayout, 0, BlackHoleShader->GetDescriptorSets(CurrentFrame), {});
        CurrentBuffer->draw(6, 1, 0, 0);
        CurrentBuffer->endRendering();

        //CurrentBuffer->beginRendering(RenderingInfo);
        //CurrentBuffer->bindPipeline(vk::PipelineBindPoint::eGraphics, ContainerPipeline);
        //CurrentBuffer->bindVertexBuffers(0, *VertexBuffer.GetBuffer(), Offset);
        //CurrentBuffer->pushConstants(ContainerPipelineLayout, vk::ShaderStageFlagBits::eVertex, BasicLightingShader->GetPushConstantOffset("iModel"), sizeof(glm::mat4x4), glm::value_ptr(Model));
        //CurrentBuffer->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, ContainerPipelineLayout, 0, BasicLightingShader->GetDescriptorSets(CurrentFrame), {});
        //CurrentBuffer->draw(36, 1, 0, 0);

        //Model = glm::scale(glm::translate(Model, LightPos), glm::vec3(0.2f));

        //CurrentBuffer->bindPipeline(vk::PipelineBindPoint::eGraphics, LampPipeline);
        //CurrentBuffer->pushConstants(LampPipelineLayout, vk::ShaderStageFlagBits::eVertex, LampShader->GetPushConstantOffset("iModel"), sizeof(glm::mat4x4), glm::value_ptr(Model));
        //CurrentBuffer->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, LampPipelineLayout, 0, LampShader->GetDescriptorSets(CurrentFrame), {});
        //CurrentBuffer->draw(36, 1, 0, 0);
        //CurrentBuffer->endRendering();

        //vk::ImageMemoryBarrier2 RenderEndTransitionBarrier(vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        //                                                   vk::AccessFlagBits2::eColorAttachmentWrite,
        //                                                   vk::PipelineStageFlagBits2::eFragmentShader,
        //                                                   vk::AccessFlagBits2::eShaderRead,
        //                                                   vk::ImageLayout::eColorAttachmentOptimal,
        //                                                   vk::ImageLayout::eShaderReadOnlyOptimal,
        //                                                   vk::QueueFamilyIgnored,
        //                                                   vk::QueueFamilyIgnored,
        //                                                   *PostColorAttachment->GetImage(),
        //                                                   SubresourceRange);

        //vk::ImageMemoryBarrier2 RenderEndTransitionBarrier2(vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        //                                                    vk::AccessFlagBits2::eColorAttachmentWrite,
        //                                                    vk::PipelineStageFlagBits2::eFragmentShader,
        //                                                    vk::AccessFlagBits2::eShaderRead,
        //                                                    vk::ImageLayout::eColorAttachmentOptimal,
        //                                                    vk::ImageLayout::eShaderReadOnlyOptimal,
        //                                                    vk::QueueFamilyIgnored,
        //                                                    vk::QueueFamilyIgnored,
        //                                                    *PostColorAttachment2->GetImage(),
        //                                                    SubresourceRange);

        //std::array RenderEndTransitionBarriers{ RenderEndTransitionBarrier, RenderEndTransitionBarrier2 };

        //vk::DependencyInfo RenderEndDependencyInfo = vk::DependencyInfo()
        //    .setDependencyFlags(vk::DependencyFlagBits::eByRegion)
        //    .setImageMemoryBarriers(RenderEndTransitionBarriers);

        //CurrentBuffer->pipelineBarrier2(RenderEndDependencyInfo);

        //vk::RenderingInfo PostRenderingInfo = vk::RenderingInfo()
        //    .setRenderArea(vk::Rect2D({ 0, 0 }, _WindowSize))
        //    .setLayerCount(1)
        //    .setColorAttachments(FinalOutputAttachmentInfo);

        //FinalOutputAttachmentInfo.setImageView(_VulkanContext->GetSwapchainImageView(ImageIndex));

        //CurrentBuffer->beginRendering(PostRenderingInfo);
        //CurrentBuffer->bindPipeline(vk::PipelineBindPoint::eGraphics, PostPipeline);
        //CurrentBuffer->bindVertexBuffers(0, *QuadVertexBuffer.GetBuffer(), Offset);
        //CurrentBuffer->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, PostPipelineLayout, 0, PostShader->GetDescriptorSets(CurrentFrame), {});
        //CurrentBuffer->draw(6, 1, 0, 0);
        //CurrentBuffer->endRendering();

        vk::ImageMemoryBarrier2 PresentTransitionBarrier(vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                                                         vk::AccessFlagBits2::eColorAttachmentWrite,
                                                         vk::PipelineStageFlagBits2::eBottomOfPipe,
                                                         vk::AccessFlagBits2::eNone,
                                                         vk::ImageLayout::eColorAttachmentOptimal,
                                                         vk::ImageLayout::ePresentSrcKHR,
                                                         vk::QueueFamilyIgnored,
                                                         vk::QueueFamilyIgnored,
                                                         _VulkanContext->GetSwapchainImage(ImageIndex),
                                                         SubresourceRange);

        vk::DependencyInfo FinalDependencyInfo = vk::DependencyInfo()
            .setDependencyFlags(vk::DependencyFlagBits::eByRegion)
            .setImageMemoryBarriers(PresentTransitionBarrier);

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
