#pragma once

// #include <array>
#include <functional>
#include <memory>
#include <string>
#include <utility>

#include <magic_enum/magic_enum.hpp>
#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan_handles.hpp>

#include "Engine/Core/Base/Base.h"
#include "Engine/Core/Runtime/Graphics/Vulkan/Core.h"
#include "Engine/Core/Runtime/Graphics/Vulkan/Wrappers.h"

_NPGS_BEGIN
_RUNTIME_BEGIN
_GRAPHICS_BEGIN

class FVulkanContext
{
public:
    enum class ECallbackType
    {
        kCreateSwapchain,
        kDestroySwapchain,
        kCreateDevice,
        kDestroyDevice
    };

public:
    void AddCreateDeviceCallback(const std::string& Name, const std::function<void()>& Callback);
    void AddDestroyDeviceCallback(const std::string& Name, const std::function<void()>& Callback);
    void AddCreateSwapchainCallback(const std::string& Name, const std::function<void()>& Callback);
    void AddDestroySwapchainCallback(const std::string& Name, const std::function<void()>& Callback);
    void RemoveCreateDeviceCallback(const std::string& Name);
    void RemoveDestroyDeviceCallback(const std::string& Name);
    void RemoveCreateSwapchainCallback(const std::string& Name);
    void RemoveDestroySwapchainCallback(const std::string& Name);
    void RegisterAutoRemovedCallbacks(ECallbackType Type, const std::string& Name, const std::function<void()>& Callback);
    void RemoveRegisteredCallbacks();

    void AddInstanceLayer(const char* Layer);
    void SetInstanceLayers(const std::vector<const char*>& Layers);
    void AddInstanceExtension(const char* Extension);
    void SetInstanceExtensions(const std::vector<const char*>& Extensions);
    void AddDeviceExtension(const char* Extension);
    void SetDeviceExtensions(const std::vector<const char*>& Extensions);
    vk::Result CheckInstanceLayers();
    vk::Result CheckInstanceExtensions(const std::string& Layer);
    vk::Result CheckDeviceExtensions();

    vk::Result CreateInstance(vk::InstanceCreateFlags Flags = {});
    vk::Result CreateDevice(std::uint32_t PhysicalDeviceIndex = 0, vk::DeviceCreateFlags Flags = {});
    vk::Result RecreateDevice(std::uint32_t PhysicalDeviceIndex = 0, vk::DeviceCreateFlags Flags = {});
    void       SetSurface(vk::SurfaceKHR Surface);
    vk::Result SetSurfaceFormat(const vk::SurfaceFormatKHR& SurfaceFormat);
    vk::Result CreateSwapchain(vk::Extent2D Extent, bool bLimitFps = true, vk::SwapchainCreateFlagsKHR Flags = {});
    vk::Result RecreateSwapchain();

    vk::Result ExecuteGraphicsCommands(vk::CommandBuffer CommandBuffer) const;
    vk::Result ExecuteGraphicsCommands(const FVulkanCommandBuffer& CommandBuffer) const;
    vk::Result SubmitCommandBufferToGraphics(const vk::SubmitInfo& SubmitInfo,   vk::Fence Fence           = {}) const;
    vk::Result SubmitCommandBufferToGraphics(const vk::SubmitInfo& SubmitInfo,   const FVulkanFence* Fence = nullptr) const;
    vk::Result SubmitCommandBufferToGraphics(vk::CommandBuffer Buffer,           vk::Fence Fence           = {}) const;
    vk::Result SubmitCommandBufferToGraphics(const FVulkanCommandBuffer& Buffer, const FVulkanFence* Fence = nullptr) const;

    vk::Result SubmitCommandBufferToGraphics(vk::CommandBuffer      Buffer,
                                             vk::Semaphore          WaitSemaphore   = {},
                                             vk::Semaphore          SignalSemaphore = {},
                                             vk::Fence              Fence           = {},
                                             vk::PipelineStageFlags Flags           = vk::PipelineStageFlagBits::eColorAttachmentOutput) const;

    vk::Result SubmitCommandBufferToGraphics(const FVulkanCommandBuffer& Buffer,
                                             const FVulkanSemaphore*     WaitSemaphore   = nullptr,
                                             const FVulkanSemaphore*     SignalSemaphore = nullptr, 
                                             const FVulkanFence*         Fence           = nullptr, 
                                             vk::PipelineStageFlags      Flags           = vk::PipelineStageFlagBits::eColorAttachmentOutput) const;

    vk::Result SubmitCommandBufferToPresent(vk::CommandBuffer Buffer,
                                            vk::Semaphore     WaitSemaphore   = {},
                                            vk::Semaphore     SignalSemaphore = {},
                                            vk::Fence         Fence           = {}) const;

    vk::Result SubmitCommandBufferToPresent(const FVulkanCommandBuffer& Buffer,
                                            const FVulkanSemaphore*     WaitSemaphore   = nullptr,
                                            const FVulkanSemaphore*     SignalSemaphore = nullptr,
                                            const FVulkanFence*         Fence           = nullptr) const;

    vk::Result SubmitCommandBufferToCompute(const vk::SubmitInfo& SubmitInfo,   vk::Fence Fence           = {}) const;
    vk::Result SubmitCommandBufferToCompute(const vk::SubmitInfo& SubmitInfo,   const FVulkanFence* Fence = nullptr) const;
    vk::Result SubmitCommandBufferToCompute(vk::CommandBuffer Buffer,           vk::Fence Fence           = {}) const;
    vk::Result SubmitCommandBufferToCompute(const FVulkanCommandBuffer& Buffer, const FVulkanFence* Fence = nullptr) const;

