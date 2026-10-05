#include "api/VulkanCAD_API.h"

#include "core/widget_tree.h"
#include "renderer/vulkan/vulkan_embedded_renderer.h"
#include "widgets/box.h"
#include "widgets/button.h"
#include "widgets/dialog.h"
#include "widgets/linear_layout.h"
#include "widgets/slider.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace {

template<typename Handle>
Handle fromBits(std::uint64_t bits) noexcept {
    if constexpr (std::is_pointer_v<Handle>) {
        return reinterpret_cast<Handle>(static_cast<std::uintptr_t>(bits));
    } else {
        return static_cast<Handle>(bits);
    }
}

struct Demo {
    lotui::VulkanEmbeddedRenderer renderer;
    std::unique_ptr<lotui::WidgetTree> tree;
    lotui::DialogHost* host{nullptr};
    lotui::Slider* slider{nullptr};
    bool primaryDown{false};
    std::uint32_t lastInputFlags{0};

    Demo() {
        auto toolbar = std::make_unique<lotui::Row>();
        lotui::LinearLayoutOptions layout;
        layout.padding = lotui::EdgeInsets::all(16.0F);
        layout.spacing = 12.0F;
        toolbar->setOptions(layout);

        lotui::ButtonStyle openStyle;
        openStyle.normal = {0.10F, 0.55F, 0.47F, 0.96F};
        openStyle.hovered = {0.13F, 0.69F, 0.58F, 1.0F};
        openStyle.pressed = {0.07F, 0.41F, 0.35F, 1.0F};
        openStyle.cornerRadius = 5.0F;
        toolbar->addChild(std::make_unique<lotui::Button>(
            lotui::Size{116.0F, 32.0F}, [this] { showModal(); },
            openStyle));
        lotui::SliderOptions sliderOptions;
        sliderOptions.value = 35.0;
        sliderOptions.preferredSize = {160.0F, 24.0F};
        auto sliderWidget = std::make_unique<lotui::Slider>(sliderOptions);
        slider = sliderWidget.get();
        toolbar->addChild(std::move(sliderWidget));

        auto dialogHost = std::make_unique<lotui::DialogHost>(
            std::move(toolbar));
        host = dialogHost.get();
        tree = std::make_unique<lotui::WidgetTree>(std::move(dialogHost));

        auto modelessContent = std::make_unique<lotui::Box>(
            lotui::Size{170.0F, 70.0F},
            lotui::Color{0.17F, 0.63F, 0.78F, 1.0F});
        auto modeless = std::make_unique<lotui::Dialog>(
            std::move(modelessContent), lotui::Size{224.0F, 128.0F});
        host->showModeless(std::move(modeless),
            {250.0F, 20.0F, 224.0F, 128.0F});
    }

    void showModal() {
        if (host->hasModal()) {
            return;
        }
        lotui::ButtonStyle closeStyle;
        closeStyle.normal = {0.81F, 0.34F, 0.29F, 1.0F};
        closeStyle.hovered = {0.94F, 0.42F, 0.33F, 1.0F};
        closeStyle.cornerRadius = 5.0F;
        auto close = std::make_unique<lotui::Button>(
            lotui::Size{110.0F, 32.0F},
            [this] { host->dismissModal(); }, closeStyle);
        auto dialog = std::make_unique<lotui::Dialog>(
            std::move(close), lotui::Size{280.0F, 160.0F});
        host->showModal(std::move(dialog));
    }

    std::uint32_t input(const CAD_OverlayPointerInfo& source) {
        if (source.windowWidth <= 0 || source.windowHeight <= 0) {
            return 0;
        }
        tree->layout({0.0F, 0.0F,
            static_cast<float>(source.windowWidth),
            static_cast<float>(source.windowHeight)});

        const lotui::Point pointer{
            static_cast<float>(source.x),
            static_cast<float>(source.y)};
        const auto move = tree->pointerMoved(pointer);
        const bool down = (source.buttons & 1U) != 0;
        bool handled = move.handled;
        if (down && !primaryDown) {
            handled |= tree->pointerPressed(
                pointer, lotui::PointerButton::Primary).handled;
        } else if (!down && primaryDown) {
            handled |= tree->pointerReleased(
                pointer, lotui::PointerButton::Primary).handled;
        }
        primaryDown = down;

        lastInputFlags = host->hasModal()
            ? CAD_OVERLAY_CAPTURE_POINTER | CAD_OVERLAY_CAPTURE_KEYBOARD
            : (handled ? CAD_OVERLAY_CAPTURE_POINTER : 0);
        return lastInputFlags;
    }

