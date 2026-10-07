# Standalone ribbon demo

`ribbon_demo` is a LotUI-only native-window example. It does not include or
link 3dEngine. The 72-pixel ribbon, modal dialog, button, checkbox, and slider
use the same public widget API as the engine integration sample.

`plugin_tab.cpp` is a separate application module that adds a tab using only
LotUI headers. It demonstrates how a host can give an extension access to
ordinary controls. It is compiled into the example; it is not a dynamically
loaded plugin ABI.

Build and run:

```sh
cmake -S . -B build -DLOTUI_BUILD_EXAMPLES=ON
cmake --build build --config Release --target ribbon_demo
```

Run `ribbon_demo` from the build output's `Release` directory on Windows, or
from the matching build output directory on macOS/Linux. `--smoke` renders a
few frames and exits, which is useful for integration checks.

To stage a separate example distribution:

```sh
cmake --install build --config Release --component Examples --prefix build/ribbon-demo-dist
```

The `Examples` component contains `bin/ribbon_demo` (or
`bin/ribbon_demo.exe`), `bin/resources/fonts/NotoSansKR-Regular.ttf`, and
`bin/legal`. Configure `LOTUI_EXAMPLE_FONT_FILE` with a licensed Noto Sans KR
font if the default sibling font is unavailable. Verify the font's provenance
and matching OFL notice before distributing the package. A Vulkan runtime is
required on the target machine.
