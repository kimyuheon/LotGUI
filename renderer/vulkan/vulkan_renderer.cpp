#include "renderer/vulkan/vulkan_renderer.h"

#include "platform/platform_backend.h"
#include "renderer/paint_batch.h"
#include "renderer/texture_private.h"
#include "renderer/vulkan/vulkan_surface.h"

#include "lotui/rectangle_fragment_spirv.h"
#include "lotui/rectangle_vertex_spirv.h"

#include <vulkan/vulkan.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace lotui {
namespace {

constexpr std::size_t kFramesInFlight = 2;
constexpr const char* kPortabilitySubsetExtension =
    "VK_KHR_portability_subset";

struct RectangleInstance {
    float bounds[4];
    float color[4];
    float parameters[4];
    float textureCoordinates[4];
};

struct ViewportPushConstants {
    float width;
    float height;
};

static_assert(sizeof(RectangleInstance) == sizeof(float) * 16);

struct QueueFamilies {
    std::optional<std::uint32_t> graphics;
    std::optional<std::uint32_t> present;

    bool complete() const noexcept {
        return graphics.has_value() && present.has_value();
    }
};

struct SwapchainSupport {
    VkSurfaceCapabilitiesKHR capabilities{};
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

bool hasName(
    const std::vector<VkExtensionProperties>& properties,
    const char* name) {
    return std::any_of(
        properties.begin(), properties.end(),
        [name](const VkExtensionProperties& property) {
            return std::strcmp(property.extensionName, name) == 0;
        });
}

std::vector<VkExtensionProperties> instanceExtensions() {
    std::uint32_t count = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> properties(count);
    vkEnumerateInstanceExtensionProperties(
        nullptr, &count, properties.data());
    return properties;
}

std::vector<VkExtensionProperties> deviceExtensions(
    VkPhysicalDevice physicalDevice) {
    std::uint32_t count = 0;
    vkEnumerateDeviceExtensionProperties(
        physicalDevice, nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> properties(count);
    vkEnumerateDeviceExtensionProperties(
        physicalDevice, nullptr, &count, properties.data());
    return properties;
}

QueueFamilies findQueueFamilies(
    VkPhysicalDevice physicalDevice,
    VkSurfaceKHR surface) {
    std::uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &count, nullptr);
    std::vector<VkQueueFamilyProperties> properties(count);
    vkGetPhysicalDeviceQueueFamilyProperties(
        physicalDevice, &count, properties.data());

    QueueFamilies result;
    for (std::uint32_t index = 0; index < count; ++index) {
        if ((properties[index].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) {
            result.graphics = index;
        }

        VkBool32 supportsPresent = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(
            physicalDevice, index, surface, &supportsPresent);
        if (supportsPresent == VK_TRUE) {
            result.present = index;
        }

        if (result.complete()) {
            break;
        }
    }
    return result;
}

SwapchainSupport querySwapchainSupport(
    VkPhysicalDevice physicalDevice,
    VkSurfaceKHR surface) {
    SwapchainSupport support;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        physicalDevice, surface, &support.capabilities);

    std::uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(
        physicalDevice, surface, &formatCount, nullptr);
    support.formats.resize(formatCount);
    if (formatCount > 0) {
        vkGetPhysicalDeviceSurfaceFormatsKHR(
            physicalDevice,
            surface,
            &formatCount,
            support.formats.data());
    }

    std::uint32_t presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(
        physicalDevice, surface, &presentModeCount, nullptr);
    support.presentModes.resize(presentModeCount);
    if (presentModeCount > 0) {
        vkGetPhysicalDeviceSurfacePresentModesKHR(
            physicalDevice,
            surface,
            &presentModeCount,
            support.presentModes.data());
    }
    return support;
}

VkSurfaceFormatKHR chooseSurfaceFormat(
    const std::vector<VkSurfaceFormatKHR>& formats) {
    if (formats.size() == 1 && formats.front().format == VK_FORMAT_UNDEFINED) {
        return {
            VK_FORMAT_B8G8R8A8_SRGB,
            VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
        };
    }

    for (const auto& format : formats) {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB &&
            format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }
    return formats.front();
}

VkCompositeAlphaFlagBitsKHR chooseCompositeAlpha(
    VkCompositeAlphaFlagsKHR supported) {
    constexpr std::array<VkCompositeAlphaFlagBitsKHR, 4> candidates{
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
    };
    for (const auto candidate : candidates) {
        if ((supported & candidate) != 0) {
            return candidate;
        }
    }
    throw std::runtime_error("surface reports no composite alpha mode");
}

} // namespace

class VulkanRenderer::Impl {
public:
    explicit Impl(PlatformWindow& window) : window_(window) {
        try {
            createInstance();
            surface_ = createVulkanPlatformSurface(
                instance_, window_.nativeHandle());
            pickPhysicalDevice();
            createDevice();
            createCommandPool();
            createTextureResources();
            createSyncObjects();
            createSwapchainResources();
        } catch (...) {
            cleanup();
            throw;
        }
    }

    ~Impl() {
        cleanup();
    }

    void drawFrame(const std::vector<PaintCommand>& paintCommands);
    TextureId createTexture(const TextureImage& image);
    void updateTexture(TextureId id, const TextureUpdate& update);
    std::shared_ptr<detail::TextureReleaseState> textureReleaseState() const {
        return textureReleaseState_;
    }

private:
    struct FrameSync {
        VkSemaphore imageAvailable{VK_NULL_HANDLE};
        VkSemaphore renderFinished{VK_NULL_HANDLE};
        VkFence inFlight{VK_NULL_HANDLE};
        VkCommandBuffer commandBuffer{VK_NULL_HANDLE};
        VkBuffer instanceBuffer{VK_NULL_HANDLE};
        VkDeviceMemory instanceMemory{VK_NULL_HANDLE};
        std::size_t instanceCapacity{0};
    };

    struct TextureRecord {
        std::uint32_t width{0};
        std::uint32_t height{0};
        TextureFormat format{TextureFormat::R8Unorm};
        VkImage image{VK_NULL_HANDLE};
        VkDeviceMemory memory{VK_NULL_HANDLE};
        VkImageView view{VK_NULL_HANDLE};
        VkDescriptorSet descriptorSet{VK_NULL_HANDLE};
    };

