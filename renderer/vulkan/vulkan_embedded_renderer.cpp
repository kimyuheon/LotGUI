#include "renderer/vulkan/vulkan_embedded_renderer.h"

#include "renderer/paint_batch.h"

#include "lotui/rectangle_fragment_spirv.h"
#include "lotui/rectangle_vertex_spirv.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <unordered_map>

namespace lotui {
namespace {

struct RectangleInstance {
    float bounds[4];
    float color[4];
    float parameters[4];
    float textureCoordinates[4];
};

static_assert(sizeof(RectangleInstance) == sizeof(float) * 16);

struct FrameBuffer {
    VkBuffer buffer{VK_NULL_HANDLE};
    VkDeviceMemory memory{VK_NULL_HANDLE};
    void* mapped{nullptr};
    std::size_t capacity{0};
};

struct TextureRecord {
    TextureImage image{};
    VkImage gpuImage{VK_NULL_HANDLE};
    VkDeviceMemory memory{VK_NULL_HANDLE};
    VkImageView view{VK_NULL_HANDLE};
    VkDescriptorSet descriptor{VK_NULL_HANDLE};
    bool dirty{true};
    bool uploaded{false};
};

void check(VkResult result, const char* message) {
    if (result != VK_SUCCESS) {
        throw std::runtime_error(message);
    }
}

VkShaderModule createShader(
    VkDevice device, const std::uint32_t* bytes, std::size_t size) {
    VkShaderModuleCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.codeSize = size;
    info.pCode = bytes;
    VkShaderModule shader = VK_NULL_HANDLE;
    check(vkCreateShaderModule(device, &info, nullptr, &shader),
        "failed to create LotUI embedded shader");
    return shader;
}

} // namespace

class VulkanEmbeddedRenderer::Impl {
public:
    ~Impl() { release(); }
    void draw(const VulkanEmbeddedFrame& frame,
        const std::vector<PaintCommand>& commands);
    TextureId createTexture(const TextureImage& image);
    void updateTexture(TextureId id, const TextureUpdate& update);
    void removeTexture(TextureId id) noexcept;

private:
    void release() noexcept;
    void releaseBuffer(FrameBuffer& buffer) noexcept;
    void releaseGpuTexture(TextureRecord& texture) noexcept;
    std::uint32_t memoryType(
        std::uint32_t bits, VkMemoryPropertyFlags flags) const;
    void ensureTextureResources(const VulkanEmbeddedFrame& frame);
    void createGpuTexture(TextureRecord& texture);
    void uploadTexture(TextureRecord& texture);
    VkDescriptorSet descriptorFor(TextureId id) const;
    void createPipeline(const VulkanEmbeddedFrame& frame);
    void ensureBuffer(FrameBuffer& buffer, std::size_t instanceCount);

    VkPhysicalDevice physicalDevice_{VK_NULL_HANDLE};
    VkDevice device_{VK_NULL_HANDLE};
    VkQueue queue_{VK_NULL_HANDLE};
    std::uint32_t queueFamily_{0};
    VkRenderPass renderPass_{VK_NULL_HANDLE};
    VkSampleCountFlagBits samples_{VK_SAMPLE_COUNT_1_BIT};
    VkPipelineLayout layout_{VK_NULL_HANDLE};
    VkPipeline pipeline_{VK_NULL_HANDLE};
    VkDescriptorSetLayout textureLayout_{VK_NULL_HANDLE};
    VkDescriptorPool texturePool_{VK_NULL_HANDLE};
    VkSampler sampler_{VK_NULL_HANDLE};
    VkCommandPool uploadPool_{VK_NULL_HANDLE};
    TextureRecord whiteTexture_{};
    std::unordered_map<TextureId, TextureRecord> textures_;
    TextureId nextTextureId_{1};
    std::vector<FrameBuffer> buffers_;
};

TextureId VulkanEmbeddedRenderer::Impl::createTexture(
    const TextureImage& image) {
    if (!isValidTextureImage(image)) {
        throw std::invalid_argument("invalid LotUI embedded texture image");
    }
    TextureId id = nextTextureId_++;
    while (id == invalidTextureId || textures_.count(id) != 0) {
        id = nextTextureId_++;
    }
    TextureRecord record;
    record.image = image;
    textures_.emplace(id, std::move(record));
    return id;
}

void VulkanEmbeddedRenderer::Impl::updateTexture(
    TextureId id, const TextureUpdate& update) {
    const auto found = textures_.find(id);
    if (found == textures_.end() ||
        !isValidTextureUpdate(update, found->second.image.width,
            found->second.image.height, found->second.image.format)) {
        throw std::invalid_argument("invalid LotUI embedded texture update");
    }
    TextureRecord& record = found->second;
    const std::size_t bytesPerPixel =
        textureBytesPerPixel(record.image.format);
    const std::size_t rowBytes = update.width * bytesPerPixel;
    for (std::uint32_t row = 0; row < update.height; ++row) {
        const std::size_t destination =
            (static_cast<std::size_t>(update.y + row) * record.image.width +
                update.x) * bytesPerPixel;
        std::memcpy(record.image.pixels.data() + destination,
            update.pixels.data() + static_cast<std::size_t>(row) * rowBytes,
            rowBytes);
    }
    record.dirty = true;
}

void VulkanEmbeddedRenderer::Impl::removeTexture(TextureId id) noexcept {
    const auto found = textures_.find(id);
    if (found == textures_.end()) {
        return;
    }
    if (device_ != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device_);
        releaseGpuTexture(found->second);
    }
    textures_.erase(found);
}

