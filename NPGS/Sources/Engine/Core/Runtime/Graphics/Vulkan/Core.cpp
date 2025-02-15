#include "Core.h"

#include <cstdlib>
#include <algorithm>
#include <limits>

#include <vulkan/vulkan.hpp>

#include "Engine/Core/Runtime/Graphics/Vulkan/ExtFunctionsImpl.h"
#include "Engine/Utils/Logger.h"
#include "Engine/Utils/Utils.h"

_NPGS_BEGIN
_RUNTIME_BEGIN
_GRAPHICS_BEGIN

FVulkanCore::FVulkanCore()
    :
    _GraphicsQueueFamilyIndex(vk::QueueFamilyIgnored),
    _PresentQueueFamilyIndex(vk::QueueFamilyIgnored),
    _ComputeQueueFamilyIndex(vk::QueueFamilyIgnored),
    _CurrentImageIndex(std::numeric_limits<std::uint32_t>::max()),
    _ApiVersion(VK_API_VERSION_1_4)
{
    UseLatestApiVersion();
}

FVulkanCore::~FVulkanCore()
{
    if (_Instance)
    {
        if (_Device)
        {
            WaitIdle();
            if (_Swapchain)
            {
                for (auto& Callback : _DestroySwapchainCallbacks)
                {
                    Callback.second();
                }
                for (auto& ImageView : _SwapchainImageViews)
                {
                    if (ImageView)
                    {
                        _Device.destroyImageView(ImageView);
                    }
                }
                _SwapchainImageViews.clear();
                NpgsCoreInfo("Destroyed image views.");
                _Device.destroySwapchainKHR(_Swapchain);
                NpgsCoreInfo("Destroyed swapchain.");
            }

            for (auto& Callback : _DestroyDeviceCallbacks)
            {
                Callback.second();
            }
            _Device.destroy();
            NpgsCoreInfo("Destroyed logical device.");
        }

        if (_Surface)
        {
            _Instance.destroySurfaceKHR(_Surface);
            NpgsCoreInfo("Destroyed surface.");
        }

        if (_DebugUtilsMessenger)
        {
            _Instance.destroyDebugUtilsMessengerEXT(_DebugUtilsMessenger);
            NpgsCoreInfo("Destroyed debug messenger.");
        }

        _CreateSwapchainCallbacks.clear();
        _DestroySwapchainCallbacks.clear();
        _CreateDeviceCallbacks.clear();
        _DestroyDeviceCallbacks.clear();

        _DebugUtilsMessenger = vk::DebugUtilsMessengerEXT();
        _Surface             = vk::SurfaceKHR();
        _PhysicalDevice      = vk::PhysicalDevice();
        _Device              = vk::Device();
        _Swapchain           = vk::SwapchainKHR();
        _SwapchainCreateInfo = vk::SwapchainCreateInfoKHR();

        _Instance.destroy();
        NpgsCoreInfo("Destroyed Vulkan instance.");
    }
}

vk::Result FVulkanCore::CheckInstanceLayers()
{
    std::vector<vk::LayerProperties> AvailableLayers;
    try
    {
        AvailableLayers = vk::enumerateInstanceLayerProperties();
    }
    catch (const vk::SystemError& e)
    {
        NpgsCoreError("Failed to enumerate instance layers: {}", e.what());
        return static_cast<vk::Result>(e.code().value());
    }

    if (AvailableLayers.empty())
    {
        _InstanceLayers.clear();
        return vk::Result::eSuccess;
    }

    for (const char*& RequestedLayer : _InstanceLayers)
    {
        bool bLayerFound = false;
        for (const auto& AvailableLayer : AvailableLayers)
        {
            if (Util::Equal(RequestedLayer, AvailableLayer.layerName))
            {
                bLayerFound = true;
                break;
            }
        }
        if (!bLayerFound)
        {
            RequestedLayer = nullptr;
        }
    }

    std::erase_if(_InstanceLayers, [](const char* Layer) -> bool
    {
        return Layer == nullptr;
    });

    return vk::Result::eSuccess;
}

vk::Result FVulkanCore::CheckInstanceExtensions(const std::string& Layer)
{
    std::vector<vk::ExtensionProperties> AvailableExtensions;
    try
    {
        AvailableExtensions = vk::enumerateInstanceExtensionProperties(Layer);
    }
    catch (const vk::SystemError& e)
    {
        NpgsCoreError("Failed to enumerate instance extensions: {}", e.what());
        return static_cast<vk::Result>(e.code().value());
    }

    if (AvailableExtensions.empty())
    {
        _InstanceExtensions.clear();
        return vk::Result::eSuccess;
    }

    for (const char*& RequestedExtension : _InstanceExtensions)
    {
        bool bExtensionFound = false;
        for (const auto& AvailableExtension : AvailableExtensions)
        {
            if (Util::Equal(RequestedExtension, AvailableExtension.extensionName))
            {
                bExtensionFound = true;
                break;
            }
        }
        if (!bExtensionFound)
        {
            RequestedExtension = nullptr;
        }
    }

    std::erase_if(_InstanceExtensions, [](const char* Extension) -> bool
    {
        return Extension == nullptr;
    });

    return vk::Result::eSuccess;
}

