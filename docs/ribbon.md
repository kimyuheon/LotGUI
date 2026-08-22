# LotUI ribbon control

`Ribbon` is a retained container for application commands. It owns stable tabs,
shows only the selected tab's widget subtree, and paints through ordinary
`PaintCommand` values. It has no Vulkan or operating-system types in its public
API.

## C++ construction

```cpp
auto commands = std::make_unique<lotui::Row>();
commands->addChild(std::make_unique<lotui::Button>(
    lotui::Size{104.0F, 52.0F}, [] { saveDocument(); }));

auto group = std::make_unique<lotui::RibbonGroup>(
    textEngine, "파일", std::move(commands));
auto home = std::make_unique<lotui::RibbonTab>(
    "home", "홈", std::move(group));

auto ribbon = std::make_unique<lotui::Ribbon>(textEngine);
ribbon->addTab(std::move(home));
ribbon->selectTab("home");
```

`RibbonGroup` deliberately accepts a normal widget rather than a separate
command object. Applications can compose `Button`, `Checkbox`, `TextField`,
rows, columns, and custom widgets without a second event or styling system.

## Interaction contract

- A primary click selects a tab and moves focus to the ribbon.
- Left and Right cycle through tabs; Home and End select an edge tab.
- Only the selected tab is arranged, painted, hit-tested, and included in focus
  traversal.
- `TabChangedHandler` receives the selected index and stable tab ID after a
  user or programmatic selection change.
- Initial construction and LotML `selectedTab` do not emit a change callback.

## Current boundary

This milestone supplies the desktop ribbon foundation: tabs, groups, arbitrary
command content, styling, keyboard selection, C++ construction, and LotML.
Overflow scrolling, collapsed responsive groups, split/drop-down buttons,
key-tip overlays, and a quick-access toolbar remain future additions. Closeable
CAD document tabs are a separate `DocumentTabView` control and will not be
coupled to the ribbon.
