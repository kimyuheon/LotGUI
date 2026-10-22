# TreeView and RadioGroup

`TreeView` accepts flat preorder nodes with stable string IDs. A child follows
its parent at depth one greater. Expand and selection state stay with the view;
`setNodes` preserves selection by ID when that row remains visible.
Subtle branch guides end at the last sibling. Nodes can opt into a checkbox;
check state is independent of selection and expansion and survives model
refreshes with the same ID.

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
activation, and mouse-wheel scrolling is clipped to the view. Tree drag/drop,
visibility toggles and inline rename are not yet part of this control.

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
