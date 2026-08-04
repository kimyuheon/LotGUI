#include "core/widget_tree.h"
#include "widgets/button.h"
#include "widgets/label.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "text_widget_tests failed: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

bool near(float left, float right) {
    return std::abs(left - right) < 0.01F;
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
        commands.push_back({
            {origin.x, origin.y, size_.width, size_.height},
            clip,
            color,
            17,
            0.0F,
        });
    }

private:
    lotui::Size size_{};
};

class TestTextEngine final : public lotui::TextEngine {
public:
    explicit TestTextEngine(lotui::Size layoutSize)
        : layoutSize_(layoutSize) {
    }

    std::unique_ptr<lotui::TextLayout> createLayout(
        std::string_view text,
        const lotui::TextStyle&,
        const lotui::TextLayoutOptions& options) const override {
        ++layoutCount;
        lastText.assign(text);
        lastOptions = options;
        lotui::Size result = layoutSize_;
        if (std::isfinite(options.maximumWidth)) {
            result.width = std::min(result.width, options.maximumWidth);
        }
        return std::make_unique<TestTextLayout>(result);
    }

    mutable int layoutCount{0};
    mutable std::string lastText;
    mutable lotui::TextLayoutOptions lastOptions{};

private:
    lotui::Size layoutSize_{};
};

void validatesPublicTextConfiguration() {
    lotui::TextStyle style;
    require(lotui::isValidTextStyle(style),
            "default text style must be valid");
    style.fontSize = 0.0F;
    require(!lotui::isValidTextStyle(style),
            "zero-sized fonts must be rejected");

    lotui::TextLayoutOptions options;
    require(lotui::isValidTextLayoutOptions(options),
            "default text layout options must be valid");
    options.maximumWidth = -1.0F;
    require(!lotui::isValidTextLayoutOptions(options),
            "negative layout width must be rejected");
}

void cachesAndInvalidatesLabelLayout() {
    auto engine = std::make_shared<TestTextEngine>(
        lotui::Size{80.0F, 18.0F});
    lotui::Label label(engine, "LotUI");

    const auto first = label.measure(
        lotui::LayoutConstraints::loose({60.0F, 100.0F}));
    const auto second = label.measure(
        lotui::LayoutConstraints::loose({60.0F, 100.0F}));
    require(near(first.width, 60.0F) && near(second.height, 18.0F),
            "label must honor the text layout dimensions");
    require(engine->layoutCount == 1 && engine->lastOptions.wrap,
            "equal-width measurement must reuse the cached layout");

    label.setText("공용 GUI");
    label.measure(lotui::LayoutConstraints::loose({60.0F, 100.0F}));
    require(engine->layoutCount == 2 && engine->lastText == "공용 GUI",
            "changing UTF-8 text must invalidate the cached layout");
}

void alignsLabelPaintCommands() {
    auto engine = std::make_shared<TestTextEngine>(
        lotui::Size{30.0F, 10.0F});
    auto label = std::make_unique<lotui::Label>(engine, "Label");
    label->setHorizontalAlignment(lotui::HorizontalTextAlignment::Center);
    label->setVerticalAlignment(lotui::VerticalTextAlignment::Bottom);
    lotui::WidgetTree tree(std::move(label));
    tree.layout({10.0F, 20.0F, 100.0F, 40.0F});

    std::vector<lotui::PaintCommand> commands;
    tree.paint(commands);
    require(commands.size() == 1,
            "label layout must emit its paint commands");
    require(near(commands[0].bounds.x, 45.0F) &&
                near(commands[0].bounds.y, 50.0F),
            "label alignment must position the text layout correctly");
}

void buttonOwnsAndCentersContent() {
    auto engine = std::make_shared<TestTextEngine>(
        lotui::Size{40.0F, 12.0F});
    auto label = std::make_unique<lotui::Label>(engine, "Button");
    lotui::Label* observedLabel = label.get();

    lotui::ButtonStyle style;
    style.contentPadding = {10.0F, 5.0F, 10.0F, 5.0F};
    int clicks = 0;
    auto button = std::make_unique<lotui::Button>(
        std::move(label),
        lotui::Size{20.0F, 20.0F},
        [&clicks]() { ++clicks; },
        style);
    const lotui::Size measured = button->measure(
        lotui::LayoutConstraints::loose({200.0F, 100.0F}));
    require(near(measured.width, 60.0F) && near(measured.height, 22.0F),
            "button size must include content padding");

    lotui::WidgetTree tree(std::move(button));
    tree.layout({0.0F, 0.0F, 100.0F, 40.0F});
    require(near(observedLabel->bounds().x, 30.0F) &&
                near(observedLabel->bounds().y, 14.0F),
            "button content must be centered inside its padding");

    std::vector<lotui::PaintCommand> commands;
    tree.paint(commands);
    require(commands.size() == 2 &&
                commands[0].texture == lotui::invalidTextureId &&
                commands[1].texture == 17,
            "button background must paint before its content");

    tree.pointerPressed({50.0F, 20.0F}, lotui::PointerButton::Primary);
    tree.pointerReleased({50.0F, 20.0F}, lotui::PointerButton::Primary);
    require(clicks == 1,
            "content must not break the button click behavior");
}

} // namespace

int main() {
    validatesPublicTextConfiguration();
    cachesAndInvalidatesLabelLayout();
    alignsLabelPaintCommands();
    buttonOwnsAndCentersContent();
    std::cout << "text_widget_tests passed\n";
    return EXIT_SUCCESS;
}
