#include "Context.h"

_NPGS_BEGIN
_RUNTIME_BEGIN
_GRAPHICS_BEGIN

NPGS_INLINE void FVulkanContext::AddCreateDeviceCallback(const std::string& Name, const std::function<void()>& Callback)
{
    _VulkanCore->AddCreateDeviceCallback(Name, Callback);
}

NPGS_INLINE void FVulkanContext::AddDestroyDeviceCallback(const std::string& Name, const std::function<void()>& Callback)
{
    _VulkanCore->AddDestroyDeviceCallback(Name, Callback);
}

NPGS_INLINE void FVulkanContext::AddCreateSwapchainCallback(const std::string& Name, const std::function<void()>& Callback)
{
    _VulkanCore->AddCreateSwapchainCallback(Name, Callback);
}

NPGS_INLINE void FVulkanContext::AddDestroySwapchainCallback(const std::string& Name, const std::function<void()>& Callback)
{
    _VulkanCore->AddDestroySwapchainCallback(Name, Callback);
}

NPGS_INLINE void FVulkanContext::RemoveCreateDeviceCallback(const std::string& Name)
{
    _VulkanCore->RemoveCreateDeviceCallback(Name);
}

NPGS_INLINE void FVulkanContext::RemoveDestroyDeviceCallback(const std::string& Name)
{
    _VulkanCore->RemoveDestroyDeviceCallback(Name);
}

NPGS_INLINE void FVulkanContext::RemoveCreateSwapchainCallback(const std::string& Name)
{
    _VulkanCore->RemoveCreateSwapchainCallback(Name);
}

NPGS_INLINE void FVulkanContext::RemoveDestroySwapchainCallback(const std::string& Name)
{
    _VulkanCore->RemoveDestroySwapchainCallback(Name);
}

NPGS_INLINE void FVulkanContext::AddInstanceLayer(const char* Layer)
{
    _VulkanCore->AddInstanceLayer(Layer);
}

NPGS_INLINE void FVulkanContext::SetInstanceLayers(const std::vector<const char*>& Layers)
{
    _VulkanCore->SetInstanceLayers(Layers);
}

NPGS_INLINE void FVulkanContext::AddInstanceExtension(const char* Extension)
{
    _VulkanCore->AddInstanceExtension(Extension);
}

NPGS_INLINE void FVulkanContext::SetInstanceExtensions(const std::vector<const char*>& Extensions)
{
    _VulkanCore->SetInstanceExtensions(Extensions);
}

NPGS_INLINE void FVulkanContext::AddDeviceExtension(const char* Extension)
{
    _VulkanCore->AddDeviceExtension(Extension);
}

NPGS_INLINE void FVulkanContext::SetDeviceExtensions(const std::vector<const char*>& Extensions)
{
    _VulkanCore->SetDeviceExtensions(Extensions);
}

NPGS_INLINE vk::Result FVulkanContext::CheckInstanceLayers()
{
    return _VulkanCore->CheckInstanceLayers();
}

NPGS_INLINE vk::Result FVulkanContext::CheckInstanceExtensions(const std::string& Layer)
{
    return _VulkanCore->CheckInstanceExtensions(Layer);
}

NPGS_INLINE vk::Result FVulkanContext::CheckDeviceExtensions()
{
    return _VulkanCore->CheckDeviceExtensions();
}

NPGS_INLINE vk::Result FVulkanContext::CreateInstance(vk::InstanceCreateFlags Flags)
{
    return _VulkanCore->CreateInstance(Flags);
}

NPGS_INLINE vk::Result FVulkanContext::CreateDevice(std::uint32_t PhysicalDeviceIndex, vk::DeviceCreateFlags Flags)
{
    return _VulkanCore->CreateDevice(PhysicalDeviceIndex, Flags);
}

NPGS_INLINE vk::Result FVulkanContext::RecreateDevice(std::uint32_t PhysicalDeviceIndex, vk::DeviceCreateFlags Flags)
{
    return _VulkanCore->RecreateDevice(PhysicalDeviceIndex, Flags);
}

NPGS_INLINE void FVulkanContext::SetSurface(vk::SurfaceKHR Surface)
{
    _VulkanCore->SetSurface(Surface);
}

