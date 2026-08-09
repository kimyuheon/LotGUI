#include "core/widget_tree.h"
#include "widgets/checkbox.h"
#include "widgets/label.h"
#include "widgets/numeric_input.h"

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
        std::cerr << "form_widget_tests failed: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

bool near(double left, double right) {
    return std::abs(left - right) < 0.0001;
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
            clip, color, 31, 0.0F,
        });
    }

private:
    lotui::Size size_{};
};

class TestTextEngine final : public lotui::TextEngine {
public:
    std::unique_ptr<lotui::TextLayout> createLayout(
        std::string_view text,
        const lotui::TextStyle& style,
        const lotui::TextLayoutOptions&) const override {
        return std::make_unique<TestTextLayout>(lotui::Size{
            static_cast<float>(text.size()) * style.fontSize * 0.5F,
            style.fontSize * 1.2F,
        });
    }
};

void togglesCheckboxWithPointerAndKeyboard() {
    auto engine = std::make_shared<TestTextEngine>();
    auto label = std::make_unique<lotui::Label>(engine, "GPU 가속");
    int changes = 0;
    bool lastValue = false;
    auto checkbox = std::make_unique<lotui::Checkbox>(
        std::move(label),
        false,
        [&changes, &lastValue](bool checked) {
            ++changes;
            lastValue = checked;
        });
    lotui::Checkbox* observed = checkbox.get();
    lotui::WidgetTree tree(std::move(checkbox));
    tree.layout({10.0F, 10.0F, 180.0F, 40.0F});

    tree.pointerPressed({20.0F, 20.0F}, lotui::PointerButton::Primary);
    tree.pointerReleased({20.0F, 20.0F}, lotui::PointerButton::Primary);
    require(observed->isChecked() && changes == 1 && lastValue,
        "pointer release must toggle the checkbox once");

    tree.keyPressed(lotui::KeyCode::Space);
    require(observed->isPressed(),
        "Space press must show checkbox keyboard feedback");
    tree.keyReleased(lotui::KeyCode::Space);
    require(!observed->isChecked() && changes == 2 && !lastValue,
        "Space release must toggle the focused checkbox");

    tree.keyPressed(lotui::KeyCode::Enter);
    tree.cancelKeyboard();
    tree.keyReleased(lotui::KeyCode::Enter);
    require(changes == 2 && !observed->isPressed(),
        "keyboard cancellation must prevent a checkbox toggle");

    std::vector<lotui::PaintCommand> commands;
    observed->setChecked(true);
    tree.paint(commands);
    require(commands.size() >= 3,
        "checked checkbox must paint its box, mark, and label");
}

void editsNumericValueWithinItsRange() {
    auto engine = std::make_shared<TestTextEngine>();
    lotui::NumericInputOptions options;
    options.value = 1.5;
    options.minimum = 0.0;
    options.maximum = 2.0;
    options.step = 0.25;
    options.decimalPlaces = 2;

    int changes = 0;
    double lastValue = 0.0;
    auto input = std::make_unique<lotui::NumericInput>(
        engine,
        options,
        [&changes, &lastValue](double value) {
            ++changes;
            lastValue = value;
        });
    lotui::NumericInput* observed = input.get();
    lotui::WidgetTree tree(std::move(input));
    tree.layout({0.0F, 0.0F, 160.0F, 40.0F});

    tree.keyPressed(lotui::KeyCode::Tab);
    require(observed->isFocused(),
        "Tab must focus the numeric input");
    tree.keyPressed(lotui::KeyCode::Up);
    tree.keyReleased(lotui::KeyCode::Up);
    require(near(observed->value(), 1.75) && changes == 1 &&
            near(lastValue, 1.75),
        "Up must increment by one configured step");

    tree.keyPressed(lotui::KeyCode::PageUp);
    require(near(observed->value(), 2.0) && changes == 2,
        "PageUp must clamp a large step to the maximum");
    tree.keyPressed(lotui::KeyCode::PageUp);
    require(changes == 2,
        "attempting to exceed the maximum must not emit a change");
    tree.keyPressed(lotui::KeyCode::Home);
    require(near(observed->value(), 0.0) && changes == 3,
        "Home must move to the minimum");

    tree.pointerPressed({150.0F, 5.0F}, lotui::PointerButton::Primary);
    tree.pointerReleased({150.0F, 5.0F}, lotui::PointerButton::Primary);
    require(near(observed->value(), 0.25) && changes == 4,
        "upper step button must increment the value");
    tree.pointerPressed({150.0F, 35.0F}, lotui::PointerButton::Primary);
    tree.pointerReleased({150.0F, 35.0F}, lotui::PointerButton::Primary);
    require(near(observed->value(), 0.0) && changes == 5,
        "lower step button must decrement the value");

    observed->setValue(1.25);
    require(near(observed->value(), 1.25) && changes == 5,
        "programmatic value updates must not invoke user callbacks");
    observed->setRange(1.5, 3.0);
    require(near(observed->value(), 1.5),
        "range changes must clamp the current value");

    std::vector<lotui::PaintCommand> commands;
    tree.paint(commands);
    require(commands.size() >= 7,
        "numeric input must paint field, step controls, and text");
}

void rejectsInvalidNumericConfiguration() {
    auto engine = std::make_shared<TestTextEngine>();
    lotui::NumericInputOptions options;
    options.minimum = 10.0;
    options.maximum = 1.0;
    try {
        lotui::NumericInput input(engine, options);
    } catch (const std::invalid_argument&) {
        return;
    }
    require(false, "invalid numeric ranges must be rejected");
}

} // namespace

int main() {
    togglesCheckboxWithPointerAndKeyboard();
    editsNumericValueWithinItsRange();
    rejectsInvalidNumericConfiguration();
    std::cout << "form_widget_tests passed\n";
    return EXIT_SUCCESS;
}
