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
        std::string_view text, const lotui::TextStyle& style,
        const lotui::TextLayoutOptions& options) const override {
        requestedSizes.push_back(style.fontSize);
        const float width = static_cast<float>(text.size()) * 7.0F;
        const float available = options.wrap ?
            std::max(1.0F, options.maximumWidth) : width;
        const float lines = std::max(1.0F,
            std::ceil(width / std::max(1.0F, available)));
        return std::make_unique<StubLayout>(lotui::Size{
            std::min(width, available), lines * 16.0F});
    }
    mutable std::vector<float> requestedSizes;
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
    const auto tabbed = lotui::tokenizeCodeLine("python", "\t  return 3\r");
    require(tabbed.size() >= 2 && tabbed.front().text == "      " &&
            tabbed[1].text == "return" &&
            tabbed[1].kind == lotui::CodeTokenKind::Keyword,
        "tabs must expand to four-column stops without losing indentation");
}

void layoutScrollAndSelection() {
    auto engine = std::make_shared<StubEngine>();
    std::vector<lotui::ConversationMessage> messages{
        {"one", "민지", "10:00", false,
            lotui::parseMessageBlocks("안녕하세요")},
        {"two", "나", "10:01", true,
            lotui::parseMessageBlocks("```python\ndef run():\n\treturn 3\n```")},
        {"three", "민지", "10:02", false,
            lotui::parseMessageBlocks("다음 메시지")},
    };
    lotui::ConversationListStyle visualStyle;
    visualStyle.bodyFontSize = 16.0F;
    visualStyle.metaFontSize = 12.0F;
    visualStyle.codeFontSize = 16.0F;
    auto view = std::make_unique<lotui::ConversationList>(engine, messages,
        lotui::Size{400.0F, 150.0F}, visualStyle);
    auto* list = view.get();
    int selected = 0;
    list->setOnSelectionChanged([&](std::string_view id) {
        if (id == "one") ++selected;
    });
    lotui::WidgetTree tree(std::move(view));
    tree.layout({0.0F, 0.0F, 400.0F, 150.0F});
    require(std::find(engine->requestedSizes.begin(),
                engine->requestedSizes.end(), 16.0F) !=
            engine->requestedSizes.end() &&
            std::find(engine->requestedSizes.begin(),
                engine->requestedSizes.end(), 12.0F) !=
            engine->requestedSizes.end(),
        "configured font sizes must reach the text engine");
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
    float lastKeywordY = -1.0F;
    float lastKeywordX = -1.0F;
    for (const auto& command : commands) {
        if (command.texture == 11 &&
            command.color.red == colors.keyword.red &&
            command.bounds.y > lastKeywordY) {
            lastKeywordY = command.bounds.y;
            lastKeywordX = command.bounds.x;
        }
    }
    require(lastKeywordX == second.x + colors.bubblePadding + 10.0F + 28.0F,
        "tab indentation must shift rendered code by four spaces");
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

void copiesOriginalCodeWithoutSelectingMessage() {
    auto engine = std::make_shared<StubEngine>();
    auto view = std::make_unique<lotui::ConversationList>(engine,
        std::vector<lotui::ConversationMessage>{{
            "code", "민지", "10:00", false,
            lotui::parseMessageBlocks(
                "예제\n```cpp\nint value = 3;\n```\n"
                "```python\n# 한글 주석\ndef run():\n\treturn 3\n```")}});
    auto* list = view.get();
    std::string copied;
    list->setClipboardWriteHandler([&copied](std::string text) {
        copied = std::move(text);
        return true;
    });
    lotui::WidgetTree tree(std::move(view));
    tree.layout({0.0F, 0.0F, 600.0F, 400.0F});
    require(!lotui::hasArea(list->codeCopyBounds("code", 0)) &&
            lotui::hasArea(list->codeCopyBounds("code", 1)) &&
            lotui::hasArea(list->codeCopyBounds("code", 2)),
        "copy buttons must appear only on code blocks");
    const auto button = list->codeCopyBounds("code", 2);
    const lotui::Point center{button.x + button.width * 0.5F,
        button.y + button.height * 0.5F};
    tree.pointerPressed(center, lotui::PointerButton::Primary);
    tree.pointerReleased(center, lotui::PointerButton::Primary);
    require(copied == "# 한글 주석\ndef run():\n\treturn 3" &&
            !list->selectedId(),
        "copy must preserve UTF-8 code text and indentation without selecting");
    std::vector<lotui::PaintCommand> painted;
    tree.paint(painted);
    const auto success = lotui::ConversationListStyle{}.copyButtonCopied;
    require(std::any_of(painted.begin(), painted.end(), [success](
            const lotui::PaintCommand& command) {
            return command.color.red == success.red &&
                command.color.green == success.green &&
                command.color.blue == success.blue;
        }), "successful copy must show its completion state");
    list->setClipboardWriteHandler({});
    require(!lotui::hasArea(list->codeCopyBounds("code", 2)),
        "copy controls must hide when no clipboard writer is available");
}

void wideAnswersWrapWhenResized() {
    auto engine = std::make_shared<StubEngine>();
    lotui::ConversationListStyle style;
    style.incomingFullWidth = true;
    auto view = std::make_unique<lotui::ConversationList>(engine,
        std::vector<lotui::ConversationMessage>{
            {"question", "나", "10:31", true,
                lotui::parseMessageBlocks("짧은 질문")},
            {"answer", "LotUI", "10:31", false,
                lotui::parseMessageBlocks(
                    "한글로 작성한 답변은 가로 폭이 줄면 자동으로 다음 줄에 "
                    "계속 표시되어야 합니다. 끝까지 표시합니다.")},
        }, lotui::Size{600.0F, 400.0F}, style);
    auto* list = view.get();
    lotui::WidgetTree tree(std::move(view));
    tree.layout({0.0F, 0.0F, 600.0F, 600.0F});
    const auto wideAnswer = list->messageBounds("answer");
    const auto question = list->messageBounds("question");
    require(wideAnswer.width > question.width &&
            question.x > wideAnswer.x,
        "questions must sit right of full-width answers");
    tree.layout({0.0F, 0.0F, 320.0F, 600.0F});
    const auto narrowAnswer = list->messageBounds("answer");
    require(narrowAnswer.width < wideAnswer.width &&
            narrowAnswer.height > wideAnswer.height,
        "long answers must reflow into more lines when the view narrows");
}

} // namespace

int main() {
    parserAndLanguageTokens();
    layoutScrollAndSelection();
    copiesOriginalCodeWithoutSelectingMessage();
    wideAnswersWrapWhenResized();
    std::cout << "conversation_list_tests passed\n";
    return EXIT_SUCCESS;
}