vk::Result FVulkanCore::CheckDeviceExtensions()
{
    std::vector<vk::ExtensionProperties> AvailableExtensions;
    try
    {
        AvailableExtensions = _PhysicalDevice.enumerateDeviceExtensionProperties();
    }
    catch (const vk::SystemError& e)
    {
        NpgsCoreError("Failed to enumerate device extensions: {}", e.what());
        return static_cast<vk::Result>(e.code().value());
    }

    if (AvailableExtensions.empty())
    {
        _DeviceExtensions.clear();
        return vk::Result::eSuccess;
    }

    for (const char*& RequestedExtension : _DeviceExtensions)
    {
        bool bExtensionFound = false;
        for (const auto& AvailableExtension : AvailableExtensions)
        {
            if (Util::Equal(RequestedExtension, AvailableExtension.extensionName))
            {
                bExtensionFound = true;
                break;
            }
        }
        if (!bExtensionFound)
        {
            RequestedExtension = nullptr;
        }
    }

    std::erase_if(_DeviceExtensions, [](const char* Extension) -> bool
    {
        return Extension == nullptr;
    });

    return vk::Result::eSuccess;
}

vk::Result FVulkanCore::CreateInstance(vk::InstanceCreateFlags Flags)
{
#ifdef _DEBUG
    AddInstanceLayer("VK_LAYER_KHRONOS_validation");
    AddInstanceExtension(vk::EXTDebugUtilsExtensionName);
#endif // _DEBUG

    vk::ApplicationInfo ApplicationInfo("Von-Neumann in Galaxy Simulator", VK_MAKE_VERSION(1, 0, 0),
                                        "No Engine", VK_MAKE_VERSION(1, 0, 0), _ApiVersion);
    vk::InstanceCreateInfo InstanceCreateInfo(Flags, &ApplicationInfo, _InstanceLayers, _InstanceExtensions);

    try
    {
        _Instance = vk::createInstance(InstanceCreateInfo);
    }
    catch (const vk::SystemError& e)
    {
        NpgsCoreError("Failed to create Vulkan instance: {}", e.what());
        return static_cast<vk::Result>(e.code().value());
    }

#ifdef _DEBUG
    if (vk::Result Result = CreateDebugMessenger(); Result != vk::Result::eSuccess)
    {
        return Result;
    }
#endif // _DEBUG

    NpgsCoreInfo("Vulkan instance created successfully.");
    return vk::Result::eSuccess;
}

vk::Result FVulkanCore::CreateDevice(std::uint32_t PhysicalDeviceIndex, vk::DeviceCreateFlags Flags)
{
    EnumeratePhysicalDevices();
    DeterminePhysicalDevice(PhysicalDeviceIndex, true, true);

    float QueuePriority = 1.0f;
    std::vector<vk::DeviceQueueCreateInfo> DeviceQueueCreateInfos;

    if (_GraphicsQueueFamilyIndex != vk::QueueFamilyIgnored)
    {
        DeviceQueueCreateInfos.emplace_back(vk::DeviceQueueCreateFlags{}, _GraphicsQueueFamilyIndex, 1, &QueuePriority);
    }
    if (_PresentQueueFamilyIndex != vk::QueueFamilyIgnored &&
        _PresentQueueFamilyIndex != _GraphicsQueueFamilyIndex)
    {
        DeviceQueueCreateInfos.emplace_back(vk::DeviceQueueCreateFlags{}, _PresentQueueFamilyIndex, 1, &QueuePriority);
    }
    if (_ComputeQueueFamilyIndex != vk::QueueFamilyIgnored &&
        _ComputeQueueFamilyIndex != _GraphicsQueueFamilyIndex &&
        _ComputeQueueFamilyIndex != _PresentQueueFamilyIndex)
    {
        DeviceQueueCreateInfos.emplace_back(vk::DeviceQueueCreateFlags{}, _ComputeQueueFamilyIndex, 1, &QueuePriority);
    }

    vk::PhysicalDeviceFeatures2 Features2;
    vk::PhysicalDeviceVulkan11Features Features11;
    vk::PhysicalDeviceVulkan12Features Features12;
    vk::PhysicalDeviceVulkan13Features Features13;
    vk::PhysicalDeviceVulkan14Features Features14;

    Features2.setPNext(&Features11);
    Features11.setPNext(&Features12);
    Features12.setPNext(&Features13);
    Features13.setPNext(&Features14);

    _PhysicalDevice.getFeatures2(&Features2);

    void* pNext = nullptr;
    vk::PhysicalDeviceFeatures PhysicalDeviceFeatures = Features2.features;

    if (_ApiVersion >= VK_API_VERSION_1_1)
    {
        Features11.setPNext(pNext);
        pNext = &Features11;
    }
    if (_ApiVersion >= VK_API_VERSION_1_2)
    {
        Features12.setPNext(pNext);
        pNext = &Features12;
    }
    if (_ApiVersion >= VK_API_VERSION_1_3)
    {
        Features13.setPNext(pNext);
        pNext = &Features13;
    }
    if (_ApiVersion >= VK_API_VERSION_1_4)
    {
        Features14.setPNext(pNext);
        pNext = &Features14;
    }

    vk::DeviceCreateInfo DeviceCreateInfo(Flags, DeviceQueueCreateInfos, {}, _DeviceExtensions, &PhysicalDeviceFeatures, pNext);

    try
    {
        _Device = _PhysicalDevice.createDevice(DeviceCreateInfo);
    }
    catch (const vk::SystemError& e)
    {
        NpgsCoreError("Failed to create logical device: {}", e.what());
        return static_cast<vk::Result>(e.code().value());
    }

    // 由于创建的队列族指定了每个队列族只有一个队列，所以这里直接获取第一个队列
    if (_GraphicsQueueFamilyIndex != vk::QueueFamilyIgnored)
    {
        _GraphicsQueue = _Device.getQueue(_GraphicsQueueFamilyIndex, 0);
    }
    if (_PresentQueueFamilyIndex != vk::QueueFamilyIgnored)
    {
        _PresentQueue = _Device.getQueue(_PresentQueueFamilyIndex, 0);
    }
    if (_ComputeQueueFamilyIndex != vk::QueueFamilyIgnored)
    {
        _ComputeQueue = _Device.getQueue(_ComputeQueueFamilyIndex, 0);
    }

    _PhysicalDeviceProperties       = _PhysicalDevice.getProperties();
    _PhysicalDeviceMemoryProperties = _PhysicalDevice.getMemoryProperties();
    NpgsCoreInfo("Logical device created successfully.");
    NpgsCoreInfo("Renderer: {}", _PhysicalDeviceProperties.deviceName.data());

    for (auto& Callback : _CreateDeviceCallbacks)
    {
        Callback.second();
    }

    return vk::Result::eSuccess;
}

