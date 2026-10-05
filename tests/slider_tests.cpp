#include "core/widget_tree.h"
#include "widgets/slider.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "slider_tests failed: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

bool near(double left, double right) {
    return std::abs(left - right) < 0.0001;
}

void dragsAndClamps() {
    lotui::SliderOptions options;
    options.minimum = 0.0;
    options.maximum = 10.0;
    options.step = 0.5;
    options.value = 2.0;
    int changes = 0;
    auto slider = std::make_unique<lotui::Slider>(options,
        [&changes](double) { ++changes; });
    auto* observed = slider.get();
    lotui::WidgetTree tree(std::move(slider));
    tree.layout({10.0F, 20.0F, 160.0F, 24.0F});

    tree.pointerPressed({90.0F, 32.0F}, lotui::PointerButton::Primary);
    require(observed->isDragging() && near(observed->value(), 5.0),
        "press must set the nearest step and begin capture");
    tree.pointerMoved({250.0F, 32.0F});
    require(near(observed->value(), 10.0),
        "dragging outside must clamp to the maximum");
    tree.pointerReleased({-20.0F, 32.0F}, lotui::PointerButton::Primary);
    require(!observed->isDragging() && near(observed->value(), 0.0),
        "release outside must clamp and end dragging");
    require(changes == 3, "each distinct user value must notify once");

    observed->setValue(7.0);
    require(near(observed->value(), 7.0) && changes == 3,
        "programmatic updates must not notify");
    observed->setRange(1.0, 4.0);
    require(near(observed->value(), 4.0),
        "range changes must clamp the current value");
}

void supportsKeyboardAndDisabledState() {
    lotui::SliderOptions options;
    options.minimum = -2.0;
    options.maximum = 8.0;
    options.step = 0.25;
    options.value = 1.0;
    int changes = 0;
    auto slider = std::make_unique<lotui::Slider>(options,
        [&changes](double) { ++changes; });
    auto* observed = slider.get();
    lotui::WidgetTree tree(std::move(slider));
    tree.layout({0.0F, 0.0F, 160.0F, 24.0F});

    tree.keyPressed(lotui::KeyCode::Tab);
    require(observed->isFocused(), "Tab must focus the slider");
    tree.keyPressed(lotui::KeyCode::Right);
    require(near(observed->value(), 1.25),
        "Right must increment by one step");
    tree.keyPressed(lotui::KeyCode::PageDown);
    require(near(observed->value(), -1.25),
        "PageDown must decrement by ten steps");
    tree.keyPressed(lotui::KeyCode::End);
    require(near(observed->value(), 8.0),
        "End must move to the maximum");
    tree.keyPressed(lotui::KeyCode::Right);
    require(changes == 3, "unchanged value must not notify");

    std::vector<lotui::PaintCommand> commands;
    tree.paint(commands);
    require(commands.size() >= 3,
        "focused slider must paint track, fill, thumb, and focus ring");
    observed->setEnabled(false);
    tree.keyPressed(lotui::KeyCode::Home);
    tree.pointerPressed({40.0F, 12.0F}, lotui::PointerButton::Primary);
    require(near(observed->value(), 8.0) && changes == 3 &&
            !observed->isDragging(),
        "disabled slider must ignore pointer and keyboard input");
}

void rejectsInvalidConfiguration() {
    try {
        lotui::SliderOptions options;
        options.minimum = 4.0;
        options.maximum = 4.0;
        lotui::Slider slider(options);
    } catch (const std::invalid_argument&) {
        return;
    }
    require(false, "empty range must be rejected");
}

} // namespace

int main() {
    dragsAndClamps();
    supportsKeyboardAndDisabledState();
    rejectsInvalidConfiguration();
    std::cout << "slider_tests passed\n";
    return EXIT_SUCCESS;
}
