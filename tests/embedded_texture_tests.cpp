#include "renderer/vulkan/vulkan_embedded_renderer.h"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "embedded_texture_tests failed: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

template<typename Action>
void requireInvalid(Action action, const char* message) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return;
    }
    require(false, message);
}

} // namespace

int main() {
    lotui::TextureImage image{4, 4, lotui::TextureFormat::R8Unorm,
        std::vector<std::uint8_t>(16, 0)};
    lotui::VulkanEmbeddedRenderer first;
    lotui::VulkanEmbeddedRenderer second;
    auto texture = first.createTexture(image);
    require(static_cast<bool>(texture), "texture creation failed");
    const lotui::TextureUpdate update{1, 1, 2, 2, {1, 2, 3, 4}};
    first.updateTexture(texture, update);
    requireInvalid([&] { second.updateTexture(texture, update); },
        "foreign renderer accepted a texture");
    requireInvalid([&] {
        first.updateTexture(texture,
            lotui::TextureUpdate{3, 3, 2, 2, {1, 2, 3, 4}});
    }, "out-of-bounds update was accepted");
    requireInvalid([&] {
        first.createTexture(lotui::TextureImage{});
    }, "invalid texture was accepted");
    texture.reset();
    require(!texture, "reset retained a texture handle");

    lotui::Texture retained;
    {
        auto owner = std::make_unique<lotui::VulkanEmbeddedRenderer>();
        retained = owner->createTexture(image);
    }
    retained.reset();
    std::cout << "embedded_texture_tests passed\n";
    return EXIT_SUCCESS;
}
