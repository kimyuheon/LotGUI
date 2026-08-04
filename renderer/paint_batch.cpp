#include "renderer/paint_batch.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace lotui {
namespace {

bool intersects(const Rect& left, const Rect& right) noexcept {
    if (!lotui::hasArea(left) || !lotui::hasArea(right)) {
        return false;
    }

    return left.x < right.x + right.width &&
        right.x < left.x + left.width &&
        left.y < right.y + right.height &&
        right.y < left.y + left.height;
}

bool sameScissor(
    const PixelScissor& left,
    const PixelScissor& right) noexcept {
    return left.x == right.x && left.y == right.y &&
        left.width == right.width && left.height == right.height;
}

PixelScissor makeScissor(
    const Rect& clip,
    float dpiScale,
    std::uint32_t framebufferWidth,
    std::uint32_t framebufferHeight) {
    const double scale = static_cast<double>(dpiScale);
    const double left = std::clamp(
        static_cast<double>(clip.x) * scale,
        0.0,
        static_cast<double>(framebufferWidth));
    const double top = std::clamp(
        static_cast<double>(clip.y) * scale,
        0.0,
        static_cast<double>(framebufferHeight));
    const double right = std::clamp(
        static_cast<double>(clip.x + clip.width) * scale,
        0.0,
        static_cast<double>(framebufferWidth));
    const double bottom = std::clamp(
        static_cast<double>(clip.y + clip.height) * scale,
        0.0,
        static_cast<double>(framebufferHeight));

    const auto pixelLeft = static_cast<std::uint32_t>(std::floor(left));
    const auto pixelTop = static_cast<std::uint32_t>(std::floor(top));
    const auto pixelRight = static_cast<std::uint32_t>(std::ceil(right));
    const auto pixelBottom = static_cast<std::uint32_t>(std::ceil(bottom));

    return {
        static_cast<std::int32_t>(pixelLeft),
        static_cast<std::int32_t>(pixelTop),
        pixelRight > pixelLeft ? pixelRight - pixelLeft : 0,
        pixelBottom > pixelTop ? pixelBottom - pixelTop : 0,
    };
}

} // namespace

PaintBatchPlan buildPaintBatchPlan(
    const std::vector<PaintCommand>& paintCommands,
    float dpiScale,
    std::uint32_t framebufferWidth,
    std::uint32_t framebufferHeight) {
    if (!std::isfinite(dpiScale) || dpiScale <= 0.0F) {
        throw std::invalid_argument("dpiScale must be finite and positive");
    }
    if (framebufferWidth >
            static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max()) ||
        framebufferHeight >
            static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max())) {
        throw std::invalid_argument("framebuffer dimensions exceed scissor limits");
    }

    PaintBatchPlan plan;
    plan.commandIndices.reserve(paintCommands.size());
    plan.batches.reserve(paintCommands.size());

    for (std::size_t commandIndex = 0;
         commandIndex < paintCommands.size();
         ++commandIndex) {
        const PaintCommand& command = paintCommands[commandIndex];
        if (!intersects(command.bounds, command.clip)) {
            continue;
        }

        const PixelScissor scissor = makeScissor(
            command.clip,
            dpiScale,
            framebufferWidth,
            framebufferHeight);
        if (scissor.width == 0 || scissor.height == 0) {
            continue;
        }

        const auto firstInstance = static_cast<std::uint32_t>(
            plan.commandIndices.size());
        plan.commandIndices.push_back(commandIndex);

        if (!plan.batches.empty() &&
            plan.batches.back().texture == command.texture &&
            sameScissor(plan.batches.back().scissor, scissor)) {
            ++plan.batches.back().instanceCount;
            continue;
        }

        plan.batches.push_back({
            scissor,
            command.texture,
            firstInstance,
            1,
        });
    }

    return plan;
}

} // namespace lotui
