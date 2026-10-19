# Separate development sources from public SDK samples

The development repository owns the framework implementation. The distribution
repository is `lotcad/LotGUI_Sample`, containing only public headers, application
examples and documentation. Native backend `.cpp`/`.mm`, Vulkan pipeline creation
and shader sources are not copied into it.

`tools/export_samples.py --destination ../LotGUI_Sample` refreshes a maintained
allowlist of headers/examples without copying a source tree or deleting consumer
customizations. It never invokes Git or changes repository visibility.

Build on the target OS with CMake. To install the **full standalone** SDK, use:

```sh
cmake --install build-sdk --config Release --component DesktopSDK --prefix /path/to/sdk
```

The pre-existing `SDK` component remains the smaller embedded engine SDK. Install
these variants into different prefixes because each supplies `LotUIConfig.cmake`.
The DesktopSDK exports Platform, Vulkan, TextFreeType and optional Declarative in
addition to Core/Text/Widgets/Renderer/VulkanEmbedded.

To install and stage a matching binary package in the sample checkout:

```sh
python tools/export_samples.py --destination ../LotGUI_Sample --build-dir build-sdk --triplet windows-x64-msvc --config Release
```

Use `macos-arm64-clang`, `macos-x64-clang` or `linux-x64-gcc` on their own target
build machines. Do not export one machine's archives under another triplet.
The exporter does not cross-compile. Generated package paths use CMake's install
prefix, not developer-machine source or Vulkan SDK paths. External dependencies
are represented by imported targets and remain separately installed binary packages.

This step supplies static archives. It does not introduce a stable DLL/C ABI or
change the existing source code license. Public C++ headers retain the declarations,
inline bodies and templates needed by consumers, including `Widget::onPaint` for
application-owned derived controls.

Check the sample repository's `docs/sdk-distribution.md` for platform-specific
producer/consumer commands and resource packaging. Run `python tests/test_export_samples.py`
before export to check the distribution boundary.
