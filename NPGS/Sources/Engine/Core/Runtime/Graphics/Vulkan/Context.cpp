#include "Context.h"

#include <cstddef>

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_to_string.hpp>
#include "Engine/Core/Runtime/Graphics/Vulkan/Wrappers.h"

_NPGS_BEGIN
_RUNTIME_BEGIN
_GRAPHICS_BEGIN

namespace
{
    vk::SubmitInfo CreateSubmitInfo(const vk::CommandBuffer& CommandBuffer, const vk::Semaphore& WaitSemaphore,
                                    const vk::Semaphore& SignalSemaphore, vk::PipelineStageFlags Flags = {});
}

FVulkanContext::FVulkanContext()
    :
    _VulkanCore(FVulkanCore::GetClassInstance()),
    _TransferCommandBuffer(std::make_unique<FVulkanCommandBuffer>()),
    _PresentCommandBuffer(std::make_unique<FVulkanCommandBuffer>())
{
    auto InitializeCommandPool = [this]() -> void
    {
        if (_VulkanCore->GetGraphicsQueueFamilyIndex() != vk::QueueFamilyIgnored)
        {
            _GraphicsCommandPool =
                std::make_unique<FVulkanCommandPool>(_VulkanCore->GetDevice(), _VulkanCore->GetGraphicsQueueFamilyIndex(),
                                                     vk::CommandPoolCreateFlagBits::eResetCommandBuffer);
            _GraphicsCommandPool->AllocateBuffer(vk::CommandBufferLevel::ePrimary, *_TransferCommandBuffer);
        }
        if (_VulkanCore->GetComputeQueueFamilyIndex() != vk::QueueFamilyIgnored)
        {
            _ComputeCommandPool =
                std::make_unique<FVulkanCommandPool>(_VulkanCore->GetDevice(), _VulkanCore->GetComputeQueueFamilyIndex(),
                                                     vk::CommandPoolCreateFlagBits::eResetCommandBuffer);
        }
        if (_VulkanCore->GetPresentQueueFamilyIndex() != vk::QueueFamilyIgnored &&
            _VulkanCore->GetPresentQueueFamilyIndex() != _VulkanCore->GetGraphicsQueueFamilyIndex() &&
            _VulkanCore->GetSwapchainCreateInfo().imageSharingMode == vk::SharingMode::eExclusive)
        {
            _PresentCommandPool =
                std::make_unique<FVulkanCommandPool>(_VulkanCore->GetDevice(), _VulkanCore->GetPresentQueueFamilyIndex(),
                                                     vk::CommandPoolCreateFlagBits::eResetCommandBuffer);
            _PresentCommandPool->AllocateBuffer(vk::CommandBufferLevel::ePrimary, *_PresentCommandBuffer);
        }
    };

    // auto InitializeFormatProperties = [this]() -> void
    // {
    //     std::size_t Index = 0;
    //     for (vk::Format Format : magic_enum::enum_values<vk::Format>())
    //     {
    //         _FormatProperties[Index++] = _VulkanCore->GetPhysicalDevice().getFormatProperties(Format);
    //     }
    // };

    _VulkanCore->AddCreateDeviceCallback("InitializeCommandPool", InitializeCommandPool);
    // _VulkanCore->AddCreateDeviceCallback("InitializeFormatProperties", InitializeFormatProperties);
}

FVulkanContext::~FVulkanContext()
{
    WaitIdle();
    RemoveRegisteredCallbacks();
}

void FVulkanContext::RegisterAutoRemovedCallbacks(ECallbackType Type, const std::string& Name, const std::function<void()>& Callback)
{
    switch (Type)
    {
    case ECallbackType::kCreateSwapchain:
        AddCreateSwapchainCallback(Name, Callback);
        break;
    case ECallbackType::kDestroySwapchain:
        AddDestroySwapchainCallback(Name, Callback);
        break;
    case ECallbackType::kCreateDevice:
        AddCreateDeviceCallback(Name, Callback);
        break;
    case ECallbackType::kDestroyDevice:
        AddDestroyDeviceCallback(Name, Callback);
        break;
    default:
        break;
    }

    std::pair<ECallbackType, std::string> AutoRemovedCallback(Type, Name);
    _AutoRemovedCallbacks.push_back(AutoRemovedCallback);
}

