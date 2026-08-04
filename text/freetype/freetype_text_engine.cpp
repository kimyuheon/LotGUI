#include "text/freetype/freetype_text_engine.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include <hb-ft.h>
#include <hb.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <functional>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

namespace lotui {
namespace {

struct GlyphQuad {
    Rect bounds{};
    Rect textureCoordinates{};
};

class FreetypeLayout final : public TextLayout {
public:
    FreetypeLayout(
        Size size,
        float baseline,
        std::shared_ptr<const Texture> atlas,
        std::vector<GlyphQuad> glyphs)
        : size_(size),
          baseline_(baseline),
          atlas_(std::move(atlas)),
          glyphs_(std::move(glyphs)) {
    }

    Size size() const noexcept override {
        return size_;
    }

    float baseline() const noexcept override {
        return baseline_;
    }

    void appendPaintCommands(
        Point origin,
        Rect clip,
        Color color,
        std::vector<PaintCommand>& commands) const override {
        commands.reserve(commands.size() + glyphs_.size());
        for (const GlyphQuad& glyph : glyphs_) {
            commands.push_back({
                translated(glyph.bounds, origin),
                clip,
                color,
                atlas_->id(),
                0.0F,
                glyph.textureCoordinates,
            });
        }
    }

private:
    Size size_{};
    float baseline_{0.0F};
    std::shared_ptr<const Texture> atlas_;
    std::vector<GlyphQuad> glyphs_;
};

std::string lowerAscii(std::string value) {
    std::transform(
        value.begin(), value.end(), value.begin(),
        [](unsigned char character) {
            return character >= 'A' && character <= 'Z'
                ? static_cast<char>(character - 'A' + 'a')
                : static_cast<char>(character);
        });
    return value;
}

std::vector<std::uint8_t> readBinaryFile(
    const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream) {
        throw std::runtime_error(
            "failed to open font file: " + path.u8string());
    }
    const std::streamoff length = stream.tellg();
    if (length <= 0 ||
        static_cast<std::uint64_t>(length) >
            std::numeric_limits<std::size_t>::max()) {
        throw std::runtime_error(
            "font file is empty or too large: " + path.u8string());
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
    stream.seekg(0, std::ios::beg);
    stream.read(
        reinterpret_cast<char*>(bytes.data()),
        static_cast<std::streamsize>(bytes.size()));
    if (!stream) {
        throw std::runtime_error(
            "failed to read font file: " + path.u8string());
    }
    return bytes;
}

struct Utf8CodePoint {
    std::size_t begin{0};
    std::size_t end{0};
    std::uint32_t value{0xFFFDU};
};

std::vector<Utf8CodePoint> decodeUtf8(std::string_view text) {
    std::vector<Utf8CodePoint> result;
    std::size_t position = 0;
    while (position < text.size()) {
        const std::size_t begin = position;
        const auto first = static_cast<unsigned char>(text[position++]);
        std::uint32_t value = 0xFFFDU;
        std::size_t continuationCount = 0;
        if (first < 0x80U) {
            value = first;
        } else if ((first & 0xE0U) == 0xC0U) {
            value = first & 0x1FU;
            continuationCount = 1;
        } else if ((first & 0xF0U) == 0xE0U) {
            value = first & 0x0FU;
            continuationCount = 2;
        } else if ((first & 0xF8U) == 0xF0U) {
            value = first & 0x07U;
            continuationCount = 3;
        }

        bool valid = continuationCount > 0 || first < 0x80U;
        if (position + continuationCount > text.size()) {
            valid = false;
        }
        for (std::size_t index = 0;
             valid && index < continuationCount;
             ++index) {
            const auto next = static_cast<unsigned char>(text[position]);
            if ((next & 0xC0U) != 0x80U) {
                valid = false;
                break;
            }
            value = (value << 6U) | (next & 0x3FU);
            ++position;
        }
        if (!valid || value > 0x10FFFFU ||
            (value >= 0xD800U && value <= 0xDFFFU) ||
            (continuationCount == 1 && value < 0x80U) ||
            (continuationCount == 2 && value < 0x800U) ||
            (continuationCount == 3 && value < 0x10000U)) {
            position = begin + 1;
            value = 0xFFFDU;
        }
        result.push_back({begin, position, value});
    }
    return result;
}

} // namespace

class FreetypeTextEngine::Impl {
public:
    Impl(
        TextureStore& textureStore,
        std::vector<FontSource> fonts,
        std::uint32_t atlasSize)
        : textureStore_(textureStore),
          atlasSize_(atlasSize) {
        if (fonts.empty()) {
            throw std::invalid_argument(
                "FreetypeTextEngine requires at least one font");
        }
        if (atlasSize_ < 128 || atlasSize_ > 8192) {
            throw std::invalid_argument(
                "glyph atlas size must be between 128 and 8192");
        }
        if (FT_Init_FreeType(&library_) != 0) {
            throw std::runtime_error("failed to initialize FreeType");
        }

        try {
            for (FontSource& source : fonts) {
                addFace(std::move(source));
            }
            TextureImage atlasImage;
            atlasImage.width = atlasSize_;
            atlasImage.height = atlasSize_;
            atlasImage.format = TextureFormat::R8Unorm;
            atlasImage.pixels.resize(
                static_cast<std::size_t>(atlasSize_) * atlasSize_, 0);
            atlas_ = std::make_shared<Texture>(
                textureStore_.createTexture(atlasImage));
        } catch (...) {
            cleanup();
            throw;
        }
    }

