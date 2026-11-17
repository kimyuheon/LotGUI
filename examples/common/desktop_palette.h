#pragma once

#include "core/color.h"

#include <cmath>
#include <cstdint>

namespace lotui::example {

// Author the sample palette in display sRGB and convert to the renderer's
// linear color values for an sRGB swapchain.
inline Color desktopColor(std::uint32_t rgb, float alpha = 1.0F) {
    const auto channel = [](std::uint32_t value) {
        const float srgb = static_cast<float>(value & 0xffu) / 255.0F;
        return srgb <= 0.04045F ? srgb / 12.92F
            : std::pow((srgb + 0.055F) / 1.055F, 2.4F);
    };
    return {channel(rgb >> 16), channel(rgb >> 8), channel(rgb), alpha};
}

} // namespace lotui::example
