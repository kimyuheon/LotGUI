#include "examples/platform_probe/platform_event_bridge.h"

#include "core/widget_tree.h"
#include "platform/platform_backend.h"
#include "platform/runtime_paths.h"
#include "renderer/vulkan/vulkan_renderer.h"
#include "text/freetype/freetype_text_engine.h"
#include "widgets/label.h"
#include "widgets/linear_layout.h"
#include "widgets/timeline_view.h"

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
void savePpm(const lotui::VulkanRenderer::CapturedFrame& frame,
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
            std::cerr << "usage: timeline_view_demo [--smoke|--capture output.ppm]\n";
            return 2;
        }
        auto platform = lotui::createPlatformBackend();
        auto window = platform->createWindow({"LotUI TimelineView", 1024, 480, true});
        window->show();
        lotui::VulkanRenderer renderer(*window);
        const auto font = lotui::executableDirectory() /
            "resources/fonts/NotoSansKR-Regular.ttf";
        auto text = std::make_shared<lotui::FreetypeTextEngine>(renderer,
            std::vector<lotui::FontSource>{{"Noto Sans KR", font}});
        lotui::TextStyle heading;
        heading.fontFamilies = {"Noto Sans KR"};
        heading.fontSize = 19.0F;
        auto root = std::make_unique<lotui::Column>();
        root->setDecoration(lotui::BoxDecoration{
            {0.012F, 0.017F, 0.024F, 1.0F}, 0.0F});
        lotui::LinearLayoutOptions options;
        options.padding = {16.0F, 15.0F, 16.0F, 15.0F};
        options.spacing = 9.0F;
        root->setOptions(options);
        root->addChild(std::make_unique<lotui::Label>(text,
            "LotUI 타임라인", heading),
            {0.0F, {0.0F, 30.0F}, {lotui::unboundedLayoutSize, 30.0F}});
        heading.fontSize = 13.0F;
        root->addChild(std::make_unique<lotui::Label>(text,
            "재생 / 일시정지  |  시간 스크럽  |  키 이동  |  휠 확대",
            heading), {0.0F, {0.0F, 26.0F},
                {lotui::unboundedLayoutSize, 26.0F}});
        auto timeline = std::make_unique<lotui::TimelineView>(text,
            lotui::Size{980.0F, 320.0F}, lotui::TimelineViewStyle{}, heading);
        auto* view = timeline.get();
        view->setDuration(12.0);
        view->setView(0.0, 12.0);
        view->setTime(3.0);
        view->setTracks({
            {"camera", "카메라 위치",
                {{"c0", 0.0}, {"c1", 3.0}, {"c2", 7.5}, {"c3", 12.0}},
                {0.39F, 0.75F, 0.96F, 1.0F}},
            {"rotation", "카메라 회전",
                {{"r0", 0.0}, {"r1", 4.0}, {"r2", 9.0}},
                {0.70F, 0.78F, 0.48F, 1.0F}},
            {"visibility", "부품 표시",
                {{"v0", 1.0}, {"v1", 5.0}, {"v2", 10.0}},
                {0.91F, 0.60F, 0.74F, 1.0F}},
            {"opacity", "투명도",
                {{"o0", 2.0}, {"o1", 6.0}, {"o2", 11.0}},
                {0.98F, 0.70F, 0.39F, 1.0F}}
        });
        auto status = std::make_unique<lotui::Label>(text,
            "3.0초 · 키를 선택하거나 드래그하세요", heading);
        auto* statusLabel = status.get();
        view->setOnKeySelected([statusLabel](auto key) {
            if (key) statusLabel->setText("선택: " + key->trackId +
                " / " + key->keyId);
        });
        view->setOnKeyMoved([statusLabel](const auto& key, double) {
            statusLabel->setText("이동 완료: " + key.trackId +
                " / " + key.keyId);
        });
        root->addChild(std::move(timeline),
            {1.0F, {400.0F, 210.0F},
                {lotui::unboundedLayoutSize, lotui::unboundedLayoutSize}});
        root->addChild(std::move(status),
            {0.0F, {0.0F, 24.0F},
                {lotui::unboundedLayoutSize, 24.0F}});
        lotui::WidgetTree tree(std::move(root));
        const auto layout = [](lotui::WidgetTree& widgets,
            const lotui::WindowMetrics& metrics) {
            widgets.layout({0.0F, 0.0F,
                static_cast<float>(metrics.width),
                static_cast<float>(metrics.height)});
        };
        layout(tree, window->metrics());
        std::vector<lotui::PaintCommand> commands;
        if (smoke) {
            const auto marker = view->keyBounds("camera", "c1");
            const lotui::Point point{marker.x + 6.0F, marker.y + 6.0F};
            tree.pointerPressed(point, lotui::PointerButton::Primary);
            tree.pointerMoved({point.x + 28.0F, point.y});
            tree.pointerReleased({point.x + 28.0F, point.y},
                lotui::PointerButton::Primary);
            if (!view->selectedKey() ||
                view->tracks()[0].keys[1].time <= 3.0)
                throw std::runtime_error("timeline interaction failed");
            tree.paint(commands);
            renderer.drawFrame(commands);
            std::cout << "timeline-view-smoke: " << commands.size()
                << " paint commands\n";
            return 0;
        }
        bool running = true;
        int frame = 0;
        auto previous = std::chrono::steady_clock::now();
        while (running) {
            bool received = false;
            lotui::PlatformEvent event{};
            while (window->pollEvent(event)) {
                received = true;
                running = lotui::example::dispatchPlatformEvent(
                    event, tree, *window, layout) && running;
            }
            if (!running) break;
            const auto now = std::chrono::steady_clock::now();
            const double elapsed = std::chrono::duration<double>(
                now - previous).count();
            previous = now;
            if (view->playing()) {
                view->setTime(std::min(view->duration(),
                    view->time() + elapsed));
                if (view->time() >= view->duration())
                    view->setPlaying(false);
            }
            commands.clear();
            tree.paint(commands);
            if (capture && frame == 6) {
                savePpm(renderer.drawFrameCaptured(commands), argv[2]);
                return 0;
            }
            renderer.drawFrame(commands);
            ++frame;
            if (!received)
                std::this_thread::sleep_for(std::chrono::milliseconds(8));
        }
    } catch (const std::exception& error) {
        std::cerr << "timeline_view_demo failed: " << error.what() << '\n';
        return 1;
    }
}
