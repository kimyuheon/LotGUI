# Standalone ribbon demo

`ribbon_demo` is a LotUI-only native-window example. It does not include or
link 3dEngine. The 72-pixel ribbon, modal dialog, button, checkbox, and slider
use the same public widget API as the engine integration sample.

`ribbon_demo_plugin` is a separate shared library loaded beside the executable.
Its entry point returns a versioned C descriptor from `plugin_api.h` for a
button, checkbox, and slider. The host validates the descriptor and creates
ordinary LotUI widgets in `plugin_tab.cpp`; the plugin does not link LotUI or
own Vulkan objects. The plugin stays loaded until its widget tree is gone.
This example ABI is intentionally local to the demo, not a stable public SDK.

Build and run:

```sh
cmake -S . -B build -DLOTUI_BUILD_EXAMPLES=ON
cmake --build build --config Release --target ribbon_demo
```

Run `ribbon_demo` from the build output's `Release` directory on Windows, or
from the matching build output directory on macOS/Linux. `--smoke` loads the
plugin, renders a few frames, and exits. `--input-test` also clicks and drags
the plugin controls and checks their callbacks.

To stage a separate example distribution:

```sh
cmake --install build --config Release --component Examples --prefix build/ribbon-demo-dist
```

The `Examples` component contains `bin/ribbon_demo` (or
`bin/ribbon_demo.exe`), the adjacent `ribbon_demo_plugin` shared library,
`bin/resources/fonts/NotoSansKR-Regular.ttf`, and `bin/legal`. Keep the
executable and plugin together. Configure `LOTUI_EXAMPLE_FONT_FILE` with a
licensed Noto Sans KR font if the default sibling font is unavailable. Verify
the font's provenance and matching OFL notice before distributing the package.
A Vulkan runtime is required on the target machine.
