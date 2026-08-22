#include "core/widget_tree.h"
#include "widgets/button.h"
#include "widgets/ribbon.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "ribbon_tests failed: " << message << '\n';
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
                clip, color, 73, 0.0F,
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
        float width = static_cast<float>(text.size()) * style.fontSize * 0.5F;
        if (std::isfinite(options.maximumWidth)) {
            width = std::min(width, options.maximumWidth);
        }
        return std::make_unique<TestTextLayout>(
            lotui::Size{width, style.fontSize * 1.2F});
    }
};

std::unique_ptr<lotui::RibbonTab> makeTab(
    const std::shared_ptr<const lotui::TextEngine>& engine,
    std::string id,
    std::string title,
    int& clicks,
    lotui::Button*& button) {
    auto command = std::make_unique<lotui::Button>(
        lotui::Size{100.0F, 48.0F},
        [&clicks]() { ++clicks; });
    button = command.get();
    auto group = std::make_unique<lotui::RibbonGroup>(
        engine, title + " 명령", std::move(command));
    return std::make_unique<lotui::RibbonTab>(
        std::move(id), std::move(title), std::move(group));
}

void switchesTabsWithPointerAndKeyboard() {
    auto engine = std::make_shared<TestTextEngine>();
    int tabChanges = 0;
    std::string selected;
    auto ribbon = std::make_unique<lotui::Ribbon>(
        engine,
        lotui::RibbonStyle{},
        lotui::TextStyle{},
        [&tabChanges, &selected](std::size_t, std::string_view id) {
            ++tabChanges;
            selected = id;
        });
    lotui::Ribbon* observed = ribbon.get();
    int homeClicks = 0;
    int viewClicks = 0;
    lotui::Button* homeButton = nullptr;
    lotui::Button* viewButton = nullptr;
    ribbon->addTab(makeTab(
        engine, "home", "홈", homeClicks, homeButton));
    ribbon->addTab(makeTab(
        engine, "view", "보기", viewClicks, viewButton));

    lotui::WidgetTree tree(std::move(ribbon));
    tree.layout({0.0F, 0.0F, 500.0F, 160.0F});
    require(observed->selectedId() == "home" &&
            homeButton->bounds().width > 0.0F &&
            viewButton->bounds().width == 0.0F,
        "the first ribbon tab must be active and arranged initially");

    const lotui::Rect viewHeader = observed->tabHeaderBounds(1);
    const lotui::Point viewHeaderCenter{
        viewHeader.x + viewHeader.width * 0.5F,
        viewHeader.y + viewHeader.height * 0.5F,
    };
    tree.pointerPressed(viewHeaderCenter, lotui::PointerButton::Primary);
    tree.pointerReleased(viewHeaderCenter, lotui::PointerButton::Primary);
    require(observed->selectedId() == "view" && tabChanges == 1 &&
            selected == "view" && observed->isFocused() &&
            viewButton->bounds().width > 0.0F,
        "a tab header click must focus the ribbon and activate its content");

    const lotui::Rect commandBounds = viewButton->bounds();
    const lotui::Point commandCenter{
        commandBounds.x + commandBounds.width * 0.5F,
        commandBounds.y + commandBounds.height * 0.5F,
    };
    tree.pointerPressed(commandCenter, lotui::PointerButton::Primary);
    tree.pointerReleased(commandCenter, lotui::PointerButton::Primary);
    require(viewClicks == 1 && homeClicks == 0,
        "only commands in the active ribbon tab may receive input");

    observed->setEnabled(false);
    tree.pointerPressed(commandCenter, lotui::PointerButton::Primary);
    tree.pointerReleased(commandCenter, lotui::PointerButton::Primary);
    require(viewClicks == 1 && !viewButton->isFocused(),
        "disabling a ribbon must suppress input and clear child focus");
    observed->setEnabled(true);

    tree.keyPressed(lotui::KeyCode::Tab);
    tree.keyPressed(lotui::KeyCode::Left);
    require(observed->selectedId() == "home" && tabChanges == 2,
        "Left and Right must switch tabs while the ribbon has focus");

    std::vector<lotui::PaintCommand> commands;
    tree.paint(commands);
    require(commands.size() >= 8,
        "a ribbon must paint its bar, tabs, group, content, and titles");
}

void rejectsDuplicateTabIds() {
    auto engine = std::make_shared<TestTextEngine>();
    lotui::Ribbon ribbon(engine);
    ribbon.addTab(std::make_unique<lotui::RibbonTab>("home", "홈"));
    try {
        ribbon.addTab(std::make_unique<lotui::RibbonTab>("home", "중복"));
    } catch (const std::invalid_argument&) {
        return;
    }
    require(false, "duplicate ribbon tab ids must be rejected");
}

} // namespace

int main() {
    switchesTabsWithPointerAndKeyboard();
    rejectsDuplicateTabIds();
    std::cout << "ribbon_tests passed\n";
    return EXIT_SUCCESS;
}
