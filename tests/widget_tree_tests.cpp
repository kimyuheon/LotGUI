#include "core/widget_tree.h"
#include "widgets/box.h"
#include "widgets/button.h"
#include "widgets/linear_layout.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "widget_tree_tests failed: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

bool near(float left, float right) {
    return std::abs(left - right) < 0.01F;
}

void laysOutAndPaintsOwnedWidgets() {
    auto row = std::make_unique<lotui::Row>();
    lotui::LinearLayoutOptions options;
    options.spacing = 10.0F;
    options.padding = lotui::EdgeInsets::all(10.0F);
    options.crossAxisAlignment = lotui::CrossAxisAlignment::Stretch;
    row->setOptions(options);
    row->setDecoration(lotui::BoxDecoration{
        {0.1F, 0.2F, 0.3F, 1.0F}, 6.0F});

    auto& first = static_cast<lotui::Box&>(row->addChild(
        std::make_unique<lotui::Box>(
            lotui::Size{20.0F, 20.0F},
            lotui::Color{1.0F, 0.0F, 0.0F, 1.0F}),
        {1.0F, {20.0F, 10.0F}, {200.0F, 80.0F}}));
    auto& second = static_cast<lotui::Box&>(row->addChild(
        std::make_unique<lotui::Box>(
            lotui::Size{20.0F, 20.0F},
            lotui::Color{0.0F, 1.0F, 0.0F, 1.0F}),
        {2.0F, {20.0F, 10.0F}, {200.0F, 80.0F}}));

    lotui::WidgetTree tree(std::move(row));
    tree.layout({10.0F, 20.0F, 300.0F, 100.0F});

    require(near(first.bounds().x, 20.0F) &&
                near(first.bounds().width, 96.667F),
            "first flex child has the wrong bounds");
    require(near(second.bounds().x, 126.667F) &&
                near(second.bounds().width, 173.333F),
            "second flex child has the wrong bounds");

    std::vector<lotui::PaintCommand> commands;
    tree.paint(commands);
    require(commands.size() == 3,
            "container and both children must produce paint commands");
    require(near(commands[0].cornerRadius, 6.0F),
            "container must paint before its children");
}

void routesCapturedButtonClicks() {
    int clickCount = 0;
    auto button = std::make_unique<lotui::Button>(
        lotui::Size{100.0F, 40.0F},
        [&clickCount]() { ++clickCount; });
    lotui::Button* observedButton = button.get();
    lotui::WidgetTree tree(std::move(button));
    tree.layout({10.0F, 10.0F, 100.0F, 40.0F});

    const auto press = tree.pointerPressed(
        {20.0F, 20.0F}, lotui::PointerButton::Primary);
    require(press.captureStarted && observedButton->isPressed(),
            "primary press must request pointer capture");

    tree.pointerMoved({200.0F, 200.0F});
    require(!observedButton->isHovered(),
            "captured button must stop hovering outside its clip");
    const auto outsideRelease = tree.pointerReleased(
        {200.0F, 200.0F}, lotui::PointerButton::Primary);
    require(outsideRelease.captureEnded && clickCount == 0,
            "release outside must end capture without a click");

    tree.pointerPressed(
        {20.0F, 20.0F}, lotui::PointerButton::Primary);
    tree.pointerReleased(
        {20.0F, 20.0F}, lotui::PointerButton::Primary);
    require(clickCount == 1 && !observedButton->isPressed(),
            "release inside must invoke the click handler once");
}

void propagatesAncestorClipping() {
    auto button = std::make_unique<lotui::Button>();
    lotui::Button* observedButton = button.get();
    lotui::WidgetTree tree(std::move(button));
    tree.layout(
        {20.0F, 20.0F, 100.0F, 40.0F},
        {0.0F, 0.0F, 80.0F, 50.0F});

    require(near(observedButton->clip().x, 20.0F) &&
                near(observedButton->clip().width, 60.0F) &&
                near(observedButton->clip().height, 30.0F),
            "widget clip must be intersected with its ancestor clip");
    require(!tree.pointerPressed(
                {90.0F, 30.0F}, lotui::PointerButton::Primary)
                .captureStarted,
            "hit testing must reject the clipped portion");
}

} // namespace

int main() {
    laysOutAndPaintsOwnedWidgets();
    routesCapturedButtonClicks();
    propagatesAncestorClipping();
    std::cout << "widget_tree_tests passed\n";
    return EXIT_SUCCESS;
}
