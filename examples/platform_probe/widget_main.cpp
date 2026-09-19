#include "examples/platform_probe/demo_ui.h"
#include "examples/platform_probe/platform_event_bridge.h"

#include "platform/platform_backend.h"
#include "platform/runtime_paths.h"
#include "renderer/vulkan/vulkan_renderer.h"
#ifdef LOTUI_HAS_EXAMPLE_TEXT
#include "text/freetype/freetype_text_engine.h"
#endif

#include <chrono>
#include <exception>
#include <iomanip>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

int main() {
    try {
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
        auto tree = lotui::example::createDemoUi(
            demoMask.id(), textEngine);
        lotui::example::updateDemoLayout(*tree, window->metrics());
        window->setTextInputState(tree->textInputState());

        const auto initial = window->metrics();
        std::cout << "window-created: width=" << initial.width
                  << " height=" << initial.height
                  << " framebuffer=" << initial.framebufferWidth << 'x'
                  << initial.framebufferHeight
                  << " dpi-scale=" << std::fixed << std::setprecision(2)
                  << initial.dpiScale << '\n';

        std::vector<lotui::PaintCommand> commands;
        bool running = true;
        while (running) {
            bool receivedEvent = false;
            lotui::PlatformEvent event{};
            while (window->pollEvent(event)) {
                receivedEvent = true;
                running = lotui::example::dispatchPlatformEvent(
                    event, *tree, *window,
                    lotui::example::updateDemoLayout) && running;
            }

            if (running) {
                commands.clear();
                tree->paint(commands);
                window->setTextInputState(tree->textInputState());
                renderer.drawFrame(commands);
            }
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
