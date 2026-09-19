# LookupBox

`LookupBox` is a renderer-neutral searchable picker for forms and table cells.
Each `LookupItem` has a stable application ID, a visible label, and optional
secondary text. The widget does not depend on a database, network protocol, or
Vulkan type.

```cpp
std::vector<lotui::LookupItem> suppliers{
    {"supplier-1", "Northwind", "Seattle"},
    {"supplier-2", "Contoso", "London"},
    {"supplier-3", "부산 공급사", "부산, 대한민국"},
};

auto lookup = std::make_unique<lotui::LookupBox>(
    textEngine,
    suppliers,
    std::nullopt,
    lotui::Size{240.0F, 32.0F},
    [&suppliers](std::optional<std::size_t> index) {
        if (index) {
            saveSupplierId(suppliers[*index].id);
        } else {
            clearSupplier();
        }
    });

lotui::LookupBox* lookupPointer = lookup.get();
content->addChild(std::move(lookup));
auto host = std::make_unique<lotui::PopupHost>(std::move(content));
lookupPointer->setPopupHost(host.get());
```

Pointer click, Enter, or Space opens the popup and focuses its `TextField`.
Typing filters label, secondary text, and ID. Up and Down move the highlighted
result even while focus remains in the search field, and Enter selects it.
ASCII matching ignores case;
non-ASCII UTF-8 text such as Korean is matched without altering its bytes, so
the platform IME composition and commit path remains the existing `TextField`
path. Enter commits the highlighted result. Delete or Backspace clears a
closed standalone lookup. Escape and outside click dismiss the popup.

## ListControl integration

`ListCellAction::OpenLookup` supplies the full cell as `event.anchor`.
Applications can map their domain objects to `LookupItem` and update the cell
after a selection:

```cpp
if (event.action == lotui::ListCellAction::OpenLookup) {
    lotui::LookupBox::showPopupAt(
        popupHost,
        textEngine,
        event.anchor,
        suppliers,
        std::nullopt,
        [&list, &suppliers, address = event.address](
            std::optional<std::size_t> index) {
            if (index) {
                list.setCellText(address, suppliers[*index].label);
            }
        });
}
```

For remote lookup sources, update an application-owned item vector before
opening the popup. A future provider API will add loading, cancellation, and
incremental results without changing the selected item contract.