std::uint32_t VulkanEmbeddedRenderer::Impl::memoryType(
    std::uint32_t bits, VkMemoryPropertyFlags flags) const {
    VkPhysicalDeviceMemoryProperties properties{};
    vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &properties);
    for (std::uint32_t i = 0; i < properties.memoryTypeCount; ++i) {
        if ((bits & (1U << i)) != 0 &&
            (properties.memoryTypes[i].propertyFlags & flags) == flags) {
            return i;
        }
    }
    throw std::runtime_error("LotUI embedded texture memory is unavailable");
}

void VulkanEmbeddedRenderer::Impl::releaseGpuTexture(
    TextureRecord& texture) noexcept {
    if (texture.descriptor != VK_NULL_HANDLE &&
        texturePool_ != VK_NULL_HANDLE) {
        vkFreeDescriptorSets(device_, texturePool_, 1, &texture.descriptor);
    }
    if (texture.view != VK_NULL_HANDLE) {
        vkDestroyImageView(device_, texture.view, nullptr);
    }
    if (texture.gpuImage != VK_NULL_HANDLE) {
        vkDestroyImage(device_, texture.gpuImage, nullptr);
    }
    if (texture.memory != VK_NULL_HANDLE) {
        vkFreeMemory(device_, texture.memory, nullptr);
    }
    texture.descriptor = VK_NULL_HANDLE;
    texture.view = VK_NULL_HANDLE;
    texture.gpuImage = VK_NULL_HANDLE;
    texture.memory = VK_NULL_HANDLE;
    texture.uploaded = false;
    texture.dirty = true;
}

void VulkanEmbeddedRenderer::Impl::ensureTextureResources(
    const VulkanEmbeddedFrame& frame) {
    if (textureLayout_ != VK_NULL_HANDLE) {
        return;
    }
    if (frame.graphicsQueue == VK_NULL_HANDLE) {
        throw std::invalid_argument(
            "LotUI embedded texture rendering requires a graphics queue");
    }
    queue_ = frame.graphicsQueue;
    queueFamily_ = frame.graphicsQueueFamily;

    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &binding;
    check(vkCreateDescriptorSetLayout(
        device_, &layoutInfo, nullptr, &textureLayout_),
        "failed to create LotUI embedded texture layout");

    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = 256;
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = 256;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    check(vkCreateDescriptorPool(device_, &poolInfo, nullptr, &texturePool_),
        "failed to create LotUI embedded texture pool");

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    check(vkCreateSampler(device_, &samplerInfo, nullptr, &sampler_),
        "failed to create LotUI embedded texture sampler");

    VkCommandPoolCreateInfo commandPoolInfo{};
    commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolInfo.queueFamilyIndex = queueFamily_;
    check(vkCreateCommandPool(
        device_, &commandPoolInfo, nullptr, &uploadPool_),
        "failed to create LotUI embedded upload pool");

    whiteTexture_.image = {1, 1, TextureFormat::R8Unorm, {255}};
    createGpuTexture(whiteTexture_);
    uploadTexture(whiteTexture_);
}

