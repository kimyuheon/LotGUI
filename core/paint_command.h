#pragma once

#include "core/color.h"
#include "core/geometry.h"

#include <cstdint>
#include <algorithm>
#include <cmath>

namespace lotui {

using TextureId = std::uint32_t;
inline constexpr TextureId invalidTextureId = 0;

enum class PaintPrimitive : std::uint8_t { Rectangle, Line };

struct PaintCommand {
    Rect bounds{};
    Rect clip{};
    Color color{};
    TextureId texture{invalidTextureId};
    float cornerRadius{0.0F};
    Rect textureCoordinates{0.0F, 0.0F, 1.0F, 1.0F};
    bool snapToPixel{false};
    PaintPrimitive primitive{PaintPrimitive::Rectangle};
    Point lineStart{};
    Point lineEnd{};
    float lineWidth{1.0F};
};

inline PaintCommand linePaintCommand(Point start, Point end, float width,
    Rect clip, Color color) noexcept {
    PaintCommand command{};
    command.primitive = PaintPrimitive::Line;
    command.lineStart = start;
    command.lineEnd = end;
    command.lineWidth = width;
    command.clip = clip;
    command.color = color;
    if (std::isfinite(start.x) && std::isfinite(start.y) &&
        std::isfinite(end.x) && std::isfinite(end.y) &&
        std::isfinite(width) && width > 0.0F) {
        const float half = width * 0.5F + 1.0F;
        command.bounds = {std::min(start.x, end.x) - half,
            std::min(start.y, end.y) - half,
            std::abs(end.x - start.x) + 2.0F * half,
            std::abs(end.y - start.y) + 2.0F * half};
    }
    return command;
}

} // namespace lotui
