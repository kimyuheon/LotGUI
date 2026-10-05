# VulkanCAD integration smoke

The integration lives here while the LotUI renderer is developed. Copy
`lotui_smoke/` to `../3dengine/samples/lotui_smoke/`, then apply
`overlay-sample-count.patch`, `overlay-input.patch`, and `slider-source.patch`
from the 3dEngine repository root. The engine's
existing `VulkanAppLotGUI` CMake target picks up the sample. `VulkanApp` is
unchanged.

The renderer borrows the engine's device, render pass, and active command
buffer. It neither creates a surface/swapchain nor submits or presents.
`CAD_OverlayFrameInfo::sampleCount` is appended to preserve the existing
struct prefix. The sample accepts an older frame without that field only for
single-sample rendering.

From the LotGUI repository root, build on Windows with:

```powershell
cmake -S ../3dengine -B ../3dengine/build
cmake --build ../3dengine/build --config Release --target VulkanAppLotGUI
../3dengine/build/Release/VulkanAppLotGUI.exe
```

For an automated frame capture, run the executable from the engine's Release
output directory (where its assets and DLL are located):

```powershell
./VulkanAppLotGUI.exe --capture capture.png
```

Build `VulkanApp` separately to check that the existing launcher still links.
Only the new executable compiles LotUI sources; the engine remains in its
existing `VulkanCADCore` shared library.

This smoke renders solid UI geometry only, including a draggable slider.
Textured glyphs, keyboard events/IME, and release packaging are still pending.

The [engine smoke capture](../../docs/images/engine-lotui-smoke.png) shows
the controls over the CAD viewport. It does not represent ImGui replacement.