void VulkanEmbeddedRenderer::Impl::createGpuTexture(
    TextureRecord& texture) {
    const VkFormat format = texture.image.format == TextureFormat::R8Unorm
        ? VK_FORMAT_R8_UNORM : VK_FORMAT_R8G8B8A8_UNORM;
    try {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.format = format;
        imageInfo.extent = {texture.image.width, texture.image.height, 1};
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT |
            VK_IMAGE_USAGE_SAMPLED_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        check(vkCreateImage(device_, &imageInfo, nullptr,
            &texture.gpuImage), "failed to create LotUI embedded texture");

        VkMemoryRequirements requirements{};
        vkGetImageMemoryRequirements(
            device_, texture.gpuImage, &requirements);
        VkMemoryAllocateInfo allocation{};
        allocation.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocation.allocationSize = requirements.size;
        allocation.memoryTypeIndex = memoryType(requirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        check(vkAllocateMemory(device_, &allocation, nullptr,
            &texture.memory), "failed to allocate LotUI embedded texture");
        check(vkBindImageMemory(device_, texture.gpuImage, texture.memory, 0),
            "failed to bind LotUI embedded texture");

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = texture.gpuImage;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;
        if (texture.image.format == TextureFormat::R8Unorm) {
            viewInfo.components = {VK_COMPONENT_SWIZZLE_ONE,
                VK_COMPONENT_SWIZZLE_ONE, VK_COMPONENT_SWIZZLE_ONE,
                VK_COMPONENT_SWIZZLE_R};
        }
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.layerCount = 1;
        check(vkCreateImageView(device_, &viewInfo, nullptr, &texture.view),
            "failed to create LotUI embedded texture view");

        VkDescriptorSetAllocateInfo descriptorInfo{};
        descriptorInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        descriptorInfo.descriptorPool = texturePool_;
        descriptorInfo.descriptorSetCount = 1;
        descriptorInfo.pSetLayouts = &textureLayout_;
        check(vkAllocateDescriptorSets(device_, &descriptorInfo,
            &texture.descriptor),
            "failed to allocate LotUI embedded texture descriptor");
        VkDescriptorImageInfo descriptorImage{};
        descriptorImage.sampler = sampler_;
        descriptorImage.imageView = texture.view;
        descriptorImage.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = texture.descriptor;
        write.dstBinding = 0;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.pImageInfo = &descriptorImage;
        vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);
    } catch (...) {
        releaseGpuTexture(texture);
        throw;
    }
}