NPGS_INLINE vk::Result FVulkanContext::SetSurfaceFormat(const vk::SurfaceFormatKHR& SurfaceFormat)
{
    return _VulkanCore->SetSurfaceFormat(SurfaceFormat);
}

NPGS_INLINE vk::Result FVulkanContext::CreateSwapchain(vk::Extent2D Extent, bool bLimitFps, vk::SwapchainCreateFlagsKHR Flags)
{
    return _VulkanCore->CreateSwapchain(Extent, bLimitFps, Flags);
}

NPGS_INLINE vk::Result FVulkanContext::RecreateSwapchain()
{
    return _VulkanCore->RecreateSwapchain();
}

NPGS_INLINE vk::Result FVulkanContext::ExecuteGraphicsCommands(const FVulkanCommandBuffer& CommandBuffer) const
{
    return ExecuteGraphicsCommands(*CommandBuffer);
}

NPGS_INLINE vk::Result FVulkanContext::ExecutePresentCommands(const FVulkanCommandBuffer& CommandBuffer) const
{
    return ExecutePresentCommands(*CommandBuffer);
}

NPGS_INLINE vk::Result FVulkanContext::ExecuteComputeCommands(const FVulkanCommandBuffer& CommandBuffer) const
{
    return ExecuteComputeCommands(*CommandBuffer);
}

NPGS_INLINE vk::Result
FVulkanContext::SubmitCommandBufferToGraphics(const vk::SubmitInfo& SubmitInfo, const FVulkanFence* Fence) const
{
    return SubmitCommandBufferToGraphics(SubmitInfo, Fence ? **Fence : vk::Fence());
}

NPGS_INLINE vk::Result
FVulkanContext::SubmitCommandBufferToGraphics(const FVulkanCommandBuffer& Buffer, const FVulkanFence* Fence) const
{
    return SubmitCommandBufferToGraphics(*Buffer,  Fence ? **Fence : vk::Fence());
}

NPGS_INLINE vk::Result
FVulkanContext::SubmitCommandBufferToGraphics(const FVulkanCommandBuffer& Buffer,
                                              const FVulkanSemaphore*     WaitSemaphore,
                                              const FVulkanSemaphore*     SignalSemaphore,
                                              const FVulkanFence*         Fence,
                                              vk::PipelineStageFlags      Flags) const
{
    return SubmitCommandBufferToGraphics(*Buffer, WaitSemaphore ? **WaitSemaphore : vk::Semaphore(),
                                         SignalSemaphore ? **SignalSemaphore : vk::Semaphore(),
                                         Fence ? **Fence : vk::Fence(), Flags);
}

NPGS_INLINE vk::Result
FVulkanContext::SubmitCommandBufferToPresent(const vk::SubmitInfo& SubmitInfo, const FVulkanFence* Fence) const
{
    return SubmitCommandBufferToPresent(SubmitInfo, Fence ? **Fence : vk::Fence());
}

NPGS_INLINE vk::Result
FVulkanContext::SubmitCommandBufferToPresent(const FVulkanCommandBuffer& Buffer, const FVulkanFence* Fence) const
{
    return SubmitCommandBufferToPresent(*Buffer, **Fence);
}

NPGS_INLINE vk::Result
FVulkanContext::SubmitCommandBufferToPresent(const FVulkanCommandBuffer& Buffer,
                                             const FVulkanSemaphore*     WaitSemaphore,
                                             const FVulkanSemaphore*     SignalSemaphore,
                                             const FVulkanFence*         Fence) const
{
    SubmitCommandBufferToPresent(*Buffer, WaitSemaphore ? **WaitSemaphore : vk::Semaphore(),
                                 SignalSemaphore ? **SignalSemaphore : vk::Semaphore(),
                                 Fence ? **Fence : vk::Fence());
}

NPGS_INLINE vk::Result
FVulkanContext::SubmitCommandBufferToCompute(const vk::SubmitInfo& SubmitInfo, const FVulkanFence* Fence) const
{
    return SubmitCommandBufferToCompute(SubmitInfo, Fence ? **Fence : vk::Fence());
}

NPGS_INLINE vk::Result
FVulkanContext::SubmitCommandBufferToCompute(const FVulkanCommandBuffer& Buffer, const FVulkanFence* Fence) const
{
    return SubmitCommandBufferToCompute(*Buffer, **Fence);
}

