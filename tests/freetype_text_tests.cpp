#include "text/freetype/freetype_text_engine.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

class MemoryTextureStore final : public lotui::TextureStore {
public:
    MemoryTextureStore()
        : releaseState_(makeTextureReleaseState(
              [this](lotui::TextureId id) { images_.erase(id); })) {
    }

    ~MemoryTextureStore() override {
        deactivateTextureReleaseState(releaseState_);
    }

    lotui::Texture createTexture(const lotui::TextureImage& image) override {
        require(lotui::isValidTextureImage(image), "invalid texture image");
        const lotui::TextureId id = nextId_++;
        images_.emplace(id, image);
        ++createCount_;
        return adoptTexture(id, releaseState_);
    }

    void updateTexture(
        const lotui::Texture& texture,
        const lotui::TextureUpdate& update) override {
        require(ownsTexture(texture, releaseState_),
            "texture belongs to a different store");
        auto found = images_.find(texture.id());
        require(found != images_.end(), "texture is no longer alive");
        lotui::TextureImage& image = found->second;
        require(lotui::isValidTextureUpdate(
            update, image.width, image.height, image.format),
            "invalid texture update");

        const std::size_t bytesPerPixel =
            lotui::textureBytesPerPixel(image.format);
        for (std::uint32_t row = 0; row < update.height; ++row) {
            const std::size_t destination =
                ((static_cast<std::size_t>(update.y) + row) * image.width +
                    update.x) * bytesPerPixel;
            const std::size_t source =
                static_cast<std::size_t>(row) * update.width * bytesPerPixel;
            std::copy_n(
                update.pixels.data() + source,
                static_cast<std::size_t>(update.width) * bytesPerPixel,
                image.pixels.data() + destination);
        }
        ++updateCount_;
    }

    std::size_t createCount() const noexcept { return createCount_; }
    std::size_t updateCount() const noexcept { return updateCount_; }
    std::size_t aliveTextureCount() const noexcept { return images_.size(); }

private:
    lotui::TextureId nextId_{1};
    std::shared_ptr<lotui::detail::TextureReleaseState> releaseState_;
    std::unordered_map<lotui::TextureId, lotui::TextureImage> images_;
    std::size_t createCount_{0};
    std::size_t updateCount_{0};
};

void testKoreanTextLayout(const std::filesystem::path& fontFile) {
    MemoryTextureStore textures;
    lotui::FreetypeTextEngine engine(
        textures,
        {{"Noto Sans KR", fontFile}},
        512);

    lotui::TextStyle style;
    style.fontFamilies = {"Noto Sans KR"};
    style.fontSize = 24.0F;
    style.letterSpacing = 0.5F;

    const auto layout = engine.createLayout(
        "LotUI 한글 입력",
        style,
        {});
    require(layout->size().width > 0.0F, "layout width is empty");
    require(layout->size().height > 0.0F, "layout height is empty");
    require(layout->baseline() > 0.0F, "layout baseline is empty");

    std::vector<lotui::PaintCommand> commands;
    layout->appendPaintCommands(
        {10.0F, 20.0F},
        {0.0F, 0.0F, 500.0F, 100.0F},
        {1.0F, 1.0F, 1.0F, 1.0F},
        commands);
    require(!commands.empty(), "layout emitted no glyph commands");
    require(textures.createCount() == 1, "expected one glyph atlas");
    require(textures.updateCount() > 0, "glyph atlas was not updated");
    for (const auto& command : commands) {
        require(command.texture != lotui::invalidTextureId,
            "glyph command has no texture");
        require(command.textureCoordinates.x >= 0.0F &&
                command.textureCoordinates.y >= 0.0F &&
                command.textureCoordinates.x +
                    command.textureCoordinates.width <= 1.0F &&
                command.textureCoordinates.y +
                    command.textureCoordinates.height <= 1.0F,
            "glyph UV is outside the atlas");
    }

    const std::size_t updatesAfterFirstLayout = textures.updateCount();
    const auto cachedLayout = engine.createLayout(
        "LotUI 한글 입력", style, {});
    require(cachedLayout->size().width == layout->size().width,
        "cached layout has a different width");
    require(textures.updateCount() == updatesAfterFirstLayout,
        "cached glyphs were uploaded twice");
}

void testLayoutKeepsAtlasAlive(const std::filesystem::path& fontFile) {
    MemoryTextureStore textures;
    std::unique_ptr<lotui::TextLayout> layout;
    {
        lotui::FreetypeTextEngine engine(
            textures,
            {{"Noto Sans KR", fontFile}},
            256);
        layout = engine.createLayout("한글", {}, {});
        require(textures.aliveTextureCount() == 1,
            "glyph atlas was not created");
    }
    require(textures.aliveTextureCount() == 1,
        "layout did not retain its glyph atlas");
    layout.reset();
    require(textures.aliveTextureCount() == 0,
        "glyph atlas outlived its last layout");
}

} // namespace

int main(int argc, char** argv) {
    try {
        require(argc > 0, "missing executable path");
        const auto fontFile = std::filesystem::path(argv[0])
            .parent_path() / "resources" / "fonts" /
            "NotoSansKR-Regular.ttf";
        require(std::filesystem::exists(fontFile), "test font is missing");
        testKoreanTextLayout(fontFile);
        testLayoutKeepsAtlasAlive(fontFile);
        std::cout << "freetype_text_tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "freetype_text_tests failed: " << error.what() << '\n';
        return 1;
    }
}
