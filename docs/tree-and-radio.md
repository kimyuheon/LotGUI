# TreeView and RadioGroup

`TreeView` accepts flat preorder nodes with stable string IDs. A child follows
its parent at depth one greater. Expand and selection state stay with the view;
`setNodes` preserves selection by ID when that row remains visible.
Subtle branch guides end at the last sibling. Nodes can opt into a checkbox;
check state is independent of selection and expansion and survives model
refreshes with the same ID. A checkable group can also display `Mixed` when
only some descendants are visible. Clicking it requests `Checked`; the host
decides whether to apply that state to descendants.

```cpp
auto tree = std::make_unique<lotui::TreeView>(textEngine,
    std::vector<lotui::TreeNode>{
        {"scene", "Scene", 0, true},
        {"part", "Part", 1, false, true, true, true},
    });
tree->setOnSelectionChanged([](auto id) { /* selection changed */ });
tree->setOnExpansionChanged([](std::string_view id, bool open) {
    /* expansion changed */
});
tree->setOnCheckedChanged([](std::string_view id, bool checked) {
    /* check changed */
});
```

Only viewport rows are shaped and painted. Arrow keys navigate; Left and Right
collapse, expand, or move to parent/child. Space toggles a selected checkbox,
or expansion when the node has no checkbox. Enter emits
activation, and mouse-wheel scrolling is clipped to the view. Tree drag/drop
and the engine visibility adapter are not yet part of this control.

```cpp
tree->setCheckState("group", lotui::TreeCheckState::Mixed);
tree->setOnCheckedChanged([](std::string_view id, bool checked) {
    // Apply the requested visibility to the application's scene model.
});
```

`platform_probe` shows a partially visible scene and updates its child check
states when a group is toggled. `setCheckState` and `setChecked` are silent
model updates; user clicks and Space invoke `setOnCheckedChanged`.

F2 requests an inline rename for the selected node. The tree reports a stable
node ID and label-area anchor; the host owns the editor and validates and
commits the new name. `platform_probe` connects this request to
`InlineTextEditor`, including Korean IME input, Enter commit, and Escape cancel.
The ribbon's Rename command calls the same `requestRename` method. On commit,
`setLabel` updates the retained row and invalidates only its text layout.
The example checks `LOTUI_TREE_VIEW_HAS_RENAME` so it can still compile
against older platform SDK archives until those archives are rebuilt.

```cpp
tree->setOnRenameRequested([&](std::string_view id, lotui::Rect anchor) {
    // Show InlineTextEditor at anchor and commit into the app's model.
});
```

`RadioGroup` owns one selection across its options. Pointer clicks and arrow
keys choose enabled options; Home and End jump to the first and last enabled
option. Programmatic `setSelectedId` does not emit the user-change callback.

```cpp
auto operation = std::make_unique<lotui::RadioGroup>(textEngine,
    std::vector<lotui::RadioOption>{
        {"union", "Union"},
        {"subtract", "Subtract"},
        {"intersect", "Intersect"},
    });
operation->setOnChanged([](std::string_view id) {
    /* operation changed */
});
```

The `platform_probe` example contains both controls, including Korean labels,
when built with a matching SDK. Older SDK packages still build the previous
probe layout until their platform archives are refreshed.
