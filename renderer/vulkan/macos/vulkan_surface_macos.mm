#import <QuartzCore/CAMetalLayer.h>

#include "renderer/vulkan/vulkan_surface.h"

#include <stdexcept>

namespace lotui {

VulkanPlatformSurfaceInfo vulkanPlatformSurfaceInfo() {
    VulkanPlatformSurfaceInfo info{{
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_EXT_METAL_SURFACE_EXTENSION_NAME,
        VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME,
    }};
    info.instanceFlags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    return info;
}

VkSurfaceKHR createVulkanPlatformSurface(
    VkInstance instance,
    const NativeWindowHandle& window) {
    if (window.system != NativeWindowSystem::MetalLayer || window.window == 0) {
        throw std::runtime_error("invalid macOS CAMetalLayer handle");
    }

    VkMetalSurfaceCreateInfoEXT createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT;
    createInfo.pLayer =
        (__bridge CAMetalLayer*)reinterpret_cast<void*>(window.window);

    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (vkCreateMetalSurfaceEXT(instance, &createInfo, nullptr, &surface) !=
        VK_SUCCESS) {
        throw std::runtime_error("failed to create the macOS Vulkan surface");
    }
    return surface;
}

} // namespace lotui
