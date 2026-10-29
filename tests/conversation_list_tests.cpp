#include "core/widget_tree.h"
#include "widgets/conversation_list.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "conversation_list_tests failed: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

class StubLayout final : public lotui::TextLayout {
public:
    explicit StubLayout(lotui::Size size) : size_(size) {}
    lotui::Size size() const noexcept override { return size_; }
    float baseline() const noexcept override { return 12.0F; }
    void appendPaintCommands(lotui::Point origin, lotui::Rect clip,
        lotui::Color color,
        std::vector<lotui::PaintCommand>& commands) const override {
        commands.push_back({{origin.x, origin.y, size_.width, size_.height},
            clip, color, 11, 0.0F});
    }
private:
    lotui::Size size_{};
};

class StubEngine final : public lotui::TextEngine {
public:
    std::unique_ptr<lotui::TextLayout> createLayout(
        std::string_view text, const lotui::TextStyle&,
        const lotui::TextLayoutOptions& options) const override {
        const float width = static_cast<float>(text.size()) * 7.0F;
        const float available = options.wrap ?
            std::max(1.0F, options.maximumWidth) : width;
        const float lines = std::max(1.0F,
            std::ceil(width / std::max(1.0F, available)));
        return std::make_unique<StubLayout>(lotui::Size{
            std::min(width, available), lines * 16.0F});
    }
};

void parserAndLanguageTokens() {
    const auto blocks = lotui::parseMessageBlocks(
        "설명\n```cpp\nint value = 42; // note\n```\n"
        "다음\n```python\ndef run(): # note\n```");
    require(blocks.size() == 4 &&
            blocks[0].kind == lotui::MessageBlockKind::Text &&
            blocks[1].kind == lotui::MessageBlockKind::Code &&
            blocks[1].language == "cpp" &&
            blocks[2].text == "다음" &&
            blocks[3].language == "python",
        "fences must preserve text and language-specific code blocks");
    auto cpp = lotui::tokenizeCodeLine("c++", "int value = 42; // note");
    require(cpp.front().kind == lotui::CodeTokenKind::Keyword &&
            cpp[6].kind == lotui::CodeTokenKind::Number &&
            cpp.back().kind == lotui::CodeTokenKind::Comment,
        "C++ keywords, numbers and comments must be distinct");
    auto python = lotui::tokenizeCodeLine("python", "def f(): # note");
    require(python.front().kind == lotui::CodeTokenKind::Keyword &&
            python.back().kind == lotui::CodeTokenKind::Comment,
        "Python syntax must use its own rules");
    auto json = lotui::tokenizeCodeLine("json", "{\"count\": 3}");
    require(json[1].kind == lotui::CodeTokenKind::String &&
            json[4].kind == lotui::CodeTokenKind::Number,
        "JSON strings and numbers must be highlighted");
    const auto korean = lotui::tokenizeCodeLine("python", "변수 = 3");
    require(!korean.empty() && korean.front().text == "변수",
        "UTF-8 code identifiers must stay intact for text shaping");
}

void layoutScrollAndSelection() {
    auto engine = std::make_shared<StubEngine>();
    std::vector<lotui::ConversationMessage> messages{
        {"one", "민지", "10:00", false,
            lotui::parseMessageBlocks("안녕하세요")},
        {"two", "나", "10:01", true,
            lotui::parseMessageBlocks("```python\ndef run():\n    return 3\n```")},
        {"three", "민지", "10:02", false,
            lotui::parseMessageBlocks("다음 메시지")},
    };
    auto view = std::make_unique<lotui::ConversationList>(engine, messages);
    auto* list = view.get();
    int selected = 0;
    list->setOnSelectionChanged([&](std::string_view id) {
        if (id == "one") ++selected;
    });
    lotui::WidgetTree tree(std::move(view));
    tree.layout({0.0F, 0.0F, 400.0F, 150.0F});
    const auto first = list->messageBounds("one");
    const auto second = list->messageBounds("two");
    require(first.x < second.x && first.y < second.y,
        "incoming and outgoing bubbles must be on opposite sides");
    const lotui::Point click{first.x + 20.0F, first.y + 20.0F};
    tree.pointerPressed(click, lotui::PointerButton::Primary);
    tree.pointerReleased(click, lotui::PointerButton::Primary);
    require(selected == 1 && list->selectedId() == "one",
        "click must select a message by stable ID");
    list->setScrollOffset(65.0F);
    std::vector<lotui::PaintCommand> commands;
    tree.paint(commands);
    require(!commands.empty(), "conversation must paint");
    const lotui::ConversationListStyle colors;
    require(std::any_of(commands.begin(), commands.end(),
            [&colors](const auto& command) {
                return command.texture == 11 &&
                    command.color.red == colors.keyword.red &&
                    command.color.green == colors.keyword.green &&
                    command.color.blue == colors.keyword.blue;
            }), "code keywords must reach paint commands with their color");
    list->setScrollOffset(0.0F);
    tree.scroll({100.0F, 80.0F}, {0.0F, 3.0F},
        lotui::ScrollDeltaMode::Line);
    require(list->scrollOffset() > 0.0F,
        "mouse wheel must scroll the message list");
    list->scrollToEnd();
    const float before = list->scrollOffset();
    list->appendMessage({"four", "나", "10:03", true,
        lotui::parseMessageBlocks("새 메시지")});
    require(list->messages().size() == 4 &&
            list->scrollOffset() > before,
        "append at the end must keep the latest message visible");
    bool duplicateRejected = false;
    try {
        list->appendMessage({"four", "나", "10:04", true, {}});
    } catch (const std::invalid_argument&) { duplicateRejected = true; }
    require(duplicateRejected && list->messages().size() == 4,
        "duplicate message IDs must be rejected without changing the model");
    list->setMessages({messages[1]});
    require(!list->selectedId(),
        "removed selection must clear after model replacement");
}

} // namespace

int main() {
    parserAndLanguageTokens();
    layoutScrollAndSelection();
    std::cout << "conversation_list_tests passed\n";
    return EXIT_SUCCESS;
}
