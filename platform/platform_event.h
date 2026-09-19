#pragma once

#include "core/key_event.h"
#include "core/pointer_event.h"
#include "core/scroll_event.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace lotui {

enum class PlatformEventType {
    CloseRequested,
    Resized,
    MouseMoved,
    MouseButtonPressed,
    MouseButtonReleased,
    MouseWheel,
    PointerCaptureLost,
    KeyPressed,
    KeyReleased,
    TextInput,
    TextComposition,
    TextCompositionEnd,
    FocusGained,
    FocusLost,
    DpiChanged,
};

struct PlatformEvent {
    PlatformEventType type{PlatformEventType::CloseRequested};
    int width{0};
    int height{0};
    float x{0.0F};
    float y{0.0F};
    float scrollX{0.0F};
    float scrollY{0.0F};
    ScrollDeltaMode scrollMode{ScrollDeltaMode::Pixel};
    float dpiScale{1.0F};
    PointerButton button{PointerButton::Unspecified};
    KeyCode key{KeyCode::Unknown};
    KeyModifiers modifiers{};
    bool repeat{false};
    std::string text;
    std::size_t selectionStart{0};
    std::size_t selectionLength{0};
};

constexpr const char* eventTypeName(PlatformEventType type) noexcept {
    switch (type) {
    case PlatformEventType::CloseRequested:
        return "close-requested";
    case PlatformEventType::Resized:
        return "resized";
    case PlatformEventType::MouseMoved:
        return "mouse-moved";
    case PlatformEventType::MouseButtonPressed:
        return "mouse-button-pressed";
    case PlatformEventType::MouseButtonReleased:
        return "mouse-button-released";
    case PlatformEventType::MouseWheel:
        return "mouse-wheel";
    case PlatformEventType::PointerCaptureLost:
        return "pointer-capture-lost";
    case PlatformEventType::KeyPressed:
        return "key-pressed";
    case PlatformEventType::KeyReleased:
        return "key-released";
    case PlatformEventType::TextInput:
        return "text-input";
    case PlatformEventType::TextComposition:
        return "text-composition";
    case PlatformEventType::TextCompositionEnd:
        return "text-composition-end";
    case PlatformEventType::FocusGained:
        return "focus-gained";
    case PlatformEventType::FocusLost:
        return "focus-lost";
    case PlatformEventType::DpiChanged:
        return "dpi-changed";
    }
    return "unknown";
}

} // namespace lotui
