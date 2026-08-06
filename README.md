# LotUI

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

LotUI is a retained-mode, cross-platform C++ GUI framework. Native platform
backends own windows and events while widgets emit backend-neutral
`PaintCommand` values. Vulkan is the first renderer.

## Build

```sh
cmake -S . -B build
cmake --build build --config Debug
```

The reusable targets are `LotUI::Core`, `LotUI::Text`, `LotUI::TextFreeType`,
`LotUI::Widgets`, `LotUI::Platform`, `LotUI::Renderer`, and `LotUI::Vulkan`.
Applications can initially consume LotUI with `add_subdirectory()` and link
only the targets they use. `platform_probe` demonstrates the full native-window
to widget-tree to Vulkan path.

FreeType 2.14.3 and HarfBuzz 14.2.1 are pinned for reproducible source builds.
CMake first checks installed packages, then the relative sibling directories
`../FreeType` and `../HarfBuzz`, and finally downloads the pinned sources when
`LOTUI_FETCH_TEXT_DEPENDENCIES` is enabled. Every location is a cache variable;
no developer-machine absolute path is required.

## Widget API

Include `lotui/lotui.h`, compose widgets with RAII ownership, then keep the
`WidgetTree` as application state:

```cpp
auto row = std::make_unique<lotui::Row>();
row->addChild(
    std::make_unique<lotui::Button>(
        lotui::Size{120.0F, 40.0F},
        [] { /* application action */ }),
    lotui::ChildLayout{1.0F});

lotui::WidgetTree tree(std::move(row));
tree.layout({0.0F, 0.0F, 800.0F, 600.0F});

std::vector<lotui::PaintCommand> commands;
tree.paint(commands);
```

The application forwards normalized pointer events to `WidgetTree` and applies
the returned `captureStarted` and `captureEnded` flags to its platform window.
Widgets do not depend on Vulkan or operating-system native types.

Native keyboard values are normalized into `KeyCode` and `KeyModifiers`.
`WidgetTree` provides Tab/Shift+Tab traversal, pointer-to-focus behavior, and
focused key dispatch. Buttons display a focus ring and activate with Enter or
Space. See [`docs/input-and-focus.md`](docs/input-and-focus.md).

`Label` and content-bearing `Button` use the backend-neutral `TextEngine` and
`TextLayout` contracts. `LotUI::TextFreeType` provides real UTF-8 shaping,
per-code-point font fallback, FreeType rasterization, and an R8 glyph atlas.
The example copies its configured font beside the executable and resolves it
with `lotui::executableDirectory()`, so launching from another working
directory remains safe. Text and IME boundaries are documented in
[`docs/text-rendering.md`](docs/text-rendering.md).

```cpp
auto text = std::make_shared<lotui::FreetypeTextEngine>(
    renderer,
    std::vector<lotui::FontSource>{
        {"Noto Sans KR", resourceRoot / "fonts/NotoSansKR-Regular.ttf"}});
auto label = std::make_unique<lotui::Label>(text, "LotUI 한글");
```

Render backends expose the RAII `TextureStore` API. The Vulkan backend supports
R8 glyph masks, RGBA images, normalized UV coordinates, and partial texture
updates without exposing Vulkan handles to widgets.

Current text limitations are deliberate and visible: layout is single-line,
the glyph cache uses one atlas page, and IME/TextField integration is the next
milestone. The public text contract does not need to change to add those
features.

## License

LotGUI is available under the [MIT License](LICENSE). Applications may use it
in free, commercial, open-source, or closed-source software. Distributors must
also include the applicable notices described in
[`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md), especially when packaging
FreeType, HarfBuzz, Noto Sans KR, the Vulkan Loader, or MoltenVK.
The `platform_probe` build copies these documents into the executable's
`legal` directory as an example of a binary distribution layout.
