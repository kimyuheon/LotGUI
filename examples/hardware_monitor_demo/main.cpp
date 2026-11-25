#include "examples/dashboard_demo/dial_gauge.h"
#include "examples/hardware_monitor_demo/sensors.h"
#include "examples/platform_probe/platform_event_bridge.h"
#include "platform/platform_backend.h"
#include "platform/runtime_paths.h"
#include "renderer/vulkan/vulkan_renderer.h"
#include "text/freetype/freetype_text_engine.h"
#include "widgets/label.h"
#include "widgets/linear_layout.h"
#include "widgets/plot_view.h"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace {

using lotui::example::DialGauge;
using lotui::example::desktopColor;
using lotui::example::HardwareReadings;
using Clock = std::chrono::steady_clock;
using namespace std::chrono_literals;
constexpr auto unlimited = lotui::unboundedLayoutSize;

struct SensorSample {
    Clock::time_point received;
    HardwareReadings values;
};

class SensorPoller {
public:
    SensorPoller() : worker_([this] { run(); }) {}
    ~SensorPoller() {
        {
            std::lock_guard<std::mutex> guard(lock_);
            stop_ = true;
        }
        changed_.notify_one();
        worker_.join();
    }

    std::optional<SensorSample> takeLatest() {
        std::lock_guard<std::mutex> guard(lock_);
        auto result = std::move(latest_);
        latest_.reset();
        return result;
    }

private:
    void run() {
        for (;;) {
            auto readings = lotui::example::readHardwareSensors();
            std::unique_lock<std::mutex> guard(lock_);
            latest_ = SensorSample{Clock::now(), std::move(readings)};
            if (changed_.wait_for(guard, 2s, [this] { return stop_; })) return;
        }
    }

    std::mutex lock_;
    std::condition_variable changed_;
    std::optional<SensorSample> latest_;
    bool stop_{};
    std::thread worker_;
};

lotui::TextStyle textStyle(float size) {
    lotui::TextStyle style;
    style.fontFamilies = {"Noto Sans KR"};
    style.fontSize = size;
    return style;
}

std::unique_ptr<lotui::Label> label(
    const std::shared_ptr<const lotui::TextEngine>& text,
    std::string value, float size, std::uint32_t color = 0xe9eff7) {
    auto result = std::make_unique<lotui::Label>(
        text, std::move(value), textStyle(size));
    result->setColor(desktopColor(color));
    result->setVerticalAlignment(lotui::VerticalTextAlignment::Center);
    return result;
}

lotui::ChildLayout fixedHeight(float height) {
    return {0.0F, {0.0F, height}, {unlimited, height}};
}

lotui::LinearLayoutOptions layoutOptions(float padding, float spacing) {
    lotui::LinearLayoutOptions options;
    options.padding = lotui::EdgeInsets::all(padding);
    options.spacing = spacing;
    options.crossAxisAlignment = lotui::CrossAxisAlignment::Stretch;
    return options;
}

struct SensorGauge {
    DialGauge* dial{};
    lotui::Label* source{};
    lotui::Label* device{};

    void update(std::optional<int> value, const std::string& origin,
        const std::string& name) const {
        if (value) dial->setValue(*value);
        else dial->clearValue();
        source->setText(value ? origin : "센서 값 없음");
        device->setText(name);
    }
};

struct Dashboard {
    std::unique_ptr<lotui::WidgetTree> tree;
    SensorGauge cpu;
    SensorGauge gpu;
    SensorGauge gpuLoad;
    lotui::PlotView* chart{};
    lotui::Label* status{};
    std::vector<lotui::PlotPoint> cpuHistory;
    std::vector<lotui::PlotPoint> gpuHistory;
    Clock::time_point start{Clock::now()};

    void update(const SensorSample& sample) {
        const auto& v = sample.values;
        cpu.update(v.cpuTemperature, v.cpuSource,
            v.cpuTemperature ? "패키지 센서" : "");
        gpu.update(v.gpuTemperature, v.gpuSource, v.gpuName);
        gpuLoad.update(v.gpuLoad, v.gpuSource, v.gpuName);
        const double seconds =
            std::chrono::duration<double>(sample.received - start).count();
        const double first = std::max(0.0, seconds - 120.0);
        const auto extend = [&](std::vector<lotui::PlotPoint>& history,
                                std::optional<int> current) {
            if (!current) {
                history.clear();
                return;
            }
            history.erase(history.begin(), std::lower_bound(history.begin(),
                history.end(), first,
                [](const lotui::PlotPoint& point, double threshold) {
                    return point.x < threshold;
                }));
            history.push_back({seconds, static_cast<double>(*current)});
        };
        extend(cpuHistory, v.cpuTemperature);
        extend(gpuHistory, v.gpuTemperature);
        chart->setSeries({
            {"cpu", "CPU", cpuHistory, desktopColor(0x8eb9ff), true},
            {"gpu", "GPU", gpuHistory, desktopColor(0xeac28e), true},
        });
        chart->setRange({first, std::max(10.0, seconds), 0, 120});
        status->setText(v.cpuTemperature || v.gpuTemperature || v.gpuLoad
            ? "실시간 센서  ·  약 2초마다 갱신"
            : "사용 가능한 센서가 없습니다.");
    }
};

