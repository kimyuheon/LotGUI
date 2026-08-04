#pragma once

#include "core/paint_command.h"
#include "renderer/texture.h"

#include <memory>
#include <vector>

namespace lotui {

class PlatformWindow;

class VulkanRenderer : public TextureStore {
public:
    explicit VulkanRenderer(PlatformWindow& window);
    ~VulkanRenderer();

    VulkanRenderer(const VulkanRenderer&) = delete;
    VulkanRenderer& operator=(const VulkanRenderer&) = delete;
    VulkanRenderer(VulkanRenderer&&) = delete;
    VulkanRenderer& operator=(VulkanRenderer&&) = delete;

    void drawFrame(const std::vector<PaintCommand>& paintCommands);
    Texture createTexture(const TextureImage& image) override;
    void updateTexture(
        const Texture& texture,
        const TextureUpdate& update) override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace lotui
