#include "ExtFunctionsImpl.h"

PFN_vkCreateDebugUtilsMessengerEXT  kVkCreateDebugUtilsMessengerExt  = nullptr;
PFN_vkDestroyDebugUtilsMessengerEXT kVkDestroyDebugUtilsMessengerExt = nullptr;

VKAPI_ATTR VkResult VKAPI_CALL
vkCreateDebugUtilsMessengerEXT(VkInstance                                Instance,
                               const VkDebugUtilsMessengerCreateInfoEXT* CreateInfo,
                               const VkAllocationCallbacks*              Allocator,
                               VkDebugUtilsMessengerEXT*                 Messenger)
{
    return kVkCreateDebugUtilsMessengerExt(Instance, CreateInfo, Allocator, Messenger);
}

VKAPI_ATTR void VKAPI_CALL
vkDestroyDebugUtilsMessengerEXT(VkInstance                   Instance,
                                VkDebugUtilsMessengerEXT     Messenger,
                                const VkAllocationCallbacks* Allocator)
{
    kVkDestroyDebugUtilsMessengerExt(Instance, Messenger, Allocator);
}
