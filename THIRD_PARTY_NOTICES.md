# Third-party notices

LotGUI itself is distributed under the MIT License in `LICENSE`.

Depending on the selected build options and the files included by an
application, LotGUI uses the following third-party components. Distributors
must ship the applicable license files with source and binary packages.

## FreeType

FreeType is used by `LotUI::TextFreeType` to rasterize glyphs. It is available
under the FreeType Project License or GPLv2; LotGUI uses the FreeType Project
License option. Binary distributions must acknowledge that the software is
based in part on the work of the FreeType Team.

The complete license is in [`licenses/FREETYPE-FTL.txt`](licenses/FREETYPE-FTL.txt).

## HarfBuzz

HarfBuzz is used by `LotUI::TextFreeType` for text shaping and is distributed
under the Old MIT license.

The complete notice and license are in
[`licenses/HARFBUZZ-COPYING.txt`](licenses/HARFBUZZ-COPYING.txt).

## Noto Sans KR

The example can package Noto Sans KR when `LOTUI_EXAMPLE_FONT_FILE` points to
an official SIL Open Font License copy. The font may be bundled with software,
but the copyright notice and OFL license must accompany every distributed
copy. The font remains under the OFL and is not relicensed under LotGUI's MIT
license.

The complete license is in
[`licenses/NOTO-SANS-KR-OFL.txt`](licenses/NOTO-SANS-KR-OFL.txt).

The repository does not currently store a font binary. Before making a release,
verify the provenance of the configured font file and package the matching
copyright notice and license with it.

## Vulkan Loader and MoltenVK

LotGUI links against the Vulkan implementation supplied by the target system
or SDK. It does not copy the Vulkan SDK into its source package. If a binary
distribution bundles the Vulkan Loader, MoltenVK, or another SDK component,
the distributor must include that component's own license and NOTICE files.
The official Vulkan Loader and MoltenVK are primarily Apache-2.0 licensed.

## TinyXML-2

`LotUI::Declarative` uses TinyXML-2 to parse LotML documents. TinyXML-2 is
distributed under the zlib license. The complete notice is in
[`licenses/TINYXML2-ZLIB.txt`](licenses/TINYXML2-ZLIB.txt).