void VulkanEmbeddedRenderer::Impl::uploadTexture(TextureRecord& texture) {
    VkBuffer staging = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkCommandBuffer command = VK_NULL_HANDLE;
    try {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = texture.image.pixels.size();
        bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        check(vkCreateBuffer(device_, &bufferInfo, nullptr, &staging),
            "failed to create LotUI embedded staging buffer");
        VkMemoryRequirements requirements{};
        vkGetBufferMemoryRequirements(device_, staging, &requirements);
        VkMemoryAllocateInfo allocation{};
        allocation.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocation.allocationSize = requirements.size;
        allocation.memoryTypeIndex = memoryType(requirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        check(vkAllocateMemory(device_, &allocation, nullptr, &memory),
            "failed to allocate LotUI embedded staging memory");
        check(vkBindBufferMemory(device_, staging, memory, 0),
            "failed to bind LotUI embedded staging buffer");
        void* mapped = nullptr;
        check(vkMapMemory(device_, memory, 0, bufferInfo.size, 0, &mapped),
            "failed to map LotUI embedded staging buffer");
        std::memcpy(mapped, texture.image.pixels.data(),
            texture.image.pixels.size());
        vkUnmapMemory(device_, memory);

        VkCommandBufferAllocateInfo commandInfo{};
        commandInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        commandInfo.commandPool = uploadPool_;
        commandInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        commandInfo.commandBufferCount = 1;
        check(vkAllocateCommandBuffers(device_, &commandInfo, &command),
            "failed to allocate LotUI embedded upload command");
        VkCommandBufferBeginInfo begin{};
        begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        check(vkBeginCommandBuffer(command, &begin),
            "failed to begin LotUI embedded upload command");

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = texture.uploaded
            ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            : VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = texture.gpuImage;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = texture.uploaded
            ? VK_ACCESS_SHADER_READ_BIT : 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        vkCmdPipelineBarrier(command,
            texture.uploaded ? VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
                             : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
            0, nullptr, 0, nullptr, 1, &barrier);

        VkBufferImageCopy copy{};
        copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copy.imageSubresource.layerCount = 1;
        copy.imageExtent = {
            texture.image.width, texture.image.height, 1};
        vkCmdCopyBufferToImage(command, staging, texture.gpuImage,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
            0, nullptr, 0, nullptr, 1, &barrier);
        check(vkEndCommandBuffer(command),
            "failed to record LotUI embedded upload command");

        VkSubmitInfo submit{};
        submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &command;
        check(vkQueueSubmit(queue_, 1, &submit, VK_NULL_HANDLE),
            "failed to submit LotUI embedded texture upload");
        check(vkQueueWaitIdle(queue_),
            "failed to finish LotUI embedded texture upload");
        texture.uploaded = true;
        texture.dirty = false;
    } catch (...) {
        if (command != VK_NULL_HANDLE) {
            vkFreeCommandBuffers(device_, uploadPool_, 1, &command);
        }
        if (staging != VK_NULL_HANDLE) {
            vkDestroyBuffer(device_, staging, nullptr);
        }
        if (memory != VK_NULL_HANDLE) {
            vkFreeMemory(device_, memory, nullptr);
        }
        throw;
    }
    vkFreeCommandBuffers(device_, uploadPool_, 1, &command);
    vkDestroyBuffer(device_, staging, nullptr);
    vkFreeMemory(device_, memory, nullptr);
}

VkDescriptorSet VulkanEmbeddedRenderer::Impl::descriptorFor(
    TextureId id) const {
    if (id == invalidTextureId) {
        return whiteTexture_.descriptor;
    }
    const auto found = textures_.find(id);
    if (found == textures_.end() ||
        found->second.descriptor == VK_NULL_HANDLE) {
        throw std::runtime_error(
            "LotUI embedded paint references an unknown texture");
    }
    return found->second.descriptor;
}

void VulkanEmbeddedRenderer::Impl::releaseBuffer(
    FrameBuffer& buffer) noexcept {
    if (buffer.mapped) {
        vkUnmapMemory(device_, buffer.memory);
    }
    if (buffer.buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device_, buffer.buffer, nullptr);
    }
    if (buffer.memory != VK_NULL_HANDLE) {
        vkFreeMemory(device_, buffer.memory, nullptr);
    }
    buffer = {};
}

void VulkanEmbeddedRenderer::Impl::release() noexcept {
    if (device_ == VK_NULL_HANDLE) {
        return;
    }
    vkDeviceWaitIdle(device_);
    for (auto& buffer : buffers_) {
        releaseBuffer(buffer);
    }
    buffers_.clear();
    for (auto& entry : textures_) {
        releaseGpuTexture(entry.second);
    }
    releaseGpuTexture(whiteTexture_);
    if (pipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, pipeline_, nullptr);
    }
    if (layout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device_, layout_, nullptr);
    }
    if (uploadPool_ != VK_NULL_HANDLE) {
        vkDestroyCommandPool(device_, uploadPool_, nullptr);
    }
    if (sampler_ != VK_NULL_HANDLE) {
        vkDestroySampler(device_, sampler_, nullptr);
    }
    if (texturePool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device_, texturePool_, nullptr);
    }
    if (textureLayout_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device_, textureLayout_, nullptr);
    }
    pipeline_ = VK_NULL_HANDLE;
    layout_ = VK_NULL_HANDLE;
    uploadPool_ = VK_NULL_HANDLE;
    sampler_ = VK_NULL_HANDLE;
    texturePool_ = VK_NULL_HANDLE;
    textureLayout_ = VK_NULL_HANDLE;
    queue_ = VK_NULL_HANDLE;
    queueFamily_ = 0;
    renderPass_ = VK_NULL_HANDLE;
    physicalDevice_ = VK_NULL_HANDLE;
    device_ = VK_NULL_HANDLE;
}

