#pragma once

#include "core/paint_command.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace lotui {

struct PixelScissor {
    std::int32_t x{0};
    std::int32_t y{0};
    std::uint32_t width{0};
    std::uint32_t height{0};
};

struct PaintBatch {
    PixelScissor scissor{};
    TextureId texture{invalidTextureId};
    std::uint32_t firstInstance{0};
    std::uint32_t instanceCount{0};
};

struct PaintBatchPlan {
    std::vector<std::size_t> commandIndices;
    std::vector<PaintBatch> batches;
};

PaintBatchPlan buildPaintBatchPlan(
    const std::vector<PaintCommand>& paintCommands,
    float dpiScale,
    std::uint32_t framebufferWidth,
    std::uint32_t framebufferHeight);

} // namespace lotui
