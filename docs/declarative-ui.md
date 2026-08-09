# Declarative UI (LotML)

LotML is LotUI's optional declarative layout format. It produces the same
retained `WidgetTree` as the C++ widget API; rendering, input, focus, and event
dispatch do not have separate declarative implementations.

Applications that want LotML link `LotUI::Declarative`. Code-only applications
can omit that target and do not need TinyXML-2.

```xml
<Column id="dialog" spacing="12" padding="20">
  <Label text="프로젝트 설정" fontSize="22" />
  <Checkbox text="격자에 맞춤" checked="true" onChanged="changed" />
  <NumericInput value="10" minimum="0.5" maximum="100"
                step="0.5" decimalPlaces="1" onChanged="changed" />
  <Row spacing="8" mainAlign="end">
    <Button text="취소" onClick="cancel" />
    <Button id="saveButton" text="저장" onClick="save" />
  </Row>
</Column>
```

```cpp
lotui::declarative::LoadOptions options;
options.textEngine = textEngine;
options.events.emplace("save", [] { saveSettings(); });
options.events.emplace("cancel", [] { closeDialog(); });
options.events.emplace("changed", [] { markSettingsDirty(); });

lotui::declarative::LotmlLoader loader;
auto ui = loader.loadFile(resourceRoot / "settings.lotml", options);
ui.tree().layout(windowBounds);

auto* save = dynamic_cast<lotui::Button*>(ui.find("saveButton"));
```

## Initial schema

- Containers: `Row`, `Column`
- Content: `Label`, `Box`, `Button`, `Checkbox`, `NumericInput`
- Child layout: `flex`, `minWidth`, `minHeight`, `maxWidth`, `maxHeight`
- Shared lookup: optional unique `id`
- Events: named `onClick` handlers supplied by the application
- Value changes: named `onChanged` handlers; the current value is available
  through the typed widget returned by `LoadedUi::find()`
- Insets: one value, horizontal/vertical pair, or left/top/right/bottom
- Colors: `#RRGGBB` or `#RRGGBBAA`

The loader is strict. Unknown widgets, unknown properties, duplicate IDs,
missing event handlers, invalid values, and unsupported child content produce
a `LotmlError` with source-line information where available. Label text and
text-only buttons require the application's `TextEngine`.

`WidgetRegistry::descriptors()` exposes widget and property metadata. A future
Visual Studio or VS Code designer can use this for property panels, completion,
validation, and live preview without inventing a second layout model.

## Deliberately deferred

Data binding, resources/styles, templates, conditional content, hot reload, and
drag-and-drop designer serialization are not part of the first schema. They can
be added after the base widgets and layout behavior stabilize without changing
the `WidgetTree` or renderer contracts.

`NumericInput` currently supports pointer step controls and
Up/Down/PageUp/PageDown/Home/End. Direct digit entry will use the same committed
text and IME event path as the future `TextField`; it is intentionally not
implemented as platform-specific virtual-key parsing.