    ~Impl() {
        cleanup();
    }

    std::unique_ptr<TextLayout> createLayout(
        std::string_view text,
        const TextStyle& style,
        const TextLayoutOptions& options) {
        if (!isValidTextStyle(style) ||
            !isValidTextLayoutOptions(options)) {
            throw std::invalid_argument("text layout request is invalid");
        }
        if (text.size() >
            static_cast<std::size_t>(std::numeric_limits<int>::max())) {
            throw std::length_error("text is too large to shape");
        }

        std::lock_guard<std::mutex> lock(mutex_);
        const std::uint32_t pixelSize = static_cast<std::uint32_t>(
            std::max(1.0F, std::round(style.fontSize)));
        const auto codePoints = decodeUtf8(text);
        if (codePoints.empty()) {
            FaceRecord& face = *faces_.front();
            prepareFace(face, pixelSize);
            const float height = lineHeight(face, style);
            const float baseline = ascender(face);
            return std::make_unique<FreetypeLayout>(
                Size{0.0F, height}, baseline, atlas_,
                std::vector<GlyphQuad>{});
        }

        struct Run {
            FaceRecord* face{nullptr};
            std::size_t begin{0};
            std::size_t end{0};
        };
        std::vector<Run> runs;
        for (const Utf8CodePoint& codePoint : codePoints) {
            FaceRecord* face = chooseFace(
                codePoint.value, style, pixelSize);
            if (!runs.empty() && runs.back().face == face &&
                runs.back().end == codePoint.begin) {
                runs.back().end = codePoint.end;
            } else {
                runs.push_back({face, codePoint.begin, codePoint.end});
            }
        }

        struct PendingGlyph {
            float x{0.0F};
            float topFromBaseline{0.0F};
            GlyphBitmap bitmap{};
        };
        std::vector<PendingGlyph> pending;
        float cursorX = 0.0F;
        float baseline = 0.0F;
        float automaticHeight = 0.0F;

        for (std::size_t runIndex = 0; runIndex < runs.size(); ++runIndex) {
            const Run& run = runs[runIndex];
            prepareFace(*run.face, pixelSize);
            baseline = std::max(baseline, ascender(*run.face));
            automaticHeight = std::max(
                automaticHeight, faceHeight(*run.face));

            std::unique_ptr<hb_buffer_t, decltype(&hb_buffer_destroy)> buffer(
                hb_buffer_create(), &hb_buffer_destroy);
            if (buffer == nullptr) {
                throw std::runtime_error("failed to create a HarfBuzz buffer");
            }
            const std::string_view runText = text.substr(
                run.begin, run.end - run.begin);
            hb_buffer_add_utf8(
                buffer.get(),
                runText.data(),
                static_cast<int>(runText.size()),
                0,
                static_cast<int>(runText.size()));
            hb_buffer_guess_segment_properties(buffer.get());
            hb_shape(run.face->harfbuzzFont, buffer.get(), nullptr, 0);

            unsigned int glyphCount = 0;
            const hb_glyph_info_t* infos = hb_buffer_get_glyph_infos(
                buffer.get(), &glyphCount);
            const hb_glyph_position_t* positions =
                hb_buffer_get_glyph_positions(buffer.get(), &glyphCount);
            for (unsigned int index = 0; index < glyphCount; ++index) {
                const GlyphBitmap bitmap = glyph(
                    *run.face, infos[index].codepoint, pixelSize);
                if (bitmap.width > 0 && bitmap.height > 0) {
                    pending.push_back({
                        cursorX + positions[index].x_offset / 64.0F +
                            bitmap.bearingX,
                        -positions[index].y_offset / 64.0F -
                            bitmap.bearingY,
                        bitmap,
                    });
                }
                cursorX += positions[index].x_advance / 64.0F;
                if (index + 1 < glyphCount || runIndex + 1 < runs.size()) {
                    cursorX += style.letterSpacing;
                }
            }
        }

        std::vector<GlyphQuad> glyphs;
        glyphs.reserve(pending.size());
        for (const PendingGlyph& glyph : pending) {
            glyphs.push_back({
                {
                    glyph.x,
                    baseline + glyph.topFromBaseline,
                    static_cast<float>(glyph.bitmap.width),
                    static_cast<float>(glyph.bitmap.height),
                },
                glyph.bitmap.textureCoordinates,
            });
        }

        const float requestedHeight = style.lineHeight > 0.0F
            ? style.lineHeight
            : automaticHeight;
        float width = std::max(0.0F, cursorX);
        if (std::isfinite(options.maximumWidth)) {
            width = std::min(width, options.maximumWidth);
        }
        return std::make_unique<FreetypeLayout>(
            Size{width, std::max(requestedHeight, baseline)},
            baseline,
            atlas_,
            std::move(glyphs));
    }

private:
    struct FaceRecord {
        FontSource source;
        std::string normalizedFamily;
        std::vector<std::uint8_t> data;
        FT_Face face{nullptr};
        hb_font_t* harfbuzzFont{nullptr};
        std::uint32_t currentPixelSize{0};
    };

