#include "examples/platform_probe/demo_ui.h"
#include "examples/platform_probe/platform_event_bridge.h"

#include "platform/platform_backend.h"
#include "platform/runtime_paths.h"
#include "renderer/vulkan/vulkan_renderer.h"
#if __has_include("widgets/tree_view.h")
#include "widgets/tree_view.h"
#endif
#ifdef LOTUI_HAS_EXAMPLE_TEXT
#include "text/freetype/freetype_text_engine.h"
#endif

#include <chrono>
#include <algorithm>
#include <exception>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <vector>

int main(int argc, char** argv) {
    try {
        const bool smoke = argc == 2 &&
            std::string_view(argv[1]) == "--smoke";
        const bool capture = argc == 3 &&
            (std::string_view(argv[1]) == "--capture" ||
             std::string_view(argv[1]) == "--capture-drag");
        const bool captureDrag = capture &&
            std::string_view(argv[1]) == "--capture-drag";
        if ((argc > 1 && !smoke && !capture)) {
            std::cerr << "usage: platform_probe [--smoke|--capture output.ppm|--capture-drag output.ppm]\n";
            return 2;
        }
        auto platform = lotui::createPlatformBackend();
        auto window = platform->createWindow(
            {"LotUI Platform Probe", 960, 640, true});
        window->show();

        lotui::VulkanRenderer renderer(*window);
        lotui::Texture demoMask = renderer.createTexture(
            lotui::example::createDemoMask());
        std::shared_ptr<const lotui::TextEngine> textEngine;
#ifdef LOTUI_HAS_EXAMPLE_TEXT
        const auto fontFile = lotui::executableDirectory() /
            "resources" / "fonts" / "NotoSansKR-Regular.ttf";
        textEngine = std::make_shared<lotui::FreetypeTextEngine>(
            renderer,
            std::vector<lotui::FontSource>{
                {"Noto Sans KR", fontFile}});
        std::cout << "font-loaded: " << fontFile.string() << '\n';
#endif
        lotui::TreeView* sceneTree = nullptr;
        auto tree = lotui::example::createDemoUi(
            demoMask.id(), textEngine, *window, &sceneTree);
        lotui::example::updateDemoLayout(*tree, window->metrics());
#ifdef LOTUI_TREE_VIEW_HAS_DROP
        if (captureDrag) {
            if (!sceneTree) throw std::runtime_error("scene tree is missing");
            const auto source = sceneTree->labelBounds("shaft");
            const auto target = sceneTree->labelBounds("fasteners");
            const lotui::Point from{source.x + 20.0F,
                source.y + source.height * 0.5F};
            const lotui::Point to{target.x + 20.0F,
                target.y + target.height * 0.5F};
            tree->pointerMoved(from);
            tree->pointerPressed(from, lotui::PointerButton::Primary);
            tree->pointerMoved(to);
            tree->pointerReleased(to, lotui::PointerButton::Primary);
            const auto& nodes = sceneTree->nodes();
            const auto moved = std::find_if(nodes.begin(), nodes.end(),
                [](const auto& node) { return node.id == "shaft"; });
            const auto group = std::find_if(nodes.begin(), nodes.end(),
                [](const auto& node) { return node.id == "fasteners"; });
            if (moved == nodes.end() || group == nodes.end() ||
                moved < group || moved->depth != group->depth + 1) {
                throw std::runtime_error("scene tree drag did not move shaft");
            }
        }
#else
        if (captureDrag) throw std::runtime_error(
            "this SDK does not include TreeView drag support");
#endif
        window->setTextInputState(tree->textInputState());

        const auto initial = window->metrics();
        std::cout << "window-created: width=" << initial.width
                  << " height=" << initial.height
                  << " framebuffer=" << initial.framebufferWidth << 'x'
                  << initial.framebufferHeight
                  << " dpi-scale=" << std::fixed << std::setprecision(2)
                  << initial.dpiScale << '\n';

        std::vector<lotui::PaintCommand> commands;
        if (smoke) {
            tree->paint(commands);
            renderer.drawFrame(commands);
            std::cout << "platform-probe-smoke: " << commands.size()
                      << " paint commands\n";
            return 0;
        }
        bool running = true;
        int frames = 0;
        while (running) {
            bool receivedEvent = false;
            lotui::PlatformEvent event{};
            while (window->pollEvent(event)) {
                receivedEvent = true;
                running = lotui::example::dispatchPlatformEvent(
                    event, *tree, *window,
                    lotui::example::updateDemoLayout) && running;
            }

            if (receivedEvent) {
                lotui::example::updateDemoLayout(*tree, window->metrics());
            }

            if (running) {
                commands.clear();
                tree->paint(commands);
                window->setTextInputState(tree->textInputState());
                if (capture && frames == 7) {
                    const auto captured = renderer.drawFrameCaptured(commands);
                    std::ofstream output(argv[2], std::ios::binary);
                    if (!output || captured.rgba.empty()) {
                        throw std::runtime_error("platform probe capture failed");
                    }
                    output << "P6\n" << captured.width << ' ' <<
                        captured.height << "\n255\n";
                    for (std::size_t pixel = 0;
                            pixel < captured.rgba.size(); pixel += 4) {
                        output.write(reinterpret_cast<const char*>(
                            captured.rgba.data() + pixel), 3);
                    }
                    if (!output) {
                        throw std::runtime_error("platform probe capture write failed");
                    }
                } else {
                    renderer.drawFrame(commands);
                }
                ++frames;
            }
            if (capture && frames >= 8) running = false;
            if (!receivedEvent) {
                std::this_thread::sleep_for(std::chrono::milliseconds(8));
            }
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "platform_probe failed: " << error.what() << '\n';
        return 1;
    }
}
