#pragma once

#include "core/paint_command.h"

#include <functional>

namespace lotui::detail {

struct TextureReleaseState {
    bool active{true};
    std::function<void(TextureId)> release;
};

} // namespace lotui::detail