    void createInstance();
    void pickPhysicalDevice();
    bool isPhysicalDeviceSuitable(VkPhysicalDevice physicalDevice) const;
    void createDevice();
    void createCommandPool();
    void createTextureResources();
    void createSampler();
    void createTextureDescriptorResources();
    TextureRecord createTextureRecord(const TextureImage& image);
    void destroyTexture(TextureId id) noexcept;
    void destroyTextureRecord(TextureRecord& texture) noexcept;
    void uploadTextureRegion(
        TextureRecord& texture,
        const TextureUpdate& update,
        VkImageLayout oldLayout);
    VkCommandBuffer beginOneTimeCommands();
    void endOneTimeCommands(VkCommandBuffer commandBuffer);
    void transitionImageLayout(
        VkCommandBuffer commandBuffer,
        VkImage image,
        VkImageLayout oldLayout,
        VkImageLayout newLayout);
    void createBuffer(
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags properties,
        VkBuffer& buffer,
        VkDeviceMemory& memory) const;
    void createSyncObjects();
    void createSwapchainResources();
    void createSwapchain();
    void createImageViews();
    void createRenderPass();
    void createGraphicsPipeline();
    void createFramebuffers();
    VkShaderModule createShaderModule(
        const std::uint32_t* code,
        std::size_t byteCount) const;
    std::uint32_t findMemoryType(
        std::uint32_t typeFilter,
        VkMemoryPropertyFlags properties) const;
    void ensureInstanceCapacity(FrameSync& frame, std::size_t instanceCount);
    PaintBatchPlan uploadInstances(
        FrameSync& frame,
        const std::vector<PaintCommand>& paintCommands);
    void recordCommandBuffer(
        VkCommandBuffer commandBuffer,
        std::uint32_t imageIndex,
        const FrameSync& frame,
        const std::vector<PaintBatch>& batches) const;
    bool recreateSwapchain();
    void destroySwapchainResources();
    void cleanup();
    VkExtent2D desiredExtent(
        const VkSurfaceCapabilitiesKHR& capabilities) const;
    bool framebufferSizeChanged() const;
    VkDescriptorSet descriptorSetFor(TextureId texture) const;

    PlatformWindow& window_;
    VkInstance instance_{VK_NULL_HANDLE};
    VkSurfaceKHR surface_{VK_NULL_HANDLE};
    VkPhysicalDevice physicalDevice_{VK_NULL_HANDLE};
    VkDevice device_{VK_NULL_HANDLE};
    std::uint32_t graphicsQueueFamily_{0};
    std::uint32_t presentQueueFamily_{0};
    VkQueue graphicsQueue_{VK_NULL_HANDLE};
    VkQueue presentQueue_{VK_NULL_HANDLE};
    VkCommandPool commandPool_{VK_NULL_HANDLE};
    VkSampler textureSampler_{VK_NULL_HANDLE};
    VkDescriptorSetLayout textureDescriptorSetLayout_{VK_NULL_HANDLE};
    VkDescriptorPool textureDescriptorPool_{VK_NULL_HANDLE};
    TextureRecord whiteTexture_{};
    std::unordered_map<TextureId, TextureRecord> textures_;
    TextureId nextTextureId_{1};
    std::shared_ptr<detail::TextureReleaseState> textureReleaseState_;

    VkSwapchainKHR swapchain_{VK_NULL_HANDLE};
    VkFormat swapchainFormat_{VK_FORMAT_UNDEFINED};
    VkExtent2D swapchainExtent_{};
    std::vector<VkImage> swapchainImages_;
    std::vector<VkImageView> imageViews_;
    VkRenderPass renderPass_{VK_NULL_HANDLE};
    VkPipelineLayout pipelineLayout_{VK_NULL_HANDLE};
    VkPipeline graphicsPipeline_{VK_NULL_HANDLE};
    std::vector<VkFramebuffer> framebuffers_;
    std::vector<VkFence> imageFences_;

