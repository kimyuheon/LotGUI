# 3dEngine ImGui replacement matrix

The sibling project `../3dEngine` is LotUI's reference consumer. The replacement
target is the desktop ImGui layer, not the CAD engine, renderer, SwiftUI host,
or operating-system file picker implementation.

The audit covers every project source file containing an `ImGui::` call. Build
directories and vendored dependencies are excluded. This document is the
acceptance checklist for removing the desktop ImGui dependency.

## Application surfaces

| 3dEngine surface | Required LotUI capabilities | Status |
|---|---|---|
| `lot_ui_manager` | application overlay, dock host, document tabs, viewport reservation | Planned |
| `lot_main_menu` | menu bar, nested menus, checked/disabled items, shortcuts, separators | Planned |
| `lot_toolbar` | tool window, internal tabs, toggle/action buttons, tooltips | Planned |
| `lot_status_bar` | fixed bar, command `TextField`, history, transcript popup, state toggles | Text input foundation complete |
| `lot_properties_panel` | property grid, numeric/vector editors, combo boxes, checkboxes, color editor | Partial |
| `lot_material_panel` | list/selection, numeric editors, colors, actions | Partial |
| `lot_rigging_panel` | collapsible tree, inline rename, selection, vector editing, drag interaction | Planned |
| `lot_joint_panel` | collapsible groups, sliders, inline rename, combo boxes, repeated rows | Planned |
| `lot_block_panel` | thumbnail grid, selection, double-click, drag source, rename, tooltip | Planned |
| `lot_dimension_panel` | actions, numeric editor, integer slider, combo box, color editor | Partial |
| `lot_section_dialog` | modeless dialog, radio group, sliders, checkboxes, conditional content | Planned |
| `lot_array_dialog` | modeless dialog, integer/float/vector inputs, validation, disabled actions | Partial |
| `lot_boolean_dialog` | modeless dialog, radio group, selection summaries, validation | Planned |
| `lot_text_explode_dialog` | text, bullets, checkbox, action | Partial |
| viewport overlays | foreground/background drawing, hit testing, image buttons | Planned |

## Widget parity

| ImGui pattern used by 3dEngine | LotUI replacement | Status |
|---|---|---|
| `Text`, `TextWrapped`, `TextColored`, `TextDisabled`, `BulletText` | `Label`, wrapping and semantic text styles | Basic label complete; wrapping planned |
| `Button`, `SmallButton` | `Button` and size/style variants | Complete foundation |
| `Checkbox` | `Checkbox` | Complete foundation |
| `InputText` | `TextField` with committed text and IME composition | Single-line foundation complete; selection/clipboard planned |
| `InputInt`, `InputFloat`, `InputFloat3`, `DragFloat`, `DragFloat3` | numeric scalar/vector editor with typing and pointer drag | Scalar stepping partial |
| `SliderFloat`, `SliderInt` | `Slider<T>` | Planned |
| `RadioButton` | `RadioButton` and `RadioGroup` | Planned |
| `Combo`, `BeginCombo`, `Selectable` | `ComboBox`, `ListView`, selection model | Planned |
| `ColorEdit3` | RGB/RGBA field and `ColorPicker` popup | Planned |
| `BeginTabBar`, `BeginTabItem`, `TabItemButton` | `TabView` and closeable `DocumentTabView` | Next after text input |
| `CollapsingHeader`, `Indent` | `DisclosurePanel` and `TreeView` | Planned |
| `BeginChild` | clipped `ScrollView` | Planned |
| `BeginPopup`, `OpenPopup` | anchored `Popup` managed by an overlay host | Planned |
| `BeginMenu`, `MenuItem` | menu model, menu bar, nested menu popup | Planned |
| `SetTooltip` | delayed overlay `Tooltip` | Planned |
| `BeginDisabled` | common enabled state and inherited input suppression | Per-widget partial |
| `Begin`/`End` tool windows | floating/dockable `Panel` | Planned |
| `DockSpace` | `DockHost`, split nodes, drag/drop docking, persisted layout | Planned |
| draw-list circles/lines | backend-neutral vector/overlay paint commands | Planned |
| image buttons and block thumbnails | image widget, image button, tile/grid view | Planned |

## Immediate-mode migration rules

The engine must not reproduce ImGui calls one by one. Retained state is owned by
widgets and application view models:

- `SameLine` becomes a `Row`; sequential statements become `Column` children.
- `PushID` becomes stable widget IDs or item keys.
- `BeginDisabled` becomes an inherited `enabled` property.
- `IsItemHovered`, `IsItemActive`, and `IsItemClicked` become widget events and
  observable interaction state.
- `SetNextItemWidth` becomes layout constraints.
- `SetNextWindowPos/Size` becomes panel placement persisted by the shell.
- repeated immediate loops use a list/tree data model and stable item identity.
- popups, menus, tooltips, and dialogs use one overlay stack so focus, clipping,
  input blocking, and dismissal behavior remain consistent.

## Delivery order

1. `TextField`, clipboard, selection, Windows/Cocoa/X11 text and IME events.
2. `TabView`, closeable document tabs, overflow scrolling, add-tab action.
3. `Popup`, `Tooltip`, menu model, `MenuBar`, context menus.
4. `DialogHost`, modal/modeless dialogs, focus trap and restoration.
5. `ScrollView`, `ListView`, `DisclosurePanel`, `TreeView` and inline rename.
6. `Slider`, radio group, combo box, vector numeric editor, color editor.
7. toolbar, status bar, property grid and block thumbnail grid composites.
8. floating panels, docking, layout persistence and viewport integration.
9. port each 3dEngine surface and remove its corresponding ImGui calls.

LotUI remains an independent library. The parity examples may mirror 3dEngine
workflows, but LotUI does not include or link CAD engine classes.
