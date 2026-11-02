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

#include <chrono>
#include <cmath>
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
}

int main(int argc, char** argv) {
    try {
        const bool smoke = argc == 2 && std::string_view(argv[1]) == "--smoke";
        const bool capture = argc == 3 && std::string_view(argv[1]) == "--capture";
        if (argc > 1 && !smoke && !capture) {
            std::cerr << "usage: plot_view_demo [--smoke|--capture output.ppm]\n";
            return 2;
        }
        auto platform = lotui::createPlatformBackend();
        auto window = platform->createWindow({"LotUI PlotView", 900, 560, true});
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
            "LotUI 그래프 · 시뮬레이션 결과", heading),
            {0.0F, {0.0F, 34.0F}, {lotui::unboundedLayoutSize, 34.0F}});
        heading.fontSize = 13.0F;
        root->addChild(std::make_unique<lotui::Label>(text,
            "스크롤: 확대/축소    드래그: 이동    Home: 전체 보기", heading),
            {0.0F, {0.0F, 24.0F}, {lotui::unboundedLayoutSize, 24.0F}});
        auto plot = std::make_unique<lotui::PlotView>(text,
            lotui::Size{850.0F, 370.0F}, lotui::PlotViewStyle{}, heading);
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
        plot->setSeries({
            {"temperature", "Temperature", std::move(temperature),
                {0.98F, 0.67F, 0.32F, 1.0F}, true},
            {"pressure", "Pressure", std::move(pressure),
                {0.31F, 0.75F, 0.96F, 1.0F}, true},
            {"flow", "Flow", std::move(flow),
                {0.55F, 0.84F, 0.62F, 1.0F}, true}});
        root->addChild(std::move(plot),
            {1.0F, {0.0F, 270.0F},
                {lotui::unboundedLayoutSize, lotui::unboundedLayoutSize}});
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
        addLegend("온도", {0.98F, 0.67F, 0.32F, 1.0F});
        addLegend("압력", {0.31F, 0.75F, 0.96F, 1.0F});
        addLegend("유량", {0.55F, 0.84F, 0.62F, 1.0F});
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
        if (smoke) {
            tree.paint(commands);
            renderer.drawFrame(commands);
            std::cout << "plot-view-smoke: " << commands.size()
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
        std::cerr << "plot_view_demo failed: " << error.what() << '\n';
        return 1;
    }
}
