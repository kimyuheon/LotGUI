#include "examples/common/desktop_palette.h"
#include "examples/platform_probe/platform_event_bridge.h"
#include "platform/platform_backend.h"
#include "platform/runtime_paths.h"
#include "renderer/vulkan/vulkan_renderer.h"
#include "text/freetype/freetype_text_engine.h"
#include "widgets/checkbox.h"
#include "widgets/label.h"
#include "widgets/linear_layout.h"
#include "widgets/list_control.h"
#include "widgets/plot_view.h"
#include "widgets/slider.h"
#include "widgets/text_field.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <exception>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace {

using lotui::example::desktopColor;
constexpr auto unlimited = lotui::unboundedLayoutSize;

struct Task {
    const char* object;
    const char* action;
    bool review;
};

constexpr std::array tasks{
    Task{"로봇 베이스", "모델 수정", true},
    Task{"J2 브래킷", "치수 확인", true},
    Task{"구동 축", "정렬 완료", false},
    Task{"센서 마운트", "속성 갱신", false},
    Task{"커버 플레이트", "모델 수정", true},
    Task{"케이블 가이드", "배치 완료", false},
    Task{"리니어 레일", "치수 확인", false},
    Task{"고정 볼트", "레이어 변경", false},
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

std::unique_ptr<lotui::Column> card(float padding = 16.0F) {
    auto result = std::make_unique<lotui::Column>();
    result->setDecoration({lotui::BoxDecoration{desktopColor(0x1d242e), 9.0F}});
    result->setOptions(layoutOptions(padding, 5.0F));
    return result;
}

std::vector<lotui::ListRow> taskRows(bool reviewOnly) {
    std::vector<lotui::ListRow> rows;
    const auto cell = [](const char* value) {
        lotui::ListCell result;
        result.text = value;
        result.editable = false;
        return result;
    };
    for (const auto& task : tasks) {
        if (!reviewOnly || task.review)
            rows.push_back({cell(task.object), cell(task.action),
                cell(task.review ? "검토 필요" : "완료")});
    }
    return rows;
}

class DialGauge final : public lotui::Widget {
public:
    DialGauge(std::shared_ptr<const lotui::TextEngine> text, int maximum,
        std::string unit, lotui::Color accent)
        : text_(std::move(text)), maximum_(maximum), unit_(std::move(unit)),
          accent_(accent) {}

    void setValue(int value) { value_ = std::clamp(value, 0, maximum_); }
    int value() const noexcept { return value_; }
    lotui::Size measure(const lotui::LayoutConstraints& constraints) const override {
        return constraints.constrain({206.0F, 160.0F});
    }

protected:
    void onPaint(std::vector<lotui::PaintCommand>& commands) const override {
        const auto area = bounds();
        const float radius = std::min(area.width * 0.42F, area.height * 0.43F);
        if (radius <= 0.0F) return;
        const lotui::Point center{area.x + area.width * 0.5F,
            area.y + area.height * 0.51F};
        constexpr float pi = 3.14159265358979323846F;
        const auto locate = [center](float distance, float fraction) {
            const float theta = pi * (5.0F / 6.0F + 4.0F / 3.0F * fraction);
            return lotui::Point{center.x + distance * std::cos(theta),
                center.y + distance * std::sin(theta)};
        };
        const auto stroke = [&](lotui::Point a, lotui::Point b,
                                float width, lotui::Color color) {
            commands.push_back(lotui::linePaintCommand(a, b, width, clip(), color));
        };
        const auto centeredText = [&](std::string value, float x, float y,
                                      float size, lotui::Color color) {
            auto layout = text_->createLayout(value, textStyle(size), {});
            layout->appendPaintCommands({x - layout->size().width * 0.5F, y},
                clip(), color, commands);
        };
        const auto track = desktopColor(0x394656);
        const auto tick = desktopColor(0x8494a7);
        const float fraction = static_cast<float>(value_) / maximum_;
        for (int i = 0; i < 48; ++i) {
            const float from = i / 48.0F;
            const float to = (i + 1) / 48.0F;
            const float join = std::max(0.0F, from - 0.004F);
            stroke(locate(radius, join), locate(radius, to), 4, track);
        }
        for (int i = 0; i < 48; ++i) {
            const float from = i / 48.0F;
            if (from >= fraction) break;
            stroke(locate(radius, std::max(0.0F, from - 0.004F)),
                locate(radius, std::min((i + 1) / 48.0F, fraction)), 4, accent_);
        }
        for (int i = 0; i <= 20; ++i) {
            const float at = i / 20.0F;
            const float length = i % 5 == 0 ? 10.0F : 5.0F;
            stroke(locate(radius - 7, at), locate(radius - 7 - length, at),
                i % 5 == 0 ? 1.75F : 1.0F, tick);
        }
        stroke(locate(-10, fraction), locate(radius - 22, fraction), 2.5F,
            desktopColor(0xf0f4fa));
        commands.push_back({{center.x - 4, center.y - 4, 8, 8}, clip(),
            accent_, lotui::invalidTextureId, 4.0F});
        const float scaleY = center.y + radius * 0.50F + 5.0F;
        centeredText("0", center.x - radius * 1.06F, scaleY, 11, tick);
        centeredText(std::to_string(maximum_), center.x + radius * 1.06F,
            scaleY, 11, tick);
        centeredText(std::to_string(value_) + " " + unit_, center.x,
            center.y + radius * 0.40F, 19, desktopColor(0xe9eff7));
    }

private:
    std::shared_ptr<const lotui::TextEngine> text_;
    int maximum_;
    std::string unit_;
    lotui::Color accent_;
    int value_{0};
};

struct CommandPrompt {
    lotui::TextField* input{};
    lotui::Label* echo{};
    lotui::Label* output{};
    std::vector<std::string> history;
    std::size_t historyPosition{};
    std::string draft;

    void submit(std::string_view command, lotui::Slider& simulator,
        const std::function<void(double)>& updateGauges) {
        const auto start = command.find_first_not_of(" \t");
        if (start == std::string_view::npos) {
            input->setText("");
            return;
        }
        command = command.substr(start, command.find_last_not_of(" \t") - start + 1);
        const std::string submitted(command);
        if (history.empty() || history.back() != submitted) {
            if (history.size() == 100) history.erase(history.begin());
            history.push_back(submitted);
        }
        historyPosition = history.size();
        draft.clear();
        input->setText("");
        echo->setText("> " + submitted);

        const auto separator = submitted.find_first_of(" \t");
        std::string name = submitted.substr(0, separator);
        std::transform(name.begin(), name.end(), name.begin(),
            [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
        if (name == "HELP" && separator == std::string::npos) {
            output->setText("HELP   STATUS   LOAD <0–100>   CLEAR");
        } else if (name == "STATUS" && separator == std::string::npos) {
            output->setText("현재 모터 부하 " +
                std::to_string(static_cast<int>(simulator.value())) + "%");
        } else if (name == "CLEAR" && separator == std::string::npos) {
            echo->setText("");
            output->setText("");
        } else if (name == "LOAD") {
            const auto arg = submitted.find_first_not_of(" \t", separator);
            int value = -1;
            if (arg != std::string::npos) {
                const char* end = submitted.data() + submitted.size();
                const auto parsed = std::from_chars(submitted.data() + arg, end, value);
                if (parsed.ec == std::errc{} && parsed.ptr == end &&
                    value >= 0 && value <= 100) {
                    simulator.setValue(value);
                    updateGauges(simulator.value());
                    output->setText("모터 부하를 " + std::to_string(value) + "%로 설정했습니다.");
                    return;
                }
            }
            output->setText("사용법   LOAD <0–100>");
        } else {
            output->setText("알 수 없는 명령입니다. HELP로 사용 가능한 명령을 확인하세요.");
        }
    }

    bool handleKey(lotui::KeyCode key) {
        if (!input->isFocused() || !input->composition().empty()) return false;
        if (key == lotui::KeyCode::Escape && !input->text().empty()) {
            historyPosition = history.size();
            draft.clear();
            input->setText("");
            return true;
        }
        if (key == lotui::KeyCode::Up) {
            if (historyPosition == history.size()) draft = input->text();
            if (historyPosition > 0) input->setText(history[--historyPosition]);
            return true;
        }
        if (key == lotui::KeyCode::Down) {
            if (historyPosition < history.size()) {
                ++historyPosition;
                input->setText(historyPosition == history.size() ?
                    draft : history[historyPosition]);
            }
            return true;
        }
        return false;
    }
};

struct Dashboard {
    std::unique_ptr<lotui::WidgetTree> tree;
    lotui::ListControl* table{};
    lotui::Checkbox* reviewFilter{};
    lotui::Slider* simulator{};
    DialGauge* loadGauge{};
    std::shared_ptr<CommandPrompt> prompt;
};

Dashboard buildDashboard(const std::shared_ptr<const lotui::TextEngine>& text,
    lotui::PlatformWindow& window) {
    auto root = std::make_unique<lotui::Column>();
    root->setDecoration({lotui::BoxDecoration{desktopColor(0x12171e), 0.0F}});
    root->setOptions(layoutOptions(24.0F, 16.0F));

    auto header = std::make_unique<lotui::Row>();
    header->setOptions(layoutOptions(0, 10.0F));
    auto heading = std::make_unique<lotui::Column>();
    heading->setOptions(layoutOptions(0, 2.0F));
    heading->addChild(label(text, "CAD 시뮬레이션 계기판", 28.0F), fixedHeight(43));
    heading->addChild(label(text, "산업용 로봇 조립  /  운전 상태", 15.0F,
        0xa5b2c3), fixedHeight(24));
    header->addChild(std::move(heading), {1.0F, {}, {unlimited, unlimited}});
    auto sample = label(text, "샘플 데이터", 14.0F, 0x91b9f2);
    sample->setHorizontalAlignment(lotui::HorizontalTextAlignment::End);
    header->addChild(std::move(sample), {0.0F, {120.0F, 0.0F}});
    root->addChild(std::move(header), fixedHeight(70));

    auto metrics = std::make_unique<lotui::Row>();
    metrics->setOptions(layoutOptions(0, 12.0F));
    const auto addGauge = [&](const char* name, int maximum, const char* unit,
                              std::uint32_t color) {
        auto metric = card();
        metric->addChild(label(text, name, 17), fixedHeight(24));
        auto gauge = std::make_unique<DialGauge>(text, maximum, unit,
            desktopColor(color));
        auto* gaugePtr = gauge.get();
        metric->addChild(std::move(gauge), {1.0F, {0.0F, 144.0F}});
        metrics->addChild(std::move(metric),
            {1.0F, {185.0F, 0.0F}, {unlimited, unlimited}});
        return gaugePtr;
    };
    auto* speed = addGauge("회전 속도", 3000, "rpm", 0x8bb7f3);
    auto* load = addGauge("모터 부하", 100, "%", 0x85ceac);
    auto* temperature = addGauge("모터 온도", 100, "°C", 0xeac28e);
    const auto updateGauges = [speed, load, temperature](double level) {
        speed->setValue(static_cast<int>(level * 25));
        load->setValue(static_cast<int>(level));
        temperature->setValue(static_cast<int>(26 + level * 0.52));
    };
    constexpr double initialLoad = 58.0;
    updateGauges(initialLoad);
    root->addChild(std::move(metrics), fixedHeight(205));

    auto simulation = std::make_unique<lotui::Row>();
    auto simulationOptions = layoutOptions(0, 12.0F);
    simulationOptions.crossAxisAlignment = lotui::CrossAxisAlignment::Center;
    simulation->setOptions(simulationOptions);
    simulation->addChild(label(text, "샘플 부하 조절", 14, 0xaebaca),
        {0.0F, {122.0F, 0.0F}});
    lotui::SliderStyle simulatorStyle;
    simulatorStyle.track = desktopColor(0x384555);
    simulatorStyle.filled = desktopColor(0x5b91ce);
    auto simulator = std::make_unique<lotui::Slider>(
        lotui::SliderOptions{initialLoad, 0.0, 100.0, 1.0, {220.0F, 27.0F}},
        updateGauges, simulatorStyle);
    auto* simulatorPtr = simulator.get();
    simulation->addChild(std::move(simulator));
    root->addChild(std::move(simulation), fixedHeight(30));

    auto content = std::make_unique<lotui::Row>();
    content->setOptions(layoutOptions(0, 12.0F));
    auto chartCard = card();
    chartCard->addChild(label(text, "객체 변경 추이", 19), fixedHeight(30));
    chartCard->addChild(label(text, "최근 7일  ·  가로축: 일  /  세로축: 건", 13,
        0xaebaca), fixedHeight(23));
    auto legend = std::make_unique<lotui::Row>();
    legend->setOptions(layoutOptions(0, 16.0F));
    legend->addChild(label(text, "● 수정", 13, 0x8bb7f3));
    legend->addChild(label(text, "● 검토 완료", 13, 0x85ceac));
    chartCard->addChild(std::move(legend), fixedHeight(25));
    lotui::PlotViewStyle plotStyle;
    plotStyle.background = desktopColor(0x171e27);
    plotStyle.grid = desktopColor(0x303b49);
    plotStyle.axis = desktopColor(0x617187);
    plotStyle.text = desktopColor(0xc4d0df);
    auto chart = std::make_unique<lotui::PlotView>(text, lotui::Size{420, 278},
        plotStyle, textStyle(13));
    chart->setSeries({
        {"edits", "수정", {{1, 3}, {2, 6}, {3, 4}, {4, 9}, {5, 7}, {6, 11}, {7, 8}},
            desktopColor(0x8bb7f3), true},
        {"reviews", "검토 완료", {{1, 1}, {2, 2}, {3, 3}, {4, 3}, {5, 5}, {6, 4}, {7, 6}},
            desktopColor(0x85ceac), true},
    });
    chart->setRange({1, 7, 0, 12});
    chartCard->addChild(std::move(chart), {1.0F, {0.0F, 136.0F}});
    content->addChild(std::move(chartCard),
        {1.0F, {330.0F, 0.0F}, {unlimited, unlimited}});

    auto taskCard = card();
    taskCard->addChild(label(text, "최근 작업", 19), fixedHeight(30));
    lotui::ListControlStyle tableStyle;
    tableStyle.background = desktopColor(0x171e27);
    tableStyle.headerBackground = desktopColor(0x303b49);
    tableStyle.alternateRow = desktopColor(0x202934);
    tableStyle.gridLine = desktopColor(0x34404e);
    tableStyle.text = desktopColor(0xe9eff7);
    tableStyle.selectedCell = desktopColor(0x354d69);
    tableStyle.selectedCellFocused = desktopColor(0x405d7e);
    auto table = std::make_unique<lotui::ListControl>(text,
        std::vector<lotui::ListColumn>{{"object", "객체", 142.0F},
            {"action", "변경 내용", 112.0F}, {"status", "상태", 96.0F}},
        taskRows(false), lotui::Size{350.0F, 265.0F},
        lotui::ListControl::SelectionChangedHandler{},
        lotui::ListControl::CellActionHandler{}, tableStyle, textStyle(14));
    auto* tablePtr = table.get();
    table->setSelectionMode(lotui::ListSelectionMode::Row);
    lotui::CheckboxStyle filterStyle;
    filterStyle.boxSize = 16.0F;
    filterStyle.spacing = 8.0F;
    filterStyle.unchecked = desktopColor(0x384555);
    filterStyle.checked = desktopColor(0x396ba5);
    auto filter = std::make_unique<lotui::Checkbox>(
        label(text, "검토 필요만 표시", 14, 0xc4d0df), false,
        [tablePtr](bool checked) { tablePtr->setRows(taskRows(checked)); },
        filterStyle);
    auto* filterPtr = filter.get();
    taskCard->addChild(std::move(filter), fixedHeight(30));
    taskCard->addChild(std::move(table), {1.0F, {0.0F, 154.0F}});
    content->addChild(std::move(taskCard),
        {1.0F, {406.0F, 0.0F}, {unlimited, unlimited}});
    root->addChild(std::move(content), {1.0F, {0.0F, 270.0F}});

    auto commandCard = card(12.0F);
    commandCard->setOptions(layoutOptions(12.0F, 3.0F));
    auto prompt = std::make_shared<CommandPrompt>();
    auto consoleEcho = label(text, "명령 프롬프트  ·  ↑↓ 이전 명령", 13.0F, 0x91b9f2);
    prompt->echo = consoleEcho.get();
    commandCard->addChild(std::move(consoleEcho), fixedHeight(22));
    auto consoleOutput = label(text, "HELP로 명령 목록을 확인하세요. 예: LOAD 75",
        13.0F, 0xb2bfd0);
    prompt->output = consoleOutput.get();
    commandCard->addChild(std::move(consoleOutput), fixedHeight(22));

    auto entry = std::make_unique<lotui::Row>();
    auto entryOptions = layoutOptions(0, 10.0F);
    entryOptions.crossAxisAlignment = lotui::CrossAxisAlignment::Center;
    entry->setOptions(entryOptions);
    entry->addChild(label(text, "명령 >", 14.0F, 0x91b9f2),
        {0.0F, {67.0F, 0.0F}});
    lotui::TextFieldStyle promptStyle;
    promptStyle.normal = desktopColor(0x131a22);
    promptStyle.hovered = desktopColor(0x202a38);
    promptStyle.focusRing = desktopColor(0x81ade7);
    promptStyle.focusRingWidth = 1.0F;
    promptStyle.cornerRadius = 5.0F;
    promptStyle.contentPadding = {8.0F, 4.0F, 8.0F, 4.0F};
    auto input = std::make_unique<lotui::TextField>(text, "",
        lotui::Size{340.0F, 32.0F}, lotui::TextField::ChangedHandler{},
        [prompt, simulatorPtr, updateGauges](const std::string& command) {
            prompt->submit(command, *simulatorPtr, updateGauges);
        }, promptStyle, textStyle(14.0F));
    prompt->input = input.get();
    input->setClipboardHandlers([&window](std::string value) {
        return window.writeClipboardText(std::move(value));
    }, [&window] { return window.readClipboardText(); });
    entry->addChild(std::move(input), {1.0F, {0.0F, 32.0F}});
    commandCard->addChild(std::move(entry), fixedHeight(32));
    root->addChild(std::move(commandCard), fixedHeight(106));
    return {std::make_unique<lotui::WidgetTree>(std::move(root)), tablePtr,
        filterPtr, simulatorPtr, load, std::move(prompt)};
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
        const bool smoke = argc == 2 && std::string_view(argv[1]) == "--smoke";
        const bool capture = argc == 3 && std::string_view(argv[1]) == "--capture";
        const bool captureCommand = argc == 3 &&
            std::string_view(argv[1]) == "--capture-command";
        if (argc > 1 && !smoke && !capture && !captureCommand) return 2;
        auto platform = lotui::createPlatformBackend();
        auto window = platform->createWindow(
            {"LotUI Dashboard", 1080, 880, true, 840, 808});
        window->show();
        lotui::VulkanRenderer renderer(*window);
        const auto font = lotui::executableDirectory() /
            "resources" / "fonts" / "NotoSansKR-Regular.ttf";
        auto text = std::make_shared<lotui::FreetypeTextEngine>(renderer,
            std::vector<lotui::FontSource>{{"Noto Sans KR", font}});
        auto dashboard = buildDashboard(text, *window);
        const auto layout = [](lotui::WidgetTree& tree,
            const lotui::WindowMetrics& metrics) {
            tree.layout({0.0F, 0.0F, static_cast<float>(metrics.width),
                static_cast<float>(metrics.height)});
        };
        layout(*dashboard.tree, window->metrics());
        std::vector<lotui::PaintCommand> commands;
        if (smoke) {
            const auto area = dashboard.reviewFilter->bounds();
            const lotui::Point click{area.x + 8, area.y + area.height / 2};
            dashboard.tree->pointerPressed(click, lotui::PointerButton::Primary);
            dashboard.tree->pointerReleased(click, lotui::PointerButton::Primary);
            if (!dashboard.reviewFilter->isChecked() ||
                dashboard.table->rows().size() != 3)
                throw std::runtime_error("review filter did not update the table");
            const auto slider = dashboard.simulator->bounds();
            const lotui::Point high{slider.x + slider.width * 0.9F,
                slider.y + slider.height * 0.5F};
            dashboard.tree->pointerPressed(high, lotui::PointerButton::Primary);
            dashboard.tree->pointerReleased(high, lotui::PointerButton::Primary);
            if (dashboard.loadGauge->value() < 80)
                throw std::runtime_error("sample slider did not update the gauge");
        }
        if (smoke || captureCommand) {
            const auto field = dashboard.prompt->input->bounds();
            const lotui::Point input{field.x + 12.0F, field.y + field.height * 0.5F};
            dashboard.tree->pointerPressed(input, lotui::PointerButton::Primary);
            dashboard.tree->pointerReleased(input, lotui::PointerButton::Primary);
            dashboard.tree->textInput({lotui::TextInputEventType::Commit, "LOAD 75"});
            dashboard.tree->keyPressed(lotui::KeyCode::Enter);
            if (dashboard.loadGauge->value() != 75)
                throw std::runtime_error("command did not update the gauge");
        }
        if (smoke) {
            if (!dashboard.prompt->handleKey(lotui::KeyCode::Up) ||
                dashboard.prompt->input->text() != "LOAD 75")
                throw std::runtime_error("command history did not recall LOAD 75");
            if (!dashboard.prompt->handleKey(lotui::KeyCode::Down) ||
                !dashboard.prompt->input->text().empty())
                throw std::runtime_error("command history did not restore the draft");
            dashboard.tree->paint(commands);
            renderer.drawFrame(commands);
            return 0;
        }
        for (int frame = 0;; ++frame) {
            bool received = false;
            lotui::PlatformEvent event{};
            while (window->pollEvent(event)) {
                received = true;
                if (event.type == lotui::PlatformEventType::KeyPressed &&
                    dashboard.prompt->handleKey(event.key))
                    continue;
                if (!lotui::example::dispatchPlatformEvent(
                        event, *dashboard.tree, *window, layout))
                    return 0;
            }
            commands.clear();
            dashboard.tree->paint(commands);
            if ((capture || captureCommand) && frame == 6) {
                writeCapture(renderer.drawFrameCaptured(commands), argv[2]);
                return 0;
            }
            renderer.drawFrame(commands);
            if (!received) std::this_thread::sleep_for(std::chrono::milliseconds(8));
        }
    } catch (const std::exception& error) {
        std::cerr << "dashboard_demo failed: " << error.what() << '\n';
        return 1;
    }
}
