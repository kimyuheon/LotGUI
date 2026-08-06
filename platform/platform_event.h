#pragma once

#include "core/key_event.h"
#include "core/pointer_event.h"

#include <cstdint>

namespace lotui {

enum class PlatformEventType {
    CloseRequested,
    Resized,
    MouseMoved,
    MouseButtonPressed,
    MouseButtonReleased,
    PointerCaptureLost,
    KeyPressed,
    KeyReleased,
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
    float dpiScale{1.0F};
    PointerButton button{PointerButton::Unspecified};
    KeyCode key{KeyCode::Unknown};
    KeyModifiers modifiers{};
    bool repeat{false};
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
    case PlatformEventType::PointerCaptureLost:
        return "pointer-capture-lost";
    case PlatformEventType::KeyPressed:
        return "key-pressed";
    case PlatformEventType::KeyReleased:
        return "key-released";
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
