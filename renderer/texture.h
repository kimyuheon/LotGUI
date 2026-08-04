#pragma once

#include "core/paint_command.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace lotui {
namespace detail {
struct TextureReleaseState;
}

enum class TextureFormat : std::uint8_t {
    R8Unorm,
    Rgba8Unorm,
};

struct TextureImage {
    std::uint32_t width{0};
    std::uint32_t height{0};
    TextureFormat format{TextureFormat::R8Unorm};
    std::vector<std::uint8_t> pixels;
};

struct TextureUpdate {
    std::uint32_t x{0};
    std::uint32_t y{0};
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::vector<std::uint8_t> pixels;
};

std::size_t textureBytesPerPixel(TextureFormat format) noexcept;
bool isValidTextureImage(const TextureImage& image) noexcept;
bool isValidTextureUpdate(
    const TextureUpdate& update,
    std::uint32_t textureWidth,
    std::uint32_t textureHeight,
    TextureFormat format) noexcept;

class Texture final {
public:
    Texture() noexcept = default;
    ~Texture();

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    Texture(Texture&& other) noexcept;
    Texture& operator=(Texture&& other) noexcept;

    TextureId id() const noexcept;
    explicit operator bool() const noexcept;
    void reset() noexcept;

private:
    friend class TextureStore;
    friend class VulkanRenderer;

    Texture(
        TextureId id,
        std::shared_ptr<detail::TextureReleaseState> releaseState) noexcept;

    TextureId id_{invalidTextureId};
    std::shared_ptr<detail::TextureReleaseState> releaseState_;
};

class TextureStore {
public:
    virtual ~TextureStore() = default;

    TextureStore(const TextureStore&) = delete;
    TextureStore& operator=(const TextureStore&) = delete;
    TextureStore(TextureStore&&) = delete;
    TextureStore& operator=(TextureStore&&) = delete;

    virtual Texture createTexture(const TextureImage& image) = 0;
    virtual void updateTexture(
        const Texture& texture,
        const TextureUpdate& update) = 0;

protected:
    TextureStore() = default;

    static std::shared_ptr<detail::TextureReleaseState>
    makeTextureReleaseState(std::function<void(TextureId)> release);
    static Texture adoptTexture(
        TextureId id,
        std::shared_ptr<detail::TextureReleaseState> releaseState) noexcept;
    static bool ownsTexture(
        const Texture& texture,
        const std::shared_ptr<detail::TextureReleaseState>& releaseState)
        noexcept;
    static void deactivateTextureReleaseState(
        const std::shared_ptr<detail::TextureReleaseState>& releaseState)
        noexcept;
};

} // namespace lotui