NPGS_INLINE vk::Result
FVulkanContext::SubmitCommandBufferToCompute(const FVulkanCommandBuffer& Buffer,
                                             const FVulkanSemaphore*     WaitSemaphore,
                                             const FVulkanSemaphore*     SignalSemaphore,
                                             const FVulkanFence*         Fence,
                                             vk::PipelineStageFlags      Flags) const
{
    return SubmitCommandBufferToCompute(*Buffer, WaitSemaphore ? **WaitSemaphore : vk::Semaphore(),
                                        SignalSemaphore ? **SignalSemaphore : vk::Semaphore(),
                                        Fence ? **Fence : vk::Fence(), Flags);
}

NPGS_INLINE vk::Result FVulkanContext::SwapImage(vk::Semaphore Semaphore)
{
    return _VulkanCore->SwapImage(Semaphore);
}

NPGS_INLINE vk::Result FVulkanContext::SwapImage(const FVulkanSemaphore& Semaphore)
{
    return SwapImage(*Semaphore);
}

NPGS_INLINE vk::Result FVulkanContext::PresentImage(const vk::PresentInfoKHR& PresentInfo)
{
    return _VulkanCore->PresentImage(PresentInfo);
}

NPGS_INLINE vk::Result FVulkanContext::PresentImage(vk::Semaphore Semaphore)
{
    return _VulkanCore->PresentImage(Semaphore);
}

NPGS_INLINE vk::Result FVulkanContext::PresentImage(const FVulkanSemaphore& Semaphore)
{
    return PresentImage(*Semaphore);
}

NPGS_INLINE vk::Result FVulkanContext::WaitIdle() const
{
    return _VulkanCore->WaitIdle();
}

NPGS_INLINE vk::Instance FVulkanContext::GetInstance() const
{
    return _VulkanCore->GetInstance();
}

NPGS_INLINE vk::SurfaceKHR FVulkanContext::GetSurface() const
{
    return _VulkanCore->GetSurface();
}

NPGS_INLINE vk::PhysicalDevice FVulkanContext::GetPhysicalDevice() const
{
    return _VulkanCore->GetPhysicalDevice();
}

NPGS_INLINE vk::Device FVulkanContext::GetDevice() const
{
    return _VulkanCore->GetDevice();
}

NPGS_INLINE vk::Queue FVulkanContext::GetGraphicsQueue() const
{
    return _VulkanCore->GetGraphicsQueue();
}

NPGS_INLINE vk::Queue FVulkanContext::GetPresentQueue() const
{
    return _VulkanCore->GetPresentQueue();
}

NPGS_INLINE vk::Queue FVulkanContext::GetComputeQueue() const
{
    return _VulkanCore->GetComputeQueue();
}

NPGS_INLINE vk::SwapchainKHR FVulkanContext::GetSwapchain() const
{
    return _VulkanCore->GetSwapchain();
}

NPGS_INLINE VmaAllocator FVulkanContext::GetVmaAllocator() const
{
    return _VulkanCore->GetVmaAllocator();
}

NPGS_INLINE const vk::PhysicalDeviceProperties& FVulkanContext::GetPhysicalDeviceProperties() const
{
    return _VulkanCore->GetPhysicalDeviceProperties();
}

NPGS_INLINE const vk::PhysicalDeviceMemoryProperties& FVulkanContext::GetPhysicalDeviceMemoryProperties() const
{
    return _VulkanCore->GetPhysicalDeviceMemoryProperties();
}

NPGS_INLINE const vk::SwapchainCreateInfoKHR& FVulkanContext::GetSwapchainCreateInfo() const
{
    return _VulkanCore->GetSwapchainCreateInfo();
}

NPGS_INLINE std::uint32_t FVulkanContext::GetAvailablePhysicalDeviceCount() const
{
    return _VulkanCore->GetAvailablePhysicalDeviceCount();
}

NPGS_INLINE std::uint32_t FVulkanContext::GetAvailableSurfaceFormatCount() const
{
    return _VulkanCore->GetAvailableSurfaceFormatCount();
}

NPGS_INLINE std::uint32_t FVulkanContext::GetSwapchainImageCount() const
{
    return _VulkanCore->GetSwapchainImageCount();
}

NPGS_INLINE std::uint32_t FVulkanContext::GetSwapchainImageViewCount() const
{
    return _VulkanCore->GetSwapchainImageViewCount();
}

