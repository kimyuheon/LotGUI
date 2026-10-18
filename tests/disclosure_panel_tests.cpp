#include "core/widget_tree.h"
#include "widgets/button.h"
#include "widgets/disclosure_panel.h"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string_view>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "disclosure_panel_tests failed: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

class TestTextLayout final : public lotui::TextLayout {
public:
    explicit TestTextLayout(lotui::Size size) : size_(size) {}

    lotui::Size size() const noexcept override { return size_; }
    float baseline() const noexcept override { return size_.height * 0.8F; }
    void appendPaintCommands(
        lotui::Point origin,
        lotui::Rect clip,
        lotui::Color color,
        std::vector<lotui::PaintCommand>& commands) const override {
        commands.push_back({
            {origin.x, origin.y, size_.width, size_.height},
            clip, color, 41, 0.0F,
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
            static_cast<float>(text.size()) * 5.0F,
            style.fontSize * 1.2F,
        });
    }
};

void togglesAndHidesContentFromPointerAndFocus() {
    auto engine = std::make_shared<TestTextEngine>();
    int clicks = 0;
    int changes = 0;
    auto button = std::make_unique<lotui::Button>(
        lotui::Size{80.0F, 24.0F}, [&clicks]() { ++clicks; });
    auto* child = button.get();
    auto panel = std::make_unique<lotui::DisclosurePanel>(
        engine, "Settings", std::move(button), true,
        [&changes](bool) { ++changes; });
    auto* observed = panel.get();
    lotui::WidgetTree tree(std::move(panel));
    const lotui::LayoutConstraints constraints =
        lotui::LayoutConstraints::loose({200.0F, 200.0F});
    const auto expandedSize = observed->measure(constraints);
    require(expandedSize.height == 55.0F,
        "expanded panel must measure header, spacing, and child");
    tree.layout({0.0F, 0.0F, 200.0F, expandedSize.height});
    const auto childPoint = lotui::Point{
        child->bounds().x + 10.0F, child->bounds().y + 10.0F};
    tree.pointerPressed(childPoint, lotui::PointerButton::Primary);
    tree.pointerReleased(childPoint, lotui::PointerButton::Primary);
    require(clicks == 1 && observed->isExpanded(),
        "content click must reach the child without toggling its panel");

    tree.pointerPressed({15.0F, 14.0F}, lotui::PointerButton::Primary);
    tree.pointerReleased({15.0F, 14.0F}, lotui::PointerButton::Primary);
    require(!observed->isExpanded() && changes == 1 &&
            observed->measure(constraints).height == 28.0F,
        "header click must collapse content and change measured height");
    tree.layout({0.0F, 0.0F, 200.0F, 28.0F});
    tree.pointerPressed(childPoint, lotui::PointerButton::Primary);
    tree.pointerReleased(childPoint, lotui::PointerButton::Primary);
    require(clicks == 1,
        "collapsed child must not receive pointer input");
    tree.keyPressed(lotui::KeyCode::Tab);
    require(observed == tree.focusedWidget(),
        "collapsed child must be removed from keyboard traversal");
    tree.keyPressed(lotui::KeyCode::Right);
    require(observed->isExpanded() && changes == 2,
        "Right must expand a focused collapsed panel");
    tree.layout({0.0F, 0.0F, 200.0F, expandedSize.height});
    tree.keyPressed(lotui::KeyCode::Tab);
    require(child == tree.focusedWidget(),
        "expanded child must rejoin keyboard traversal");
    observed->setEnabled(false);
    require(!tree.moveFocus().handled && tree.focusedWidget() == nullptr,
        "disabled group must remove its header and child from focus");
    tree.pointerPressed(childPoint, lotui::PointerButton::Primary);
    tree.pointerReleased(childPoint, lotui::PointerButton::Primary);
    require(clicks == 1,
        "disabled group must block pointer input to its child");
}

void supportsKeyboardAndProgrammaticState() {
    auto engine = std::make_shared<TestTextEngine>();
    int changes = 0;
    auto panel = std::make_unique<lotui::DisclosurePanel>(
        engine, "Nodes", nullptr, false,
        [&changes](bool) { ++changes; });
    auto* observed = panel.get();
    lotui::WidgetTree tree(std::move(panel));
    tree.layout({10.0F, 10.0F, 180.0F, 28.0F});
    tree.keyPressed(lotui::KeyCode::Tab);
    tree.keyPressed(lotui::KeyCode::Space);
    require(!observed->isExpanded(),
        "keyboard press must wait for release before activation");
    tree.keyReleased(lotui::KeyCode::Space);
    require(observed->isExpanded() && changes == 1,
        "Space release must activate the focused panel");
    tree.keyPressed(lotui::KeyCode::Left);
    require(!observed->isExpanded() && changes == 2,
        "Left must collapse the focused panel");
    observed->setExpanded(true);
    require(observed->isExpanded() && changes == 2,
        "programmatic state updates must not emit user change callbacks");
    observed->setEnabled(false);
    tree.pointerPressed({20.0F, 20.0F}, lotui::PointerButton::Primary);
    tree.pointerReleased({20.0F, 20.0F}, lotui::PointerButton::Primary);
    require(observed->isExpanded() && changes == 2,
        "disabled panel must ignore pointer activation");
}

} // namespace

int main() {
    togglesAndHidesContentFromPointerAndFocus();
    supportsKeyboardAndProgrammaticState();
    std::cout << "disclosure_panel_tests passed\n";
    return EXIT_SUCCESS;
}
