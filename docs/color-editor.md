# ColorEditor

`ColorEditor` edits normalized RGB values in a compact retained widget. It
shows a live swatch and 0–255 channel values. Pointer dragging changes a
channel continuously; Tab focuses the editor, Up/Down selects a channel, and
Left/Right, PageUp/PageDown, Home/End change its value. Alpha is preserved.

```cpp
auto editor = std::make_unique<lotui::ColorEditor>(textEngine,
    lotui::Color{0.8F, 0.3F, 0.2F, 1.0F});
editor->setOnEditStarted([&] { beginUndoTransaction(); });
editor->setOnChanged([&](lotui::Color color) {
    updateSelectedObjectColor(color.red, color.green, color.blue);
});
editor->setOnEditFinished([&] { endUndoTransaction(); });
```

The callbacks are independent of Vulkan, the operating system and the CAD
engine. A host or plugin may bind them to its own color model and undo system.
`setColor` refreshes values from the model without invoking `onChanged`.
`setEnabled(false)` prevents user edits, such as when an object's color comes
from its layer. This first control exposes RGB channel sliders; a palette popup
and hex entry remain separate work.

Run `color_editor_demo` for object, layer and plugin-style examples. Use
`color_editor_demo --capture output.ppm` to save its Vulkan frame.
