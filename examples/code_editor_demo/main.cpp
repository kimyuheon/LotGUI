#include "examples/platform_probe/platform_event_bridge.h"
#include "examples/ribbon_demo/desktop_palette.h"

#include "platform/platform_backend.h"
#include "platform/runtime_paths.h"
#include "renderer/vulkan/vulkan_renderer.h"
#include "text/freetype/freetype_text_engine.h"
#include "widgets/button.h"
#include "widgets/code_editor.h"
#include "widgets/label.h"
#include "widgets/linear_layout.h"
#include "widgets/text_field.h"

#include <chrono>
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

constexpr std::string_view exampleCode =
    "#include \"api/LotCAD_API.h\"\n"
    "\n"
    "// 선택한 CAD 객체를 X 방향으로 이동합니다.\n"
    "void moveSelected(double distance) {\n"
    "    uint32_t id = 0;\n"
    "    if (CAD_GetSelection(&id, 1) != 1) return;\n"
    "\n"
    "    CAD_GeVec3 position{};\n"
    "    if (CAD_GetPosition(id, &position) != CAD_OK) return;\n"
    "    position.x += distance;\n"
    "\n"
    "    if (CAD_BeginTransaction(\"Move object\") != CAD_OK) return;\n"
    "    CAD_SetPosition(id, position);\n"
    "    CAD_EndTransaction();\n"
    "}\n";

lotui::TextStyle textStyle(float size = 14.0F) {
    lotui::TextStyle result;
    result.fontFamilies = {"Noto Sans KR"};
    result.fontSize = size;
    return result;
}

std::unique_ptr<lotui::Label> label(
    const std::shared_ptr<const lotui::TextEngine>& engine,
    std::string value, float size = 14.0F) {
    auto result = std::make_unique<lotui::Label>(
        engine, std::move(value), textStyle(size));
    result->setColor(desktopColor(0xe2e6ea));
    result->setVerticalAlignment(lotui::VerticalTextAlignment::Center);
    return result;
}

lotui::ChildLayout fixed(float width, float height) {
    return {0.0F, {width, height}, {width, height}};
}

lotui::ChildLayout height(float value) {
    return {0.0F, {0.0F, value},
        {lotui::unboundedLayoutSize, value}};
}

lotui::ChildLayout stretch(float height = lotui::unboundedLayoutSize) {
    return {1.0F, {0.0F, 0.0F},
        {lotui::unboundedLayoutSize, height}};
}

struct DemoState {
    lotui::CodeEditor* editor{nullptr};
    lotui::TextField* search{nullptr};
    lotui::Label* status{nullptr};
};

std::unique_ptr<lotui::Button> button(
    const std::shared_ptr<const lotui::TextEngine>& engine,
    std::string title, float width, std::function<void()> action) {
    lotui::ButtonStyle style;
    style.normal = desktopColor(0x343c46);
    style.hovered = desktopColor(0x45515d);
    style.pressed = desktopColor(0x2c3540);
    style.cornerRadius = 3.0F;
    return std::make_unique<lotui::Button>(
        label(engine, std::move(title), 13.0F),
        lotui::Size{width, 30.0F}, std::move(action), style);
}

