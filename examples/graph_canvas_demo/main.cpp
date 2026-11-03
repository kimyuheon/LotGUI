#include "examples/platform_probe/platform_event_bridge.h"

#include "core/widget_tree.h"
#include "platform/platform_backend.h"
#include "platform/runtime_paths.h"
#include "renderer/vulkan/vulkan_renderer.h"
#include "text/freetype/freetype_text_engine.h"
#include "widgets/graph_canvas.h"
#include "widgets/label.h"
#include "widgets/linear_layout.h"

#include <chrono>
#include <exception>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <vector>

namespace {

void writeCapture(const lotui::VulkanRenderer::CapturedFrame& frame,
    const char* path) {
    std::ofstream out(path, std::ios::binary);
    if (!out || frame.rgba.empty()) throw std::runtime_error("capture failed");
    out << "P6\n" << frame.width << ' ' << frame.height << "\n255\n";
    for (std::size_t i = 0; i < frame.rgba.size(); i += 4)
        out.write(reinterpret_cast<const char*>(frame.rgba.data() + i), 3);
    if (!out) throw std::runtime_error("capture write failed");
}

} // namespace

int main(int argc, char** argv) {
    try {
        const bool smoke = argc == 2 && std::string_view(argv[1]) == "--smoke";
        const bool capture = argc == 3 &&
            std::string_view(argv[1]) == "--capture";
        if (argc > 1 && !smoke && !capture) {
            std::cerr << "usage: graph_canvas_demo [--smoke|--capture output.ppm]\n";
            return 2;
        }

        auto platform = lotui::createPlatformBackend();
        auto window = platform->createWindow({"LotUI GraphCanvas", 1040, 640, true});
        window->show();
        lotui::VulkanRenderer renderer(*window);
        const auto font = lotui::executableDirectory() /
            "resources" / "fonts" / "NotoSansKR-Regular.ttf";
        auto text = std::make_shared<lotui::FreetypeTextEngine>(
            renderer, std::vector<lotui::FontSource>{{"Noto Sans KR", font}});

        lotui::TextStyle heading;
        heading.fontFamilies = {"Noto Sans KR"};
        heading.fontSize = 20.0F;
        auto root = std::make_unique<lotui::Column>();
        root->setDecoration(lotui::BoxDecoration{
            {0.009F, 0.012F, 0.018F, 1.0F}, 0.0F});
        lotui::LinearLayoutOptions options;
        options.padding = {18.0F, 16.0F, 18.0F, 16.0F};
        options.spacing = 8.0F;
        root->setOptions(options);
        root->addChild(std::make_unique<lotui::Label>(text,
            "LotUI 노드 그래프", heading),
            {0.0F, {0.0F, 32.0F}, {lotui::unboundedLayoutSize, 32.0F}});
        heading.fontSize = 13.0F;
        root->addChild(std::make_unique<lotui::Label>(text,
            "노드 선택·이동  |  빈 공간 드래그  |  휠 확대  |  Home 전체 보기",
            heading),
            {0.0F, {0.0F, 24.0F}, {lotui::unboundedLayoutSize, 24.0F}});
        auto canvas = std::make_unique<lotui::GraphCanvas>(text,
            lotui::Size{980.0F, 500.0F}, lotui::GraphCanvasStyle{}, heading);
        auto* graph = canvas.get();
        canvas->setGraph({
            {"source", "입력 데이터", {60.0F, 130.0F}, {178.0F, 112.0F},
                {}, {"메쉬", "속성"}, {0.34F, 0.77F, 0.96F, 1.0F}},
            {"transform", "변환", {350.0F, 76.0F}, {178.0F, 112.0F},
                {"메쉬"}, {"결과"}, {0.64F, 0.79F, 0.43F, 1.0F}},
            {"filter", "필터", {350.0F, 270.0F}, {178.0F, 112.0F},
                {"속성"}, {"선택"}, {0.91F, 0.60F, 0.76F, 1.0F}},
            {"output", "뷰포트", {660.0F, 170.0F}, {178.0F, 112.0F},
                {"형상", "선택"}, {}, {0.98F, 0.70F, 0.35F, 1.0F}}
        }, {
            {"mesh", "source", 0, "transform", 0},
            {"properties", "source", 1, "filter", 0},
            {"result", "transform", 0, "output", 0},
            {"selection", "filter", 0, "output", 1}
        });
        auto status = std::make_unique<lotui::Label>(text,
            "선택된 노드 없음", heading);
        auto* statusLabel = status.get();
        canvas->setOnSelectionChanged([statusLabel](auto id) {
            statusLabel->setText(id ? "선택: " + *id : "선택된 노드 없음");
        });
        canvas->setOnNodeMoved([statusLabel](std::string_view id,
                lotui::Point) {
            statusLabel->setText("이동: " + std::string(id));
        });
        root->addChild(std::move(canvas),
            {1.0F, {400.0F, 300.0F},
                {lotui::unboundedLayoutSize, lotui::unboundedLayoutSize}});
        root->addChild(std::move(status),
            {0.0F, {0.0F, 22.0F},
                {lotui::unboundedLayoutSize, 22.0F}});
        lotui::WidgetTree tree(std::move(root));
        const auto layout = [](lotui::WidgetTree& widgets,
            const lotui::WindowMetrics& metrics) {
            widgets.layout({0.0F, 0.0F,
                static_cast<float>(metrics.width),
                static_cast<float>(metrics.height)});
        };
        layout(tree, window->metrics());
        graph->fitToContent();

        std::vector<lotui::PaintCommand> commands;
        if (smoke) {
            const auto node = graph->nodeBounds("transform");
            const lotui::Point point{node.x + 50.0F, node.y + 15.0F};
            tree.pointerPressed(point, lotui::PointerButton::Primary);
            tree.pointerMoved({point.x + 20.0F, point.y + 10.0F});
            tree.pointerReleased({point.x + 20.0F, point.y + 10.0F},
                lotui::PointerButton::Primary);
            if (graph->selectedNode() != "transform" ||
                statusLabel->text() != "이동: transform" ||
                graph->nodes()[1].position.x <= 350.0F)
                throw std::runtime_error("node interaction failed");
            tree.paint(commands);
            renderer.drawFrame(commands);
            std::cout << "graph-canvas-smoke: " << commands.size()
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
                    event, tree, *window, layout) && running;
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
    } catch (const std::exception& error) {
        std::cerr << "graph_canvas_demo failed: " << error.what() << '\n';
        return 1;
    }
}