    struct GlyphKey {
        const FaceRecord* face{nullptr};
        std::uint32_t glyphId{0};
        std::uint32_t pixelSize{0};

        bool operator==(const GlyphKey& other) const noexcept {
            return face == other.face && glyphId == other.glyphId &&
                pixelSize == other.pixelSize;
        }
    };

    struct GlyphKeyHash {
        std::size_t operator()(const GlyphKey& key) const noexcept {
            std::size_t result = std::hash<const FaceRecord*>{}(key.face);
            result ^= std::hash<std::uint32_t>{}(key.glyphId) +
                0x9e3779b9U + (result << 6U) + (result >> 2U);
            result ^= std::hash<std::uint32_t>{}(key.pixelSize) +
                0x9e3779b9U + (result << 6U) + (result >> 2U);
            return result;
        }
    };

    struct GlyphBitmap {
        std::uint32_t width{0};
        std::uint32_t height{0};
        float bearingX{0.0F};
        float bearingY{0.0F};
        Rect textureCoordinates{};
    };

    void addFace(FontSource source) {
        if (source.family.empty() || source.file.empty()) {
            throw std::invalid_argument("font source is incomplete");
        }
        auto record = std::make_unique<FaceRecord>();
        record->source = std::move(source);
        record->normalizedFamily = lowerAscii(record->source.family);
        record->data = readBinaryFile(record->source.file);
        if (FT_New_Memory_Face(
                library_,
                record->data.data(),
                static_cast<FT_Long>(record->data.size()),
                static_cast<FT_Long>(record->source.faceIndex),
                &record->face) != 0) {
            throw std::runtime_error(
                "failed to load font face: " +
                record->source.file.u8string());
        }
        FT_Select_Charmap(record->face, FT_ENCODING_UNICODE);
        record->harfbuzzFont = hb_ft_font_create_referenced(record->face);
        if (record->harfbuzzFont == nullptr) {
            FT_Done_Face(record->face);
            record->face = nullptr;
            throw std::runtime_error("failed to create a HarfBuzz font");
        }
        faces_.push_back(std::move(record));
    }

