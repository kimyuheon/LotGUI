# Dockable quick toolbar

`DockWorkspace` places one `DockToolbar` around a retained content widget.
Its grip can be dragged to the top, bottom, left, or right edge. Commands are
ordinary LotUI widgets, so application and plugin callbacks remain attached
as the toolbar changes orientation. The content keeps the remaining space.

```cpp
auto toolbar = std::make_unique<lotui::DockToolbar>();
toolbar->addCommand(std::make_unique<lotui::Button>(
    lotui::Size{40.0F, 32.0F}, [] { /* host command */ }));
auto workspace = std::make_unique<lotui::DockWorkspace>(
    std::move(content), std::move(toolbar), lotui::DockSide::Top);
workspace->setSide(lotui::DockSide::Left); // Optional saved layout.
```

The public API is in `widgets/dock_workspace.h`. `setSide()` restores a saved
position; `side()` reads the current one. `DockWorkspaceStyle` sets toolbar
thickness, the edge drop zone, minimum content size, and colors. The toolbar
keeps its command widgets and pointer targets when moved. Dropping outside an
edge zone leaves the current side unchanged. A host can save the `DockSide`
with its own workspace settings; this widget does not write files.

The ribbon demo uses the same widget under its ribbon. Run `ribbon_demo`
to drag the grip, `ribbon_demo --input-test` to check all four positions, or
`ribbon_demo --capture-dock-left output.ppm` to capture a vertical toolbar.
