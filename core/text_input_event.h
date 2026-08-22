#pragma once

#include "core/geometry.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace lotui {

enum class TextInputEventType : std::uint8_t {
    Commit,
    Composition,
    CompositionEnd,
};

struct TextInputEvent {
    TextInputEventType type{TextInputEventType::Commit};
    std::string text;
    // UTF-8 byte offsets inside `text`, never native UTF-16 indices.
    std::size_t selectionStart{0};
    std::size_t selectionLength{0};
};

struct TextInputState {
    bool enabled{false};
    // Logical window coordinates used for the native IME candidate window.
    Rect inputRect{};
};

} // namespace lotui
