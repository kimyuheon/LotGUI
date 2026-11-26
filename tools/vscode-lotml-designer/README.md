# LotUI Designer for VS Code

Open a `.lotml` layout from Explorer with **LotUI: Open Designer**, or use
**LotUI: New LotML Layout** in the Command Palette. Try `examples/settings.lotml`
for form controls and `examples/ribbon.lotml` for a ribbon.

- Click a widget in the outline or on the canvas to edit its properties.
- Select a container and click a palette entry, or drag a widget from the
  palette onto a container. The outline and canvas both follow the LotML source.
- Use the property panel to reorder siblings or remove the selected widget.
  Save and undo with the usual VS Code shortcuts.
- Double-click a widget to open its C++ event code. In the property panel, use
  **코드** beside a specific event to open that handler.
- Open **LotML 소스** to edit the markup. The canvas refreshes when the source changes.

The design canvas uses a layout approximation. **Vulkan 미리보기** opens the
current in-memory document, including unsaved changes, in the actual LotUI
renderer at the selected window size. Double-clicking a widget in this window
closes the preview and opens the same event code in VS Code.

## Connect event code to a C++ application

If an event is unassigned, opening its code adds an event name to LotML and
creates a handler in a companion header such as `settings.lotml.events.h`.
Opening it again retains the handler body. The designer also looks up named
handlers registered through common `LoadOptions::events` expressions in the workspace.

Include the companion header and call its `bindEvents` before loading the layout.
Use `examples/settings.lotml` and `examples/settings.lotml.events.h` as a pair.

```cpp
#include "settings.lotml.events.h"

// In the application, after setting up options.textEngine:
lotui::designer::layout_settings::bindEvents(options);
auto ui = loader.loadFile(layoutPath, std::move(options));
```

The preview uses placeholder callbacks so application code runs when the
layout is loaded by its C++ host.

## Install

From the LotGUI or LotGUI_Sample root, build and install the extension locally.
Python 3 and VS Code with the `code` command are required.

```sh
python tools/vscode-lotml-designer/package_vsix.py out
code --install-extension out/lotml-designer-0.2.0.vsix
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
