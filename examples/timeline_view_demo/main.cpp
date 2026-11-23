#include "examples/platform_probe/platform_event_bridge.h"

#include "core/widget_tree.h"
#include "platform/platform_backend.h"
#include "platform/runtime_paths.h"
#include "renderer/vulkan/vulkan_renderer.h"
#include "text/freetype/freetype_text_engine.h"
#include "widgets/label.h"
#include "widgets/linear_layout.h"
#include "widgets/timeline_view.h"

#include <algorithm>
#include <chrono>
#include <exception>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <utility>
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
        const bool captureEdits = argc == 3 &&
            std::string_view(argv[1]) == "--capture-edits";
        const bool captureCompact = argc == 3 &&
            std::string_view(argv[1]) == "--capture-compact";
        const bool captureExpanded = argc == 3 &&
            std::string_view(argv[1]) == "--capture-expanded";
        const bool captureRange = argc == 3 &&
            std::string_view(argv[1]) == "--capture-range";
        const bool captureMarquee = argc == 3 &&
            std::string_view(argv[1]) == "--capture-marquee";
        const bool capture = argc == 3 &&
            (std::string_view(argv[1]) == "--capture" || captureEdits ||
                captureCompact || captureExpanded || captureRange ||
                captureMarquee);
        if (argc > 1 && !smoke && !capture) {
            std::cerr << "usage: timeline_view_demo [--smoke|--capture output.ppm|--capture-edits output.ppm|--capture-compact output.ppm|--capture-expanded output.ppm|--capture-range output.ppm|--capture-marquee output.ppm]\n";
            return 2;
        }
        auto platform = lotui::createPlatformBackend();
        const int windowWidth = captureCompact ? 420 :
            captureExpanded ? 1440 : 1024;
        const int windowHeight = captureCompact ? 400 :
            captureExpanded ? 560 : 480;
        auto window = platform->createWindow({"LotUI TimelineView",
            windowWidth, windowHeight, true});
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
        options.crossAxisAlignment = lotui::CrossAxisAlignment::Stretch;
        root->setOptions(options);
        root->addChild(std::make_unique<lotui::Label>(text,
            "LotUI 타임라인", heading),
            {0.0F, {0.0F, 30.0F}, {lotui::unboundedLayoutSize, 30.0F}});
        heading.fontSize = 13.0F;
        root->addChild(std::make_unique<lotui::Label>(text,
            "Shift 범위  |  드래그 영역  |  Ctrl 추가  |  휠 확대",
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
                lotui::colorFromSrgb(0x7797AC)},
            {"rotation", "카메라 회전",
                {{"r0", 0.0}, {"r1", 4.0}, {"r2", 9.0}},
                lotui::colorFromSrgb(0x8E9D78)},
            {"visibility", "부품 표시",
                {{"v0", 1.0}, {"v1", 5.0}, {"v2", 10.0}},
                lotui::colorFromSrgb(0xA98C9D)},
            {"opacity", "투명도",
                {{"o0", 2.0}, {"o1", 6.0}, {"o2", 11.0}},
                lotui::colorFromSrgb(0xAB9475)}
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
        view->setOnSelectionChanged([statusLabel](const auto& keys) {
            if (keys.size() > 1)
                statusLabel->setText("선택된 키 " +
                    std::to_string(keys.size()) + "개");
        });
        view->setOnKeysMoved([statusLabel](const auto& keys, double) {
            statusLabel->setText("키 " + std::to_string(keys.size()) +
                "개 이동 완료");
        });
        view->setOnInterpolationChanged([statusLabel](const auto& keys,
                lotui::TimelineInterpolation) {
            statusLabel->setText("키 " + std::to_string(keys.size()) +
                "개 보간 변경");
        });
        view->setOnKeysDeleted([statusLabel](const auto& keys) {
            statusLabel->setText("키 " + std::to_string(keys.size()) +
                "개 삭제");
        });
        root->addChild(std::move(timeline),
            {1.0F, {0.0F, 0.0F},
                {lotui::unboundedLayoutSize, lotui::unboundedLayoutSize}});
        root->addChild(std::move(status),
            {0.0F, {0.0F, 24.0F},
                {lotui::unboundedLayoutSize, 24.0F}});
        lotui::WidgetTree tree(std::move(root));
        const auto layout = [](lotui::WidgetTree& widgets,
            const lotui::WindowMetrics& metrics) {
            const float scale = std::max(0.01F, metrics.dpiScale);
            widgets.layout({0.0F, 0.0F,
                static_cast<float>(metrics.width) / scale,
                static_cast<float>(metrics.height) / scale});
        };
        layout(tree, window->metrics());
        if (captureEdits) {
            const auto first = view->keyBounds("camera", "c1");
            const auto second = view->keyBounds("visibility", "v1");
            tree.pointerPressed({first.x + 6.0F, first.y + 6.0F},
                lotui::PointerButton::Primary);
            tree.pointerReleased({first.x + 6.0F, first.y + 6.0F},
                lotui::PointerButton::Primary);
            lotui::KeyModifiers additive;
            additive.control = true;
            tree.pointerPressed({second.x + 6.0F, second.y + 6.0F},
                lotui::PointerButton::Primary, additive);
            tree.pointerReleased({second.x + 6.0F, second.y + 6.0F},
                lotui::PointerButton::Primary, additive);
            tree.pointerPressed({60.0F + view->bounds().x,
                15.0F + view->bounds().y}, lotui::PointerButton::Primary);
            tree.pointerReleased({60.0F + view->bounds().x,
                15.0F + view->bounds().y}, lotui::PointerButton::Primary);
        }
        if (captureRange || captureMarquee) {
            const auto first = view->keyBounds("camera", "c1");
            const auto last = view->keyBounds("visibility", "v1");
            const lotui::Point a{first.x + first.width * 0.5F,
                first.y + first.height * 0.5F};
            const lotui::Point b{last.x + last.width * 0.5F,
                last.y + last.height * 0.5F};
            if (captureRange) {
                tree.pointerPressed(a, lotui::PointerButton::Primary);
                tree.pointerReleased(a, lotui::PointerButton::Primary);
                lotui::KeyModifiers shift;
                shift.shift = true;
                tree.pointerPressed(b, lotui::PointerButton::Primary, shift);
                tree.pointerReleased(b, lotui::PointerButton::Primary, shift);
            } else {
                tree.pointerPressed({a.x - 15.0F, a.y - 9.0F},
                    lotui::PointerButton::Primary);
                tree.pointerMoved({b.x + 15.0F, b.y + 9.0F});
            }
            if (view->selectedKeys().size() != 3)
                throw std::runtime_error("timeline range selection failed");
        }
        std::vector<lotui::PaintCommand> commands;
        if (smoke) {
            for (const auto [width, height] : {
                    std::pair{336.0F, 320.0F},
                    {1152.0F, 448.0F}}) {
                tree.layout({0.0F, 0.0F, width, height});
                const auto ruler = view->rulerBounds();
                const auto key = view->keyBounds("camera", "c1");
                if (ruler.width < 100.0F || key.x < ruler.x ||
                        key.x + key.width > ruler.x + ruler.width ||
                        view->bounds().x + view->bounds().width > width + 0.1F ||
                        view->bounds().y + view->bounds().height > height + 0.1F)
                    throw std::runtime_error("timeline resize layout failed");
            }
            layout(tree, window->metrics());
            const auto marker = view->keyBounds("camera", "c1");
            const lotui::Point point{marker.x + 6.0F, marker.y + 6.0F};
            tree.pointerPressed(point, lotui::PointerButton::Primary);
            tree.pointerMoved({point.x + 28.0F, point.y});
            tree.pointerReleased({point.x + 28.0F, point.y},
                lotui::PointerButton::Primary);
            if (!view->selectedKey() ||
                view->tracks()[0].keys[1].time <= 3.0)
                throw std::runtime_error("timeline interaction failed");
            const auto other = view->keyBounds("visibility", "v1");
            lotui::KeyModifiers additive;
            additive.control = true;
            tree.pointerPressed({other.x + 6.0F, other.y + 6.0F},
                lotui::PointerButton::Primary, additive);
            tree.pointerReleased({other.x + 6.0F, other.y + 6.0F},
                lotui::PointerButton::Primary, additive);
            if (view->selectedKeys().size() != 2)
                throw std::runtime_error("timeline multi-selection failed");
            tree.pointerPressed({60.0F + view->bounds().x,
                15.0F + view->bounds().y}, lotui::PointerButton::Primary);
            tree.pointerReleased({60.0F + view->bounds().x,
                15.0F + view->bounds().y}, lotui::PointerButton::Primary);
            if (view->tracks()[0].keys[1].interpolation !=
                    lotui::TimelineInterpolation::Linear ||
                view->tracks()[2].keys[1].interpolation !=
                    lotui::TimelineInterpolation::Linear)
                throw std::runtime_error("timeline interpolation failed");
            const auto moved = view->keyBounds("camera", "c1");
            const lotui::Point movedPoint{moved.x + 6.0F,
                moved.y + 6.0F};
            tree.pointerPressed(movedPoint, lotui::PointerButton::Primary);
            tree.pointerReleased(movedPoint, lotui::PointerButton::Primary);
            lotui::KeyModifiers shift;
            shift.shift = true;
            const auto rangeEnd = view->keyBounds("visibility", "v1");
            const lotui::Point end{rangeEnd.x + 6.0F,
                rangeEnd.y + 6.0F};
            tree.pointerPressed(end, lotui::PointerButton::Primary, shift);
            tree.pointerReleased(end, lotui::PointerButton::Primary, shift);
            if (view->selectedKeys().size() != 3)
                throw std::runtime_error("timeline Shift range failed");
            const auto rangeStart = view->keyBounds("camera", "c1");
            tree.pointerPressed({rangeStart.x - 10.0F,
                    rangeStart.y - 4.0F}, lotui::PointerButton::Primary);
            tree.pointerMoved({rangeEnd.x + rangeEnd.width + 10.0F,
                rangeEnd.y + rangeEnd.height + 4.0F});
            tree.pointerReleased({rangeEnd.x + rangeEnd.width + 10.0F,
                rangeEnd.y + rangeEnd.height + 4.0F},
                lotui::PointerButton::Primary);
            if (view->selectedKeys().size() != 3)
                throw std::runtime_error("timeline marquee failed");
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
