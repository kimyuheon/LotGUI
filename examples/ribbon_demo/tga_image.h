#pragma once

#include "renderer/texture.h"

#include <filesystem>

namespace lotui::example {

// A small sample decoder for uncompressed, top-left-origin 32-bit TGA icons.
// Applications can decode PNG/JPEG with their own image library and pass the
// same Rgba8Unorm TextureImage to TextureStore::createTexture.
TextureImage loadRibbonTga(const std::filesystem::path& path);

} // namespace lotui::example
