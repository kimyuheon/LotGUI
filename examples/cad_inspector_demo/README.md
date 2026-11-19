# CAD inspector example

This standalone LotUI example models the scene-tree and property-panel workflow
used in the sibling 3dEngine project. It builds with LotUI only. Selecting an
object in the tree refreshes its name, visibility, world position, and color.
Edits update the selected object; a visibility edit also updates the tree
checkbox. The reset button sets X, Y, and Z to zero.

The example composes public `TreeView`, `TextField`, `Checkbox`,
`NumericInput`, and `ColorEditor` widgets. Its small `CadObject` model and
callbacks live in `main.cpp`, so an engine can replace the in-memory model
with its own adapter. In 3dEngine, the corresponding API calls are
`CAD_GetSelection`, `CAD_GetObjectName` / `CAD_SetObjectName`,
`CAD_IsObjectVisible` / `CAD_SetObjectVisible`, `CAD_GetPosition` /
`CAD_SetPosition`, and `CAD_GetColor` / `CAD_SetColor`. Engine-side code must
validate edits and refresh the panel when engine selection or scene revision
changes. No engine headers or classes are linked into LotUI.

Build and run from the repository root:

```sh
cmake -S . -B build -DLOTUI_BUILD_EXAMPLES=ON
cmake --build build --config Release --target cad_inspector_demo
```

The executable is `build/Release/cad_inspector_demo.exe` on Windows. Run
`--self-test` to exercise tree selection, numeric editing, visibility, naming,
and reset. `--capture output.ppm` saves a Vulkan-rendered frame.

![CAD inspector example](../../docs/images/cad-inspector-demo.png)
