#pragma once

#include "renderer/texture.h"
#include "text/text_layout.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace lotui {

struct FontSource {
    std::string family;
    std::filesystem::path file;
    std::uint32_t faceIndex{0};
    FontWeight weight{FontWeight::Normal};
    bool italic{false};
};

class FreetypeTextEngine final : public TextEngine {
public:
    FreetypeTextEngine(
        TextureStore& textureStore,
        std::vector<FontSource> fonts,
        std::uint32_t atlasSize = 1024);
    ~FreetypeTextEngine() override;

    std::unique_ptr<TextLayout> createLayout(
        std::string_view utf8Text,
        const TextStyle& style,
        const TextLayoutOptions& options) const override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace lotui