vk::Result FVulkanCore::RecreateDevice(std::uint32_t PhysicalDeviceIndex, vk::DeviceCreateFlags Flags)
{
    WaitIdle();

    if (_Swapchain)
    {
        for (auto& Callback : _DestroySwapchainCallbacks)
        {
            Callback.second();
        }
        for (auto& ImageView : _SwapchainImageViews)
        {
            if (ImageView)
            {
                _Device.destroyImageView(ImageView);
            }
        }
        _SwapchainImageViews.clear();

        _Device.destroySwapchainKHR(_Swapchain);
        _Swapchain           = vk::SwapchainKHR();
        _SwapchainCreateInfo = vk::SwapchainCreateInfoKHR();
    }

    for (auto& Callback : _DestroyDeviceCallbacks)
    {
        Callback.second();
    }
    if (_Device)
    {
        _Device.destroy();
        _Device = vk::Device();
    }

    return CreateDevice(PhysicalDeviceIndex, Flags);
}

vk::Result FVulkanCore::SetSurfaceFormat(const vk::SurfaceFormatKHR& SurfaceFormat)
{
    bool bFormatAvailable = false;
    if (SurfaceFormat.format == vk::Format::eUndefined)
    {
        for (const auto& AvailableSurfaceFormat : _AvailableSurfaceFormats)
        {
            if (AvailableSurfaceFormat.colorSpace == SurfaceFormat.colorSpace)
            {
                _SwapchainCreateInfo.setImageFormat(AvailableSurfaceFormat.format)
                                    .setImageColorSpace(AvailableSurfaceFormat.colorSpace);
                bFormatAvailable = true;
                break;
            }
        }
    }
    else
    {
        for (const auto& AvailableSurfaceFormat : _AvailableSurfaceFormats)
        {
            if (AvailableSurfaceFormat.format     == SurfaceFormat.format &&
                AvailableSurfaceFormat.colorSpace == SurfaceFormat.colorSpace)
            {
                _SwapchainCreateInfo.setImageFormat(AvailableSurfaceFormat.format)
                                    .setImageColorSpace(AvailableSurfaceFormat.colorSpace);
                bFormatAvailable = true;
                break;
            }
        }
    }

    if (!bFormatAvailable)
    {
        return vk::Result::eErrorFormatNotSupported;
    }

    if (_Swapchain)
    {
        return RecreateSwapchain();
    }

    return vk::Result::eSuccess;
}

