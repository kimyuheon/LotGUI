#pragma once

#include "core/color.h"
#include "core/geometry.h"

#include <cstdint>

namespace lotui {

using TextureId = std::uint32_t;
inline constexpr TextureId invalidTextureId = 0;

struct PaintCommand {
    Rect bounds{};
    Rect clip{};
    Color color{};
    TextureId texture{invalidTextureId};
    float cornerRadius{0.0F};
    Rect textureCoordinates{0.0F, 0.0F, 1.0F, 1.0F};
};

} // namespace lotui
