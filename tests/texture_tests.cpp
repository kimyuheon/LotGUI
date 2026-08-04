#include "renderer/texture.h"
#include "widgets/box.h"

#include <cstdlib>
#include <iostream>
#include <type_traits>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "texture_tests failed: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

void validatesTextureImages() {
    lotui::TextureImage mask;
    mask.width = 4;
    mask.height = 3;
    mask.format = lotui::TextureFormat::R8Unorm;
    mask.pixels.resize(12, 255);
    require(lotui::isValidTextureImage(mask),
            "valid R8 texture was rejected");

    lotui::TextureImage color = mask;
    color.format = lotui::TextureFormat::Rgba8Unorm;
    color.pixels.resize(48, 255);
    require(lotui::isValidTextureImage(color),
            "valid RGBA texture was rejected");
    color.pixels.pop_back();
    require(!lotui::isValidTextureImage(color),
            "incorrect RGBA byte count was accepted");

    mask.width = 0;
    require(!lotui::isValidTextureImage(mask),
            "zero-sized texture was accepted");
}

void validatesPartialUpdates() {
    lotui::TextureUpdate update;
    update.x = 6;
    update.y = 4;
    update.width = 2;
    update.height = 3;
    update.pixels.resize(6, 128);
    require(lotui::isValidTextureUpdate(
                update, 8, 8, lotui::TextureFormat::R8Unorm),
            "valid edge-aligned update was rejected");

    update.x = 7;
    require(!lotui::isValidTextureUpdate(
                update, 8, 8, lotui::TextureFormat::R8Unorm),
            "out-of-bounds update was accepted");
}

void boxPreservesTextureCoordinates() {
    lotui::Box box(
        {40.0F, 20.0F},
        {0.5F, 0.7F, 1.0F, 1.0F},
        4.0F);
    box.setTexture(42, {0.25F, 0.5F, 0.5F, 0.25F});
    box.arrange(
        {0.0F, 0.0F, 40.0F, 20.0F},
        {0.0F, 0.0F, 40.0F, 20.0F});
    std::vector<lotui::PaintCommand> commands;
    box.paint(commands);

    require(commands.size() == 1 && commands[0].texture == 42,
            "box did not emit its texture ID");
    require(commands[0].textureCoordinates.x == 0.25F &&
                commands[0].textureCoordinates.height == 0.25F,
            "box did not preserve normalized texture coordinates");
}

} // namespace

static_assert(std::is_move_constructible<lotui::Texture>::value,
              "Texture must support RAII ownership transfer");
static_assert(!std::is_copy_constructible<lotui::Texture>::value,
              "Texture must not duplicate resource ownership");

int main() {
    validatesTextureImages();
    validatesPartialUpdates();
    boxPreservesTextureCoordinates();
    std::cout << "texture_tests passed\n";
    return EXIT_SUCCESS;
}