vk::Result FVulkanCore::CreateSwapchain(vk::Extent2D Extent, bool bLimitFps, vk::SwapchainCreateFlagsKHR Flags)
{
    // Swapchain 需要的信息：
    // 1.基本 Surface 能力（Swapchain 中图像的最小/最大数量、宽度和高度）
    vk::SurfaceCapabilitiesKHR SurfaceCapabilities;
    try
    {
        SurfaceCapabilities = _PhysicalDevice.getSurfaceCapabilitiesKHR(_Surface);
    }
    catch (const vk::SystemError& e)
    {
        NpgsCoreError("Failed to get surface capabilities: {}", e.what());
        return static_cast<vk::Result>(e.code().value());
    }

    vk::Extent2D SwapchainExtent;
    if (SurfaceCapabilities.currentExtent.width == std::numeric_limits<std::uint32_t>::max())
    {
        SwapchainExtent = vk::Extent2D
        {
            // 限制 Swapchain 的大小在支持的范围内
            std::clamp(Extent.width,  SurfaceCapabilities.minImageExtent.width,  SurfaceCapabilities.maxImageExtent.width),
            std::clamp(Extent.height, SurfaceCapabilities.minImageExtent.height, SurfaceCapabilities.maxImageExtent.height)
        };
    }
    else
    {
        SwapchainExtent = SurfaceCapabilities.currentExtent;
    }
    _SwapchainExtent = SwapchainExtent;

    std::uint32_t MinImageCount =
        SurfaceCapabilities.minImageCount + (SurfaceCapabilities.maxImageCount > SurfaceCapabilities.minImageCount);
    _SwapchainCreateInfo.setFlags(Flags)
                        .setSurface(_Surface)
                        .setMinImageCount(MinImageCount)
                        .setImageExtent(SwapchainExtent)
                        .setImageArrayLayers(1)
                        .setImageSharingMode(vk::SharingMode::eExclusive)
                        .setPreTransform(SurfaceCapabilities.currentTransform)
                        .setClipped(vk::True);

    // 设置图像格式
    if (SurfaceCapabilities.supportedCompositeAlpha & vk::CompositeAlphaFlagBitsKHR::eInherit) // 优先使用继承模式
    {
        _SwapchainCreateInfo.setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eInherit);
    }
    else
    {
        // 找不到继承模式，随便挑一个凑合用的
        static const vk::CompositeAlphaFlagBitsKHR kCompositeAlphaFlags[]
        {
            vk::CompositeAlphaFlagBitsKHR::eOpaque,
            vk::CompositeAlphaFlagBitsKHR::ePreMultiplied,
            vk::CompositeAlphaFlagBitsKHR::ePostMultiplied
        };

        for (auto CompositeAlphaFlag : kCompositeAlphaFlags)
        {
            if (SurfaceCapabilities.supportedCompositeAlpha & CompositeAlphaFlag)
            {
                _SwapchainCreateInfo.setCompositeAlpha(CompositeAlphaFlag);
                break;
            }
        }
    }

    _SwapchainCreateInfo.setImageUsage(vk::ImageUsageFlagBits::eColorAttachment);
    if (SurfaceCapabilities.supportedUsageFlags & vk::ImageUsageFlagBits::eTransferSrc)
    {
        _SwapchainCreateInfo.setImageUsage(_SwapchainCreateInfo.imageUsage | vk::ImageUsageFlagBits::eTransferSrc);
    }
    if (SurfaceCapabilities.supportedUsageFlags & vk::ImageUsageFlagBits::eTransferDst)
    {
        _SwapchainCreateInfo.setImageUsage(_SwapchainCreateInfo.imageUsage | vk::ImageUsageFlagBits::eTransferDst);
    }
    else
    {
        NpgsCoreError("Failed to get supported usage flags.");
        return vk::Result::eErrorFeatureNotPresent;
    }

    // 2.设置 Swapchain 像素格式和色彩空间
    vk::Result Result;
    if (_AvailableSurfaceFormats.empty())
    {
        if ((Result = ObtainPhysicalDeviceSurfaceFormats()) != vk::Result::eSuccess)
        {
            return Result;
        }
    }

    if (_SwapchainCreateInfo.imageFormat == vk::Format::eUndefined)
    {
        if (SetSurfaceFormat({ vk::Format::eR8G8B8A8Unorm, vk::ColorSpaceKHR::eSrgbNonlinear }) != vk::Result::eSuccess &&
            SetSurfaceFormat({ vk::Format::eB8G8R8A8Unorm, vk::ColorSpaceKHR::eSrgbNonlinear }) != vk::Result::eSuccess)
        {
            _SwapchainCreateInfo.setImageFormat(_AvailableSurfaceFormats[0].format)
                                .setImageColorSpace(_AvailableSurfaceFormats[0].colorSpace);
            NpgsCoreWarn("Failed to select a four-component unsigned normalized surface format.");
        }
    }

    // 3.设置 Swapchain 呈现模式
    std::vector<vk::PresentModeKHR> SurfacePresentModes;
    try
    {
        SurfacePresentModes = _PhysicalDevice.getSurfacePresentModesKHR(_Surface);
    }
    catch (const vk::SystemError& e)
    {
        NpgsCoreError("Failed to get surface present modes: {}", e.what());
        return static_cast<vk::Result>(e.code().value());
    }

    if (bLimitFps)
    {
        _SwapchainCreateInfo.setPresentMode(vk::PresentModeKHR::eFifo);
    }
    else
    {
        for (const auto& SurfacePresentMode : SurfacePresentModes)
        {
            if (SurfacePresentMode == vk::PresentModeKHR::eMailbox)
            {
                _SwapchainCreateInfo.setPresentMode(vk::PresentModeKHR::eMailbox);
                break;
            }
        }
    }

    if ((Result = CreateSwapchainInternal()) != vk::Result::eSuccess)
    {
        return Result;
    }

    for (auto& Callback : _CreateSwapchainCallbacks)
    {
        Callback.second();
    }

    NpgsCoreInfo("Swapchain created successfully.");
    return vk::Result::eSuccess;
}

