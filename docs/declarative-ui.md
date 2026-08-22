# Declarative UI (LotML)

LotML is LotUI's optional declarative layout format. It produces the same
retained `WidgetTree` as the C++ widget API; rendering, input, focus, and event
dispatch do not have separate declarative implementations.

Applications that want LotML link `LotUI::Declarative`. Code-only applications
can omit that target and do not need TinyXML-2.

```xml
<Column id="dialog" spacing="12" padding="20">
  <Label text="프로젝트 설정" fontSize="22" />
  <TextField text="새 프로젝트" onChanged="changed" onSubmitted="save" />
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

- Containers: `Row`, `Column`, `Ribbon`, `RibbonTab`, `RibbonGroup`
- Content: `Label`, `Box`, `Button`, `Checkbox`, `NumericInput`, `TextField`
- Child layout: `flex`, `minWidth`, `minHeight`, `maxWidth`, `maxHeight`
- Shared lookup: optional unique `id`
- Events: named `onClick` and `onSubmitted` handlers supplied by the application
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
Up/Down/PageUp/PageDown/Home/End. Direct digit entry will reuse `TextField`'s
committed text and IME event path; it is intentionally not implemented as
platform-specific virtual-key parsing.

## Ribbon layout

`RibbonTab` and `RibbonGroup` are retained structural widgets. A group accepts
one child, normally a `Row` or `Column`, and that child may contain any normal
LotUI controls.

```xml
<Ribbon selectedTab="home" onChanged="tabChanged">
  <RibbonTab tabId="home" title="홈">
    <Row spacing="8" crossAlign="stretch">
      <RibbonGroup title="파일">
        <Row spacing="6">
          <Button text="새로 만들기" onClick="newDocument" />
          <Button text="저장" onClick="saveDocument" />
        </Row>
      </RibbonGroup>
    </Row>
  </RibbonTab>
</Ribbon>
```

`tabId` is the stable application identity used by `selectedTab` and the C++
selection API. The optional normal `id` still identifies the widget for
`LoadedUi::find()` and designer tooling.
