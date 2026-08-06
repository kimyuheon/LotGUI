#pragma once

#include <cstdint>

namespace lotui {

enum class KeyCode : std::uint16_t {
    Unknown,
    Tab,
    Enter,
    Space,
    Escape,
    Backspace,
    Delete,
    Left,
    Right,
    Up,
    Down,
    Home,
    End,
    PageUp,
    PageDown,
};

struct KeyModifiers {
    bool shift{false};
    bool control{false};
    bool alt{false};
    bool meta{false};
};

enum class WidgetKeyEventType : std::uint8_t {
    Press,
    Release,
    Cancel,
};

struct WidgetKeyEvent {
    WidgetKeyEventType type{WidgetKeyEventType::Press};
    KeyCode key{KeyCode::Unknown};
    KeyModifiers modifiers{};
    bool repeat{false};
};

constexpr const char* keyCodeName(KeyCode key) noexcept {
    switch (key) {
    case KeyCode::Unknown: return "unknown";
    case KeyCode::Tab: return "tab";
    case KeyCode::Enter: return "enter";
    case KeyCode::Space: return "space";
    case KeyCode::Escape: return "escape";
    case KeyCode::Backspace: return "backspace";
    case KeyCode::Delete: return "delete";
    case KeyCode::Left: return "left";
    case KeyCode::Right: return "right";
    case KeyCode::Up: return "up";
    case KeyCode::Down: return "down";
    case KeyCode::Home: return "home";
    case KeyCode::End: return "end";
    case KeyCode::PageUp: return "page-up";
    case KeyCode::PageDown: return "page-down";
    }
    return "unknown";
}

} // namespace lotui