vk::Result FVulkanCore::RecreateSwapchain()
{
    vk::SurfaceCapabilitiesKHR SurfaceCapabilities;
    try
    {
        SurfaceCapabilities = _PhysicalDevice.getSurfaceCapabilitiesKHR(_Surface);
    }
    catch (const vk::SystemError& e)
    {
        NpgsCoreError("Failed to get surface capabilities: {}", e.what());
        return static_cast<vk::Result>(e.code().value());
    }

    if (SurfaceCapabilities.currentExtent.width == 0 || SurfaceCapabilities.currentExtent.height == 0)
    {
        return vk::Result::eSuboptimalKHR;
    }
    _SwapchainCreateInfo.setImageExtent(SurfaceCapabilities.currentExtent);

    if (_SwapchainCreateInfo.oldSwapchain)
    {
        _Device.destroySwapchainKHR(_SwapchainCreateInfo.oldSwapchain);
    }
    _SwapchainCreateInfo.setOldSwapchain(_Swapchain);

    try
    {
        _GraphicsQueue.waitIdle();
    }
    catch (const vk::SystemError& e)
    {
        NpgsCoreError("Failed to wait for graphics queue to be idle: {}", e.what());
        return static_cast<vk::Result>(e.code().value());
    }

    if (_GraphicsQueueFamilyIndex != _PresentQueueFamilyIndex)
    {
        try
        {
            _PresentQueue.waitIdle();
        }
        catch (const vk::SystemError& e)
        {
            NpgsCoreError("Failed to wait for present queue to be idle: {}", e.what());
            return static_cast<vk::Result>(e.code().value());
        }
    }

    for (auto& Callback : _DestroySwapchainCallbacks)
    {
        Callback.second();
    }
    for (auto& ImageView : _SwapchainImageViews)
    {
        if (ImageView)
        {
            _Device.destroyImageView(ImageView);
        }
    }
    _SwapchainImageViews.clear();

    if (vk::Result Result = CreateSwapchainInternal(); Result != vk::Result::eSuccess)
    {
        return Result;
    }

    for (auto& Callback : _CreateSwapchainCallbacks)
    {
        Callback.second();
    }

    NpgsCoreInfo("Swapchain recreated successfully.");
    return vk::Result::eSuccess;
}

vk::Result FVulkanCore::SwapImage(vk::Semaphore Semaphore)
{
    if (_SwapchainCreateInfo.oldSwapchain && _SwapchainCreateInfo.oldSwapchain != _Swapchain) [[unlikely]]
    {
        _Device.destroySwapchainKHR(_SwapchainCreateInfo.oldSwapchain);
        _SwapchainCreateInfo.setOldSwapchain(vk::SwapchainKHR());
    }

    vk::Result Result;
    while ((Result = _Device.acquireNextImageKHR(_Swapchain, std::numeric_limits<std::uint64_t>::max(),
                                                 Semaphore, vk::Fence(), &_CurrentImageIndex)) != vk::Result::eSuccess)
    {
        switch (Result)
        {
        case vk::Result::eSuboptimalKHR:
        case vk::Result::eErrorOutOfDateKHR:
            if ((Result = RecreateSwapchain()) != vk::Result::eSuccess)
            {
                return Result;
            }
            break;
        default:
            NpgsCoreError("Failed to acquire next image: {}.", vk::to_string(Result));
            return Result;
        }
    }

    return vk::Result::eSuccess;
}

vk::Result FVulkanCore::PresentImage(const vk::PresentInfoKHR& PresentInfo)
{
    try
    {
        vk::Result Result = _PresentQueue.presentKHR(PresentInfo);
        switch (Result)
        {
        case vk::Result::eSuccess:
            return Result;
        case vk::Result::eSuboptimalKHR:
            return RecreateSwapchain();
        default:
            NpgsCoreError("Failed to present image: {}.", vk::to_string(Result));
            return Result;
        }
    }
    catch (const vk::SystemError& e)
    {
        vk::Result ErrorResult = static_cast<vk::Result>(e.code().value());
        switch (ErrorResult)
        {
        case vk::Result::eErrorOutOfDateKHR:
            return RecreateSwapchain();
        default:
            NpgsCoreError("Failed to present image: {}.", e.what());
            return ErrorResult;
        }
    }
}

vk::Result FVulkanCore::PresentImage(vk::Semaphore Semaphore)
{
    vk::PresentInfoKHR PresentInfo;
    if (Semaphore)
    {
        PresentInfo = vk::PresentInfoKHR(Semaphore, _Swapchain, _CurrentImageIndex);
    }
    else
    {
        PresentInfo = vk::PresentInfoKHR({}, _Swapchain, _CurrentImageIndex);
    }
    return PresentImage(PresentInfo);
}