    void prepareFace(FaceRecord& face, std::uint32_t pixelSize) {
        if (face.currentPixelSize == pixelSize) {
            return;
        }
        if (FT_Set_Pixel_Sizes(face.face, 0, pixelSize) != 0) {
            throw std::runtime_error("failed to set font pixel size");
        }
        hb_ft_font_changed(face.harfbuzzFont);
        face.currentPixelSize = pixelSize;
    }

    FaceRecord* chooseFace(
        std::uint32_t codePoint,
        const TextStyle& style,
        std::uint32_t pixelSize) {
        const auto supports = [codePoint, pixelSize, this](FaceRecord& face) {
            prepareFace(face, pixelSize);
            return FT_Get_Char_Index(face.face, codePoint) != 0;
        };

        for (const std::string& requested : style.fontFamilies) {
            const std::string normalized = lowerAscii(requested);
            FaceRecord* best = nullptr;
            int bestScore = std::numeric_limits<int>::max();
            for (const auto& face : faces_) {
                if (normalized != "sans-serif" &&
                    face->normalizedFamily != normalized) {
                    continue;
                }
                if (!supports(*face)) {
                    continue;
                }
                const int weightDifference = std::abs(
                    static_cast<int>(face->source.weight) -
                    static_cast<int>(style.weight));
                const int score = weightDifference +
                    (face->source.italic == style.italic ? 0 : 1000);
                if (score < bestScore) {
                    best = face.get();
                    bestScore = score;
                }
            }
            if (best != nullptr) {
                return best;
            }
        }
        for (const auto& face : faces_) {
            if (supports(*face)) {
                return face.get();
            }
        }
        return faces_.front().get();
    }

    float ascender(const FaceRecord& face) const noexcept {
        return face.face->size->metrics.ascender / 64.0F;
    }

    float faceHeight(const FaceRecord& face) const noexcept {
        return face.face->size->metrics.height / 64.0F;
    }

    float lineHeight(const FaceRecord& face, const TextStyle& style) const {
        return style.lineHeight > 0.0F
            ? style.lineHeight
            : faceHeight(face);
    }

