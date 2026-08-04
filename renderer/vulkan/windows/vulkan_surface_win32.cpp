#include "renderer/vulkan/vulkan_surface.h"

#include <windows.h>

#include <stdexcept>

namespace lotui {

VulkanPlatformSurfaceInfo vulkanPlatformSurfaceInfo() {
    return {{
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
    }};
}

VkSurfaceKHR createVulkanPlatformSurface(
    VkInstance instance,
    const NativeWindowHandle& window) {
    if (window.system != NativeWindowSystem::Win32 || window.window == 0) {
        throw std::runtime_error("invalid Win32 native window handle");
    }

    VkWin32SurfaceCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    createInfo.hinstance = static_cast<HINSTANCE>(window.display);
    createInfo.hwnd = reinterpret_cast<HWND>(window.window);

    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (vkCreateWin32SurfaceKHR(instance, &createInfo, nullptr, &surface) !=
        VK_SUCCESS) {
        throw std::runtime_error("failed to create the Win32 Vulkan surface");
    }
    return surface;
}

} // namespace lotui