Dashboard buildDashboard(const std::shared_ptr<const lotui::TextEngine>& text) {
    Dashboard state;
    auto root = std::make_unique<lotui::Column>();
    root->setDecoration(lotui::BoxDecoration{desktopColor(0x12171e), 0});
    root->setOptions(layoutOptions(22, 12));
    root->addChild(label(text, "하드웨어 모니터", 27), fixedHeight(42));
    auto status = label(text, "센서 확인 중…", 14, 0x91b9f2);
    state.status = status.get();
    root->addChild(std::move(status), fixedHeight(23));

    auto row = std::make_unique<lotui::Row>();
    row->setOptions(layoutOptions(0, 12));
    const auto addGauge = [&](const char* name, int maximum, const char* unit,
                              std::uint32_t color) {
        auto card = std::make_unique<lotui::Column>();
        card->setDecoration(lotui::BoxDecoration{desktopColor(0x1d242e), 9});
        card->setOptions(layoutOptions(16, 4));
        card->addChild(label(text, name, 17), fixedHeight(24));
        auto dial = std::make_unique<DialGauge>(text, maximum, unit,
            desktopColor(color));
        auto source = label(text, "센서 확인 중…", 12, 0xaebaca);
        auto device = label(text, "", 12, 0x91b9f2);
        const SensorGauge result{dial.get(), source.get(), device.get()};
        card->addChild(std::move(dial), {1.0F, {0.0F, 125.0F}});
        card->addChild(std::move(source), fixedHeight(18));
        card->addChild(std::move(device), fixedHeight(18));
        row->addChild(std::move(card), {1.0F, {200.0F, 0.0F}});
        return result;
    };
    state.cpu = addGauge("CPU 온도", 120, "°C", 0x8eb9ff);
    state.gpu = addGauge("GPU 온도", 120, "°C", 0xeac28e);
    state.gpuLoad = addGauge("GPU 사용률", 100, "%", 0x85ceac);
    root->addChild(std::move(row), fixedHeight(250));

    auto panel = std::make_unique<lotui::Column>();
    panel->setDecoration(lotui::BoxDecoration{desktopColor(0x1d242e), 9});
    panel->setOptions(layoutOptions(16, 6));
    panel->addChild(label(text, "온도 기록", 18), fixedHeight(26));
    auto legend = std::make_unique<lotui::Row>();
    legend->setOptions(layoutOptions(0, 16));
    legend->addChild(label(text, "● CPU", 13, 0x8eb9ff));
    legend->addChild(label(text, "● GPU", 13, 0xeac28e));
    legend->addChild(label(text, "최근 최대 120초  ·  °C", 13, 0xaebaca));
    panel->addChild(std::move(legend), fixedHeight(22));
    lotui::PlotViewStyle plotStyle;
    plotStyle.background = desktopColor(0x171e27);
    plotStyle.grid = desktopColor(0x303b49);
    plotStyle.axis = desktopColor(0x617187);
    plotStyle.text = desktopColor(0xc4d0df);
    auto chart = std::make_unique<lotui::PlotView>(
        text, lotui::Size{660, 216}, plotStyle, textStyle(13));
    chart->setRange({0, 10, 0, 120});
    state.chart = chart.get();
    panel->addChild(std::move(chart), {1.0F, {0.0F, 142.0F}});
    root->addChild(std::move(panel), {1.0F, {0.0F, 240.0F}});
    state.tree = std::make_unique<lotui::WidgetTree>(std::move(root));
    return state;
}

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
        if (argc == 2 && std::string_view(argv[1]) == "--read-sensors") {
            const auto values = lotui::example::readHardwareSensors();
            const auto print = [](const char* name, std::optional<int> value) {
                std::cout << name << '=' <<
                    (value ? std::to_string(*value) : "unavailable") << '\n';
            };
            print("CPU temperature (C)", values.cpuTemperature);
            print("GPU temperature (C)", values.gpuTemperature);
            print("GPU utilization (%)", values.gpuLoad);
            if (!values.gpuName.empty()) std::cout << "GPU: " << values.gpuName << '\n';
            return 0;
        }
        const bool capture = argc == 3 && std::string_view(argv[1]) == "--capture";
        if (argc != 1 && !capture) return 2;
        auto platform = lotui::createPlatformBackend();
        auto window = platform->createWindow(
            {"LotUI Hardware Monitor", 1010, 690, true, 840, 640});
        window->show();
        lotui::VulkanRenderer renderer(*window);
        const auto font = lotui::executableDirectory() /
            "resources" / "fonts" / "NotoSansKR-Regular.ttf";
        auto text = std::make_shared<lotui::FreetypeTextEngine>(renderer,
            std::vector<lotui::FontSource>{{"Noto Sans KR", font}});
        auto dashboard = buildDashboard(text);
        SensorPoller poller;
        const auto layout = [](lotui::WidgetTree& tree,
            const lotui::WindowMetrics& metrics) {
            tree.layout({0, 0, static_cast<float>(metrics.width),
                static_cast<float>(metrics.height)});
        };
        layout(*dashboard.tree, window->metrics());
        std::vector<lotui::PaintCommand> commands;
        const auto captureDeadline = Clock::now() + 10s;
        unsigned int samples = 0;
        for (;;) {
            lotui::PlatformEvent event{};
            while (window->pollEvent(event)) {
                if (!lotui::example::dispatchPlatformEvent(
                        event, *dashboard.tree, *window, layout))
                    return 0;
            }
            if (const auto latest = poller.takeLatest()) {
                dashboard.update(*latest);
                ++samples;
            }
            commands.clear();
            dashboard.tree->paint(commands);
            if (capture && (samples >= 3 || Clock::now() >= captureDeadline)) {
                writeCapture(renderer.drawFrameCaptured(commands), argv[2]);
                return 0;
            }
            renderer.drawFrame(commands);
            std::this_thread::sleep_for(8ms);
        }
    } catch (const std::exception& error) {
        std::cerr << "hardware_monitor_demo failed: " << error.what() << '\n';
        return 1;
    }
}
