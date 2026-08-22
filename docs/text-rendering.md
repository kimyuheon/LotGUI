# LotUI text rendering architecture

Text is split into a backend-neutral layout contract and backend-specific
texture resources. Widgets must not include FreeType, HarfBuzz, Vulkan, or
native operating-system headers.

## Public flow

1. `Label` sends UTF-8 text, `TextStyle`, and width constraints to a
   `TextEngine`.
2. `TextEngine` returns an immutable `TextLayout` containing measured size,
   baseline, glyph positions, and the resources needed to paint it.
3. `TextLayout::appendPaintCommands` appends glyph quads with an opaque
   `TextureId` and normalized `textureCoordinates`.
4. The renderer batches those commands by clip and texture. Vulkan resolves
   the texture ID to a descriptor and samples the glyph atlas.

This contract lets tests use a deterministic fake engine and lets a future
Metal or Direct3D renderer consume the same widget-generated commands.

## FreeType and HarfBuzz implementation

The production text engine is the separate `LotUI::TextFreeType` target:

- HarfBuzz shapes UTF-8 runs into glyph IDs, advances, offsets, and clusters.
- FreeType rasterizes the shaped glyph IDs into grayscale atlas pages.
- Font fallback is applied per missing code point so Korean and mixed-script text
  can use multiple font faces in one layout.
- Cache keys currently include font face, pixel size, and glyph ID. Style is
  used to select the closest registered face.
- Atlas pages own renderer texture resources through RAII. A `TextLayout`
  keeps the referenced page alive while its paint commands can be emitted.

The default source-build search roots must remain relative to LotUI, with
CMake cache variables allowing applications to provide different locations.
No developer-machine font or SDK path may be committed.

## Runtime font policy

Applications can register font files or in-memory font data explicitly. A
default sans-serif family can be supplied from an installed LotUI resource
directory. Platform font discovery belongs behind a platform service and is
optional; it must not leak Win32, Cocoa, Fontconfig, or native handles into the
widget API.

Runtime resources are resolved relative to the executable or an explicit
installed-resource root, never the current working directory.

## Korean input

IME composition is separate from shaping:

- the platform backend reports composition start/update/commit/cancel events;
- `TextField` stores committed UTF-8 text and a temporary pre-edit range;
- every pre-edit update is shaped through the same `TextEngine`;
- underline and selection decorations are ordinary paint commands;
- candidate windows use the caret rectangle translated through the platform
  backend.

This separation avoids the incomplete-syllable behavior common in immediate
mode integrations: composition text remains a live pre-edit run until the OS
commits it.

## Implementation order

1. ~~Add renderer-owned sampled texture resources and atlas upload/update APIs.~~
2. ~~Add normalized UV sampling to the Vulkan rectangle/glyph pipeline.~~
3. ~~Add relative-path CMake discovery for FreeType and HarfBuzz.~~
4. ~~Implement font registration, shaping, fallback, and a grayscale glyph atlas.~~
5. ~~Connect the production engine to `Label` and button content.~~
6. ~~Add Korean/Latin mixed-script shaping and atlas integration tests.~~
7. Add line breaking, wrapping, multiple atlas pages, clipping, and DPI tests.
8. ~~Connect native IME composition events to `TextField`.~~
9. Add text selection, clipboard editing, horizontal scrolling, and undo/redo.
