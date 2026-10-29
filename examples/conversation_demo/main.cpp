#include "examples/platform_probe/platform_event_bridge.h"

#include "platform/platform_backend.h"
#include "platform/runtime_paths.h"
#include "renderer/vulkan/vulkan_renderer.h"
#include "text/freetype/freetype_text_engine.h"
#include "widgets/button.h"
#include "widgets/conversation_list.h"
#include "widgets/label.h"
#include "widgets/linear_layout.h"
#include "widgets/text_field.h"

#include <chrono>
#include <exception>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace {

std::vector<lotui::ConversationMessage> sampleMessages() {
    auto message = [](std::string id, std::string author,
            std::string time, bool outgoing, std::string_view body) {
        return lotui::ConversationMessage{std::move(id), std::move(author),
            std::move(time), outgoing, lotui::parseMessageBlocks(body)};
    };
    return {
        message("hello", "민지", "10:24", false,
            "안녕하세요! 새 대화 목록 위젯을 테스트해 볼까요?"),
        message("reply", "나", "10:25", true,
            "네. 일반 대화와 코드 블록을 함께 보여줄게요."),
        message("cpp", "민지", "10:26", false,
            "C++ 코드는 이렇게 표시됩니다.\n```cpp\n"
            "std::vector<int> rows{1, 2, 3};\n"
            "for (auto row : rows) { /* render */ }\n```"),
        message("python", "나", "10:27", true,
            "Python도 언어별 색상을 적용합니다.\n```python\n"
            "def render(items):\n    return [item for item in items]\n```"),
        message("json", "민지", "10:28", false,
            "JSON과 C# 같은 코드 블록도 넣을 수 있어요.\n```json\n"
            "{\"theme\": \"dark\", \"count\": 3}\n```\n"
            "```csharp\nvar message = \"안녕하세요\";\n```"),
    };
}

void writeCapture(const lotui::VulkanRenderer::CapturedFrame& frame,
    const char* filename) {
    std::ofstream out(filename, std::ios::binary);
    if (!out || frame.rgba.empty())
        throw std::runtime_error("capture failed");
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
        const bool capture = argc == 3 &&
            std::string_view(argv[1]) == "--capture";
        if (argc > 1 && !smoke && !capture) {
            std::cerr << "usage: conversation_demo [--smoke|--capture output.ppm]\n";
            return 2;
        }
        auto platform = lotui::createPlatformBackend();
        auto window = platform->createWindow(
            {"LotUI Conversation", 820, 700, true});
        window->show();
        lotui::VulkanRenderer renderer(*window);
        const auto font = lotui::executableDirectory() /
            "resources" / "fonts" / "NotoSansKR-Regular.ttf";
        auto textEngine = std::make_shared<lotui::FreetypeTextEngine>(
            renderer, std::vector<lotui::FontSource>{{"Noto Sans KR", font}});

        auto root = std::make_unique<lotui::Column>();
        root->setDecoration(lotui::BoxDecoration{
            {0.10F, 0.13F, 0.18F, 1.0F}, 0.0F});
        lotui::LinearLayoutOptions options;
        options.padding = {18.0F, 16.0F, 18.0F, 16.0F};
        options.spacing = 12.0F;
        options.crossAxisAlignment = lotui::CrossAxisAlignment::Stretch;
        root->setOptions(options);

        lotui::TextStyle titleStyle;
        titleStyle.fontFamilies = {"Noto Sans KR"};
        titleStyle.fontSize = 20.0F;
        titleStyle.weight = lotui::FontWeight::SemiBold;
        root->addChild(std::make_unique<lotui::Label>(
            textEngine, "LotUI 대화 · 언어별 코드 블록", titleStyle),
            {0.0F, {0.0F, 30.0F}, {lotui::unboundedLayoutSize, 30.0F}});
        auto list = std::make_unique<lotui::ConversationList>(
            textEngine, sampleMessages());
        auto* conversations = list.get();
        list->setOnSelectionChanged([](std::string_view id) {
            std::cout << "selected-message: " << id << '\n';
        });
        root->addChild(std::move(list), {1.0F});

        auto composer = std::make_unique<lotui::Row>();
        lotui::LinearLayoutOptions composerOptions;
        composerOptions.spacing = 8.0F;
        composerOptions.crossAxisAlignment =
            lotui::CrossAxisAlignment::Stretch;
        composer->setOptions(composerOptions);
        auto field = std::make_unique<lotui::TextField>(textEngine);
        auto* input = field.get();
        auto send = [conversations, input]() {
            if (input->text().empty()) return;
            conversations->appendMessage({
                "new-" + std::to_string(conversations->messages().size()),
                "나", "지금", true,
                lotui::parseMessageBlocks(input->text())});
            input->setText("");
        };
        input->setOnSubmitted([send](const std::string&) { send(); });
        composer->addChild(std::move(field), {1.0F});
        auto label = std::make_unique<lotui::Label>(
            textEngine, "보내기", lotui::TextStyle{});
        composer->addChild(std::make_unique<lotui::Button>(
            std::move(label), lotui::Size{94.0F, 40.0F}, send),
            {0.0F, {94.0F, 40.0F}, {94.0F, 40.0F}});
        root->addChild(std::move(composer),
            {0.0F, {0.0F, 40.0F}, {lotui::unboundedLayoutSize, 40.0F}});

        lotui::WidgetTree tree(std::move(root));
        auto updateLayout = [](lotui::WidgetTree& widgets,
                const lotui::WindowMetrics& metrics) {
            widgets.layout({0.0F, 0.0F, static_cast<float>(metrics.width),
                static_cast<float>(metrics.height)});
        };
        updateLayout(tree, window->metrics());
        window->setTextInputState(tree.textInputState());
        std::vector<lotui::PaintCommand> commands;
        if (smoke) {
            tree.paint(commands);
            renderer.drawFrame(commands);
            std::cout << "conversation-demo-smoke: " << commands.size()
                << " paint commands\n";
            return 0;
        }
        int frame = 0;
        bool running = true;
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
            window->setTextInputState(tree.textInputState());
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
        std::cerr << "conversation_demo failed: " << error.what() << '\n';
        return 1;
    }
}
