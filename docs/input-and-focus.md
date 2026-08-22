# LotUI input and focus

LotUI normalizes native input before it reaches widgets. Core and widget code
must never compare Win32 virtual keys, macOS hardware key codes, or X11
keysyms.

## Keyboard flow

1. The active platform backend converts the native key to `KeyCode` and
   records Shift, Control, Alt, and Meta in `KeyModifiers`.
2. The application forwards `PlatformEventType::KeyPressed` and
   `PlatformEventType::KeyReleased` to `WidgetTree`.
3. `WidgetTree` handles unmodified Tab and Shift+Tab as focus traversal.
4. Other key events are delivered only to the focused widget.
5. A window focus loss calls `WidgetTree::cancelKeyboard()` so a control
   cannot remain visually pressed after an interrupted key sequence.

Text entry is intentionally separate from key events. `TextField` receives
committed UTF-8 and composition/pre-edit updates from the platform backend
instead of attempting to derive characters from `KeyCode`.

## Text input flow

1. A focused text widget exposes a logical caret rectangle through
   `WidgetTree::textInputState()`.
2. The application forwards that state to `PlatformWindow::setTextInputState()`.
3. The native backend enables its text-input service and emits `TextInput`,
   `TextComposition`, and `TextCompositionEnd` platform events.
4. The application converts them to `TextInputEvent` and forwards them to the
   tree. Selection offsets in these events are UTF-8 byte offsets.
5. Windows and macOS render the live pre-edit run inside `TextField`; X11 uses
   the input method's native pre-edit UI and forwards committed UTF-8.

The state should be refreshed after layout and painting so the operating
system's IME candidate window follows the current caret.

## Focus order

Focusable widgets are collected in retained widget-tree order. Disabled
widgets are omitted. The current implementation wraps at either end:

- Tab moves forward;
- Shift+Tab moves backward;
- a primary pointer press focuses the hit focusable widget;
- removing or disabling the focused widget clears it on the next tree sync.

`FocusManager` contains only target ordering and selection state. WidgetTree is
responsible for dispatching focus gained/lost notifications and repainting.
This keeps platform handles out of the focus model.

## Button behavior

A focused enabled button responds to Enter and Space. The press changes the
visual state, and the matching release invokes the same callback used by a
pointer click. Repeated key-down events do not invoke the callback repeatedly.
Focus loss or keyboard cancellation clears the pressed state without clicking.
