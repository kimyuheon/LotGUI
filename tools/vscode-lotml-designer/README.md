# LotUI Designer for VS Code

Open a `.lotml` layout from Explorer with **LotUI: Open Designer**, or use
**LotUI: New LotML Layout** in the Command Palette. Try `examples/settings.lotml`
for form controls and `examples/ribbon.lotml` for a ribbon.

- Click a widget in the outline or on the canvas to edit its properties.
- Select a container and click a palette entry, or drag a widget from the
  palette onto a container. The outline and canvas both follow the LotML source.
- Use the property panel to reorder siblings or remove the selected widget.
  Save and undo with the usual VS Code shortcuts.
- Open **LotML 소스** to edit the markup. The canvas refreshes when the source changes.

The design canvas uses a layout approximation. **Vulkan 미리보기** opens the
current in-memory document, including unsaved changes, in the actual LotUI
renderer at the selected window size. Preview accepts named UI events as
placeholders, so the same markup can be reused with real C++ callbacks.

## Install

From the LotGUI or LotGUI_Sample root, build and install the extension locally.
Python 3 and VS Code with the `code` command are required.

```sh
python tools/vscode-lotml-designer/package_vsix.py out
code --install-extension out/lotml-designer-0.1.1.vsix
```

The VSIX can also be installed from the Extensions view using **Install from VSIX…**.

## Build the Vulkan preview

Build `lotml_preview` with Vulkan, FreeType, the example font and LotML enabled.
In LotGUI_Sample, configure against a DesktopSDK that exports
`LotUI::Declarative`, then build the same target.

```sh
cmake --build build --config Release --target lotml_preview
```

Use the workspace setting `lotgui.previewExecutable` if your executable lives
outside the default build directories. Native preview is available in trusted
workspaces. Relaunch it to see changes made since the previous launch.

When the C++ LotML widget registry changes, regenerate the bundled descriptor
list with `lotml_preview --schema` and save its UTF-8 output as `schema.json`.