vk::Result FVulkanCore::WaitIdle() const
{
    try
    {
        _Device.waitIdle();
    }
    catch (const vk::SystemError& e)
    {
        NpgsCoreError("Failed to wait for device to be idle: {}", e.what());
        return static_cast<vk::Result>(e.code().value());
    }

    return vk::Result::eSuccess;
}

FVulkanCore* FVulkanCore::GetClassInstance()
{
    static FVulkanCore Instance;
    return &Instance;
}

void FVulkanCore::AddElementChecked(const char* Element, std::vector<const char*>& Vector)
{
    auto it = std::find_if(Vector.begin(), Vector.end(), [&Element](const char* ElementInVector)
    {
        return Util::Equal(Element, ElementInVector);
    });

    if (it == Vector.end())
    {
        Vector.push_back(Element);
    }
}

vk::Result FVulkanCore::UseLatestApiVersion()
{
    if (reinterpret_cast<PFN_vkEnumerateInstanceVersion>(vkGetInstanceProcAddr(nullptr, "vkEnumerateInstanceVersion")))
    {
        try
        {
            _ApiVersion = vk::enumerateInstanceVersion();
        }
        catch (const vk::SystemError& e)
        {
            NpgsCoreError("Failed to enumerate instance version: {}", e.what());
            return static_cast<vk::Result>(e.code().value());
        }
    }
    else
    {
        _ApiVersion = VK_API_VERSION_1_0;
        NpgsCoreInfo("Vulkan 1.1+ not available, using Vulkan 1.0.");
    }

    NpgsCoreInfo("Vulkan API version: {}.{}.{}", VK_VERSION_MAJOR(_ApiVersion),
                 VK_VERSION_MINOR(_ApiVersion), VK_VERSION_PATCH(_ApiVersion));
    return vk::Result::eSuccess;
}

vk::Result FVulkanCore::CreateDebugMessenger()
{
    kVkCreateDebugUtilsMessengerExt =
        reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(_Instance.getProcAddr("vkCreateDebugUtilsMessengerEXT"));
    if (kVkCreateDebugUtilsMessengerExt == nullptr)
    {
        NpgsCoreError("Failed to get vkCreateDebugUtilsMessengerEXT function pointer.");
        return vk::Result::eErrorExtensionNotPresent;
    }

    kVkDestroyDebugUtilsMessengerExt =
        reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(_Instance.getProcAddr("vkDestroyDebugUtilsMessengerEXT"));
    if (vkDestroyDebugUtilsMessengerEXT == nullptr)
    {
        NpgsCoreError("Failed to get vkDestroyDebugUtilsMessengerEXT function pointer.");
        return vk::Result::eErrorExtensionNotPresent;
    }

    vk::PFN_DebugUtilsMessengerCallbackEXT DebugCallback =
    [](vk::DebugUtilsMessageSeverityFlagBitsEXT      MessageSeverity,
       vk::DebugUtilsMessageTypeFlagsEXT             MessageType,
       const vk::DebugUtilsMessengerCallbackDataEXT* CallbackData,
       void*                                         UserData) -> vk::Bool32
    {
        std::string Severity;
        if (MessageSeverity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose) Severity = "VERBOSE";
        if (MessageSeverity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo)    Severity = "INFO";
        if (MessageSeverity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning) Severity = "WARNING";
        if (MessageSeverity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eError)   Severity = "ERROR";

        if (Severity == "VERBOSE")
            NpgsCoreTrace("Validation layer: {}", CallbackData->pMessage);
        else if (Severity == "INFO")
            NpgsCoreInfo("Validation layer: {}", CallbackData->pMessage);
        else if (Severity == "ERROR")
            NpgsCoreError("Validation layer: {}", CallbackData->pMessage);
        else if (Severity == "WARNING")
            NpgsCoreWarn("Validation layer: {}", CallbackData->pMessage);

        // if (CallbackData->queueLabelCount > 0)
        // 	NpgsCoreTrace("Queue Labels: {}", CallbackData->queueLabelCount);
        // if (CallbackData->cmdBufLabelCount > 0)
        // 	NpgsCoreTrace("Command Buffer Labels: {}", CallbackData->cmdBufLabelCount);
        // if (CallbackData->objectCount > 0)
        // 	NpgsCoreTrace("Objects: {}", CallbackData->objectCount);

        return vk::False;
    };

    auto MessageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
                           vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo    |
                           vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                           vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
    auto MessageType     = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral     |
                           vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation  |
                           vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;

    vk::DebugUtilsMessengerCreateInfoEXT DebugUtilsMessengerCreateInfo({}, MessageSeverity, MessageType, DebugCallback);
    try
    {
        _DebugUtilsMessenger = _Instance.createDebugUtilsMessengerEXT(DebugUtilsMessengerCreateInfo);
    }
    catch (const vk::SystemError& e)
    {
        NpgsCoreError("Failed to create debug messenger: {}", e.what());
        return static_cast<vk::Result>(e.code().value());
    }

    NpgsCoreInfo("Debug messenger created successfully.");
    return vk::Result::eSuccess;
}

