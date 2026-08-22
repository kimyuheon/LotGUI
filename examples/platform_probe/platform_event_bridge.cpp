#include "examples/platform_probe/platform_event_bridge.h"

#include "examples/platform_probe/demo_ui.h"

#include <iomanip>
#include <iostream>

namespace lotui::example {
namespace {

void applyPointerUpdate(
    const WidgetPointerUpdate& update,
    WidgetTree& tree,
    PlatformWindow& window) {
    if (update.captureStarted && !window.setPointerCapture(true)) {
        tree.cancelPointer();
        std::cerr << "pointer-capture failed\n";
    }
    if (update.captureEnded) {
        window.setPointerCapture(false);
    }
}

void printEvent(const PlatformEvent& event) {
    std::cout << "event: " << eventTypeName(event.type);
    switch (event.type) {
    case PlatformEventType::Resized:
        std::cout << " width=" << event.width << " height=" << event.height;
        break;
    case PlatformEventType::MouseMoved:
        std::cout << " x=" << event.x << " y=" << event.y;
        break;
    case PlatformEventType::MouseButtonPressed:
    case PlatformEventType::MouseButtonReleased:
        std::cout << " x=" << event.x << " y=" << event.y
                  << " button=" << pointerButtonName(event.button);
        break;
    case PlatformEventType::KeyPressed:
    case PlatformEventType::KeyReleased:
        std::cout << " key=" << keyCodeName(event.key)
                  << " shift=" << event.modifiers.shift
                  << " control=" << event.modifiers.control
                  << " alt=" << event.modifiers.alt
                  << " meta=" << event.modifiers.meta
                  << " repeat=" << std::boolalpha << event.repeat;
        break;
    case PlatformEventType::DpiChanged:
        std::cout << " scale=" << std::fixed << std::setprecision(2)
                  << event.dpiScale;
        break;
    case PlatformEventType::TextInput:
    case PlatformEventType::TextComposition:
        std::cout << " text=" << event.text
                  << " selection=" << event.selectionStart
                  << '+' << event.selectionLength;
        break;
    case PlatformEventType::CloseRequested:
    case PlatformEventType::PointerCaptureLost:
    case PlatformEventType::TextCompositionEnd:
    case PlatformEventType::FocusGained:
    case PlatformEventType::FocusLost:
        break;
    }
    std::cout << '\n';
}

} // namespace

bool dispatchPlatformEvent(
    const PlatformEvent& event,
    WidgetTree& tree,
    PlatformWindow& window) {
    printEvent(event);
    if (event.type == PlatformEventType::Resized ||
        event.type == PlatformEventType::DpiChanged) {
        updateDemoLayout(tree, window.metrics());
    }

    if (event.type == PlatformEventType::KeyPressed) {
        tree.keyPressed(event.key, event.modifiers, event.repeat);
    } else if (event.type == PlatformEventType::KeyReleased) {
        tree.keyReleased(event.key, event.modifiers);
    } else if (event.type == PlatformEventType::TextInput) {
        tree.textInput({
            TextInputEventType::Commit,
            event.text,
            event.selectionStart,
            event.selectionLength});
    } else if (event.type == PlatformEventType::TextComposition) {
        tree.textInput({
            TextInputEventType::Composition,
            event.text,
            event.selectionStart,
            event.selectionLength});
    } else if (event.type == PlatformEventType::TextCompositionEnd) {
        tree.textInput({TextInputEventType::CompositionEnd});
    } else if (event.type == PlatformEventType::FocusLost) {
        tree.cancelKeyboard();
    }

    const Point position{event.x, event.y};
    WidgetPointerUpdate pointerUpdate;
    if (event.type == PlatformEventType::MouseMoved) {
        pointerUpdate = tree.pointerMoved(position);
    } else if (event.type == PlatformEventType::MouseButtonPressed &&
        event.button == PointerButton::Primary) {
        pointerUpdate = tree.pointerPressed(position, event.button);
    } else if (event.type == PlatformEventType::MouseButtonReleased &&
        event.button == PointerButton::Primary) {
        pointerUpdate = tree.pointerReleased(position, event.button);
    } else if (event.type == PlatformEventType::PointerCaptureLost ||
        event.type == PlatformEventType::FocusLost) {
        pointerUpdate = tree.cancelPointer();
    }
    applyPointerUpdate(pointerUpdate, tree, window);
    window.setTextInputState(
        event.type == PlatformEventType::FocusLost
            ? TextInputState{}
            : tree.textInputState());
    return event.type != PlatformEventType::CloseRequested;
}

} // namespace lotui::example
