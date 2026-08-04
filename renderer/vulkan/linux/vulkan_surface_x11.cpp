#include "renderer/vulkan/vulkan_surface.h"

#include <X11/Xlib.h>

#include <stdexcept>

namespace lotui {

VulkanPlatformSurfaceInfo vulkanPlatformSurfaceInfo() {
    return {{
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_XLIB_SURFACE_EXTENSION_NAME,
    }};
}

VkSurfaceKHR createVulkanPlatformSurface(
    VkInstance instance,
    const NativeWindowHandle& window) {
    if (window.system != NativeWindowSystem::X11 ||
        window.display == nullptr || window.window == 0) {
        throw std::runtime_error("invalid X11 native window handle");
    }

    VkXlibSurfaceCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
    createInfo.dpy = static_cast<Display*>(window.display);
    createInfo.window = static_cast<::Window>(window.window);

    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (vkCreateXlibSurfaceKHR(instance, &createInfo, nullptr, &surface) !=
        VK_SUCCESS) {
        throw std::runtime_error("failed to create the X11 Vulkan surface");
    }
    return surface;
}

} // namespace lotui