std::unique_ptr<lotui::WidgetTree> buildUi(
    const std::shared_ptr<const lotui::TextEngine>& engine,
    lotui::PlatformWindow& window, DemoState& state) {
    auto root = std::make_unique<lotui::Column>();
    root->setDecoration(lotui::BoxDecoration{
        desktopColor(0x1b1d21), 0.0F});
    lotui::LinearLayoutOptions rootOptions;
    rootOptions.padding = {16.0F, 14.0F, 16.0F, 14.0F};
    rootOptions.spacing = 10.0F;
    rootOptions.crossAxisAlignment = lotui::CrossAxisAlignment::Stretch;
    root->setOptions(rootOptions);
    root->addChild(label(engine, "코드 편집기  /  lot_plugin.cpp", 18.0F),
        height(30.0F));

    auto toolbar = std::make_unique<lotui::Row>();
    lotui::LinearLayoutOptions toolbarOptions;
    toolbarOptions.spacing = 7.0F;
    toolbarOptions.crossAxisAlignment = lotui::CrossAxisAlignment::Center;
    toolbar->setOptions(toolbarOptions);
    toolbar->addChild(button(engine, "실행 취소", 84.0F,
        [&state] { state.editor->undo(); }), fixed(84.0F, 30.0F));
    toolbar->addChild(button(engine, "다시 실행", 84.0F,
        [&state] { state.editor->redo(); }), fixed(84.0F, 30.0F));

    lotui::TextFieldStyle searchStyle;
    searchStyle.normal = desktopColor(0x343c46);
    searchStyle.hovered = desktopColor(0x3d4854);
    searchStyle.text = desktopColor(0xf0f2f4);
    searchStyle.focusRing = desktopColor(0x809db5);
    searchStyle.focusRingWidth = 1.0F;
    searchStyle.cornerRadius = 3.0F;
    searchStyle.contentPadding = {8.0F, 3.0F, 8.0F, 3.0F};
    auto search = std::make_unique<lotui::TextField>(engine,
        "CAD_GetPosition", lotui::Size{190.0F, 30.0F},
        lotui::TextField::ChangedHandler{},
        [&state](const std::string& value) {
            state.editor->findNext(value);
        }, searchStyle, textStyle(13.0F));
    state.search = search.get();
    search->setClipboardHandlers(
        [&window](std::string text) {
            return window.writeClipboardText(std::move(text));
        }, [&window] { return window.readClipboardText(); });
    toolbar->addChild(std::move(search), fixed(190.0F, 30.0F));
    toolbar->addChild(button(engine, "다음 찾기", 84.0F,
        [&state] {
            if (!state.editor->findNext(state.search->text()))
                state.status->setText("검색 결과가 없습니다.");
        }), fixed(84.0F, 30.0F));
    toolbar->addChild(button(engine, "전체 선택", 84.0F,
        [&state] { state.editor->selectAll(); }), fixed(84.0F, 30.0F));
    root->addChild(std::move(toolbar), height(34.0F));

    lotui::CodeEditorStyle editorStyle;
    editorStyle.background = desktopColor(0x24282e);
    editorStyle.gutter = desktopColor(0x20242a);
    editorStyle.currentLine = desktopColor(0x2c333b);
    editorStyle.lineNumber = desktopColor(0x9daab5);
    editorStyle.text = desktopColor(0xe4e9ed);
    editorStyle.keyword = desktopColor(0x95bde9);
    editorStyle.string = desktopColor(0xb6d38d);
    editorStyle.number = desktopColor(0xe4ae85);
    editorStyle.comment = desktopColor(0x9cb0a9);
    editorStyle.selection = desktopColor(0x435e78);
    editorStyle.lineHeight = 25.0F;
    auto editor = std::make_unique<lotui::CodeEditor>(engine,
        std::string(exampleCode), "cpp", lotui::Size{900.0F, 500.0F},
        [&state](const std::string&) {
            state.status->setText("편집 중  ·  " +
                std::to_string(state.editor->lineCount()) + "줄");
        }, editorStyle, textStyle(15.0F));
    state.editor = editor.get();
    editor->setClipboardHandlers(
        [&window](std::string text) {
            return window.writeClipboardText(std::move(text));
        }, [&window] { return window.readClipboardText(); });
    root->addChild(std::move(editor), stretch());

    auto status = label(engine,
        "C++  ·  Ctrl+F 대신 위 검색창 사용  ·  Tab 들여쓰기  ·  Ctrl+Z 실행 취소",
        12.0F);
    state.status = status.get();
    root->addChild(std::move(status), height(23.0F));
    return std::make_unique<lotui::WidgetTree>(std::move(root));
}

void writeCapture(const lotui::VulkanRenderer::CapturedFrame& frame,
    const char* path) {
    std::ofstream out(path, std::ios::binary);
    if (!out || frame.rgba.empty())
        throw std::runtime_error("code editor capture failed");
    out << "P6\n" << frame.width << ' ' << frame.height << "\n255\n";
    for (std::size_t i = 0; i < frame.rgba.size(); i += 4)
        out.write(reinterpret_cast<const char*>(frame.rgba.data() + i), 3);
    if (!out) throw std::runtime_error("capture write failed");
}

} // namespace

int main(int argc, char** argv) {
    try {
        const bool smoke = argc == 2 &&
            std::string_view(argv[1]) == "--smoke";
        const bool test = argc == 2 &&
            std::string_view(argv[1]) == "--self-test";
        const bool capture = argc == 3 &&
            std::string_view(argv[1]) == "--capture";
        if (argc > 1 && !smoke && !test && !capture) return 2;
        auto platform = lotui::createPlatformBackend();
        auto window = platform->createWindow(
            {"LotUI Code Editor", 960, 680, true});
        window->show();
        lotui::VulkanRenderer renderer(*window);
        const auto font = lotui::executableDirectory() /
            "resources" / "fonts" / "NotoSansKR-Regular.ttf";
        auto engine = std::make_shared<lotui::FreetypeTextEngine>(renderer,
            std::vector<lotui::FontSource>{{"Noto Sans KR", font}});
        DemoState state;
        auto widgets = buildUi(engine, *window, state);
        const auto updateLayout = [](lotui::WidgetTree& tree,
                const lotui::WindowMetrics& metrics) {
            tree.layout({0.0F, 0.0F,
                static_cast<float>(metrics.width),
                static_cast<float>(metrics.height)});
        };
        updateLayout(*widgets, window->metrics());
        if (test && (!state.editor->findNext("CAD_GetPosition") ||
                state.editor->selectedText() != "CAD_GetPosition"))
            throw std::runtime_error("code editor search failed");
        std::vector<lotui::PaintCommand> commands;
        if (smoke || test) {
            widgets->paint(commands);
            renderer.drawFrame(commands);
            std::cout << "code-editor: " << commands.size()
                      << " paint commands\n";
            return 0;
        }
        bool running = true;
        int frames = 0;
        while (running) {
            bool received = false;
            lotui::PlatformEvent event{};
            while (window->pollEvent(event)) {
                received = true;
                running = lotui::example::dispatchPlatformEvent(
                    event, *widgets, *window, updateLayout) && running;
            }
            if (!running) break;
            commands.clear();
            widgets->paint(commands);
            if (capture && frames == 6) {
                writeCapture(renderer.drawFrameCaptured(commands), argv[2]);
                return 0;
            }
            renderer.drawFrame(commands);
            ++frames;
            if (!received)
                std::this_thread::sleep_for(std::chrono::milliseconds(8));
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "code_editor_demo failed: " << error.what() << '\n';
        return 1;
    }
}
