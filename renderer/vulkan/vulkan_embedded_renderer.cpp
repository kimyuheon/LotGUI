#include "renderer/vulkan/vulkan_embedded_renderer.h"

#include "renderer/paint_batch.h"

#include "lotui/rectangle_solid_fragment_spirv.h"
#include "lotui/rectangle_solid_vertex_spirv.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace lotui {
namespace {

struct RectangleInstance {
    float bounds[4];
    float color[4];
    float parameters[4];
};

struct FrameBuffer {
    VkBuffer buffer{VK_NULL_HANDLE};
    VkDeviceMemory memory{VK_NULL_HANDLE};
    void* mapped{nullptr};
    std::size_t capacity{0};
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

private:
    void release() noexcept;
    void releaseBuffer(FrameBuffer& buffer) noexcept;
    void createPipeline(const VulkanEmbeddedFrame& frame);
    void ensureBuffer(FrameBuffer& buffer, std::size_t instanceCount);

    VkPhysicalDevice physicalDevice_{VK_NULL_HANDLE};
    VkDevice device_{VK_NULL_HANDLE};
    VkRenderPass renderPass_{VK_NULL_HANDLE};
    VkSampleCountFlagBits samples_{VK_SAMPLE_COUNT_1_BIT};
    VkPipelineLayout layout_{VK_NULL_HANDLE};
    VkPipeline pipeline_{VK_NULL_HANDLE};
    std::vector<FrameBuffer> buffers_;
};

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
    if (pipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, pipeline_, nullptr);
    }
    if (layout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device_, layout_, nullptr);
    }
    pipeline_ = VK_NULL_HANDLE;
    layout_ = VK_NULL_HANDLE;
    renderPass_ = VK_NULL_HANDLE;
    physicalDevice_ = VK_NULL_HANDLE;
    device_ = VK_NULL_HANDLE;
}

void VulkanEmbeddedRenderer::Impl::createPipeline(
    const VulkanEmbeddedFrame& frame) {
    const auto vertex = createShader(
        frame.device, generated::rectangleSolidVertexSpirv,
        sizeof(generated::rectangleSolidVertexSpirv));
    VkShaderModule fragment = VK_NULL_HANDLE;
    try {
        fragment = createShader(
            frame.device, generated::rectangleSolidFragmentSpirv,
            sizeof(generated::rectangleSolidFragmentSpirv));

        const VkPipelineShaderStageCreateInfo stages[] = {
            {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr,
             0, VK_SHADER_STAGE_VERTEX_BIT, vertex, "main", nullptr},
            {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr,
             0, VK_SHADER_STAGE_FRAGMENT_BIT, fragment, "main", nullptr},
        };
        const VkVertexInputBindingDescription binding{
            0, sizeof(RectangleInstance), VK_VERTEX_INPUT_RATE_INSTANCE};
        const std::array<VkVertexInputAttributeDescription, 3> attributes{{
            {0, 0, VK_FORMAT_R32G32B32A32_SFLOAT,
             static_cast<std::uint32_t>(offsetof(RectangleInstance, bounds))},
            {1, 0, VK_FORMAT_R32G32B32A32_SFLOAT,
             static_cast<std::uint32_t>(offsetof(RectangleInstance, color))},
            {2, 0, VK_FORMAT_R32G32B32A32_SFLOAT,
             static_cast<std::uint32_t>(offsetof(RectangleInstance, parameters))},
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
    if (device_ != frame.device || physicalDevice_ != frame.physicalDevice) {
        release();
        device_ = frame.device;
        physicalDevice_ = frame.physicalDevice;
    }
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
        if (commands[index].texture != invalidTextureId) {
            throw std::runtime_error(
                "textured PaintCommand is not supported by the embedded renderer yet");
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
            {command.cornerRadius * scale, 0.0F, 0.0F, 0.0F}};
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
        const VkRect2D scissor{
            {batch.scissor.x, batch.scissor.y},
            {batch.scissor.width, batch.scissor.height}};
        vkCmdSetScissor(frame.commandBuffer, 0, 1, &scissor);
        vkCmdDraw(frame.commandBuffer, 6, batch.instanceCount,
            0, batch.firstInstance);
    }
}

VulkanEmbeddedRenderer::VulkanEmbeddedRenderer()
    : impl_(std::make_unique<Impl>()) {}

VulkanEmbeddedRenderer::~VulkanEmbeddedRenderer() = default;

void VulkanEmbeddedRenderer::draw(
    const VulkanEmbeddedFrame& frame,
    const std::vector<PaintCommand>& commands) {
    impl_->draw(frame, commands);
}

} // namespace lotui
