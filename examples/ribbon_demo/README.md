# Standalone ribbon demo

`ribbon_demo` is a LotUI-only native-window example. It does not include or
link 3dEngine. Its compact 86-pixel ribbon and object list use the same public
widget API as the engine integration sample. Its six tabs are Home, Insert,
View, Data, Inspect, and Plugin. Home provides add, duplicate, delete, undo,
redo, and a modal dialog. Insert creates assembly, sketch, view, and table
rows. View changes row density and frozen columns. Data sorts by name and
filters to visible rows. Inspect shows selected-row details and changes
visibility. The list itself supports row selection and visibility checkboxes.

Ribbon commands use a 20-glyph subset of the Lucide 1.55.0 icon font. The
subset is stored in `resources/fonts/lucide-subset.ttf`; its ISC/MIT license
notice is in `resources/LUCIDE-LICENSE.txt`. Icons are rendered through the
same FreeType text engine on all supported platforms.

`ribbon_demo_plugin` is a separate shared library loaded beside the executable.
Its entry point returns a versioned C descriptor from `plugin_api.h` for a
button, checkbox, and slider. The host validates the descriptor and creates
ordinary LotUI widgets in `plugin_tab.cpp`; the plugin does not link LotUI or
own Vulkan objects. Plugin actions add a row, freeze the name columns, and
change row density. The plugin stays loaded until its widget tree is gone.
This example ABI is intentionally local to the demo, not a stable public SDK.

Build and run:

```sh
cmake -S . -B build -DLOTUI_BUILD_EXAMPLES=ON
cmake --build build --config Release --target ribbon_demo
```

Run `ribbon_demo` from the build output's `Release` directory on Windows, or
from the matching build output directory on macOS/Linux. `--smoke` loads the
plugin, renders a few frames, and exits. `--input-test` checks workspace
commands, ribbon tab switching, and the plugin controls' callbacks.

To stage a separate example distribution:

```sh
cmake --install build --config Release --component Examples --prefix build/ribbon-demo-dist
```

The `Examples` component contains `bin/ribbon_demo` (or
`bin/ribbon_demo.exe`), the adjacent `ribbon_demo_plugin` shared library,
`bin/resources/fonts/NotoSansKR-Regular.ttf`,
`bin/resources/fonts/lucide-subset.ttf`, and `bin/legal`. Keep the
executable and plugin together. Configure `LOTUI_EXAMPLE_FONT_FILE` with a
licensed Noto Sans KR font if the default sibling font is unavailable. Verify
the font's provenance and matching OFL notice before distributing the package.
A Vulkan runtime is required on the target machine.
