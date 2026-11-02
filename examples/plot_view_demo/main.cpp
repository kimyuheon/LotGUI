#include "examples/platform_probe/platform_event_bridge.h"

#include "core/widget_tree.h"
#include "platform/platform_backend.h"
#include "platform/runtime_paths.h"
#include "renderer/vulkan/vulkan_renderer.h"
#include "text/freetype/freetype_text_engine.h"
#include "widgets/box.h"
#include "widgets/label.h"
#include "widgets/linear_layout.h"
#include "widgets/plot_view.h"
#include "widgets/tree_view.h"

#include <chrono>
#include <cmath>
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
    const char* path) {
    std::ofstream out(path, std::ios::binary);
    if (!out || frame.rgba.empty()) throw std::runtime_error("capture failed");
    out << "P6\n" << frame.width << ' ' << frame.height << "\n255\n";
    for (std::size_t i = 0; i < frame.rgba.size(); i += 4)
        out.write(reinterpret_cast<const char*>(frame.rgba.data() + i), 3);
    if (!out) throw std::runtime_error("capture write failed");
}
}

int main(int argc, char** argv) {
    try {
        const bool smoke = argc == 2 && std::string_view(argv[1]) == "--smoke";
        const bool capture = argc == 3 && std::string_view(argv[1]) == "--capture";
        const bool objectSmoke = argc == 2 &&
            std::string_view(argv[1]) == "--smoke-objects";
        const bool objectCapture = argc == 3 &&
            std::string_view(argv[1]) == "--capture-objects";
        const bool objects = argc == 2 &&
            std::string_view(argv[1]) == "--objects";
        const bool objectMode = objectSmoke || objectCapture || objects;
        if (argc > 1 && !smoke && !capture && !objectMode) {
            std::cerr << "usage: plot_view_demo [--smoke|--capture output.ppm|"
                "--objects|--smoke-objects|--capture-objects output.ppm]\n";
            return 2;
        }
        auto platform = lotui::createPlatformBackend();
        auto window = platform->createWindow({objectMode
            ? "LotUI Object Plot" : "LotUI PlotView", 900, 560, true});
        window->show();
        lotui::VulkanRenderer renderer(*window);
        const auto font = lotui::executableDirectory() /
            "resources" / "fonts" / "NotoSansKR-Regular.ttf";
        auto text = std::make_shared<lotui::FreetypeTextEngine>(
            renderer, std::vector<lotui::FontSource>{{"Noto Sans KR", font}});

        auto root = std::make_unique<lotui::Column>();
        root->setDecoration(lotui::BoxDecoration{
            {0.009F, 0.012F, 0.018F, 1.0F}, 0.0F});
        lotui::LinearLayoutOptions options;
        options.padding = {22.0F, 20.0F, 22.0F, 20.0F};
        options.spacing = 10.0F;
        root->setOptions(options);
        lotui::TextStyle heading;
        heading.fontFamilies = {"Noto Sans KR"};
        heading.fontSize = 21.0F;
        root->addChild(std::make_unique<lotui::Label>(text,
            objectMode ? "LotUI 그래프 · 객체 치수 선택"
                       : "LotUI 그래프 · 시뮬레이션 결과", heading),
            {0.0F, {0.0F, 34.0F}, {lotui::unboundedLayoutSize, 34.0F}});
        heading.fontSize = 13.0F;
        root->addChild(std::make_unique<lotui::Label>(text,
            objectMode
                ? "왼쪽에서 객체를 선택 · 스크롤 확대 · 드래그 이동 · Home 전체 보기"
                : "스크롤: 확대/축소    드래그: 이동    Home: 전체 보기",
            heading),
            {0.0F, {0.0F, 24.0F}, {lotui::unboundedLayoutSize, 24.0F}});
        auto plot = std::make_unique<lotui::PlotView>(text,
            lotui::Size{850.0F, 370.0F}, lotui::PlotViewStyle{}, heading);
        auto* graph = plot.get();
        lotui::TreeView* objectTree = nullptr;
        std::vector<bool> included(24, true);
        if (objectMode) {
            auto populate = [graph, &included]() {
                std::vector<lotui::PlotPoint> widths, depths, heights;
                for (std::size_t i = 0; i < included.size(); ++i) {
                    if (!included[i]) continue;
                    const double x = static_cast<double>(i + 1);
                    widths.push_back({x, 0.8 + 0.025 * i});
                    depths.push_back({x, 0.7 + 0.18 *
                        (1.0 + std::sin(i * 0.65))});
                    heights.push_back({x, 0.9 + 0.035 * i +
                        0.12 * std::sin(i * 0.42)});
                }
                graph->setSeries({
                    {"width", "Width", std::move(widths),
                        {0.98F, 0.67F, 0.32F, 1.0F}, true},
                    {"depth", "Depth", std::move(depths),
                        {0.31F, 0.75F, 0.96F, 1.0F}, true},
                    {"height", "Height", std::move(heights),
                        {0.55F, 0.84F, 0.62F, 1.0F}, true}});
            };
            populate();
            std::vector<lotui::TreeNode> nodes;
            for (std::size_t i = 0; i < included.size(); ++i)
                nodes.push_back({"object:" + std::to_string(i),
                    "Graph box " + std::to_string(i + 1), 0, false,
                    true, true, true, false});
            lotui::TreeViewStyle selectorStyle;
            selectorStyle.rowHeight = 24.0F;
            auto selector = std::make_unique<lotui::TreeView>(text,
                std::move(nodes), lotui::Size{220.0F, 370.0F},
                selectorStyle, heading);
            objectTree = selector.get();
            selector->setOnCheckedChanged([&included, populate](
                    std::string_view key, bool checked) {
                const auto index = static_cast<std::size_t>(
                    std::stoul(std::string(key.substr(7))));
                if (index >= included.size()) return;
                included[index] = checked;
                populate();
            });
            selector->setOnSelectionChanged([objectTree, &included,
                    populate](auto key) {
                if (!key) return;
                const auto index = static_cast<std::size_t>(
                    std::stoul(std::string(key->substr(7))));
                if (index >= included.size()) return;
                included[index] = true;
                objectTree->setChecked(*key, true);
                populate();
            });
            auto row = std::make_unique<lotui::Row>();
            lotui::LinearLayoutOptions rowOptions;
            rowOptions.crossAxisAlignment =
                lotui::CrossAxisAlignment::Stretch;
            row->setOptions(rowOptions);
            row->addChild(std::move(selector),
                {0.0F, {220.0F, 0.0F},
                    {220.0F, lotui::unboundedLayoutSize}});
            row->addChild(std::move(plot),
                {1.0F, {300.0F, 270.0F},
                    {lotui::unboundedLayoutSize,
                     lotui::unboundedLayoutSize}});
            root->addChild(std::move(row),
                {1.0F, {0.0F, 270.0F},
                    {lotui::unboundedLayoutSize,
                     lotui::unboundedLayoutSize}});
        } else {
            std::vector<lotui::PlotPoint> temperature, pressure, flow;
            for (int i = 0; i <= 2400; ++i) {
                const double x = i / 120.0;
                temperature.push_back({x, 24.0 + 34.0 *
                    (1.0 - std::exp(-x / 5.0)) + 1.8 * std::sin(x * 2.0)});
                pressure.push_back({x, 33.0 + 8.0 * std::sin(x * 0.72) *
                    std::exp(-x / 25.0)});
                flow.push_back({x, 18.0 + 14.0 *
                    (1.0 - std::exp(-x / 9.0)) + 2.0 * std::sin(x * 1.3)});
            }
            graph->setSeries({
                {"temperature", "Temperature", std::move(temperature),
                    {0.98F, 0.67F, 0.32F, 1.0F}, true},
                {"pressure", "Pressure", std::move(pressure),
                    {0.31F, 0.75F, 0.96F, 1.0F}, true},
                {"flow", "Flow", std::move(flow),
                    {0.55F, 0.84F, 0.62F, 1.0F}, true}});
            root->addChild(std::move(plot),
                {1.0F, {0.0F, 270.0F},
                    {lotui::unboundedLayoutSize,
                     lotui::unboundedLayoutSize}});
        }
        auto legend = std::make_unique<lotui::Row>();
        lotui::LinearLayoutOptions legendOptions;
        legendOptions.spacing = 9.0F;
        legendOptions.crossAxisAlignment = lotui::CrossAxisAlignment::Center;
        legend->setOptions(legendOptions);
        auto addLegend = [&](const char* title, lotui::Color color) {
            legend->addChild(std::make_unique<lotui::Box>(
                lotui::Size{12.0F, 4.0F}, color, 2.0F),
                {0.0F, {12.0F, 4.0F}, {12.0F, 4.0F}});
            legend->addChild(std::make_unique<lotui::Label>(text,
                title, heading), {0.0F, {40.0F, 24.0F}, {56.0F, 24.0F}});
        };
        addLegend(objectMode ? "너비" : "온도",
            {0.98F, 0.67F, 0.32F, 1.0F});
        addLegend(objectMode ? "깊이" : "압력",
            {0.31F, 0.75F, 0.96F, 1.0F});
        addLegend(objectMode ? "높이" : "유량",
            {0.55F, 0.84F, 0.62F, 1.0F});
        root->addChild(std::move(legend),
            {0.0F, {0.0F, 24.0F}, {lotui::unboundedLayoutSize, 24.0F}});

        lotui::WidgetTree tree(std::move(root));
        auto layout = [](lotui::WidgetTree& widgets,
            const lotui::WindowMetrics& metrics) {
            widgets.layout({0.0F, 0.0F,
                static_cast<float>(metrics.width),
                static_cast<float>(metrics.height)});
        };
        layout(tree, window->metrics());
        std::vector<lotui::PaintCommand> commands;
        if (smoke || objectSmoke) {
            if (objectSmoke) {
                const auto row = objectTree->rowBounds("object:0");
                const lotui::Point checkbox{row.x + 33.0F,
                    row.y + row.height * 0.5F};
                tree.pointerPressed(checkbox,
                    lotui::PointerButton::Primary);
                tree.pointerReleased(checkbox,
                    lotui::PointerButton::Primary);
                if (objectTree->isChecked("object:0") ||
                    graph->series()[0].points.size() != 23)
                    throw std::runtime_error("object selection did not update plot");
            }
            tree.paint(commands);
            renderer.drawFrame(commands);
            std::cout << (objectMode ? "object-plot-smoke: "
                                     : "plot-view-smoke: ") << commands.size()
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
            if ((capture || objectCapture) && frame == 6) {
                writeCapture(renderer.drawFrameCaptured(commands), argv[2]);
                return 0;
            }
            renderer.drawFrame(commands);
            ++frame;
            if (!received)
                std::this_thread::sleep_for(std::chrono::milliseconds(8));
        }
    } catch (const std::exception& error) {
        std::cerr << "plot_view_demo failed: " << error.what() << '\n';
        return 1;
    }
}
