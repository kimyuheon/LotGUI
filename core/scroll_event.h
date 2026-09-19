#pragma once

#include "core/geometry.h"

#include <cstdint>

namespace lotui {

enum class ScrollDeltaMode : std::uint8_t {
    Pixel,
    Line,
};

struct WidgetScrollEvent {
    Point position{};
    Point delta{};
    ScrollDeltaMode mode{ScrollDeltaMode::Pixel};
};

} // namespace lotui
