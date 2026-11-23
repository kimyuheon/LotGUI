#pragma once

#include "core/color.h"

#include <cstdint>

namespace lotui::example {

// Author the sample palette in display sRGB and convert to the renderer's
// linear color values for an sRGB swapchain.
inline Color desktopColor(std::uint32_t rgb, float alpha = 1.0F) {
    return colorFromSrgb(rgb, alpha);
}

} // namespace lotui::example
