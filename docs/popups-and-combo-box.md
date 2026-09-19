# Popups and ComboBox

`PopupHost` is the root overlay service for non-modal transient UI. It keeps
application widgets renderer-neutral while providing one place for anchored
placement, hit testing, focus scope changes, and safe deferred destruction.

Wrap the normal root widget once:

```cpp
auto content = std::make_unique<lotui::Column>();
auto combo = std::make_unique<lotui::ComboBox>(
    textEngine,
    std::vector<std::string>{"Small", "Medium", "Large"},
    0,
    lotui::Size{180.0F, 32.0F},
    [](std::size_t index) {
        // Store the selected value in the application model.
    });
lotui::ComboBox* comboPointer = combo.get();
content->addChild(std::move(combo));

auto host = std::make_unique<lotui::PopupHost>(std::move(content));
comboPointer->setPopupHost(host.get());
lotui::WidgetTree tree(std::move(host));
```

The combo opens by pointer, Enter, or Space. A closed combo also changes its
selection with Up and Down. The open list supports Up, Down, Home, End, Enter,
and Space. Escape and an outside primary press dismiss it without activating
the covered application content.

`PopupPlacement::Auto` opens below the anchor when possible and flips above
near the bottom edge. Explicit start/end and above/below placements are also
available. Popup bounds are clamped to the host, and the default width is at
least the anchor width.

`PopupPlacement::Anchor` overlays the anchor instead of opening beside it.
Together with `exactAnchorWidth` and `exactAnchorHeight`, this is used by the
inline text editor to cover a retained cell precisely.

## ListControl cells

`ComboBox::showPopupAt` exposes the same popup implementation for a retained
cell or another custom control. `ListCellEvent::anchor` covers the full combo
cell, so the list popup aligns with the column rather than only the arrow.

```cpp
if (event.action == lotui::ListCellAction::OpenComboBox) {
    const lotui::ListCell* cell = list.cell(event.address);
    lotui::ComboBox::showPopupAt(
        popupHost,
        textEngine,
        event.anchor,
        cell->options,
        cell->selectedOption,
        [&list, address = event.address](std::size_t index) {
            list.setComboSelection(address, index);
        });
}
```

Only one transient popup is active in a host. Opening another closes the old
one with `PopupCloseReason::Replaced`. Modal dialogs remain a separate concern
and can wrap or be wrapped by a popup host depending on the application's
desired overlay order.
