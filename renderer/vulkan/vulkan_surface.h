#pragma once

#include "platform/platform_backend.h"

#include <vulkan/vulkan.h>

#include <vector>

namespace lotui {

struct VulkanPlatformSurfaceInfo {
    std::vector<const char*> instanceExtensions;
    VkInstanceCreateFlags instanceFlags{0};
};

VulkanPlatformSurfaceInfo vulkanPlatformSurfaceInfo();
VkSurfaceKHR createVulkanPlatformSurface(
    VkInstance instance,
    const NativeWindowHandle& window);

} // namespace lotui
