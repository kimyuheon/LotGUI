# ListControl

`ListControl` is LotUI's retained, virtualized table widget for ordinary C++
desktop applications. It is not tied to VulkanCAD or to any application model.
Applications provide columns and rows containing backend-neutral values, and
receive selection and cell-action callbacks.

The initial cell kinds are:

- `Text`: display text and request inline editing with Enter or double-click.
- `CheckBox`: toggle from its cell control or Space.
- `ComboBox`: open the shared anchored option popup through `PopupHost`.
- `Lookup`: open the shared searchable picker through `LookupBox`.
- `ActionButton`: invoke the button shown at the right edge of the cell.

```cpp
std::vector<lotui::ListColumn> columns{
    {"name", "Name", 180.0F},
    {"category", "Category", 140.0F},
    {"supplier", "Supplier", 180.0F},
    {"actions", "", 64.0F},
};

lotui::ListCell name;
name.text = "Desk lamp";

lotui::ListCell category;
category.kind = lotui::ListCellKind::ComboBox;
category.options = {"Lighting", "Furniture", "Office"};

lotui::ListCell supplier;
supplier.kind = lotui::ListCellKind::Lookup;
supplier.text = "Northwind";
supplier.options = {"Northwind", "Contoso", "Fabrikam"};

lotui::ListCell action;
action.kind = lotui::ListCellKind::ActionButton;
action.text = "Details";

auto list = std::make_unique<lotui::ListControl>(
    textEngine,
    std::move(columns),
    std::vector<lotui::ListRow>{{name, category, supplier, action}},
    lotui::Size{640.0F, 360.0F},
    [](std::optional<lotui::ListCellAddress> selected) {
        // Synchronize application selection.
    },
    [](const lotui::ListCellEvent& event) {
        // Open a popup at event.anchor or invoke an application command.
    });
```

Only visible rows are shaped and painted. The selected cell is kept visible
during arrow, Home, End, Page Up, and Page Down navigation. Horizontal and
vertical scroll bars appear when the content exceeds the viewport. Their
thumbs can be dragged and clicking a track pages the view.

Mouse wheels and touchpads use the same backend-neutral scroll path. Platform
backends label a delta as `ScrollDeltaMode::Line` or
`ScrollDeltaMode::Pixel`; `ListControl` converts line deltas using its row
height while preserving precise macOS touchpad deltas. Applications can query
the current geometry through `verticalScrollBarBounds()`,
`verticalScrollThumbBounds()`, `horizontalScrollBarBounds()`, and
`horizontalScrollThumbBounds()` when testing or composing related controls.

## Column resizing and sorting

When a column has `resizable == true`, dragging its header divider updates the
column width immediately and clamps it to `minimumWidth`. Applications can
persist the result with `setOnColumnResized`. `setColumnWidth` provides the
same validated operation for restored layouts.

Sortable header clicks alternate between ascending and descending order. The
control stores and paints a `ListSortDescriptor`, then reports it through
`setOnSortChanged`:

```cpp
list->setOnSortChanged(
    [](std::optional<lotui::ListSortDescriptor> sort) {
        if (sort) {
            // Sort the application model, database query, or virtual provider,
            // then publish the resulting rows to the ListControl.
        }
    });
```

`ListControl` deliberately does not reorder the application model itself.
This keeps the same API usable for local vectors, database-backed tables, and
large virtual data sources. Set `sortable` or `resizable` to `false` on an
individual `ListColumn` when that header operation is inappropriate.

## Column reordering and frozen columns

Dragging a header beyond `columnReorderDragThreshold` moves the complete
column. `ListControl` moves the `ListColumn` metadata and every corresponding
row cell together, then remaps selection and the active sort descriptor.
Applications can persist the new order through `setOnColumnReordered`, or
restore one directly with `moveColumn(from, to)`. Set a column's
`reorderable` field to `false` when it must remain under application control.

`setFrozenColumnCount(count)` fixes the first `count` columns to the left edge.
Later columns continue to use the horizontal scroll offset, while painting,
hit testing, inline-editor anchors, and the horizontal scroll bar all exclude
the frozen region. The frozen boundary and reorder insertion marker are
ordinary backend-neutral `PaintCommand` output, so they work with standalone
and embedded Vulkan renderers alike.

## Completion path

The public event contract deliberately separates a cell action from the popup
or editor used to perform it. `ComboBox::showPopupAt` and
`LookupBox::showPopupAt` now provide the common popup implementations. The next
implementation stages are:

1. Asynchronous lookup data providers with loading and empty states.
2. Row and rectangular multi-selection, clipboard copy/paste, and range fill.
3. Large-data provider API, row reuse, and incremental loading.

This sequence keeps `ListControl` useful now without embedding application
types or a specific renderer into its API.
