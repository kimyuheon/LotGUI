#include "text/text_layout.h"

#include <cmath>

namespace lotui {

bool isValidTextStyle(const TextStyle& style) noexcept {
    if (style.fontFamilies.empty() ||
        !std::isfinite(style.fontSize) || style.fontSize <= 0.0F ||
        !std::isfinite(style.letterSpacing) ||
        !std::isfinite(style.lineHeight) || style.lineHeight < 0.0F) {
        return false;
    }
    for (const std::string& family : style.fontFamilies) {
        if (family.empty()) {
            return false;
        }
    }
    const auto weight = static_cast<std::uint16_t>(style.weight);
    return weight >= 100 && weight <= 900 && weight % 100 == 0;
}

bool isValidTextLayoutOptions(
    const TextLayoutOptions& options) noexcept {
    return (std::isfinite(options.maximumWidth) ||
            options.maximumWidth == unboundedLayoutSize) &&
        options.maximumWidth >= 0.0F;
}

} // namespace lotui
