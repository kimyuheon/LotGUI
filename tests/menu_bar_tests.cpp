#include "core/widget_tree.h"
#include "widgets/button.h"
#include "widgets/linear_layout.h"
#include "widgets/menu_bar.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "menu_bar_tests failed: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

class Layout final : public lotui::TextLayout {
public:
    explicit Layout(lotui::Size size) : size_(size) {}
    lotui::Size size() const noexcept override { return size_; }
    float baseline() const noexcept override { return size_.height * 0.8F; }
    void appendPaintCommands(lotui::Point origin, lotui::Rect clip,
        lotui::Color color, std::vector<lotui::PaintCommand>& commands)
        const override {
        commands.push_back({{origin.x, origin.y, size_.width, size_.height},
            clip, color, 67, 0.0F});
    }
private:
    lotui::Size size_{};
};

class Engine final : public lotui::TextEngine {
public:
    std::unique_ptr<lotui::TextLayout> createLayout(std::string_view text,
        const lotui::TextStyle& style,
        const lotui::TextLayoutOptions&) const override {
        return std::make_unique<Layout>(lotui::Size{
            static_cast<float>(text.size()) * style.fontSize * 0.5F,
            style.fontSize * 1.2F});
    }
};

lotui::MenuEntry command(std::string id, std::string label,
    bool enabled = true) {
    return {lotui::MenuEntryKind::Command,
        std::move(id), std::move(label), {}, enabled, false, {}};
}

std::vector<lotui::Menu> menus() {
    return {
        {"file", "File", {
            command("new", "New"),
            {lotui::MenuEntryKind::Separator},
            command("disabled", "Disabled", false),
            {lotui::MenuEntryKind::Submenu, {}, "Recent", {}, true,
                false, {command("recent-a", "A"),
                    command("recent-b", "B")}},
            {lotui::MenuEntryKind::Toggle, "autosave", "Autosave",
                {}, true, true, {}},
        }},
        {"edit", "Edit", {command("undo", "Undo")}},
    };
}

struct Fixture {
    std::unique_ptr<lotui::WidgetTree> tree;
    lotui::MenuBar* bar{nullptr};
    lotui::PopupHost* host{nullptr};
    std::vector<std::string> actions;
    int backgroundClicks{0};

    explicit Fixture(lotui::Rect viewport = {0, 0, 500, 280}) {
        auto menuBar = std::make_unique<lotui::MenuBar>(
            std::make_shared<Engine>(), menus(),
            [this](std::string_view id) { actions.emplace_back(id); });
        bar = menuBar.get();
        auto content = std::make_unique<lotui::Column>();
        content->addChild(std::move(menuBar),
            {0.0F, {0.0F, 28.0F},
                {lotui::unboundedLayoutSize, 28.0F}});
        content->addChild(std::make_unique<lotui::Button>(
            lotui::Size{100.0F, 40.0F},
            [this] { ++backgroundClicks; }),
            {1.0F, {}, {lotui::unboundedLayoutSize,
                lotui::unboundedLayoutSize}});
        auto popupHost = std::make_unique<lotui::PopupHost>(
            std::move(content));
        host = popupHost.get();
        bar->setPopupHost(host);
        tree = std::make_unique<lotui::WidgetTree>(std::move(popupHost));
        tree->layout(viewport);
    }

    void click(lotui::Point point) {
        tree->pointerPressed(point, lotui::PointerButton::Primary);
        tree->pointerReleased(point, lotui::PointerButton::Primary);
    }

    void openFile() {
        const auto rect = bar->menuBounds(0);
        click({rect.x + rect.width * 0.5F,
            rect.y + rect.height * 0.5F});
        require(bar->isOpen() && host->hasPopup(),
            "File menu must open on click");
    }
};

