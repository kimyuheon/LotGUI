# Inline text editing

`InlineTextEditor` places the existing `TextField` over an exact retained-view
cell rectangle. It is not coupled to `ListControl`, so TreeView, PropertyGrid,
and application-specific views can reuse the same editor and validation
contract.

```cpp
if (event.action == lotui::ListCellAction::BeginEdit) {
    const lotui::ListCell* cell = list.cell(event.address);

    lotui::InlineTextEditHandlers handlers;
    handlers.committed = [&list, address = event.address](std::string text) {
        list.setCellText(address, std::move(text));
    };
    handlers.validate = [](std::string_view text)
        -> std::optional<std::string> {
        return text.empty()
            ? std::optional<std::string>{"A value is required"}
            : std::nullopt;
    };
    handlers.validationFailed = [](std::string message) {
        showValidationMessage(message);
    };

    lotui::InlineTextEditor::showAt(
        popupHost,
        textEngine,
        event.anchor,
        cell->text,
        std::move(handlers));
}
```

Enter validates and commits. A validation error keeps the editor open and
changes its focus ring to the configured invalid color. Escape cancels without
changing the model. By default, clicking outside commits a valid value; set
`InlineTextEditorOptions::commitOnFocusLoss` to `false` to cancel instead.
If a focus-loss value is invalid, the editor closes, calls `validationFailed`,
and reports cancellation because focus has already moved elsewhere.

The editor receives focus immediately after it is opened. Korean and other IME
composition events therefore follow the same native Windows, macOS, and Linux
path already used by standalone `TextField`. The cell model changes only after
the IME commits text and the editor itself is committed.

For `ListControl`, editable text cells request `BeginEdit` with Enter or a
double-click. A single click selects the cell without entering edit mode.
