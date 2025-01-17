#include "Application.h"

#include <cstddef>
#include <utility>

#include "Engine/Core/Runtime/AssetLoaders/Texture.h"
#include "Engine/Core/Runtime/Graphics/Vulkan/Buffers.h"
#include "Engine/Utils/Logger.h"

_NPGS_BEGIN

namespace Art = Runtime::Asset;
namespace Grt = Runtime::Graphics;

struct FImageMemoryBarrierPack
{
    vk::PipelineStageFlags PipelineStageFlags{ vk::PipelineStageFlagBits::eNone };
    vk::AccessFlags        AccessFlags{ vk::AccessFlagBits::eNone };
    vk::ImageLayout        ImageLayout{ vk::ImageLayout::eUndefined };
    bool                   bEnable{ false };
};

vk::Image CopyBufferToImage(const Grt::FVulkanCommandBuffer& CommandBuffer, const Grt::FVulkanBuffer& SrcBuffer, const vk::BufferImageCopy& Region,
                            const FImageMemoryBarrierPack& SrcBarrier, const FImageMemoryBarrierPack& DstBarrier)
{
    vk::Image Target = Grt::FVulkanContext::GetClassInstance()->GetSwapchainImage(Grt::FVulkanContext::GetClassInstance()->GetCurrentImageIndex());

    vk::ImageSubresourceRange ImageSubresourceRange(Region.imageSubresource.aspectMask, Region.imageSubresource.mipLevel, 1,
                                                    Region.imageSubresource.baseArrayLayer, Region.imageSubresource.layerCount);

    vk::ImageMemoryBarrier ImageMemoryBarrier(SrcBarrier.AccessFlags, vk::AccessFlagBits::eTransferWrite,
                                              SrcBarrier.ImageLayout, vk::ImageLayout::eTransferDstOptimal,
                                              vk::QueueFamilyIgnored, vk::QueueFamilyIgnored, Target, ImageSubresourceRange);

    if (SrcBarrier.bEnable)
    {
        CommandBuffer->pipelineBarrier(SrcBarrier.PipelineStageFlags, vk::PipelineStageFlagBits::eTransfer,
                                       {}, {}, {}, ImageMemoryBarrier);
    }

    CommandBuffer->copyBufferToImage(*SrcBuffer, Target, vk::ImageLayout::eTransferDstOptimal, Region);

    if (DstBarrier.bEnable)
    {
        ImageMemoryBarrier.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite)
                          .setDstAccessMask(DstBarrier.AccessFlags)
                          .setOldLayout(vk::ImageLayout::eTransferDstOptimal)
                          .setNewLayout(DstBarrier.ImageLayout);

        CommandBuffer->pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, DstBarrier.PipelineStageFlags,
                                       {}, {}, {}, ImageMemoryBarrier);
    }

    return Target;
}

vk::Image BlitImage(const Grt::FVulkanCommandBuffer& CommandBuffer, vk::Image SrcImage, const vk::ImageBlit& Region,
                    const FImageMemoryBarrierPack& SrcBarrier, const FImageMemoryBarrierPack& DstBarrier, vk::Filter Filter)
{
    vk::Image Target = Grt::FVulkanContext::GetClassInstance()->GetSwapchainImage(Grt::FVulkanContext::GetClassInstance()->GetCurrentImageIndex());

    vk::ImageSubresourceRange ImageSubresourceRange(Region.dstSubresource.aspectMask, Region.dstSubresource.mipLevel, 1,
                                                    Region.dstSubresource.baseArrayLayer, Region.dstSubresource.layerCount);

    vk::ImageMemoryBarrier ImageMemoryBarrier(SrcBarrier.AccessFlags, vk::AccessFlagBits::eTransferWrite,
                                              SrcBarrier.ImageLayout, vk::ImageLayout::eTransferDstOptimal,
                                              vk::QueueFamilyIgnored, vk::QueueFamilyIgnored, Target, ImageSubresourceRange);

    if (SrcBarrier.bEnable)
    {
        CommandBuffer->pipelineBarrier(SrcBarrier.PipelineStageFlags, vk::PipelineStageFlagBits::eTransfer,
                                       {}, {}, {}, ImageMemoryBarrier);
    }

    CommandBuffer->blitImage(SrcImage, vk::ImageLayout::eTransferSrcOptimal, Target,
                             vk::ImageLayout::eTransferDstOptimal, Region, Filter);

    if (DstBarrier.bEnable)
    {
        ImageMemoryBarrier.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite)
                          .setDstAccessMask(DstBarrier.AccessFlags)
                          .setOldLayout(vk::ImageLayout::eTransferDstOptimal)
                          .setNewLayout(DstBarrier.ImageLayout);

        CommandBuffer->pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, DstBarrier.PipelineStageFlags,
                                       {}, {}, {}, ImageMemoryBarrier);
    }

    return Target;
}