vk::Result FVulkanCore::EnumeratePhysicalDevices()
{
    try
    {
        _AvailablePhysicalDevices = _Instance.enumeratePhysicalDevices();
    }
    catch (const vk::SystemError& e)
    {
        NpgsCoreError("Failed to enumerate physical devices: {}", e.what());
        return static_cast<vk::Result>(e.code().value());
    }

    NpgsCoreInfo("Enumerate physical devices successfully, {} devices found.", _AvailablePhysicalDevices.size());
    return vk::Result::eSuccess;
}

vk::Result FVulkanCore::DeterminePhysicalDevice(std::uint32_t Index, bool bEnableGraphicsQueue, bool bEnableComputeQueue)
{
    // kNotFound 在与其进行与运算最高位是 0 的数结果还是数本身
    // 但是对于 -1U，结果是 kNotFound
    static constexpr std::uint32_t kNotFound = static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max());
    std::vector<FQueueFamilyIndicesComplex> QueueFamilyIndeicesComplexes(_AvailablePhysicalDevices.size());
    auto [GraphicsQueueFamilyIndex, PresentQueueFamilyIndex, ComputeQueueFamilyIndex] = QueueFamilyIndeicesComplexes[Index];

    // 如果任何索引已经搜索过但还是找不到，直接报错
    if ((GraphicsQueueFamilyIndex == kNotFound && bEnableGraphicsQueue) ||
        (PresentQueueFamilyIndex  == kNotFound && _Surface) ||
        (ComputeQueueFamilyIndex  == kNotFound && bEnableGraphicsQueue))
    {
        return vk::Result::eErrorFeatureNotPresent;
    }

    if ((GraphicsQueueFamilyIndex == vk::QueueFamilyIgnored && bEnableGraphicsQueue) ||
        (PresentQueueFamilyIndex  == vk::QueueFamilyIgnored && _Surface) ||
        (ComputeQueueFamilyIndex  == vk::QueueFamilyIgnored && bEnableGraphicsQueue))
    {
        FQueueFamilyIndicesComplex Indices;
        if (vk::Result Result = ObtainQueueFamilyIndices(_AvailablePhysicalDevices[Index], bEnableGraphicsQueue, bEnableComputeQueue, Indices);
            Result != vk::Result::eSuccess)
        {
            return Result;
        }

        // 如果结果是 vk::QueueFamilyIgnored，那么和 kNotFound 做与运算结果还是 kNotFound
        if (bEnableGraphicsQueue)
        {
            GraphicsQueueFamilyIndex = Indices.GraphicsQueueFamilyIndex & kNotFound;
        }
        if (_Surface)
        {
            PresentQueueFamilyIndex = Indices.PresentQueueFamilyIndex & kNotFound;
        }
        if (bEnableComputeQueue)
        {
            ComputeQueueFamilyIndex = Indices.ComputeQueueFamilyIndex & kNotFound;
        }

        _GraphicsQueueFamilyIndex = bEnableGraphicsQueue ? GraphicsQueueFamilyIndex : vk::QueueFamilyIgnored;
        _PresentQueueFamilyIndex  = _Surface             ? PresentQueueFamilyIndex  : vk::QueueFamilyIgnored;
        _ComputeQueueFamilyIndex  = bEnableComputeQueue  ? ComputeQueueFamilyIndex  : vk::QueueFamilyIgnored;
    }
    else // 如果已经找到了，直接设置索引
    {
        _GraphicsQueueFamilyIndex = bEnableGraphicsQueue ? GraphicsQueueFamilyIndex : vk::QueueFamilyIgnored;
        _PresentQueueFamilyIndex  = _Surface             ? PresentQueueFamilyIndex  : vk::QueueFamilyIgnored;
        _ComputeQueueFamilyIndex  = bEnableComputeQueue  ? ComputeQueueFamilyIndex  : vk::QueueFamilyIgnored;
    }

    _PhysicalDevice = _AvailablePhysicalDevices[Index];
    return vk::Result::eSuccess;
}