NPGS_INLINE vk::SampleCountFlagBits FVulkanContext::GetMaxUsableSampleCount() const
{
    vk::SampleCountFlags Counts = _VulkanCore->GetPhysicalDeviceProperties().limits.framebufferColorSampleCounts &
                                  _VulkanCore->GetPhysicalDeviceProperties().limits.framebufferDepthSampleCounts;

    if (Counts & vk::SampleCountFlagBits::e64) return vk::SampleCountFlagBits::e64;
    if (Counts & vk::SampleCountFlagBits::e32) return vk::SampleCountFlagBits::e32;
    if (Counts & vk::SampleCountFlagBits::e16) return vk::SampleCountFlagBits::e16;
    if (Counts & vk::SampleCountFlagBits::e8)  return vk::SampleCountFlagBits::e8;
    if (Counts & vk::SampleCountFlagBits::e4)  return vk::SampleCountFlagBits::e4;
    if (Counts & vk::SampleCountFlagBits::e2)  return vk::SampleCountFlagBits::e2;

    return vk::SampleCountFlagBits::e1;
}

NPGS_INLINE vk::PhysicalDevice FVulkanContext::GetAvailablePhysicalDevice(std::uint32_t Index) const
{
    return _VulkanCore->GetAvailablePhysicalDevice(Index);
}

NPGS_INLINE vk::Format FVulkanContext::GetAvailableSurfaceFormat(std::uint32_t Index) const
{
    return _VulkanCore->GetAvailableSurfaceFormat(Index);
}

NPGS_INLINE vk::ColorSpaceKHR FVulkanContext::GetAvailableSurfaceColorSpace(std::uint32_t Index) const
{
    return _VulkanCore->GetAvailableSurfaceColorSpace(Index);
}

NPGS_INLINE vk::Image FVulkanContext::GetSwapchainImage(std::uint32_t Index) const
{
    return _VulkanCore->GetSwapchainImage(Index);
}

NPGS_INLINE vk::ImageView FVulkanContext::GetSwapchainImageView(std::uint32_t Index) const
{
    return _VulkanCore->GetSwapchainImageView(Index);
}

NPGS_INLINE std::uint32_t FVulkanContext::GetGraphicsQueueFamilyIndex() const
{
    return _VulkanCore->GetGraphicsQueueFamilyIndex();
}

NPGS_INLINE std::uint32_t FVulkanContext::GetPresentQueueFamilyIndex() const
{
    return _VulkanCore->GetPresentQueueFamilyIndex();
}

NPGS_INLINE std::uint32_t FVulkanContext::GetComputeQueueFamilyIndex() const
{
    return _VulkanCore->GetComputeQueueFamilyIndex();
}

NPGS_INLINE std::uint32_t FVulkanContext::GetCurrentImageIndex() const
{
    return _VulkanCore->GetCurrentImageIndex();
}

NPGS_INLINE const FVulkanCommandBuffer& FVulkanContext::GetTransferCommandBuffer() const
{
    return *_TransferCommandBuffer;
}

NPGS_INLINE const FVulkanCommandBuffer& FVulkanContext::GetPresentCommandBuffer() const
{
    return *_PresentCommandBuffer;
}

NPGS_INLINE const FVulkanCommandPool& FVulkanContext::GetGraphicsCommandPool() const
{
    return *_GraphicsCommandPool;
}

NPGS_INLINE const FVulkanCommandPool& FVulkanContext::GetPresentCommandPool() const
{
    return *_PresentCommandPool;
}

NPGS_INLINE const FVulkanCommandPool& FVulkanContext::GetComputeCommandPool() const
{
    return *_ComputeCommandPool;
}

// NPGS_INLINE const vk::FormatProperties& FVulkanContext::GetFormatProperties(vk::Format Format) const
// {
//     auto Index = magic_enum::enum_index(Format);
//     if (!Index.has_value())
//     {
//         return _FormatProperties[0]; // vk::Format::eUndefined
//     }
// 
//     return _FormatProperties[Index.value()];
// }

NPGS_INLINE std::uint32_t FVulkanContext::GetApiVersion() const
{
    return _VulkanCore->GetApiVersion();
}

_GRAPHICS_END
_RUNTIME_END
_NPGS_END
