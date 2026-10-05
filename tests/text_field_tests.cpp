#include "core/widget_tree.h"
#include "widgets/text_field.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "text_field_tests failed: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

class TestTextLayout final : public lotui::TextLayout {
public:
    explicit TestTextLayout(lotui::Size size)
        : size_(size) {
    }

    lotui::Size size() const noexcept override {
        return size_;
    }

    float baseline() const noexcept override {
        return size_.height * 0.8F;
    }

    void appendPaintCommands(
        lotui::Point origin,
        lotui::Rect clip,
        lotui::Color color,
        std::vector<lotui::PaintCommand>& commands) const override {
        if (size_.width > 0.0F) {
            commands.push_back({
                {origin.x, origin.y, size_.width, size_.height},
                clip, color, 41, 0.0F,
            });
        }
    }

private:
    lotui::Size size_{};
};

class TestTextEngine final : public lotui::TextEngine {
public:
    std::unique_ptr<lotui::TextLayout> createLayout(
        std::string_view text,
        const lotui::TextStyle& style,
        const lotui::TextLayoutOptions& options) const override {
        float width = static_cast<float>(text.size()) * 5.0F;
        if (std::isfinite(options.maximumWidth)) {
            width = std::min(width, options.maximumWidth);
        }
        return std::make_unique<TestTextLayout>(
            lotui::Size{width, style.fontSize * 1.2F});
    }
};

void handlesCommittedUtf8AndCompositionSeparately() {
    auto engine = std::make_shared<TestTextEngine>();
    int changes = 0;
    int submissions = 0;
    std::string observed;
    auto field = std::make_unique<lotui::TextField>(
        engine,
        "A한",
        lotui::Size{220.0F, 40.0F},
        [&changes, &observed](const std::string& text) {
            ++changes;
            observed = text;
        },
        [&submissions](const std::string&) { ++submissions; });
    lotui::TextField* input = field.get();
    lotui::WidgetTree tree(std::move(field));
    tree.layout({10.0F, 20.0F, 220.0F, 40.0F});

    tree.keyPressed(lotui::KeyCode::Tab);
    require(input->isFocused() && tree.textInputState().enabled,
        "focused TextField must request a native text input session");

    const auto composition = tree.textInput({
        lotui::TextInputEventType::Composition,
        "가",
        std::string("가").size(),
        0,
    });
    require(composition.handled && input->text() == "A한" &&
            input->composition() == "가" && changes == 0,
        "pre-edit text must not mutate committed UTF-8 text");

    tree.textInput({lotui::TextInputEventType::Commit, "가"});
    tree.textInput({lotui::TextInputEventType::CompositionEnd});
    require(input->text() == "A한가" && input->composition().empty() &&
            changes == 1 && observed == "A한가",
        "committed IME result must insert once and notify once");

    tree.keyPressed(lotui::KeyCode::Left);
    tree.keyPressed(lotui::KeyCode::Backspace);
    require(input->text() == "A가" && changes == 2,
        "Backspace must erase one complete UTF-8 code point");

    tree.textInput({lotui::TextInputEventType::Commit, "나"});
    require(input->text() == "A나가" && changes == 3,
        "committed text must insert at the UTF-8 cursor boundary");

    tree.keyPressed(lotui::KeyCode::Enter);
    require(submissions == 1,
        "Enter must invoke the single-line submit callback");

    std::vector<lotui::PaintCommand> commands;
    tree.paint(commands);
    const lotui::TextInputState state = tree.textInputState();
    require(state.enabled && state.inputRect.x > input->bounds().x,
        "painting must update the native IME candidate position");

    tree.textInput({lotui::TextInputEventType::Composition, "한", 0, 0});
    commands.clear();
    tree.paint(commands);
    const float committedCaretX = input->bounds().x +
        input->style().contentPadding.left +
        static_cast<float>(input->cursorByteOffset()) * 5.0F;
    require(tree.textInputState().inputRect.x > committedCaretX,
        "an IME without a cursor offset must draw the caret after pre-edit text");
}

void cancelsCompositionWhenFocusLeaves() {
    auto engine = std::make_shared<TestTextEngine>();
    auto field = std::make_unique<lotui::TextField>(engine);
    lotui::TextField* input = field.get();
    lotui::WidgetTree tree(std::move(field));
    tree.layout({0.0F, 0.0F, 200.0F, 40.0F});
    tree.keyPressed(lotui::KeyCode::Tab);
    tree.textInput({lotui::TextInputEventType::Composition, "한"});
    tree.clearFocus();
    require(input->composition().empty() && !tree.textInputState().enabled,
        "losing focus must cancel composition and end native input");
}

