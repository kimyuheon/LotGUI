#include "core/widget_tree.h"
#include "widgets/text_field.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
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

} // namespace

int main() {
    handlesCommittedUtf8AndCompositionSeparately();
    cancelsCompositionWhenFocusLeaves();
    std::cout << "text_field_tests passed\n";
    return EXIT_SUCCESS;
}