void FVulkanContext::RemoveRegisteredCallbacks()
{
    for (const auto& [Type, Name] : _AutoRemovedCallbacks)
    {
        switch (Type)
        {
        case ECallbackType::kCreateSwapchain:
            RemoveCreateSwapchainCallback(Name);
            break;
        case ECallbackType::kDestroySwapchain:
            RemoveDestroySwapchainCallback(Name);
            break;
        case ECallbackType::kCreateDevice:
            RemoveCreateDeviceCallback(Name);
            break;
        case ECallbackType::kDestroyDevice:
            RemoveDestroyDeviceCallback(Name);
            break;
        default:
            break;
        }
    }
}

vk::Result FVulkanContext::ExecuteGraphicsCommands(vk::CommandBuffer CommandBuffer) const
{
    FVulkanFence Fence(_VulkanCore->GetDevice());
    vk::Result Result;
    if ((Result = SubmitCommandBufferToGraphics(CommandBuffer, *Fence)) == vk::Result::eSuccess)
    {
        Fence.Wait();
    }
    else
    {
        NpgsCoreError("Failed to execute graphics command: {}", vk::to_string(Result));
        return Result;
    }

    return vk::Result::eSuccess;
}

vk::Result FVulkanContext::SubmitCommandBufferToGraphics(const vk::SubmitInfo& SubmitInfo, vk::Fence Fence) const
{
    try
    {
        _VulkanCore->GetGraphicsQueue().submit(SubmitInfo, Fence);
    }
    catch (const vk::SystemError& e)
    {
        NpgsCoreError("Failed to submit command buffer to graphics queue: {}", e.what());
        return static_cast<vk::Result>(e.code().value());
    }

    return vk::Result::eSuccess;
}

vk::Result FVulkanContext::SubmitCommandBufferToGraphics(vk::CommandBuffer Buffer, vk::Fence Fence) const
{
    vk::SubmitInfo SubmitInfo(0, nullptr, nullptr, 1, &Buffer, 0, nullptr);
    return SubmitCommandBufferToGraphics(SubmitInfo, Fence);
}

vk::Result
FVulkanContext::SubmitCommandBufferToGraphics(vk::CommandBuffer Buffer, vk::Semaphore WaitSemaphore,
                                              vk::Semaphore SignalSemaphore, vk::Fence Fence, vk::PipelineStageFlags Flags) const
{
    vk::SubmitInfo SubmitInfo = CreateSubmitInfo(Buffer, WaitSemaphore, SignalSemaphore, Flags);
    return SubmitCommandBufferToGraphics(SubmitInfo, Fence);
}

vk::Result FVulkanContext::SubmitCommandBufferToPresent(vk::CommandBuffer Buffer, vk::Semaphore WaitSemaphore,
                                                        vk::Semaphore SignalSemaphore, vk::Fence Fence) const
{
    vk::SubmitInfo SubmitInfo = CreateSubmitInfo(Buffer, WaitSemaphore, SignalSemaphore, vk::PipelineStageFlagBits::eAllCommands);
    try
    {
        _VulkanCore->GetPresentQueue().submit(SubmitInfo, Fence);
    }
    catch (const vk::SystemError& e)
    {
        NpgsCoreError("Failed to submit command buffer to present queue: {}", e.what());
        return static_cast<vk::Result>(e.code().value());
    }

    return vk::Result::eSuccess;
}