vk::Result FVulkanCore::ObtainQueueFamilyIndices(vk::PhysicalDevice PhysicalDevice, bool bEnableGraphicsQueue,
                                                 bool bEnableComputeQueue, FQueueFamilyIndicesComplex& Indices)
{
    std::vector<vk::QueueFamilyProperties> QueueFamilyProperties = PhysicalDevice.getQueueFamilyProperties();
    if (QueueFamilyProperties.empty())
    {
        NpgsCoreError("Failed to get queue family properties.");
        return vk::Result::eErrorInitializationFailed;
    }

    auto& [GraphicsQueueFamilyIndex, PresentQueueFamilyIndex, ComputeQueueFamilyIndex] = Indices;
    GraphicsQueueFamilyIndex = PresentQueueFamilyIndex = ComputeQueueFamilyIndex = vk::QueueFamilyIgnored;

    // 队列族的位置是和队列族本身的索引是对应的
    for (std::uint32_t i = 0; i != QueueFamilyProperties.size(); ++i)
    {
        vk::Bool32 bSupportGraphics = bEnableGraphicsQueue && QueueFamilyProperties[i].queueFlags & vk::QueueFlagBits::eGraphics;
        vk::Bool32 bSupportPresent  = vk::False;
        vk::Bool32 bSupportCompute  = bEnableComputeQueue  && QueueFamilyProperties[i].queueFlags & vk::QueueFlagBits::eCompute;

        if (_Surface)
        {
            try
            {
                bSupportPresent = PhysicalDevice.getSurfaceSupportKHR(i, _Surface);
            }
            catch (const vk::SystemError& e)
            {
                NpgsCoreError("Failed to determine if the queue family supports presentation: {}", e.what());
                return static_cast<vk::Result>(e.code().value());
            }
        }

        if (bSupportGraphics && bSupportCompute) // 如果同时支持图形和计算队列
        {
            if (bSupportPresent) // 三个队列族都支持，将它们的索引设置为相同的值
            {
                GraphicsQueueFamilyIndex = PresentQueueFamilyIndex = ComputeQueueFamilyIndex = i;
                break;
            }
            if (GraphicsQueueFamilyIndex != ComputeQueueFamilyIndex || GraphicsQueueFamilyIndex == vk::QueueFamilyIgnored)
            {
                GraphicsQueueFamilyIndex = ComputeQueueFamilyIndex = i; // 确保图形和计算队列族的索引相同
            }
            if (!_Surface)
            {
                break; // 如果不需要呈现，那么只需要找到一个支持图形和计算队列族的索引即可
            }
        }

        // 将支持对应功能的队列族的索引设置为找到的第一个值
        if (bSupportGraphics && GraphicsQueueFamilyIndex == vk::QueueFamilyIgnored)
        {
            GraphicsQueueFamilyIndex = i;
        }
        if (bSupportPresent && PresentQueueFamilyIndex == vk::QueueFamilyIgnored)
        {
            PresentQueueFamilyIndex = i;
        }
        if (bSupportCompute && ComputeQueueFamilyIndex == vk::QueueFamilyIgnored)
        {
            ComputeQueueFamilyIndex = i;
        }
    }

    // 如果有任意一个需要启用的队列族的索引是 vk::QueueFamilyIgnored，报错
    if ((GraphicsQueueFamilyIndex == vk::QueueFamilyIgnored && bEnableGraphicsQueue) ||
        (PresentQueueFamilyIndex  == vk::QueueFamilyIgnored && _Surface) ||
        (ComputeQueueFamilyIndex  == vk::QueueFamilyIgnored && bEnableComputeQueue))
    {
        NpgsCoreError("Failed to obtain queue family indices.");
        return vk::Result::eErrorFeatureNotPresent;
    }

    NpgsCoreInfo("Queue family indices obtained successfully.");
    return vk::Result::eSuccess;
}

vk::Result FVulkanCore::ObtainPhysicalDeviceSurfaceFormats()
{
    try
    {
        _AvailableSurfaceFormats = _PhysicalDevice.getSurfaceFormatsKHR(_Surface);
    }
    catch (const vk::SystemError& e)
    {
        NpgsCoreError("Failed to get surface formats: {}", e.what());
        return static_cast<vk::Result>(e.code().value());
    }

    NpgsCoreInfo("Surface formats obtained successfully, {} formats found.", _AvailableSurfaceFormats.size());
    return vk::Result::eSuccess;
}

vk::Result FVulkanCore::CreateSwapchainInternal()
{
    try
    {
        _Swapchain = _Device.createSwapchainKHR(_SwapchainCreateInfo);
    }
    catch (const vk::SystemError& e)
    {
        NpgsCoreError("Failed to create swapchain: {}", e.what());
        return static_cast<vk::Result>(e.code().value());
    }

    try
    {
        _SwapchainImages = _Device.getSwapchainImagesKHR(_Swapchain);
    }
    catch (const vk::SystemError& e)
    {
        NpgsCoreError("Failed to get swapchain images: {}", e.what());
        return static_cast<vk::Result>(e.code().value());
    }

    vk::ImageSubresourceRange ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);
    vk::ImageViewCreateInfo ImageViewCreateInfo = vk::ImageViewCreateInfo()
        .setViewType(vk::ImageViewType::e2D)
        .setFormat(_SwapchainCreateInfo.imageFormat)
        .setSubresourceRange(ImageSubresourceRange);

    for (const auto& SwapchainImages : _SwapchainImages)
    {
        ImageViewCreateInfo.setImage(SwapchainImages);
        vk::ImageView ImageView;
        try
        {
            ImageView = _Device.createImageView(ImageViewCreateInfo);
        }
        catch (const vk::SystemError& e)
        {
            NpgsCoreError("Failed to create image view: {}", e.what());
            return static_cast<vk::Result>(e.code().value());
        }

        _SwapchainImageViews.push_back(ImageView);
    }

    return vk::Result::eSuccess;
}

_GRAPHICS_END
_RUNTIME_END
_NPGS_END
