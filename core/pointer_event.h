#pragma once

#include <cstdint>

namespace lotui {

enum class PointerButton : std::uint8_t {
    Unspecified,
    Primary,
    Secondary,
    Middle,
    Auxiliary1,
    Auxiliary2,
};

constexpr const char* pointerButtonName(PointerButton button) noexcept {
    switch (button) {
    case PointerButton::Unspecified:
        return "none";
    case PointerButton::Primary:
        return "primary";
    case PointerButton::Secondary:
        return "secondary";
    case PointerButton::Middle:
        return "middle";
    case PointerButton::Auxiliary1:
        return "auxiliary-1";
    case PointerButton::Auxiliary2:
        return "auxiliary-2";
    }
    return "unknown";
}

} // namespace lotui