void selectsUtf8WithPointerAndKeyboard() {
    auto engine = std::make_shared<TestTextEngine>();
    int changes = 0;
    auto field = std::make_unique<lotui::TextField>(
        engine, "A한B", lotui::Size{220.0F, 40.0F},
        [&changes](const std::string&) { ++changes; });
    lotui::TextField* input = field.get();
    lotui::WidgetTree tree(std::move(field));
    tree.layout({0.0F, 0.0F, 220.0F, 40.0F});

    tree.pointerPressed({18.0F, 20.0F}, lotui::PointerButton::Primary);
    require(input->cursorByteOffset() == 1 && input->selectedText().empty(),
        "clicking text must place the caret at a UTF-8 boundary");
    tree.pointerMoved({100.0F, 20.0F});
    tree.pointerReleased({100.0F, 20.0F}, lotui::PointerButton::Primary);
    require(input->selectedText() == "한B" &&
            input->cursorByteOffset() == input->text().size(),
        "dragging must select complete UTF-8 code points");

    std::vector<lotui::PaintCommand> commands;
    tree.paint(commands);
    const bool hasHighlight = std::any_of(
        commands.begin(), commands.end(),
        [input](const lotui::PaintCommand& command) {
            return command.color.red == input->style().selection.red &&
                command.color.green == input->style().selection.green &&
                command.bounds.width > 0.0F;
        });
    require(hasHighlight, "selection must be painted behind the text");

    tree.keyPressed(lotui::KeyCode::Backspace);
    require(input->text() == "A" && input->selectedText().empty() &&
            changes == 1,
        "Backspace must remove the selection and notify once");

    tree.keyPressed(lotui::KeyCode::Home);
    tree.keyPressed(lotui::KeyCode::Right, {true});
    require(input->selectedText() == "A",
        "Shift+Right must extend the selection");
    tree.textInput({lotui::TextInputEventType::Commit, "가"});
    require(input->text() == "가" && changes == 2,
        "committed text must replace the selection once");

    tree.keyPressed(lotui::KeyCode::A, {false, true});
    require(input->selectedText() == "가",
        "Ctrl+A must select all committed text");
    tree.textInput({lotui::TextInputEventType::Composition, "나"});
    require(input->text() == "가" && input->selectedText() == "가",
        "IME pre-edit must preserve selected committed text");
    tree.textInput({lotui::TextInputEventType::Commit, "나"});
    require(input->text() == "나" && changes == 3,
        "IME commit must replace the selection once");
    tree.keyPressed(lotui::KeyCode::A, {false, true});
    tree.keyPressed(lotui::KeyCode::Left);
    require(input->cursorByteOffset() == 0 && input->selectedText().empty(),
        "Left without Shift must collapse the selection to its start");
    tree.keyPressed(lotui::KeyCode::End, {true});
    require(input->selectedText() == "나",
        "Shift+End must extend the selection to the end");
    tree.keyPressed(lotui::KeyCode::Delete);
    require(input->text().empty() && changes == 4,
        "Delete must remove selected text once");
}

void keepsCaretVisibleInLongText() {
    auto engine = std::make_shared<TestTextEngine>();
    auto field = std::make_unique<lotui::TextField>(
        engine, std::string(30, 'x'), lotui::Size{100.0F, 40.0F});
    lotui::TextField* input = field.get();
    lotui::WidgetTree tree(std::move(field));
    tree.layout({0.0F, 0.0F, 100.0F, 40.0F});
    tree.keyPressed(lotui::KeyCode::Tab);
    const auto endRect = tree.textInputState().inputRect;
    require(endRect.x < 100.0F - input->style().contentPadding.right,
        "the IME caret must remain inside the text viewport");

    tree.pointerPressed({13.0F, 20.0F}, lotui::PointerButton::Primary);
    tree.pointerReleased({13.0F, 20.0F}, lotui::PointerButton::Primary);
    require(input->cursorByteOffset() > 0 &&
            input->cursorByteOffset() < input->text().size(),
        "clicking scrolled text must account for its horizontal offset");
    tree.keyPressed(lotui::KeyCode::Home);
    require(tree.textInputState().inputRect.x ==
            input->bounds().x + input->style().contentPadding.left,
        "moving Home must expose the start of the text immediately");
}

void supportsClipboardShortcutsThroughHandlers() {
    auto engine = std::make_shared<TestTextEngine>();
    int changes = 0;
    std::optional<std::string> clipboard;
    auto field = std::make_unique<lotui::TextField>(
        engine, "가나다", lotui::Size{220.0F, 40.0F},
        [&changes](const std::string&) { ++changes; });
    lotui::TextField* input = field.get();
    input->setClipboardHandlers(
        [&clipboard](std::string text) {
            clipboard = std::move(text);
            return true;
        },
        [&clipboard]() { return clipboard; });
    lotui::WidgetTree tree(std::move(field));
    tree.layout({0.0F, 0.0F, 220.0F, 40.0F});
    tree.keyPressed(lotui::KeyCode::Tab);
    const lotui::KeyModifiers control{false, true};

    tree.keyPressed(lotui::KeyCode::A, control);
    require(tree.keyPressed(lotui::KeyCode::C, control).handled &&
            clipboard == "가나다" && input->text() == "가나다" &&
            changes == 0,
        "copy must write selected UTF-8 without changing the field");
    tree.keyPressed(lotui::KeyCode::X, control);
    require(input->text().empty() && changes == 1,
        "cut must remove the selected text and notify once");
    tree.keyPressed(lotui::KeyCode::V, control);
    require(input->text() == "가나다" && changes == 2,
        "paste must insert clipboard UTF-8 and notify once");

    tree.keyPressed(lotui::KeyCode::A, control);
    clipboard = std::string{};
    tree.keyPressed(lotui::KeyCode::V, control);
    require(input->text().empty() && changes == 3,
        "pasting empty text over a selection must remove it");
    tree.keyPressed(lotui::KeyCode::V, control);
    require(changes == 3,
        "pasting empty text without a selection must not notify");

    input->setText("한");
    input->setClipboardHandlers(
        [](std::string) { return false; },
        [&clipboard]() { return clipboard; });
    tree.keyPressed(lotui::KeyCode::A, control);
    tree.keyPressed(lotui::KeyCode::X, control);
    require(input->text() == "한" && changes == 3,
        "cut must preserve text when clipboard writing fails");
}

} // namespace

int main() {
    handlesCommittedUtf8AndCompositionSeparately();
    cancelsCompositionWhenFocusLeaves();
    selectsUtf8WithPointerAndKeyboard();
    keepsCaretVisibleInLongText();
    supportsClipboardShortcutsThroughHandlers();
    std::cout << "text_field_tests passed\n";
    return EXIT_SUCCESS;
}