    std::array<FrameSync, kFramesInFlight> frames_{};
    std::size_t currentFrame_{0};
};

void VulkanRenderer::Impl::createInstance() {
    VkApplicationInfo applicationInfo{};
    applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    applicationInfo.pApplicationName = "LotUI Platform Probe";
    applicationInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    applicationInfo.pEngineName = "LotUI";
    applicationInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    applicationInfo.apiVersion = VK_API_VERSION_1_1;

    const auto platformInfo = vulkanPlatformSurfaceInfo();
    const auto availableExtensions = instanceExtensions();
    for (const char* required : platformInfo.instanceExtensions) {
        if (!hasName(availableExtensions, required)) {
            throw std::runtime_error(
                std::string("required Vulkan instance extension is missing: ") +
                required);
        }
    }

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.flags = platformInfo.instanceFlags;
    createInfo.pApplicationInfo = &applicationInfo;
    createInfo.enabledExtensionCount = static_cast<std::uint32_t>(
        platformInfo.instanceExtensions.size());
    createInfo.ppEnabledExtensionNames =
        platformInfo.instanceExtensions.data();

    if (vkCreateInstance(&createInfo, nullptr, &instance_) != VK_SUCCESS) {
        throw std::runtime_error("failed to create the Vulkan instance");
    }
}

bool VulkanRenderer::Impl::isPhysicalDeviceSuitable(
    VkPhysicalDevice physicalDevice) const {
    const QueueFamilies queues = findQueueFamilies(physicalDevice, surface_);
    if (!queues.complete()) {
        return false;
    }

    const auto extensions = deviceExtensions(physicalDevice);
    if (!hasName(extensions, VK_KHR_SWAPCHAIN_EXTENSION_NAME)) {
        return false;
    }

    const auto swapchain = querySwapchainSupport(physicalDevice, surface_);
    return !swapchain.formats.empty() && !swapchain.presentModes.empty();
}

void VulkanRenderer::Impl::pickPhysicalDevice() {
    std::uint32_t count = 0;
    vkEnumeratePhysicalDevices(instance_, &count, nullptr);
    if (count == 0) {
        throw std::runtime_error("no Vulkan physical device was found");
    }

    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(instance_, &count, devices.data());
    for (const auto device : devices) {
        if (isPhysicalDeviceSuitable(device)) {
            physicalDevice_ = device;
            break;
        }
    }
    if (physicalDevice_ == VK_NULL_HANDLE) {
        throw std::runtime_error(
            "no Vulkan device supports graphics, presentation, and swapchain");
    }

    const QueueFamilies queues = findQueueFamilies(physicalDevice_, surface_);
    graphicsQueueFamily_ = queues.graphics.value();
    presentQueueFamily_ = queues.present.value();

    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(physicalDevice_, &properties);
    std::cout << "vulkan-device: " << properties.deviceName << '\n';
}

void VulkanRenderer::Impl::createDevice() {
    const std::set<std::uint32_t> uniqueQueueFamilies{
        graphicsQueueFamily_, presentQueueFamily_};
    const float priority = 1.0F;
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    queueCreateInfos.reserve(uniqueQueueFamilies.size());
    for (const auto family : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueInfo{};
        queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfo.queueFamilyIndex = family;
        queueInfo.queueCount = 1;
        queueInfo.pQueuePriorities = &priority;
        queueCreateInfos.push_back(queueInfo);
    }

    std::vector<const char*> requiredExtensions{
        VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    const auto availableExtensions = deviceExtensions(physicalDevice_);
    if (hasName(availableExtensions, kPortabilitySubsetExtension)) {
        requiredExtensions.push_back(kPortabilitySubsetExtension);
    }

    VkPhysicalDeviceFeatures features{};
    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<std::uint32_t>(
        queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.enabledExtensionCount = static_cast<std::uint32_t>(
        requiredExtensions.size());
    createInfo.ppEnabledExtensionNames = requiredExtensions.data();
    createInfo.pEnabledFeatures = &features;

    if (vkCreateDevice(physicalDevice_, &createInfo, nullptr, &device_) !=
        VK_SUCCESS) {
        throw std::runtime_error("failed to create the Vulkan logical device");
    }

    vkGetDeviceQueue(device_, graphicsQueueFamily_, 0, &graphicsQueue_);
    vkGetDeviceQueue(device_, presentQueueFamily_, 0, &presentQueue_);
}

void VulkanRenderer::Impl::createCommandPool() {
    VkCommandPoolCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    createInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    createInfo.queueFamilyIndex = graphicsQueueFamily_;
    if (vkCreateCommandPool(device_, &createInfo, nullptr, &commandPool_) !=
        VK_SUCCESS) {
        throw std::runtime_error("failed to create the Vulkan command pool");
    }
}

void VulkanRenderer::Impl::createTextureResources() {
    textureReleaseState_ =
        std::make_shared<detail::TextureReleaseState>();
    textureReleaseState_->release = [this](TextureId id) {
        destroyTexture(id);
    };

    createTextureDescriptorResources();
    createSampler();

    TextureImage white;
    white.width = 1;
    white.height = 1;
    white.format = TextureFormat::R8Unorm;
    white.pixels = {255};
    whiteTexture_ = createTextureRecord(white);
}

void VulkanRenderer::Impl::createTextureDescriptorResources() {
    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType =
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &binding;
    if (vkCreateDescriptorSetLayout(
            device_, &layoutInfo, nullptr,
            &textureDescriptorSetLayout_) != VK_SUCCESS) {
        throw std::runtime_error(
            "failed to create the texture descriptor set layout");
    }

    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = 1025;
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = 1025;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    if (vkCreateDescriptorPool(
            device_, &poolInfo, nullptr, &textureDescriptorPool_) !=
        VK_SUCCESS) {
        throw std::runtime_error(
            "failed to create the texture descriptor pool");
    }
}

void VulkanRenderer::Impl::createSampler() {
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.maxLod = 0.0F;
    if (vkCreateSampler(
            device_, &samplerInfo, nullptr, &textureSampler_) != VK_SUCCESS) {
        throw std::runtime_error("failed to create the texture sampler");
    }
}

void VulkanRenderer::Impl::createBuffer(
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties,
    VkBuffer& buffer,
    VkDeviceMemory& memory) const {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(device_, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to create a texture staging buffer");
    }

    try {
        VkMemoryRequirements requirements{};
        vkGetBufferMemoryRequirements(device_, buffer, &requirements);
        VkMemoryAllocateInfo allocationInfo{};
        allocationInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocationInfo.allocationSize = requirements.size;
        allocationInfo.memoryTypeIndex = findMemoryType(
            requirements.memoryTypeBits, properties);
        if (vkAllocateMemory(
                device_, &allocationInfo, nullptr, &memory) != VK_SUCCESS ||
            vkBindBufferMemory(device_, buffer, memory, 0) != VK_SUCCESS) {
            throw std::runtime_error(
                "failed to allocate texture staging memory");
        }
    } catch (...) {
        if (memory != VK_NULL_HANDLE) {
            vkFreeMemory(device_, memory, nullptr);
            memory = VK_NULL_HANDLE;
        }
        vkDestroyBuffer(device_, buffer, nullptr);
        buffer = VK_NULL_HANDLE;
        throw;
    }
}

VkCommandBuffer VulkanRenderer::Impl::beginOneTimeCommands() {
    VkCommandBufferAllocateInfo allocationInfo{};
    allocationInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocationInfo.commandPool = commandPool_;
    allocationInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocationInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    if (vkAllocateCommandBuffers(
            device_, &allocationInfo, &commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error(
            "failed to allocate a texture upload command buffer");
    }

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        vkFreeCommandBuffers(device_, commandPool_, 1, &commandBuffer);
        throw std::runtime_error(
            "failed to begin a texture upload command buffer");
    }
    return commandBuffer;
}

void VulkanRenderer::Impl::endOneTimeCommands(
    VkCommandBuffer commandBuffer) {
    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        vkFreeCommandBuffers(device_, commandPool_, 1, &commandBuffer);
        throw std::runtime_error(
            "failed to record a texture upload command buffer");
    }

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    if (vkQueueSubmit(
            graphicsQueue_, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
        vkFreeCommandBuffers(device_, commandPool_, 1, &commandBuffer);
        throw std::runtime_error("failed to submit a texture upload");
    }
    vkQueueWaitIdle(graphicsQueue_);
    vkFreeCommandBuffers(device_, commandPool_, 1, &commandBuffer);
}

void VulkanRenderer::Impl::transitionImageLayout(
    VkCommandBuffer commandBuffer,
    VkImage image,
    VkImageLayout oldLayout,
    VkImageLayout newLayout) {
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkPipelineStageFlags destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
        newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL &&
               newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        sourceStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
               newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else {
        throw std::invalid_argument(
            "unsupported texture image layout transition");
    }

    vkCmdPipelineBarrier(
        commandBuffer,
        sourceStage,
        destinationStage,
        0,
        0,
        nullptr,
        0,
        nullptr,
        1,
        &barrier);
}

void VulkanRenderer::Impl::uploadTextureRegion(
    TextureRecord& texture,
    const TextureUpdate& update,
    VkImageLayout oldLayout) {
    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
    createBuffer(
        static_cast<VkDeviceSize>(update.pixels.size()),
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer,
        stagingMemory);

    try {
        void* mapped = nullptr;
        if (vkMapMemory(
                device_, stagingMemory, 0, update.pixels.size(), 0,
                &mapped) != VK_SUCCESS) {
            throw std::runtime_error("failed to map texture upload memory");
        }
        std::memcpy(mapped, update.pixels.data(), update.pixels.size());
        vkUnmapMemory(device_, stagingMemory);

        const VkCommandBuffer commandBuffer = beginOneTimeCommands();
        transitionImageLayout(
            commandBuffer,
            texture.image,
            oldLayout,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        VkBufferImageCopy copy{};
        copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copy.imageSubresource.layerCount = 1;
        copy.imageOffset = {
            static_cast<std::int32_t>(update.x),
            static_cast<std::int32_t>(update.y),
            0};
        copy.imageExtent = {update.width, update.height, 1};
        vkCmdCopyBufferToImage(
            commandBuffer,
            stagingBuffer,
            texture.image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &copy);
        transitionImageLayout(
            commandBuffer,
            texture.image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        endOneTimeCommands(commandBuffer);
    } catch (...) {
        vkDestroyBuffer(device_, stagingBuffer, nullptr);
        vkFreeMemory(device_, stagingMemory, nullptr);
        throw;
    }

    vkDestroyBuffer(device_, stagingBuffer, nullptr);
    vkFreeMemory(device_, stagingMemory, nullptr);
}

VulkanRenderer::Impl::TextureRecord
VulkanRenderer::Impl::createTextureRecord(const TextureImage& image) {
    if (!isValidTextureImage(image)) {
        throw std::invalid_argument("texture image data is invalid");
    }

    TextureRecord result;
    result.width = image.width;
    result.height = image.height;
    result.format = image.format;
    const VkFormat format = image.format == TextureFormat::R8Unorm
        ? VK_FORMAT_R8_UNORM
        : VK_FORMAT_R8G8B8A8_UNORM;

    try {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.format = format;
        imageInfo.extent = {image.width, image.height, 1};
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage =
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        if (vkCreateImage(device_, &imageInfo, nullptr, &result.image) !=
            VK_SUCCESS) {
            throw std::runtime_error("failed to create a texture image");
        }

        VkMemoryRequirements requirements{};
        vkGetImageMemoryRequirements(device_, result.image, &requirements);
        VkMemoryAllocateInfo allocationInfo{};
        allocationInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocationInfo.allocationSize = requirements.size;
        allocationInfo.memoryTypeIndex = findMemoryType(
            requirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (vkAllocateMemory(
                device_, &allocationInfo, nullptr, &result.memory) !=
                VK_SUCCESS ||
            vkBindImageMemory(device_, result.image, result.memory, 0) !=
                VK_SUCCESS) {
            throw std::runtime_error(
                "failed to allocate or bind texture image memory");
        }

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = result.image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;
        if (image.format == TextureFormat::R8Unorm) {
            viewInfo.components = {
                VK_COMPONENT_SWIZZLE_ONE,
                VK_COMPONENT_SWIZZLE_ONE,
                VK_COMPONENT_SWIZZLE_ONE,
                VK_COMPONENT_SWIZZLE_R,
            };
        }
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.layerCount = 1;
        if (vkCreateImageView(
                device_, &viewInfo, nullptr, &result.view) != VK_SUCCESS) {
            throw std::runtime_error("failed to create a texture image view");
        }

        VkDescriptorSetAllocateInfo descriptorInfo{};
        descriptorInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        descriptorInfo.descriptorPool = textureDescriptorPool_;
        descriptorInfo.descriptorSetCount = 1;
        descriptorInfo.pSetLayouts = &textureDescriptorSetLayout_;
        if (vkAllocateDescriptorSets(
                device_, &descriptorInfo, &result.descriptorSet) !=
            VK_SUCCESS) {
            throw std::runtime_error(
                "failed to allocate a texture descriptor set");
        }

        VkDescriptorImageInfo imageDescriptor{};
        imageDescriptor.sampler = textureSampler_;
        imageDescriptor.imageView = result.view;
        imageDescriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = result.descriptorSet;
        write.dstBinding = 0;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.pImageInfo = &imageDescriptor;
        vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);

        uploadTextureRegion(
            result,
            {0, 0, image.width, image.height, image.pixels},
            VK_IMAGE_LAYOUT_UNDEFINED);
    } catch (...) {
        destroyTextureRecord(result);
        throw;
    }
    return result;
}

TextureId VulkanRenderer::Impl::createTexture(const TextureImage& image) {
    TextureRecord texture = createTextureRecord(image);
    TextureId id = nextTextureId_++;
    while (id == invalidTextureId || textures_.count(id) != 0) {
        id = nextTextureId_++;
    }
    textures_.emplace(id, std::move(texture));
    return id;
}

void VulkanRenderer::Impl::updateTexture(
    TextureId id,
    const TextureUpdate& update) {
    const auto found = textures_.find(id);
    if (found == textures_.end()) {
        throw std::invalid_argument("texture does not belong to this renderer");
    }
    TextureRecord& texture = found->second;
    if (!isValidTextureUpdate(
            update,
            texture.width,
            texture.height,
            texture.format)) {
        throw std::invalid_argument("texture update data is invalid");
    }
    vkDeviceWaitIdle(device_);
    uploadTextureRegion(
        texture, update, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void VulkanRenderer::Impl::destroyTextureRecord(
    TextureRecord& texture) noexcept {
    if (texture.descriptorSet != VK_NULL_HANDLE &&
        textureDescriptorPool_ != VK_NULL_HANDLE) {
        vkFreeDescriptorSets(
            device_, textureDescriptorPool_, 1, &texture.descriptorSet);
        texture.descriptorSet = VK_NULL_HANDLE;
    }
    if (texture.view != VK_NULL_HANDLE) {
        vkDestroyImageView(device_, texture.view, nullptr);
        texture.view = VK_NULL_HANDLE;
    }
    if (texture.image != VK_NULL_HANDLE) {
        vkDestroyImage(device_, texture.image, nullptr);
        texture.image = VK_NULL_HANDLE;
    }
    if (texture.memory != VK_NULL_HANDLE) {
        vkFreeMemory(device_, texture.memory, nullptr);
        texture.memory = VK_NULL_HANDLE;
    }
}

void VulkanRenderer::Impl::destroyTexture(TextureId id) noexcept {
    const auto found = textures_.find(id);
    if (found == textures_.end() || device_ == VK_NULL_HANDLE) {
        return;
    }
    vkDeviceWaitIdle(device_);
    destroyTextureRecord(found->second);
    textures_.erase(found);
}

VkDescriptorSet VulkanRenderer::Impl::descriptorSetFor(
    TextureId texture) const {
    if (texture == invalidTextureId) {
        return whiteTexture_.descriptorSet;
    }
    const auto found = textures_.find(texture);
    if (found == textures_.end()) {
        throw std::runtime_error("paint command references an unknown texture");
    }
    return found->second.descriptorSet;
}

void VulkanRenderer::Impl::createSyncObjects() {
    std::array<VkCommandBuffer, kFramesInFlight> commandBuffers{};
    VkCommandBufferAllocateInfo allocationInfo{};
    allocationInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocationInfo.commandPool = commandPool_;
    allocationInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocationInfo.commandBufferCount = static_cast<std::uint32_t>(
        commandBuffers.size());
    if (vkAllocateCommandBuffers(
            device_, &allocationInfo, commandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate Vulkan command buffers");
    }

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (std::size_t index = 0; index < frames_.size(); ++index) {
        frames_[index].commandBuffer = commandBuffers[index];
        if (vkCreateSemaphore(
                device_, &semaphoreInfo, nullptr,
                &frames_[index].imageAvailable) != VK_SUCCESS ||
            vkCreateSemaphore(
                device_, &semaphoreInfo, nullptr,
                &frames_[index].renderFinished) != VK_SUCCESS ||
            vkCreateFence(
                device_, &fenceInfo, nullptr,
                &frames_[index].inFlight) != VK_SUCCESS) {
            throw std::runtime_error(
                "failed to create Vulkan frame synchronization objects");
        }
    }
}

std::uint32_t VulkanRenderer::Impl::findMemoryType(
    std::uint32_t typeFilter,
    VkMemoryPropertyFlags properties) const {
    VkPhysicalDeviceMemoryProperties memoryProperties{};
    vkGetPhysicalDeviceMemoryProperties(
        physicalDevice_, &memoryProperties);

    for (std::uint32_t index = 0;
         index < memoryProperties.memoryTypeCount;
         ++index) {
        const bool typeMatches = (typeFilter & (1U << index)) != 0;
        const bool propertiesMatch =
            (memoryProperties.memoryTypes[index].propertyFlags & properties) ==
            properties;
        if (typeMatches && propertiesMatch) {
            return index;
        }
    }
    throw std::runtime_error("no suitable Vulkan memory type was found");
}

void VulkanRenderer::Impl::ensureInstanceCapacity(
    FrameSync& frame,
    std::size_t instanceCount) {
    if (instanceCount <= frame.instanceCapacity) {
        return;
    }

    std::size_t newCapacity = std::max<std::size_t>(64, frame.instanceCapacity);
    while (newCapacity < instanceCount) {
        newCapacity *= 2;
    }

    VkBuffer newBuffer = VK_NULL_HANDLE;
    VkDeviceMemory newMemory = VK_NULL_HANDLE;
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = sizeof(RectangleInstance) * newCapacity;
    bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device_, &bufferInfo, nullptr, &newBuffer) !=
        VK_SUCCESS) {
        throw std::runtime_error("failed to create a rectangle instance buffer");
    }

    try {
        VkMemoryRequirements requirements{};
        vkGetBufferMemoryRequirements(device_, newBuffer, &requirements);

        VkMemoryAllocateInfo allocationInfo{};
        allocationInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocationInfo.allocationSize = requirements.size;
        allocationInfo.memoryTypeIndex = findMemoryType(
            requirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        if (vkAllocateMemory(
                device_, &allocationInfo, nullptr, &newMemory) != VK_SUCCESS) {
            throw std::runtime_error(
                "failed to allocate rectangle instance memory");
        }
        if (vkBindBufferMemory(device_, newBuffer, newMemory, 0) !=
            VK_SUCCESS) {
            throw std::runtime_error(
                "failed to bind rectangle instance memory");
        }
    } catch (...) {
        if (newMemory != VK_NULL_HANDLE) {
            vkFreeMemory(device_, newMemory, nullptr);
        }
        vkDestroyBuffer(device_, newBuffer, nullptr);
        throw;
    }

    if (frame.instanceBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device_, frame.instanceBuffer, nullptr);
    }
    if (frame.instanceMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device_, frame.instanceMemory, nullptr);
    }
    frame.instanceBuffer = newBuffer;
    frame.instanceMemory = newMemory;
    frame.instanceCapacity = newCapacity;
}

PaintBatchPlan VulkanRenderer::Impl::uploadInstances(
    FrameSync& frame,
    const std::vector<PaintCommand>& paintCommands) {
    const auto metrics = window_.metrics();
    const float scale = std::max(0.01F, metrics.dpiScale);
    PaintBatchPlan plan = buildPaintBatchPlan(
        paintCommands,
        scale,
        swapchainExtent_.width,
        swapchainExtent_.height);

    std::vector<RectangleInstance> instances;
    instances.reserve(plan.commandIndices.size());

    for (const std::size_t commandIndex : plan.commandIndices) {
        const PaintCommand& command = paintCommands[commandIndex];

        RectangleInstance instance{};
        instance.bounds[0] = command.bounds.x * scale;
        instance.bounds[1] = command.bounds.y * scale;
        instance.bounds[2] = command.bounds.width * scale;
        instance.bounds[3] = command.bounds.height * scale;
        instance.color[0] = command.color.red;
        instance.color[1] = command.color.green;
        instance.color[2] = command.color.blue;
        instance.color[3] = command.color.alpha;
        instance.parameters[0] = command.cornerRadius * scale;
        instance.parameters[1] =
            command.texture == invalidTextureId ? 0.0F : 1.0F;
        instance.textureCoordinates[0] = command.textureCoordinates.x;
        instance.textureCoordinates[1] = command.textureCoordinates.y;
        instance.textureCoordinates[2] = command.textureCoordinates.width;
        instance.textureCoordinates[3] = command.textureCoordinates.height;
        instances.push_back(instance);
    }

    if (instances.empty()) {
        return plan;
    }

    ensureInstanceCapacity(frame, instances.size());
    void* mappedMemory = nullptr;
    const VkDeviceSize byteCount =
        sizeof(RectangleInstance) * instances.size();
    if (vkMapMemory(
            device_, frame.instanceMemory, 0, byteCount, 0,
            &mappedMemory) != VK_SUCCESS) {
        throw std::runtime_error("failed to map rectangle instance memory");
    }
    std::memcpy(mappedMemory, instances.data(), byteCount);
    vkUnmapMemory(device_, frame.instanceMemory);

    return plan;
}

VkExtent2D VulkanRenderer::Impl::desiredExtent(
    const VkSurfaceCapabilitiesKHR& capabilities) const {
    if (capabilities.currentExtent.width !=
        std::numeric_limits<std::uint32_t>::max()) {
        return capabilities.currentExtent;
    }

    const auto metrics = window_.metrics();
    const auto width = static_cast<std::uint32_t>(
        std::max(1, metrics.framebufferWidth));
    const auto height = static_cast<std::uint32_t>(
        std::max(1, metrics.framebufferHeight));
    return {
        std::clamp(
            width,
            capabilities.minImageExtent.width,
            capabilities.maxImageExtent.width),
        std::clamp(
            height,
            capabilities.minImageExtent.height,
            capabilities.maxImageExtent.height),
    };
}

void VulkanRenderer::Impl::createSwapchain() {
    const SwapchainSupport support =
        querySwapchainSupport(physicalDevice_, surface_);
    if (support.formats.empty() || support.presentModes.empty()) {
        throw std::runtime_error("Vulkan surface has no swapchain configuration");
    }
    if ((support.capabilities.supportedUsageFlags &
         VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) == 0) {
        throw std::runtime_error(
            "Vulkan surface images cannot be used as color attachments");
    }

    const VkSurfaceFormatKHR surfaceFormat =
        chooseSurfaceFormat(support.formats);
    swapchainExtent_ = desiredExtent(support.capabilities);

    std::uint32_t imageCount = support.capabilities.minImageCount + 1;
    if (support.capabilities.maxImageCount > 0) {
        imageCount = std::min(
            imageCount, support.capabilities.maxImageCount);
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface_;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = swapchainExtent_;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    const std::uint32_t queueFamilies[]{
        graphicsQueueFamily_, presentQueueFamily_};
    if (graphicsQueueFamily_ != presentQueueFamily_) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilies;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = support.capabilities.currentTransform;
    createInfo.compositeAlpha = chooseCompositeAlpha(
        support.capabilities.supportedCompositeAlpha);
    createInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    createInfo.clipped = VK_TRUE;

    if (vkCreateSwapchainKHR(device_, &createInfo, nullptr, &swapchain_) !=
        VK_SUCCESS) {
        throw std::runtime_error("failed to create the Vulkan swapchain");
    }

    swapchainFormat_ = surfaceFormat.format;
    vkGetSwapchainImagesKHR(device_, swapchain_, &imageCount, nullptr);
    swapchainImages_.resize(imageCount);
    vkGetSwapchainImagesKHR(
        device_, swapchain_, &imageCount, swapchainImages_.data());
    imageFences_.assign(imageCount, VK_NULL_HANDLE);
}

void VulkanRenderer::Impl::createImageViews() {
    imageViews_.resize(swapchainImages_.size());
    for (std::size_t index = 0; index < swapchainImages_.size(); ++index) {
        VkImageViewCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = swapchainImages_[index];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = swapchainFormat_;
        createInfo.components = {
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
        };
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(
                device_, &createInfo, nullptr, &imageViews_[index]) !=
            VK_SUCCESS) {
            throw std::runtime_error("failed to create a swapchain image view");
        }
    }
}

void VulkanRenderer::Impl::createRenderPass() {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapchainFormat_;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorReference{};
    colorReference.attachment = 0;
    colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorReference;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    createInfo.attachmentCount = 1;
    createInfo.pAttachments = &colorAttachment;
    createInfo.subpassCount = 1;
    createInfo.pSubpasses = &subpass;
    createInfo.dependencyCount = 1;
    createInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(device_, &createInfo, nullptr, &renderPass_) !=
        VK_SUCCESS) {
        throw std::runtime_error("failed to create the Vulkan render pass");
    }
}

VkShaderModule VulkanRenderer::Impl::createShaderModule(
    const std::uint32_t* code,
    std::size_t byteCount) const {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = byteCount;
    createInfo.pCode = code;

    VkShaderModule module = VK_NULL_HANDLE;
    if (vkCreateShaderModule(device_, &createInfo, nullptr, &module) !=
        VK_SUCCESS) {
        throw std::runtime_error("failed to create a Vulkan shader module");
    }
    return module;
}

void VulkanRenderer::Impl::createGraphicsPipeline() {
    const VkShaderModule vertexShader = createShaderModule(
        generated::rectangleVertexSpirv,
        sizeof(generated::rectangleVertexSpirv));
    VkShaderModule fragmentShader = VK_NULL_HANDLE;

    try {
        fragmentShader = createShaderModule(
            generated::rectangleFragmentSpirv,
            sizeof(generated::rectangleFragmentSpirv));

        VkPipelineShaderStageCreateInfo vertexStage{};
        vertexStage.sType =
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertexStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertexStage.module = vertexShader;
        vertexStage.pName = "main";

        VkPipelineShaderStageCreateInfo fragmentStage{};
        fragmentStage.sType =
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragmentStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragmentStage.module = fragmentShader;
        fragmentStage.pName = "main";

        const VkPipelineShaderStageCreateInfo stages[]{
            vertexStage, fragmentStage};

        VkVertexInputBindingDescription binding{};
        binding.binding = 0;
        binding.stride = sizeof(RectangleInstance);
        binding.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

        const std::array<VkVertexInputAttributeDescription, 4> attributes{{
            {0, 0, VK_FORMAT_R32G32B32A32_SFLOAT,
             static_cast<std::uint32_t>(offsetof(RectangleInstance, bounds))},
            {1, 0, VK_FORMAT_R32G32B32A32_SFLOAT,
             static_cast<std::uint32_t>(offsetof(RectangleInstance, color))},
            {2, 0, VK_FORMAT_R32G32B32A32_SFLOAT,
             static_cast<std::uint32_t>(offsetof(
                 RectangleInstance, parameters))},
            {3, 0, VK_FORMAT_R32G32B32A32_SFLOAT,
             static_cast<std::uint32_t>(offsetof(
                 RectangleInstance, textureCoordinates))},
        }};

        VkPipelineVertexInputStateCreateInfo vertexInput{};
        vertexInput.sType =
            VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInput.vertexBindingDescriptionCount = 1;
        vertexInput.pVertexBindingDescriptions = &binding;
        vertexInput.vertexAttributeDescriptionCount =
            static_cast<std::uint32_t>(attributes.size());
        vertexInput.pVertexAttributeDescriptions = attributes.data();

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType =
            VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType =
            VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo rasterization{};
        rasterization.sType =
            VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterization.polygonMode = VK_POLYGON_MODE_FILL;
        rasterization.cullMode = VK_CULL_MODE_NONE;
        rasterization.frontFace = VK_FRONT_FACE_CLOCKWISE;
        rasterization.lineWidth = 1.0F;

        VkPipelineMultisampleStateCreateInfo multisample{};
        multisample.sType =
            VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineColorBlendAttachmentState blendAttachment{};
        blendAttachment.blendEnable = VK_TRUE;
        blendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        blendAttachment.dstColorBlendFactor =
            VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        blendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        blendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        blendAttachment.dstAlphaBlendFactor =
            VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        blendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
        blendAttachment.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        VkPipelineColorBlendStateCreateInfo colorBlend{};
        colorBlend.sType =
            VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlend.attachmentCount = 1;
        colorBlend.pAttachments = &blendAttachment;

        const std::array<VkDynamicState, 2> dynamicStates{
            VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType =
            VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount =
            static_cast<std::uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        pushConstantRange.size = sizeof(ViewportPushConstants);

        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges = &pushConstantRange;
        layoutInfo.setLayoutCount = 1;
        layoutInfo.pSetLayouts = &textureDescriptorSetLayout_;
        if (vkCreatePipelineLayout(
                device_, &layoutInfo, nullptr, &pipelineLayout_) !=
            VK_SUCCESS) {
            throw std::runtime_error(
                "failed to create the Vulkan pipeline layout");
        }

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType =
            VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = stages;
        pipelineInfo.pVertexInputState = &vertexInput;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterization;
        pipelineInfo.pMultisampleState = &multisample;
        pipelineInfo.pColorBlendState = &colorBlend;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = pipelineLayout_;
        pipelineInfo.renderPass = renderPass_;
        pipelineInfo.subpass = 0;

        if (vkCreateGraphicsPipelines(
                device_, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
                &graphicsPipeline_) != VK_SUCCESS) {
            throw std::runtime_error(
                "failed to create the Vulkan graphics pipeline");
        }
    } catch (...) {
        if (pipelineLayout_ != VK_NULL_HANDLE &&
            graphicsPipeline_ == VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device_, pipelineLayout_, nullptr);
            pipelineLayout_ = VK_NULL_HANDLE;
        }
        if (fragmentShader != VK_NULL_HANDLE) {
            vkDestroyShaderModule(device_, fragmentShader, nullptr);
        }
        vkDestroyShaderModule(device_, vertexShader, nullptr);
        throw;
    }

    vkDestroyShaderModule(device_, fragmentShader, nullptr);
    vkDestroyShaderModule(device_, vertexShader, nullptr);
}

void VulkanRenderer::Impl::createFramebuffers() {
    framebuffers_.resize(imageViews_.size());
    for (std::size_t index = 0; index < imageViews_.size(); ++index) {
        const VkImageView attachments[]{imageViews_[index]};
        VkFramebufferCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        createInfo.renderPass = renderPass_;
        createInfo.attachmentCount = 1;
        createInfo.pAttachments = attachments;
        createInfo.width = swapchainExtent_.width;
        createInfo.height = swapchainExtent_.height;
        createInfo.layers = 1;
        if (vkCreateFramebuffer(
                device_, &createInfo, nullptr, &framebuffers_[index]) !=
            VK_SUCCESS) {
            throw std::runtime_error("failed to create a Vulkan framebuffer");
        }
    }
}

void VulkanRenderer::Impl::createSwapchainResources() {
    createSwapchain();
    createImageViews();
    createRenderPass();
    createGraphicsPipeline();
    createFramebuffers();
}

void VulkanRenderer::Impl::recordCommandBuffer(
    VkCommandBuffer commandBuffer,
    std::uint32_t imageIndex,
    const FrameSync& frame,
    const std::vector<PaintBatch>& batches) const {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("failed to begin a Vulkan command buffer");
    }

    const VkClearValue clearColor{{{0.025F, 0.045F, 0.075F, 1.0F}}};
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass_;
    renderPassInfo.framebuffer = framebuffers_[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = swapchainExtent_;
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;

    vkCmdBeginRenderPass(
        commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    if (!batches.empty()) {
        const VkViewport viewport{
            0.0F,
            0.0F,
            static_cast<float>(swapchainExtent_.width),
            static_cast<float>(swapchainExtent_.height),
            0.0F,
            1.0F,
        };
        const ViewportPushConstants pushConstants{
            static_cast<float>(swapchainExtent_.width),
            static_cast<float>(swapchainExtent_.height),
        };
        const VkDeviceSize bufferOffset = 0;

        vkCmdBindPipeline(
            commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
            graphicsPipeline_);
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
        vkCmdPushConstants(
            commandBuffer, pipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT,
            0, sizeof(pushConstants), &pushConstants);
        vkCmdBindVertexBuffers(
            commandBuffer, 0, 1, &frame.instanceBuffer, &bufferOffset);

        for (const PaintBatch& batch : batches) {
            const VkRect2D scissor{
                {batch.scissor.x, batch.scissor.y},
                {batch.scissor.width, batch.scissor.height},
            };
            const VkDescriptorSet descriptorSet =
                descriptorSetFor(batch.texture);
            vkCmdBindDescriptorSets(
                commandBuffer,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                pipelineLayout_,
                0,
                1,
                &descriptorSet,
                0,
                nullptr);
            vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
            vkCmdDraw(
                commandBuffer,
                6,
                batch.instanceCount,
                0,
                batch.firstInstance);
        }
    }

    vkCmdEndRenderPass(commandBuffer);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to record a Vulkan command buffer");
    }
}

bool VulkanRenderer::Impl::framebufferSizeChanged() const {
    const auto metrics = window_.metrics();
    return metrics.framebufferWidth > 0 && metrics.framebufferHeight > 0 &&
        (swapchainExtent_.width !=
             static_cast<std::uint32_t>(metrics.framebufferWidth) ||
         swapchainExtent_.height !=
             static_cast<std::uint32_t>(metrics.framebufferHeight));
}

bool VulkanRenderer::Impl::recreateSwapchain() {
    const auto metrics = window_.metrics();
    if (metrics.framebufferWidth <= 0 || metrics.framebufferHeight <= 0) {
        return false;
    }
    vkDeviceWaitIdle(device_);
    destroySwapchainResources();
    createSwapchainResources();
    return true;
}

void VulkanRenderer::Impl::drawFrame(
    const std::vector<PaintCommand>& paintCommands) {
    const auto metrics = window_.metrics();
    if (metrics.framebufferWidth <= 0 || metrics.framebufferHeight <= 0) {
        return;
    }
    if (framebufferSizeChanged() && !recreateSwapchain()) {
        return;
    }

    FrameSync& frame = frames_[currentFrame_];
    vkWaitForFences(
        device_, 1, &frame.inFlight, VK_TRUE,
        std::numeric_limits<std::uint64_t>::max());

    std::uint32_t imageIndex = 0;
    const VkResult acquireResult = vkAcquireNextImageKHR(
        device_,
        swapchain_,
        std::numeric_limits<std::uint64_t>::max(),
        frame.imageAvailable,
        VK_NULL_HANDLE,
        &imageIndex);
    if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapchain();
        return;
    }
    if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("failed to acquire a Vulkan swapchain image");
    }

    if (imageFences_[imageIndex] != VK_NULL_HANDLE) {
        vkWaitForFences(
            device_, 1, &imageFences_[imageIndex], VK_TRUE,
            std::numeric_limits<std::uint64_t>::max());
    }
    imageFences_[imageIndex] = frame.inFlight;

    const PaintBatchPlan batchPlan = uploadInstances(frame, paintCommands);

    vkResetFences(device_, 1, &frame.inFlight);
    vkResetCommandBuffer(frame.commandBuffer, 0);
    recordCommandBuffer(
        frame.commandBuffer, imageIndex, frame, batchPlan.batches);

    const VkPipelineStageFlags waitStage =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &frame.imageAvailable;
    submitInfo.pWaitDstStageMask = &waitStage;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &frame.commandBuffer;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &frame.renderFinished;
    if (vkQueueSubmit(
            graphicsQueue_, 1, &submitInfo, frame.inFlight) != VK_SUCCESS) {
        throw std::runtime_error("failed to submit a Vulkan frame");
    }

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &frame.renderFinished;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain_;
    presentInfo.pImageIndices = &imageIndex;
    const VkResult presentResult =
        vkQueuePresentKHR(presentQueue_, &presentInfo);

    currentFrame_ = (currentFrame_ + 1) % frames_.size();

    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR ||
        presentResult == VK_SUBOPTIMAL_KHR ||
        framebufferSizeChanged()) {
        recreateSwapchain();
    } else if (presentResult != VK_SUCCESS) {
        throw std::runtime_error("failed to present a Vulkan frame");
    }
}

void VulkanRenderer::Impl::destroySwapchainResources() {
    for (const auto framebuffer : framebuffers_) {
        vkDestroyFramebuffer(device_, framebuffer, nullptr);
    }
    framebuffers_.clear();

    if (graphicsPipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, graphicsPipeline_, nullptr);
        graphicsPipeline_ = VK_NULL_HANDLE;
    }
    if (pipelineLayout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device_, pipelineLayout_, nullptr);
        pipelineLayout_ = VK_NULL_HANDLE;
    }

    if (renderPass_ != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device_, renderPass_, nullptr);
        renderPass_ = VK_NULL_HANDLE;
    }

    for (const auto imageView : imageViews_) {
        vkDestroyImageView(device_, imageView, nullptr);
    }
    imageViews_.clear();
    swapchainImages_.clear();
    imageFences_.clear();

    if (swapchain_ != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device_, swapchain_, nullptr);
        swapchain_ = VK_NULL_HANDLE;
    }
}

