# LotGUI_Sample DesktopSDK

The separate `../LotGUI_Sample` repository consumes LotUI as a prebuilt C++
library. Do not compile framework `.cpp` files in that repository. Build each
OS, CPU architecture, compiler and configuration on the matching platform.

```sh
cmake -S . -B build-sdk
cmake --build build-sdk --config Release
```

When `../LotGUI_Sample` exists, the desktop SDK build and staging are enabled
by default. Every build installs the matching configuration into
`../LotGUI_Sample/sdk/<triplet>/`. The sample repository already owns the
public headers in `sdk/include`, examples, and license notices; staging never
copies those. Set `LOTUI_STAGE_SAMPLE_SDK=OFF` to build
without copying, or set `LOTUI_SAMPLE_REPO_DIR` for another sample checkout.
Without the sibling checkout, both desktop SDK options default to OFF.
When LotUI's public API changes, update the sample's tracked `sdk/include`
headers separately before distributing the newly built libraries.

To export manually or record build metadata, run:

```sh
python tools/export_samples.py --destination ../LotGUI_Sample \
  --build-dir build-sdk --triplet windows-x64-msvc --config Release
```

On Windows, `py tools/export_samples.py ...` can be used when the `python`
alias is not installed.

Use `macos-arm64-clang`, `macos-x64-clang`, or `linux-x64-gcc` in the last
command when built on those platforms. The exporter checks the configured
triplet and installs into `sdk/<triplet>/`. It never copies a Windows binary
into a macOS or Linux prefix. Set `LOTUI_SAMPLE_SDK_TRIPLET` when using a
different toolchain.

The desktop prefix contains static LotUI and text dependency archives plus a
`find_package(LotUI CONFIG REQUIRED)` package that references the shared
`../include` directory. Keep the triplet prefix and `sdk/include` together
when distributing or relocating the sample SDK. The manual exporter also
writes build metadata. Release and Debug archives can share one triplet
prefix but require matching application runtime settings. The smaller `SDK`
component for embedded-engine use is separate; do not install both components
into one prefix. Example sources and consumer-owned documentation are not
overwritten by this export.
