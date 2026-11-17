#include "examples/ribbon_demo/tga_image.h"

#include <array>
#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace lotui::example {

TextureImage loadRibbonTga(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    std::array<unsigned char, 18> header{};
    if (!input || !input.read(reinterpret_cast<char*>(header.data()),
            static_cast<std::streamsize>(header.size()))) {
        throw std::runtime_error("cannot read ribbon icon: " + path.u8string());
    }
    const auto width = static_cast<std::uint32_t>(
        header[12] | (header[13] << 8));
    const auto height = static_cast<std::uint32_t>(
        header[14] | (header[15] << 8));
    // Image type 2 is raw true-color. The descriptor requires 8-bit alpha,
    // top-left origin, and no horizontal flip or interleaving.
    if (header[0] != 0 || header[1] != 0 || header[2] != 2 ||
        header[16] != 32 || header[17] != 0x28 ||
        width == 0 || height == 0 || width > 1024 || height > 1024) {
        throw std::runtime_error("unsupported ribbon icon TGA: " +
            path.u8string());
    }

    TextureImage image;
    image.width = width;
    image.height = height;
    image.format = TextureFormat::Rgba8Unorm;
    image.pixels.resize(static_cast<std::size_t>(width) * height * 4);
    if (!input.read(reinterpret_cast<char*>(image.pixels.data()),
            static_cast<std::streamsize>(image.pixels.size()))) {
        throw std::runtime_error("truncated ribbon icon TGA: " +
            path.u8string());
    }
    for (std::size_t offset = 0; offset < image.pixels.size(); offset += 4) {
        std::swap(image.pixels[offset], image.pixels[offset + 2]);
    }
    return image;
}

} // namespace lotui::example