vk::Result FVulkanContext::SubmitCommandBufferToCompute(const vk::SubmitInfo& SubmitInfo, vk::Fence Fence) const
{
    try
    {
        _VulkanCore->GetComputeQueue().submit(SubmitInfo, Fence);
    }
    catch (const vk::SystemError& e)
    {
        NpgsCoreError("Failed to submit command buffer to compute queue: {}", e.what());
        return static_cast<vk::Result>(e.code().value());
    }

    return vk::Result::eSuccess;
}

vk::Result FVulkanContext::SubmitCommandBufferToCompute(vk::CommandBuffer Buffer, vk::Fence Fence) const
{
    vk::SubmitInfo SubmitInfo(1, nullptr, nullptr, 1, &Buffer, 0, nullptr);
    return SubmitCommandBufferToCompute(SubmitInfo, Fence);
}

vk::Result
FVulkanContext::SubmitCommandBufferToCompute(vk::CommandBuffer Buffer, vk::Semaphore WaitSemaphore,
                                             vk::Semaphore SignalSemaphore, vk::Fence Fence, vk::PipelineStageFlags Flags) const
{
    vk::SubmitInfo SubmitInfo = CreateSubmitInfo(Buffer, WaitSemaphore, SignalSemaphore, Flags);
    return SubmitCommandBufferToCompute(SubmitInfo, Fence);
}

vk::Result FVulkanContext::TransferImageOwnershipToPresent(vk::CommandBuffer PresentCommandBuffer) const
{
    vk::CommandBufferBeginInfo CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    try
    {
        PresentCommandBuffer.begin(CommandBufferBeginInfo);
    }
    catch (const vk::SystemError& e)
    {
        NpgsCoreError("Failed to begin present command buffer: {}", e.what());
        return static_cast<vk::Result>(e.code().value());
    }

    TransferImageOwnershipToPresentImpl(PresentCommandBuffer);
    return vk::Result::eSuccess;
}

vk::Result FVulkanContext::TransferImageOwnershipToPresent(const FVulkanCommandBuffer& PresentCommandBuffer) const
{
    if (vk::Result Result = PresentCommandBuffer.Begin(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
        Result != vk::Result::eSuccess)
    {
        return Result;
    }

    TransferImageOwnershipToPresentImpl(*PresentCommandBuffer);
    return vk::Result::eSuccess;
}

FVulkanContext* FVulkanContext::GetClassInstance()
{
    static FVulkanContext Instance;
    return &Instance;
}

void FVulkanContext::TransferImageOwnershipToPresentImpl(vk::CommandBuffer PresentCommandBuffer) const
{
    vk::ImageMemoryBarrier ImageMemoryBarrier(vk::AccessFlagBits::eColorAttachmentWrite,
                                              vk::AccessFlagBits::eNone,
                                              vk::ImageLayout::ePresentSrcKHR,
                                              vk::ImageLayout::ePresentSrcKHR,
                                              _VulkanCore->GetGraphicsQueueFamilyIndex(), 
                                              _VulkanCore->GetPresentQueueFamilyIndex(), 
                                              _VulkanCore->GetSwapchainImage(_VulkanCore->GetCurrentImageIndex()), 
                                              vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));

    PresentCommandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput,
                                         vk::PipelineStageFlagBits::eBottomOfPipe, {}, {}, {}, ImageMemoryBarrier);
}

namespace
{
    vk::SubmitInfo CreateSubmitInfo(const vk::CommandBuffer& CommandBuffer, const vk::Semaphore& WaitSemaphore,
                                    const vk::Semaphore& SignalSemaphore, vk::PipelineStageFlags Flags)
    {
        vk::SubmitInfo SubmitInfo;
        SubmitInfo.setCommandBuffers(CommandBuffer);

        if (WaitSemaphore)
        {
            SubmitInfo.setWaitSemaphores(WaitSemaphore);
            SubmitInfo.setWaitDstStageMask(Flags);
        }

        if (SignalSemaphore)
        {
            SubmitInfo.setSignalSemaphores(SignalSemaphore);
        }

        return SubmitInfo;
    }
}

_GRAPHICS_END
_RUNTIME_END
_NPGS_END
