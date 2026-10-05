#pragma once

#include "core/paint_command.h"

#include <vulkan/vulkan.h>

#include <cstdint>
#include <memory>
#include <vector>

namespace lotui {

struct VulkanEmbeddedFrame {
    VkPhysicalDevice physicalDevice{VK_NULL_HANDLE};
    VkDevice device{VK_NULL_HANDLE};
    VkRenderPass renderPass{VK_NULL_HANDLE};
    VkCommandBuffer commandBuffer{VK_NULL_HANDLE};
    VkSampleCountFlagBits samples{VK_SAMPLE_COUNT_1_BIT};
    std::uint32_t frameIndex{0};
    std::uint32_t frameCount{0};
    std::uint32_t framebufferWidth{0};
    std::uint32_t framebufferHeight{0};
    float dpiScale{1.0F};
};

// The caller owns every Vulkan handle and the active render pass. Destroy this
// renderer before destroying the caller's device.
class VulkanEmbeddedRenderer {
public:
    VulkanEmbeddedRenderer();
    ~VulkanEmbeddedRenderer();

    VulkanEmbeddedRenderer(const VulkanEmbeddedRenderer&) = delete;
    VulkanEmbeddedRenderer& operator=(const VulkanEmbeddedRenderer&) = delete;

    void draw(const VulkanEmbeddedFrame& frame,
        const std::vector<PaintCommand>& commands);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace lotui