void VulkanEmbeddedRenderer::Impl::createPipeline(
    const VulkanEmbeddedFrame& frame) {
    const auto vertex = createShader(
        frame.device, generated::rectangleVertexSpirv,
        sizeof(generated::rectangleVertexSpirv));
    VkShaderModule fragment = VK_NULL_HANDLE;
    try {
        fragment = createShader(
            frame.device, generated::rectangleFragmentSpirv,
            sizeof(generated::rectangleFragmentSpirv));

        const VkPipelineShaderStageCreateInfo stages[] = {
            {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr,
             0, VK_SHADER_STAGE_VERTEX_BIT, vertex, "main", nullptr},
            {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr,
             0, VK_SHADER_STAGE_FRAGMENT_BIT, fragment, "main", nullptr},
        };
        const VkVertexInputBindingDescription binding{
            0, sizeof(RectangleInstance), VK_VERTEX_INPUT_RATE_INSTANCE};
        const std::array<VkVertexInputAttributeDescription, 4> attributes{{
            {0, 0, VK_FORMAT_R32G32B32A32_SFLOAT,
             static_cast<std::uint32_t>(offsetof(RectangleInstance, bounds))},
            {1, 0, VK_FORMAT_R32G32B32A32_SFLOAT,
             static_cast<std::uint32_t>(offsetof(RectangleInstance, color))},
            {2, 0, VK_FORMAT_R32G32B32A32_SFLOAT,
             static_cast<std::uint32_t>(offsetof(RectangleInstance, parameters))},
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

        VkPipelineInputAssemblyStateCreateInfo assembly{};
        assembly.sType =
            VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkPipelineViewportStateCreateInfo viewport{};
        viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewport.viewportCount = 1;
        viewport.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo raster{};
        raster.sType =
            VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        raster.polygonMode = VK_POLYGON_MODE_FILL;
        raster.cullMode = VK_CULL_MODE_NONE;
        raster.frontFace = VK_FRONT_FACE_CLOCKWISE;
        raster.lineWidth = 1.0F;

        VkPipelineMultisampleStateCreateInfo multisample{};
        multisample.sType =
            VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisample.rasterizationSamples = frame.samples;

        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType =
            VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = VK_FALSE;
        depthStencil.depthWriteEnable = VK_FALSE;
        depthStencil.stencilTestEnable = VK_FALSE;

        VkPipelineColorBlendAttachmentState attachment{};
        attachment.blendEnable = VK_TRUE;
        attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        attachment.dstColorBlendFactor =
            VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        attachment.colorBlendOp = VK_BLEND_OP_ADD;
        attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        attachment.dstAlphaBlendFactor =
            VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        attachment.alphaBlendOp = VK_BLEND_OP_ADD;
        attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
            VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
            VK_COLOR_COMPONENT_A_BIT;
        VkPipelineColorBlendStateCreateInfo blend{};
        blend.sType =
            VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        blend.attachmentCount = 1;
        blend.pAttachments = &attachment;

        const std::array<VkDynamicState, 2> states{
            VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamic{};
        dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamic.dynamicStateCount = static_cast<std::uint32_t>(states.size());
        dynamic.pDynamicStates = states.data();

        const VkPushConstantRange push{
            VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(float) * 2};
        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.setLayoutCount = 1;
        layoutInfo.pSetLayouts = &textureLayout_;
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges = &push;
        check(vkCreatePipelineLayout(
            device_, &layoutInfo, nullptr, &layout_),
            "failed to create LotUI embedded pipeline layout");

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = stages;
        pipelineInfo.pVertexInputState = &vertexInput;
        pipelineInfo.pInputAssemblyState = &assembly;
        pipelineInfo.pViewportState = &viewport;
        pipelineInfo.pRasterizationState = &raster;
        pipelineInfo.pMultisampleState = &multisample;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pColorBlendState = &blend;
        pipelineInfo.pDynamicState = &dynamic;
        pipelineInfo.layout = layout_;
        pipelineInfo.renderPass = frame.renderPass;
        check(vkCreateGraphicsPipelines(
            device_, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
            &pipeline_), "failed to create LotUI embedded pipeline");
    } catch (...) {
        if (fragment != VK_NULL_HANDLE) {
            vkDestroyShaderModule(device_, fragment, nullptr);
        }
        vkDestroyShaderModule(device_, vertex, nullptr);
        throw;
    }
    vkDestroyShaderModule(device_, fragment, nullptr);
    vkDestroyShaderModule(device_, vertex, nullptr);
    renderPass_ = frame.renderPass;
    samples_ = frame.samples;
}

void VulkanEmbeddedRenderer::Impl::ensureBuffer(
    FrameBuffer& buffer, std::size_t instanceCount) {
    if (buffer.capacity >= instanceCount) {
        return;
    }
    // Buffer growth is rare. The old buffer may still be referenced by a
    // submitted frame, so wait before replacing it.
    vkDeviceWaitIdle(device_);
    releaseBuffer(buffer);
    const std::size_t capacity = std::max(instanceCount, std::size_t{64});
    const VkDeviceSize byteCount = capacity * sizeof(RectangleInstance);
    VkBufferCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    info.size = byteCount;
    info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    check(vkCreateBuffer(device_, &info, nullptr, &buffer.buffer),
        "failed to create LotUI embedded instance buffer");

    VkMemoryRequirements requirements{};
    vkGetBufferMemoryRequirements(device_, buffer.buffer, &requirements);
    VkPhysicalDeviceMemoryProperties properties{};
    vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &properties);
    std::uint32_t memoryType = properties.memoryTypeCount;
    for (std::uint32_t i = 0; i < properties.memoryTypeCount; ++i) {
        const auto flags = properties.memoryTypes[i].propertyFlags;
        if ((requirements.memoryTypeBits & (1U << i)) != 0 &&
            (flags & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) ==
                (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                 VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
            memoryType = i;
            break;
        }
    }
    if (memoryType == properties.memoryTypeCount) {
        throw std::runtime_error(
            "LotUI embedded renderer requires coherent host-visible memory");
    }
    VkMemoryAllocateInfo allocation{};
    allocation.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocation.allocationSize = requirements.size;
    allocation.memoryTypeIndex = memoryType;
    check(vkAllocateMemory(device_, &allocation, nullptr, &buffer.memory),
        "failed to allocate LotUI embedded instance memory");
    check(vkBindBufferMemory(device_, buffer.buffer, buffer.memory, 0),
        "failed to bind LotUI embedded instance memory");
    check(vkMapMemory(device_, buffer.memory, 0, byteCount, 0, &buffer.mapped),
        "failed to map LotUI embedded instance memory");
    buffer.capacity = capacity;
}

void VulkanEmbeddedRenderer::Impl::draw(
    const VulkanEmbeddedFrame& frame,
    const std::vector<PaintCommand>& commands) {
    if (frame.physicalDevice == VK_NULL_HANDLE ||
        frame.device == VK_NULL_HANDLE ||
        frame.renderPass == VK_NULL_HANDLE ||
        frame.commandBuffer == VK_NULL_HANDLE ||
        frame.frameCount == 0 || frame.frameIndex >= frame.frameCount ||
        frame.framebufferWidth == 0 || frame.framebufferHeight == 0 ||
        !std::isfinite(frame.dpiScale) || frame.dpiScale <= 0.0F) {
        throw std::invalid_argument("invalid LotUI embedded Vulkan frame");
    }
    if (device_ != frame.device || physicalDevice_ != frame.physicalDevice ||
        (queue_ != VK_NULL_HANDLE && queue_ != frame.graphicsQueue) ||
        (uploadPool_ != VK_NULL_HANDLE &&
            queueFamily_ != frame.graphicsQueueFamily)) {
        release();
        device_ = frame.device;
        physicalDevice_ = frame.physicalDevice;
    }
    ensureTextureResources(frame);
    if (renderPass_ != frame.renderPass || samples_ != frame.samples ||
        pipeline_ == VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device_);
        if (pipeline_ != VK_NULL_HANDLE) {
            vkDestroyPipeline(device_, pipeline_, nullptr);
            pipeline_ = VK_NULL_HANDLE;
        }
        if (layout_ != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device_, layout_, nullptr);
            layout_ = VK_NULL_HANDLE;
        }
        createPipeline(frame);
    }
    if (buffers_.size() != frame.frameCount) {
        vkDeviceWaitIdle(device_);
        for (auto& buffer : buffers_) {
            releaseBuffer(buffer);
        }
        buffers_.resize(frame.frameCount);
    }

    const auto plan = buildPaintBatchPlan(
        commands, frame.dpiScale,
        frame.framebufferWidth, frame.framebufferHeight);
    if (plan.commandIndices.empty()) {
        return;
    }
    for (const auto index : plan.commandIndices) {
        const TextureId id = commands[index].texture;
        if (id == invalidTextureId) {
            continue;
        }
        const auto found = textures_.find(id);
        if (found == textures_.end()) {
            throw std::runtime_error(
                "LotUI embedded paint references an unknown texture");
        }
        if (found->second.gpuImage == VK_NULL_HANDLE) {
            createGpuTexture(found->second);
        }
        if (found->second.dirty) {
            uploadTexture(found->second);
        }
    }

    auto& buffer = buffers_[frame.frameIndex];
    ensureBuffer(buffer, plan.commandIndices.size());
    auto* instances = static_cast<RectangleInstance*>(buffer.mapped);
    for (std::size_t i = 0; i < plan.commandIndices.size(); ++i) {
        const auto& command = commands[plan.commandIndices[i]];
        const float scale = frame.dpiScale;
        const auto pixel = [scale, &command](float value) {
            return command.snapToPixel ? std::round(value * scale)
                                       : value * scale;
        };
        const float left = pixel(command.bounds.x);
        const float top = pixel(command.bounds.y);
        const float right = pixel(command.bounds.x + command.bounds.width);
        const float bottom = pixel(command.bounds.y + command.bounds.height);
        instances[i] = {{
            left, top, right - left, bottom - top},
            {command.color.red, command.color.green,
             command.color.blue, command.color.alpha},
            {command.cornerRadius * scale,
             command.texture == invalidTextureId ? 0.0F : 1.0F,
             0.0F, 0.0F},
            {command.textureCoordinates.x, command.textureCoordinates.y,
             command.textureCoordinates.width,
             command.textureCoordinates.height}};
    }

    const VkViewport viewport{
        0.0F, 0.0F,
        static_cast<float>(frame.framebufferWidth),
        static_cast<float>(frame.framebufferHeight), 0.0F, 1.0F};
    const float dimensions[]{
        static_cast<float>(frame.framebufferWidth),
        static_cast<float>(frame.framebufferHeight)};
    const VkDeviceSize offset = 0;
    vkCmdBindPipeline(frame.commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
    vkCmdSetViewport(frame.commandBuffer, 0, 1, &viewport);
    vkCmdPushConstants(frame.commandBuffer, layout_,
        VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(dimensions), dimensions);
    vkCmdBindVertexBuffers(frame.commandBuffer, 0, 1,
        &buffer.buffer, &offset);
    for (const auto& batch : plan.batches) {
        const VkDescriptorSet descriptor = descriptorFor(batch.texture);
        vkCmdBindDescriptorSets(frame.commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS, layout_, 0, 1,
            &descriptor, 0, nullptr);
        const VkRect2D scissor{
            {batch.scissor.x, batch.scissor.y},
            {batch.scissor.width, batch.scissor.height}};
        vkCmdSetScissor(frame.commandBuffer, 0, 1, &scissor);
        vkCmdDraw(frame.commandBuffer, 6, batch.instanceCount,
            0, batch.firstInstance);
    }
}

VulkanEmbeddedRenderer::VulkanEmbeddedRenderer()
    : impl_(std::make_unique<Impl>()) {
    textureReleaseState_ = makeTextureReleaseState(
        [this](TextureId id) { impl_->removeTexture(id); });
}

VulkanEmbeddedRenderer::~VulkanEmbeddedRenderer() {
    deactivateTextureReleaseState(textureReleaseState_);
}

void VulkanEmbeddedRenderer::draw(
    const VulkanEmbeddedFrame& frame,
    const std::vector<PaintCommand>& commands) {
    impl_->draw(frame, commands);
}

Texture VulkanEmbeddedRenderer::createTexture(const TextureImage& image) {
    return adoptTexture(impl_->createTexture(image), textureReleaseState_);
}

void VulkanEmbeddedRenderer::updateTexture(
    const Texture& texture, const TextureUpdate& update) {
    if (!ownsTexture(texture, textureReleaseState_)) {
        throw std::invalid_argument(
            "texture does not belong to this LotUI embedded renderer");
    }
    impl_->updateTexture(texture.id(), update);
}

} // namespace lotui