FApplication::FApplication(const vk::Extent2D& WindowSize, const std::string& WindowTitle,
                           bool bEnableVSync, bool bEnableFullscreen)
    :
    _VulkanContext(Grt::FVulkanContext::GetClassInstance()),
    _WindowTitle(WindowTitle),
    _WindowSize(WindowSize),
    _Window(nullptr),
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
            _Renderer.Framebuffers.emplace_back(FramebufferCreateInfo);
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
    Art::FTexture2D Texture("AwesomeFace.png", vk::Format::eR8G8B8A8Srgb, vk::Format::eR8G8B8A8Unorm, true);
    vk::SamplerCreateInfo SamplerCreateInfo = Art::FTextureBase::CreateSamplerCreateInfo();
    Grt::FVulkanSampler Sampler(SamplerCreateInfo);

    vk::DescriptorSetLayoutBinding DescriptorSetLayoutBinding = vk::DescriptorSetLayoutBinding()
        .setBinding(0)
        .setDescriptorType(vk::DescriptorType::eUniformBufferDynamic)
        .setDescriptorCount(1)
        .setStageFlags(vk::ShaderStageFlagBits::eVertex);

    vk::DescriptorSetLayoutBinding TextureDescriptorSetLayoutBinding = vk::DescriptorSetLayoutBinding()
        .setBinding(1)
        .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
        .setDescriptorCount(1)
        .setStageFlags(vk::ShaderStageFlagBits::eFragment);

    std::vector<vk::DescriptorSetLayoutBinding> Bindings{ DescriptorSetLayoutBinding, TextureDescriptorSetLayoutBinding };

    vk::DescriptorSetLayoutCreateInfo DescriptorSetLayoutCreateInfo({}, Bindings);
    //vk::DescriptorSetLayoutCreateInfo DescriptorSetLayoutCreateInfo({}, DescriptorSetLayoutBinding);

    std::vector<Grt::FVulkanDescriptorSetLayout> DescriptorSetLayouts;
    for (std::uint32_t i = 0; i != kFramesInFlightCount; ++i)
    {
        DescriptorSetLayouts.emplace_back(DescriptorSetLayoutCreateInfo);
    }

    std::vector<vk::DescriptorSetLayout> Temp;
    for (std::size_t i = 0; i != DescriptorSetLayouts.size(); ++i)
    {
        Temp.emplace_back(*DescriptorSetLayouts[i]);
    }

    // vk::PushConstantRange PushConstantRange(vk::ShaderStageFlagBits::eVertex, 0, 24);

    vk::PipelineLayoutCreateInfo PipelineLayoutCreateInfo;
    PipelineLayoutCreateInfo.setSetLayouts(Temp);
    // PipelineLayoutCreateInfo.setPushConstantRanges(PushConstantRange);

    _PipelineLayout = std::make_unique<Grt::FVulkanPipelineLayout>(PipelineLayoutCreateInfo);

    // Create graphics pipeline
    // ------------------------
    std::vector<glm::vec2> Offsets
    {
        glm::vec2( 0.0f, 0.5f),
        glm::vec2(-0.5f, 0.0f),
        glm::vec2( 0.5f, 0.0f)
    };

    vk::DeviceSize MinUniformAlignment = _VulkanContext->GetPhysicalDeviceProperties().limits.minUniformBufferOffsetAlignment;
    vk::DeviceSize UniformAlignment    = (sizeof(glm::vec2) + MinUniformAlignment - 1) & ~(MinUniformAlignment - 1);
    std::vector<Grt::FDeviceLocalBuffer> UniformBuffers;
    for (std::uint32_t i = 0; i != kFramesInFlightCount; ++i)
    {
        UniformBuffers.emplace_back(Offsets.size() * UniformAlignment, vk::BufferUsageFlagBits::eUniformBuffer);
        UniformBuffers[i].CopyData(3, sizeof(glm::vec2), sizeof(glm::vec2), UniformAlignment, 0, Offsets.data());
        UniformBuffers[i].EnablePersistentMapping();
    }

    std::vector<Grt::FVulkanDescriptorSet> DescriptorSets(kFramesInFlightCount);

    std::vector<vk::DescriptorPoolSize> Sizes
    {
        { vk::DescriptorType::eUniformBufferDynamic, kFramesInFlightCount },
        { vk::DescriptorType::eCombinedImageSampler, kFramesInFlightCount }
    };

    Grt::FVulkanDescriptorPool DescriptorPool(kFramesInFlightCount, Sizes);
    DescriptorPool.AllocateSets(DescriptorSetLayouts, DescriptorSets);

    for (std::uint32_t i = 0; i != kFramesInFlightCount; ++i)
    {
        vk::DescriptorBufferInfo BufferInfo(*UniformBuffers[i].GetBuffer(), 0, sizeof(glm::vec2));
        DescriptorSets[i].Write({ BufferInfo }, vk::DescriptorType::eUniformBufferDynamic, 0);

        vk::DescriptorImageInfo ImageInfo(*Sampler, *Texture.GetImageView(), vk::ImageLayout::eShaderReadOnlyOptimal);
        DescriptorSets[i].Write({ ImageInfo }, vk::DescriptorType::eCombinedImageSampler, 1);
    }

    std::vector<FVertex> Vertices
    {
        { { -0.5f, -0.5f }, { 1.0f, 1.0f, 0.0f, 1.0f }, { 0.0f, 0.0f } },
        { {  0.5f, -0.5f }, { 1.0f, 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f } },
        { { -0.5f,  0.5f }, { 0.0f, 1.0f, 0.0f, 1.0f }, { 0.0f, 1.0f } },
        { {  0.5f,  0.5f }, { 0.0f, 0.0f, 1.0f, 1.0f }, { 1.0f, 1.0f } }
    };

    std::vector<std::uint16_t> Indices
    {
        0, 1, 2,
        1, 2, 3
    };

    Grt::FDeviceLocalBuffer VertexBuffer(Vertices.size() * sizeof(FVertex), vk::BufferUsageFlagBits::eVertexBuffer);
    VertexBuffer.CopyData(Vertices);
    Grt::FDeviceLocalBuffer IndexBuffer(Indices.size() * sizeof(std::uint16_t), vk::BufferUsageFlagBits::eIndexBuffer);
    IndexBuffer.CopyData(Indices);

    static Grt::FVulkanShaderModule VertShaderModule("Sources/Engine/Shaders/Triangle.vert.spv");
    static Grt::FVulkanShaderModule FragShaderModule("Sources/Engine/Shaders/Triangle.frag.spv");

    static vk::PipelineShaderStageCreateInfo VertShaderStage = vk::PipelineShaderStageCreateInfo()
        .setStage(vk::ShaderStageFlagBits::eVertex)
        .setModule(*VertShaderModule)
        .setPName("main");
    static vk::PipelineShaderStageCreateInfo FragShaderStage = vk::PipelineShaderStageCreateInfo()
        .setStage(vk::ShaderStageFlagBits::eFragment)
        .setModule(*FragShaderModule)
        .setPName("main");

    static std::vector<vk::PipelineShaderStageCreateInfo> ShaderStageCreateInfos{ VertShaderStage, FragShaderStage };

    auto CreateGraphicsPipeline = [this]() -> void
    {
        // 先创建新管线，再销毁旧管线
        Grt::FGraphicsPipelineCreateInfoPack CreateInfoPack;
        CreateInfoPack.GraphicsPipelineCreateInfo.setLayout(**_PipelineLayout);
        CreateInfoPack.GraphicsPipelineCreateInfo.setRenderPass(**_Renderer.RenderPass);

        CreateInfoPack.VertexInputBindings.emplace_back(0, static_cast<std::uint32_t>(sizeof(FVertex)), vk::VertexInputRate::eVertex);
        // CreateInfoPack.VertexInputBindings.emplace_back(1, static_cast<std::uint32_t>(sizeof(glm::vec2)), vk::VertexInputRate::eInstance);
        CreateInfoPack.VertexInputAttributes.emplace_back(0, 0, vk::Format::eR32G32Sfloat, 0);
        CreateInfoPack.VertexInputAttributes.emplace_back(1, 0, vk::Format::eR32G32B32A32Sfloat, static_cast<std::uint32_t>(offsetof(FVertex, Color)));
        CreateInfoPack.VertexInputAttributes.emplace_back(2, 0, vk::Format::eR32G32Sfloat, static_cast<std::uint32_t>(offsetof(FVertex, TexCoord)));
        // CreateInfoPack.VertexInputAttributes.emplace_back(2, 1, vk::Format::eR32G32Sfloat, 0);

        CreateInfoPack.InputAssemblyStateCreateInfo.setTopology(vk::PrimitiveTopology::eTriangleList);
        CreateInfoPack.MultisampleStateCreateInfo.setRasterizationSamples(vk::SampleCountFlagBits::e1);

        CreateInfoPack.ShaderStages = ShaderStageCreateInfos;

        vk::PipelineColorBlendAttachmentState ColorBlendAttachmentState = vk::PipelineColorBlendAttachmentState()
            .setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                               vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);

        // CreateInfoPack.DynamicStates.emplace_back(vk::DynamicState::eViewport);
        // CreateInfoPack.DynamicStates.emplace_back(vk::DynamicState::eScissor);

        CreateInfoPack.Viewports.emplace_back(0.0f, 0.0f, static_cast<float>(_WindowSize.width),
                                              static_cast<float>(_WindowSize.height), 0.0f, 1.0f);
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

    vk::ClearValue ColorValue({ 0.0f, 0.0f, 0.0f, 1.0f });
    vk::DeviceSize Offset       = 0;
    std::uint32_t  CurrentFrame = 0;

    while (!glfwWindowShouldClose(_Window))
    {
        while (glfwGetWindowAttrib(_Window, GLFW_ICONIFIED))
        {
            glfwWaitEvents();
        }

        Offsets[0].x += static_cast<float>(0.00001f * std::sin(glfwGetTime()));
        Offsets[0].y += static_cast<float>(0.00001f * std::cos(glfwGetTime()));
        Offsets[1].x += static_cast<float>(0.00001f * std::cos(glfwGetTime()));
        Offsets[1].y += static_cast<float>(0.00001f * std::sin(glfwGetTime()));
        Offsets[2].x -= static_cast<float>(0.00001f * std::sin(glfwGetTime()));
        Offsets[2].y -= static_cast<float>(0.00001f * std::cos(glfwGetTime()));

        UniformBuffers[CurrentFrame].CopyData(3, sizeof(glm::vec2), sizeof(glm::vec2), UniformAlignment, 0, Offsets.data());

        InFlightFences[CurrentFrame].WaitAndReset();

        _VulkanContext->SwapImage(*Semaphores_ImageAvailable[CurrentFrame]);
        std::uint32_t ImageIndex = _VulkanContext->GetCurrentImageIndex();

        CommandBuffers[CurrentFrame].Begin(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
        RenderPass->CommandBegin(CommandBuffers[CurrentFrame], Framebuffers[ImageIndex], { {}, _WindowSize }, { ColorValue });
        CommandBuffers[CurrentFrame]->bindVertexBuffers(0, *VertexBuffer.GetBuffer(), Offset);
        //CommandBuffers[CurrentFrame]->bindVertexBuffers(1, *_InstanceVertexBuffer->GetBuffer(), Offset);
        CommandBuffers[CurrentFrame]->bindIndexBuffer(*IndexBuffer.GetBuffer(), Offset, vk::IndexType::eUint16);
        CommandBuffers[CurrentFrame]->bindPipeline(vk::PipelineBindPoint::eGraphics, **_GraphicsPipeline);
        for (std::uint32_t i = 0; i != 3; ++i)
        {
            std::uint32_t DyanmicOffset = i * static_cast<std::uint32_t>(UniformAlignment);
            CommandBuffers[CurrentFrame]->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, **_PipelineLayout, 0, *DescriptorSets[CurrentFrame], { DyanmicOffset });
            CommandBuffers[CurrentFrame]->drawIndexed(6, 1, 0, 0, 0);
        }
        //CommandBuffers[CurrentFrame]->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, **_PipelineLayout, 0, *DescriptorSets[CurrentFrame], {});
        //CommandBuffers[CurrentFrame]->draw(3, 1, 0, 0);
        //CommandBuffers[CurrentFrame]->pushConstants(**_PipelineLayout, vk::ShaderStageFlagBits::eVertex, 0, 24, Offsets.data());
        //CommandBuffers[CurrentFrame]->drawIndexed(6, 3, 0, 0, 0);
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
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, true);

    _Window = glfwCreateWindow(_WindowSize.width, _WindowSize.height, _WindowTitle.c_str(), nullptr, nullptr);
    if (_Window == nullptr)
    {
        NpgsCoreError("Failed to create GLFW window.");
        glfwTerminate();
        return false;
    }

    glfwSetWindowUserPointer(_Window, this);
    glfwSetFramebufferSizeCallback(_Window, &FApplication::FramebufferSizeCallback);

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

    return true;
}

void FApplication::ShowTitleFps()
{
    static double CurrentTime   = 0.0;
    static double PreviousTime  = glfwGetTime();
    static double LastFrameTime = 0.0;
    static double DeltaTime     = 0.0;
    static int    FrameCount    = 0;

    CurrentTime   = glfwGetTime();
    DeltaTime     = CurrentTime - LastFrameTime;
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
        glfwSetWindowShouldClose(_Window, GL_TRUE);
    }
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

_NPGS_END
