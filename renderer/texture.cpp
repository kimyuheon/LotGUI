#include "renderer/texture.h"

#include "renderer/texture_private.h"

#include <limits>
#include <utility>

namespace lotui {
namespace {

bool expectedByteCount(
    std::uint32_t width,
    std::uint32_t height,
    std::size_t bytesPerPixel,
    std::size_t& result) noexcept {
    if (width == 0 || height == 0 || bytesPerPixel == 0) {
        return false;
    }
    const std::size_t pixelCount = static_cast<std::size_t>(width) * height;
    if (pixelCount >
        std::numeric_limits<std::size_t>::max() / bytesPerPixel) {
        return false;
    }
    result = pixelCount * bytesPerPixel;
    return true;
}

} // namespace

std::size_t textureBytesPerPixel(TextureFormat format) noexcept {
    switch (format) {
    case TextureFormat::R8Unorm:
        return 1;
    case TextureFormat::Rgba8Unorm:
        return 4;
    }
    return 0;
}

bool isValidTextureImage(const TextureImage& image) noexcept {
    std::size_t expected = 0;
    return expectedByteCount(
            image.width,
            image.height,
            textureBytesPerPixel(image.format),
            expected) &&
        image.pixels.size() == expected;
}

bool isValidTextureUpdate(
    const TextureUpdate& update,
    std::uint32_t textureWidth,
    std::uint32_t textureHeight,
    TextureFormat format) noexcept {
    if (update.x > textureWidth || update.y > textureHeight ||
        update.width > textureWidth - update.x ||
        update.height > textureHeight - update.y) {
        return false;
    }
    std::size_t expected = 0;
    return expectedByteCount(
            update.width,
            update.height,
            textureBytesPerPixel(format),
            expected) &&
        update.pixels.size() == expected;
}

Texture::Texture(
    TextureId id,
    std::shared_ptr<detail::TextureReleaseState> releaseState) noexcept
    : id_(id),
      releaseState_(std::move(releaseState)) {
}

Texture::~Texture() {
    reset();
}

Texture::Texture(Texture&& other) noexcept
    : id_(std::exchange(other.id_, invalidTextureId)),
      releaseState_(std::move(other.releaseState_)) {
}

Texture& Texture::operator=(Texture&& other) noexcept {
    if (this != &other) {
        reset();
        id_ = std::exchange(other.id_, invalidTextureId);
        releaseState_ = std::move(other.releaseState_);
    }
    return *this;
}

TextureId Texture::id() const noexcept {
    return id_;
}

Texture::operator bool() const noexcept {
    return id_ != invalidTextureId;
}

void Texture::reset() noexcept {
    if (id_ != invalidTextureId && releaseState_ &&
        releaseState_->active && releaseState_->release) {
        releaseState_->release(id_);
    }
    id_ = invalidTextureId;
    releaseState_.reset();
}

std::shared_ptr<detail::TextureReleaseState>
TextureStore::makeTextureReleaseState(
    std::function<void(TextureId)> release) {
    auto state = std::make_shared<detail::TextureReleaseState>();
    state->release = std::move(release);
    return state;
}

Texture TextureStore::adoptTexture(
    TextureId id,
    std::shared_ptr<detail::TextureReleaseState> releaseState) noexcept {
    return Texture(id, std::move(releaseState));
}

bool TextureStore::ownsTexture(
    const Texture& texture,
    const std::shared_ptr<detail::TextureReleaseState>& releaseState)
    noexcept {
    return texture && texture.releaseState_ == releaseState;
}

void TextureStore::deactivateTextureReleaseState(
    const std::shared_ptr<detail::TextureReleaseState>& releaseState)
    noexcept {
    if (releaseState) {
        releaseState->active = false;
        releaseState->release = {};
    }
}

} // namespace lotui