    std::uint32_t render(const CAD_OverlayFrameInfo& source) {
        if (source.framebufferWidth == 0 || source.framebufferHeight == 0 ||
            source.windowWidth <= 0 || source.windowHeight <= 0) {
            return 0;
        }
        tree->layout({0.0F, 0.0F,
            static_cast<float>(source.windowWidth),
            static_cast<float>(source.windowHeight)});

        std::vector<lotui::PaintCommand> commands;
        tree->paint(commands);
        lotui::VulkanEmbeddedFrame frame;
        frame.physicalDevice = fromBits<VkPhysicalDevice>(
            source.physicalDevice);
        frame.device = fromBits<VkDevice>(source.device);
        frame.renderPass = fromBits<VkRenderPass>(source.renderPass);
        frame.commandBuffer = fromBits<VkCommandBuffer>(source.commandBuffer);
        frame.frameIndex = source.frameIndex;
        frame.frameCount = source.frameCount;
        frame.framebufferWidth = source.framebufferWidth;
        frame.framebufferHeight = source.framebufferHeight;
        frame.dpiScale = source.dpiScale;
        if (source.structSize >= offsetof(CAD_OverlayFrameInfo, sampleCount) +
                sizeof(source.sampleCount)) {
            frame.samples = static_cast<VkSampleCountFlagBits>(
                source.sampleCount);
        }
        renderer.draw(frame, commands);

        return lastInputFlags;
    }
};

std::uint32_t input(const CAD_OverlayPointerInfo* pointer, void* user) {
    if (!pointer || !user ||
        pointer->structSize < sizeof(CAD_OverlayPointerInfo)) {
        return 0;
    }
    return static_cast<Demo*>(user)->input(*pointer);
}

std::uint32_t overlay(const CAD_OverlayFrameInfo* frame, void* user) {
    if (!frame || !user ||
        frame->structSize < offsetof(CAD_OverlayFrameInfo, modifiers) +
            sizeof(frame->modifiers)) {
        return 0;
    }
    return static_cast<Demo*>(user)->render(*frame);
}

} // namespace

int main(int argc, char** argv) {
    try {
        {
            Demo demo;
            CAD_SetOverlayRenderCallback(&overlay, &demo);
            CAD_SetOverlayInputCallback(&input, &demo);
            if (!CAD_CreateEngine()) {
                throw std::runtime_error("CAD_CreateEngine failed");
            }
            const bool capture = argc == 3 &&
                std::string(argv[1]) == "--capture";
            const bool inputTest = argc == 3 &&
                std::string(argv[1]) == "--input-test";
            if (capture || inputTest) {
                if (capture) {
                    demo.showModal();
                }
                const std::filesystem::path capturePath = argv[2];
                bool captured = false;
                for (int tick = 0; tick < 240 && !CAD_ShouldClose(); ++tick) {
                    if (inputTest && tick == 5) {
                        CAD_OnMouseMove(30.0, 30.0);
                        CAD_OnMouseDown(0, 30.0, 30.0, 0);
                    }
                    if (inputTest && tick == 6) {
                        if ((demo.lastInputFlags &
                                CAD_OVERLAY_CAPTURE_POINTER) == 0) {
                            throw std::runtime_error(
                                "LotUI did not capture the pointer press");
                        }
                        CAD_OnMouseUp(0, 30.0, 30.0, 0);
                    }
                    if (inputTest && tick == 7 &&
                        !demo.host->hasModal()) {
                        throw std::runtime_error(
                            "LotUI button did not open the modal");
                    }
                    if (inputTest && tick == 7) {
                        demo.host->dismissModal();
                    }
                    if (inputTest && tick == 8) {
                        CAD_OnMouseMove(170.0, 30.0);
                        CAD_OnMouseDown(0, 170.0, 30.0, 0);
                    }
                    if (inputTest && tick == 9) {
                        CAD_OnMouseMove(295.0, 30.0);
                    }
                    if (inputTest && tick == 10) {
                        CAD_OnMouseUp(0, 295.0, 30.0, 0);
                    }
                    if (inputTest && tick == 11 &&
                        demo.slider->value() < 90.0) {
                        throw std::runtime_error(
                            "LotUI slider did not receive the drag");
                    }
                    if (tick == 20 &&
                        !CAD_CaptureViewport(argv[2], false)) {
                        throw std::runtime_error("capture request failed");
                    }
                    if (!CAD_Tick()) {
                        break;
                    }
                    if (tick > 20 &&
                        std::filesystem::exists(capturePath) &&
                        std::filesystem::file_size(capturePath) > 1024) {
                        captured = true;
                        break;
                    }
                }
                if (!captured) {
                    throw std::runtime_error("capture was not written");
                }
            } else {
                while (!CAD_ShouldClose() && CAD_Tick()) {}
            }
            CAD_SetOverlayInputCallback(nullptr, nullptr);
            CAD_SetOverlayRenderCallback(nullptr, nullptr);
        }
        // Demo owns Vulkan resources borrowed from the engine. It must die
        // before CAD_DestroyEngine destroys the device.
        CAD_DestroyEngine();
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        CAD_SetOverlayInputCallback(nullptr, nullptr);
        CAD_SetOverlayRenderCallback(nullptr, nullptr);
        std::cerr << "VulkanAppLotGUI failed: " << error.what() << '\n';
        CAD_DestroyEngine();
        return EXIT_FAILURE;
    }
}