void VulkanRenderer::Impl::cleanup() {
    if (device_ != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device_);
        if (textureReleaseState_) {
            textureReleaseState_->active = false;
            textureReleaseState_->release = {};
        }
        destroySwapchainResources();

        for (auto& entry : textures_) {
            destroyTextureRecord(entry.second);
        }
        textures_.clear();
        destroyTextureRecord(whiteTexture_);
        if (textureSampler_ != VK_NULL_HANDLE) {
            vkDestroySampler(device_, textureSampler_, nullptr);
            textureSampler_ = VK_NULL_HANDLE;
        }
        if (textureDescriptorPool_ != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(
                device_, textureDescriptorPool_, nullptr);
            textureDescriptorPool_ = VK_NULL_HANDLE;
        }
        if (textureDescriptorSetLayout_ != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(
                device_, textureDescriptorSetLayout_, nullptr);
            textureDescriptorSetLayout_ = VK_NULL_HANDLE;
        }

        for (auto& frame : frames_) {
            if (frame.instanceBuffer != VK_NULL_HANDLE) {
                vkDestroyBuffer(device_, frame.instanceBuffer, nullptr);
                frame.instanceBuffer = VK_NULL_HANDLE;
            }
            if (frame.instanceMemory != VK_NULL_HANDLE) {
                vkFreeMemory(device_, frame.instanceMemory, nullptr);
                frame.instanceMemory = VK_NULL_HANDLE;
            }
            frame.instanceCapacity = 0;
            if (frame.imageAvailable != VK_NULL_HANDLE) {
                vkDestroySemaphore(device_, frame.imageAvailable, nullptr);
                frame.imageAvailable = VK_NULL_HANDLE;
            }
            if (frame.renderFinished != VK_NULL_HANDLE) {
                vkDestroySemaphore(device_, frame.renderFinished, nullptr);
                frame.renderFinished = VK_NULL_HANDLE;
            }
            if (frame.inFlight != VK_NULL_HANDLE) {
                vkDestroyFence(device_, frame.inFlight, nullptr);
                frame.inFlight = VK_NULL_HANDLE;
            }
        }

        if (commandPool_ != VK_NULL_HANDLE) {
            vkDestroyCommandPool(device_, commandPool_, nullptr);
            commandPool_ = VK_NULL_HANDLE;
        }
        vkDestroyDevice(device_, nullptr);
        device_ = VK_NULL_HANDLE;
    }

    if (surface_ != VK_NULL_HANDLE && instance_ != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(instance_, surface_, nullptr);
        surface_ = VK_NULL_HANDLE;
    }
    if (instance_ != VK_NULL_HANDLE) {
        vkDestroyInstance(instance_, nullptr);
        instance_ = VK_NULL_HANDLE;
    }
}

VulkanRenderer::VulkanRenderer(PlatformWindow& window)
    : impl_(std::make_unique<Impl>(window)) {}

VulkanRenderer::~VulkanRenderer() = default;

void VulkanRenderer::drawFrame(
    const std::vector<PaintCommand>& paintCommands) {
    impl_->drawFrame(paintCommands);
}

Texture VulkanRenderer::createTexture(const TextureImage& image) {
    const TextureId id = impl_->createTexture(image);
    return Texture(id, impl_->textureReleaseState());
}

void VulkanRenderer::updateTexture(
    const Texture& texture,
    const TextureUpdate& update) {
    if (!texture ||
        texture.releaseState_ != impl_->textureReleaseState()) {
        throw std::invalid_argument(
            "texture does not belong to this Vulkan renderer");
    }
    impl_->updateTexture(texture.id(), update);
}

} // namespace lotui