    vk::Result SubmitCommandBufferToCompute(vk::CommandBuffer      Buffer,
                                            vk::Semaphore          WaitSemaphore   = {},
                                            vk::Semaphore          SignalSemaphore = {},
                                            vk::Fence              Fence           = {},
                                            vk::PipelineStageFlags Flags           = vk::PipelineStageFlagBits::eComputeShader) const;

    vk::Result SubmitCommandBufferToCompute(const FVulkanCommandBuffer& Buffer,
                                            const FVulkanSemaphore*     WaitSemaphore   = nullptr,
                                            const FVulkanSemaphore*     SignalSemaphore = nullptr,
                                            const FVulkanFence*         Fence           = nullptr,
                                            vk::PipelineStageFlags      Flags           = vk::PipelineStageFlagBits::eComputeShader) const;

    vk::Result TransferImageOwnershipToPresent(vk::CommandBuffer PresentCommandBuffer) const;
    vk::Result TransferImageOwnershipToPresent(const FVulkanCommandBuffer& PresentCommandBuffer) const;

    vk::Result SwapImage(vk::Semaphore Semaphore);
    vk::Result SwapImage(const FVulkanSemaphore& Semaphore);
    vk::Result PresentImage(const vk::PresentInfoKHR& PresentInfo);
    vk::Result PresentImage(vk::Semaphore Semaphore);
    vk::Result PresentImage(const FVulkanSemaphore& Semaphore);
    vk::Result WaitIdle() const;

    vk::Instance GetInstance() const;
    vk::SurfaceKHR GetSurface() const;
    vk::PhysicalDevice GetPhysicalDevice() const;
    vk::Device GetDevice() const;
    vk::Queue GetGraphicsQueue() const;
    vk::Queue GetPresentQueue() const;
    vk::Queue GetComputeQueue() const;
    vk::SwapchainKHR GetSwapchain() const;
    VmaAllocator GetVmaAllocator() const;

    const vk::PhysicalDeviceProperties& GetPhysicalDeviceProperties() const;
    const vk::PhysicalDeviceMemoryProperties& GetPhysicalDeviceMemoryProperties() const;
    const vk::SwapchainCreateInfoKHR& GetSwapchainCreateInfo() const;

    std::uint32_t GetAvailablePhysicalDeviceCount() const;
    std::uint32_t GetAvailableSurfaceFormatCount() const;
    std::uint32_t GetSwapchainImageCount() const;
    std::uint32_t GetSwapchainImageViewCount() const;
    vk::SampleCountFlagBits GetMaxUsableSampleCount() const;

    vk::PhysicalDevice GetAvailablePhysicalDevice(std::uint32_t Index) const;
    vk::Format GetAvailableSurfaceFormat(std::uint32_t Index) const;
    vk::ColorSpaceKHR GetAvailableSurfaceColorSpace(std::uint32_t Index) const;
    vk::Image GetSwapchainImage(std::uint32_t Index) const;
    vk::ImageView GetSwapchainImageView(std::uint32_t Index) const;

    std::uint32_t GetGraphicsQueueFamilyIndex() const;
    std::uint32_t GetPresentQueueFamilyIndex() const;
    std::uint32_t GetComputeQueueFamilyIndex() const;
    std::uint32_t GetCurrentImageIndex() const;
    
    const FVulkanCommandBuffer& GetTransferCommandBuffer() const;
    const FVulkanCommandBuffer& GetPresentCommandBuffer() const;
    const FVulkanCommandPool& GetGraphicsCommandPool() const;
    const FVulkanCommandPool& GetPresentCommandPool() const;
    const FVulkanCommandPool& GetComputeCommandPool() const;

    // const vk::FormatProperties& GetFormatProperties(vk::Format Format) const;

    std::uint32_t GetApiVersion() const;

    static FVulkanContext* GetClassInstance();

private:
    FVulkanContext();
    FVulkanContext(const FVulkanContext&) = delete;
    FVulkanContext(FVulkanContext&&)      = delete;
    ~FVulkanContext();

    FVulkanContext& operator=(const FVulkanContext&) = delete;
    FVulkanContext& operator=(FVulkanContext&&)      = delete;

    void TransferImageOwnershipToPresentImpl(vk::CommandBuffer PresentCommandBuffer) const;

private:
    FVulkanCore* _VulkanCore;

    std::vector<std::pair<ECallbackType, std::string>>                     _AutoRemovedCallbacks;
    // std::array<vk::FormatProperties, magic_enum::enum_count<vk::Format>()> _FormatProperties;

    std::unique_ptr<FVulkanCommandPool>    _GraphicsCommandPool;
    std::unique_ptr<FVulkanCommandPool>    _PresentCommandPool;
    std::unique_ptr<FVulkanCommandPool>    _ComputeCommandPool;
    std::unique_ptr<FVulkanCommandBuffer>  _TransferCommandBuffer;
    std::unique_ptr<FVulkanCommandBuffer>  _PresentCommandBuffer;
};

_GRAPHICS_END
_RUNTIME_END
_NPGS_END

#include "Context.inl"