void commandAndDismissal() {
    Fixture fixture;
    fixture.openFile();
    const lotui::Rect popup = fixture.host->popupBounds();
    require(popup.y >= 28.0F && popup.width > 400.0F,
        "menu popup must be anchored below the bar and fit submenus");
    fixture.click({popup.x + 30.0F, popup.y + 16.0F});
    require(fixture.actions == std::vector<std::string>{"new"} &&
        !fixture.host->hasPopup(),
        "command must dispatch once and close the menu");

    fixture.openFile();
    fixture.click({popup.x + 30.0F, popup.y + 4.0F + 27.0F +
        9.0F + 12.0F});
    require(fixture.actions.size() == 1 && fixture.host->hasPopup(),
        "disabled item must not dispatch or close");
    fixture.tree->keyPressed(lotui::KeyCode::Escape);
    require(!fixture.host->hasPopup() && !fixture.bar->isOpen(),
        "Escape must close the menu");
    fixture.openFile();
    fixture.click({480.0F, 220.0F});
    require(!fixture.host->hasPopup() && fixture.backgroundClicks == 0,
        "outside click must dismiss without activating content");
}

void scrollsClippedMenu() {
    Fixture fixture({0.0F, 0.0F, 500.0F, 116.0F});
    fixture.openFile();
    const lotui::Rect popup = fixture.host->popupBounds();
    require(popup.height < 120.0F,
        "popup must fit the available viewport height");
    const auto scrolled = fixture.tree->scroll(
        {popup.x + 30.0F, popup.y + 18.0F},
        {0.0F, 4.0F}, lotui::ScrollDeltaMode::Line);
    require(scrolled.handled, "clipped menu must consume scrolling");
    fixture.click({popup.x + 30.0F, popup.y + 18.0F});
    require(fixture.actions == std::vector<std::string>{"autosave"},
        "scrolling must reveal and activate later items");
}

void nestedToggleAndTopSwitch() {
    Fixture fixture;
    fixture.openFile();
    const lotui::Rect popup = fixture.host->popupBounds();
    fixture.click({popup.x + 30.0F,
        popup.y + 4.0F + 27.0F + 9.0F + 27.0F + 13.0F});
    require(fixture.host->hasPopup(),
        "submenu row must expand without accepting");
    fixture.click({popup.x + popup.width * 0.5F + 30.0F,
        popup.y + 16.0F});
    require(fixture.actions == std::vector<std::string>{"recent-a"},
        "nested command must return its stable id");

    fixture.openFile();
    fixture.click({popup.x + 30.0F,
        popup.y + 4.0F + 27.0F + 9.0F + 27.0F * 2.0F + 13.0F});
    require(fixture.actions.back() == "autosave" &&
        !fixture.host->hasPopup(),
        "checked toggle must emit action without mutating owner state");

    fixture.openFile();
    const lotui::Rect edit = fixture.bar->menuBounds(1);
    fixture.tree->pointerMoved({edit.x + edit.width * 0.5F,
        edit.y + edit.height * 0.5F});
    require(fixture.bar->isOpen() &&
        fixture.host->popupBounds().x == edit.x,
        "hovering another top menu must switch the open popup");
    const lotui::Rect editPopup = fixture.host->popupBounds();
    fixture.click({editPopup.x + 30.0F, editPopup.y + 16.0F});
    require(fixture.actions.back() == "undo",
        "switched menu command must dispatch");
}

void keyboardAndValidation() {
    Fixture fixture;
    fixture.openFile();
    fixture.tree->keyPressed(lotui::KeyCode::Down);
    fixture.tree->keyPressed(lotui::KeyCode::Right);
    fixture.tree->keyPressed(lotui::KeyCode::Down);
    fixture.tree->keyPressed(lotui::KeyCode::Enter);
    require(fixture.actions == std::vector<std::string>{"recent-b"},
        "keyboard must skip disabled rows and enter a submenu");

    auto engine = std::make_shared<Engine>();
    try {
        lotui::MenuBar invalid(engine,
            {{"file", "File", {command("same", "A"),
                command("same", "B")}}});
    } catch (const std::invalid_argument&) {
        return;
    }
    require(false, "duplicate command ids must be rejected");
}

} // namespace

int main() {
    commandAndDismissal();
    nestedToggleAndTopSwitch();
    scrollsClippedMenu();
    keyboardAndValidation();
    std::cout << "menu_bar_tests passed\n";
    return EXIT_SUCCESS;
}
