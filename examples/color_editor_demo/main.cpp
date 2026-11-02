#include "examples/platform_probe/platform_event_bridge.h"

#include "platform/platform_backend.h"
#include "platform/runtime_paths.h"
#include "renderer/vulkan/vulkan_renderer.h"
#include "text/freetype/freetype_text_engine.h"
#include "widgets/color_editor.h"
#include "widgets/label.h"
#include "widgets/linear_layout.h"

#include <chrono>
#include <exception>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {

void writeCapture(const lotui::VulkanRenderer::CapturedFrame& frame,
    const char* filename) {
    std::ofstream output(filename, std::ios::binary);
    if (!output || frame.rgba.empty())
        throw std::runtime_error("color editor capture failed");
    output << "P6\n" << frame.width << ' ' << frame.height << "\n255\n";
    for (std::size_t i = 0; i < frame.rgba.size(); i += 4)
        output.write(reinterpret_cast<const char*>(
            frame.rgba.data() + i), 3);
    if (!output) throw std::runtime_error("capture write failed");
}

} // namespace

int main(int argc, char** argv) {
    try {
        const bool smoke = argc == 2 &&
            std::string_view(argv[1]) == "--smoke";
        const bool capture = argc == 3 &&
            std::string_view(argv[1]) == "--capture";
        if (argc > 1 && !smoke && !capture) {
            std::cerr << "usage: color_editor_demo [--smoke|--capture output.ppm]\n";
            return 2;
        }

        auto platform = lotui::createPlatformBackend();
        auto window = platform->createWindow(
            {"LotUI Color Editor", 560, 550, true});
        window->show();
        lotui::VulkanRenderer renderer(*window);
        const auto font = lotui::executableDirectory() /
            "resources" / "fonts" / "NotoSansKR-Regular.ttf";
        auto textEngine = std::make_shared<lotui::FreetypeTextEngine>(
            renderer, std::vector<lotui::FontSource>{{"Noto Sans KR", font}});

        auto root = std::make_unique<lotui::Column>();
        root->setDecoration(lotui::BoxDecoration{
            {0.009F, 0.012F, 0.018F, 1.0F}, 0.0F});
        lotui::LinearLayoutOptions options;
        options.padding = {22.0F, 18.0F, 22.0F, 18.0F};
        options.spacing = 10.0F;
        options.crossAxisAlignment = lotui::CrossAxisAlignment::Start;
        root->setOptions(options);

        lotui::TextStyle heading;
        heading.fontFamilies = {"Noto Sans KR"};
        heading.fontSize = 20.0F;
        root->addChild(std::make_unique<lotui::Label>(
            textEngine, "LotUI 색상 편집기", heading),
            {0.0F, {0.0F, 34.0F}, {lotui::unboundedLayoutSize, 34.0F}});
        lotui::TextStyle detail = heading;
        detail.fontSize = 13.0F;
        root->addChild(std::make_unique<lotui::Label>(textEngine,
            "객체 · 레이어 · 플러그인 속성에 같은 컨트롤 사용", detail),
            {0.0F, {0.0F, 24.0F}, {lotui::unboundedLayoutSize, 24.0F}});

        lotui::TextStyle rowLabel = heading;
        rowLabel.fontSize = 15.0F;
        auto addEditor = [&](std::string_view name, lotui::Color initial) {
            root->addChild(std::make_unique<lotui::Label>(textEngine,
                std::string(name), rowLabel),
                {0.0F, {0.0F, 22.0F},
                    {lotui::unboundedLayoutSize, 22.0F}});
            auto editor = std::make_unique<lotui::ColorEditor>(textEngine,
                initial, [name](lotui::Color color) {
                    std::cout << name << ": " << color.red << ", " <<
                        color.green << ", " << color.blue << '\n';
                }, lotui::Size{280.0F, 100.0F}, lotui::ColorEditorStyle{},
                detail);
            editor->setOnEditStarted([name]() {
                std::cout << name << ": edit-start\n";
            });
            editor->setOnEditFinished([name]() {
                std::cout << name << ": edit-finish\n";
            });
            root->addChild(std::move(editor),
                {0.0F, {280.0F, 100.0F}, {280.0F, 100.0F}});
        };
        addEditor("객체 색상", {0.82F, 0.34F, 0.20F, 1.0F});
        addEditor("레이어 색상", {0.18F, 0.61F, 0.83F, 1.0F});
        addEditor("플러그인 색상", {0.45F, 0.78F, 0.31F, 1.0F});

        lotui::WidgetTree tree(std::move(root));
        auto updateLayout = [](lotui::WidgetTree& widgets,
                const lotui::WindowMetrics& metrics) {
            widgets.layout({0.0F, 0.0F,
                static_cast<float>(metrics.width),
                static_cast<float>(metrics.height)});
        };
        updateLayout(tree, window->metrics());
        std::vector<lotui::PaintCommand> commands;
        if (smoke) {
            tree.paint(commands);
            renderer.drawFrame(commands);
            std::cout << "color-editor-smoke: " << commands.size()
                << " paint commands\n";
            return 0;
        }

        bool running = true;
        int frame = 0;
        while (running) {
            bool received = false;
            lotui::PlatformEvent event{};
            while (window->pollEvent(event)) {
                received = true;
                running = lotui::example::dispatchPlatformEvent(
                    event, tree, *window, updateLayout) && running;
            }
            if (!running) break;
            commands.clear();
            tree.paint(commands);
            if (capture && frame == 6) {
                writeCapture(renderer.drawFrameCaptured(commands), argv[2]);
                return 0;
            }
            renderer.drawFrame(commands);
            ++frame;
            if (!received)
                std::this_thread::sleep_for(std::chrono::milliseconds(8));
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "color_editor_demo failed: " << error.what() << '\n';
        return 1;
    }
}