    GlyphBitmap glyph(
        FaceRecord& face,
        std::uint32_t glyphId,
        std::uint32_t pixelSize) {
        const GlyphKey key{&face, glyphId, pixelSize};
        const auto cached = glyphs_.find(key);
        if (cached != glyphs_.end()) {
            return cached->second;
        }

        prepareFace(face, pixelSize);
        if (FT_Load_Glyph(face.face, glyphId, FT_LOAD_DEFAULT) != 0 ||
            FT_Render_Glyph(face.face->glyph, FT_RENDER_MODE_NORMAL) != 0) {
            throw std::runtime_error("failed to rasterize a glyph");
        }
        const FT_GlyphSlot slot = face.face->glyph;
        const FT_Bitmap& bitmap = slot->bitmap;
        GlyphBitmap result;
        result.width = bitmap.width;
        result.height = bitmap.rows;
        result.bearingX = static_cast<float>(slot->bitmap_left);
        result.bearingY = static_cast<float>(slot->bitmap_top);

        if (result.width > 0 && result.height > 0) {
            if (bitmap.pixel_mode != FT_PIXEL_MODE_GRAY) {
                throw std::runtime_error(
                    "FreeType returned a non-grayscale glyph bitmap");
            }
            allocateAtlas(result.width, result.height);
            const std::uint32_t atlasX = atlasCursorX_;
            const std::uint32_t atlasY = atlasCursorY_;
            TextureUpdate update;
            update.x = atlasX;
            update.y = atlasY;
            update.width = result.width;
            update.height = result.height;
            update.pixels.resize(
                static_cast<std::size_t>(result.width) * result.height);
            for (std::uint32_t row = 0; row < result.height; ++row) {
                const int sourceRow = bitmap.pitch >= 0
                    ? static_cast<int>(row)
                    : static_cast<int>(result.height - 1 - row);
                const unsigned char* source = bitmap.buffer +
                    sourceRow * std::abs(bitmap.pitch);
                std::copy_n(
                    source,
                    result.width,
                    update.pixels.begin() +
                        static_cast<std::size_t>(row) * result.width);
            }
            textureStore_.updateTexture(*atlas_, update);
            result.textureCoordinates = {
                static_cast<float>(atlasX) / atlasSize_,
                static_cast<float>(atlasY) / atlasSize_,
                static_cast<float>(result.width) / atlasSize_,
                static_cast<float>(result.height) / atlasSize_,
            };
            atlasCursorX_ += result.width + 1;
            atlasRowHeight_ = std::max(
                atlasRowHeight_, result.height + 1);
        }
        glyphs_.emplace(key, result);
        return result;
    }

    void allocateAtlas(std::uint32_t width, std::uint32_t height) {
        if (width + 2 > atlasSize_ || height + 2 > atlasSize_) {
            throw std::runtime_error("glyph is larger than the atlas page");
        }
        if (atlasCursorX_ + width + 1 > atlasSize_) {
            atlasCursorX_ = 1;
            atlasCursorY_ += atlasRowHeight_;
            atlasRowHeight_ = 0;
        }
        if (atlasCursorY_ + height + 1 > atlasSize_) {
            throw std::runtime_error(
                "glyph atlas is full; multiple pages are not implemented yet");
        }
    }

    void cleanup() noexcept {
        atlas_.reset();
        glyphs_.clear();
        for (auto& face : faces_) {
            if (face->harfbuzzFont != nullptr) {
                hb_font_destroy(face->harfbuzzFont);
                face->harfbuzzFont = nullptr;
            }
            if (face->face != nullptr) {
                FT_Done_Face(face->face);
                face->face = nullptr;
            }
        }
        faces_.clear();
        if (library_ != nullptr) {
            FT_Done_FreeType(library_);
            library_ = nullptr;
        }
    }

    TextureStore& textureStore_;
    std::uint32_t atlasSize_{1024};
    FT_Library library_{nullptr};
    std::vector<std::unique_ptr<FaceRecord>> faces_;
    std::shared_ptr<Texture> atlas_;
    std::uint32_t atlasCursorX_{1};
    std::uint32_t atlasCursorY_{1};
    std::uint32_t atlasRowHeight_{0};
    std::unordered_map<GlyphKey, GlyphBitmap, GlyphKeyHash> glyphs_;
    std::mutex mutex_;
};

FreetypeTextEngine::FreetypeTextEngine(
    TextureStore& textureStore,
    std::vector<FontSource> fonts,
    std::uint32_t atlasSize)
    : impl_(std::make_unique<Impl>(
          textureStore, std::move(fonts), atlasSize)) {
}

FreetypeTextEngine::~FreetypeTextEngine() = default;

std::unique_ptr<TextLayout> FreetypeTextEngine::createLayout(
    std::string_view utf8Text,
    const TextStyle& style,
    const TextLayoutOptions& options) const {
    return impl_->createLayout(utf8Text, style, options);
}

} // namespace lotui
