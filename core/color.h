#pragma once

#include <cmath>
#include <cstdint>

namespace lotui {

struct Color {
    float red{0.0F};
    float green{0.0F};
    float blue{0.0F};
    float alpha{1.0F};
};

// Convert a display sRGB color to linear values for an sRGB render target.
inline Color colorFromSrgb(std::uint32_t rgb, float alpha = 1.0F) noexcept {
    const auto channel = [](std::uint32_t value) noexcept {
        const float srgb = static_cast<float>(value & 0xffu) / 255.0F;
        return srgb <= 0.04045F ? srgb / 12.92F
            : std::pow((srgb + 0.055F) / 1.055F, 2.4F);
    };
    return {channel(rgb >> 16), channel(rgb >> 8), channel(rgb), alpha};
}

} // namespace lotui
