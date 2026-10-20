# LotGUI_Sample SDK distribution

LotGUI owns the framework implementation. The separate `../LotGUI_Sample`
checkout owns its examples, public `sdk/include` headers, and license notices.
Building LotGUI does not overwrite any of those sample-owned files. When a
public API changes, update the tracked sample headers separately before
distributing new binaries.

With the sibling sample checkout present, a fresh CMake configuration enables
`LOTUI_BUILD_DESKTOP_SDK` and `LOTUI_STAGE_SAMPLE_SDK` by default:

```sh
cmake -S . -B build-sdk
cmake --build build-sdk --config Release
```

An existing build cache can explicitly enable staging with
`-DLOTUI_BUILD_DESKTOP_SDK=ON -DLOTUI_STAGE_SAMPLE_SDK=ON` during configuration.
Set `LOTUI_STAGE_SAMPLE_SDK=OFF` to build without copying, or set
`LOTUI_SAMPLE_REPO_DIR` for another sample checkout. Without the sibling
checkout, both desktop SDK options default to OFF.

Each build installs only static LotUI libraries, bundled static text/parser
dependencies, and the `find_package(LotUI CONFIG REQUIRED)` CMake files under
`../LotGUI_Sample/sdk/<OS-architecture-toolchain>/lib`. CMake imports the
sample's existing `sdk/include` through a relative path. Keep the entire
`sdk` directory together when relocating the SDK. The smaller `SDK` component
for embedded-engine use is separate; do not install both components into one
prefix.

To export manually and write source-commit metadata, run:

```sh
python tools/export_samples.py --destination ../LotGUI_Sample \
  --build-dir build-sdk --triplet windows-x64-msvc --config Release
```

On Windows, use `py` if the `python` alias is unavailable. Build the
`macos-arm64-clang`, `macos-x64-clang`, and `linux-x64-gcc` triplets on their
respective machines. The exporter rejects a triplet that does not match the
configured build. Release and Debug archives can share a triplet prefix but
must match the consuming application's runtime settings.

The sample uses a static C++ ABI, not a stable DLL/C ABI. Vulkan and any OS
system libraries remain target-platform dependencies. Do not distribute a
complete Vulkan SDK as part of the sample package. No repository visibility
or license change is performed by the exporter.
