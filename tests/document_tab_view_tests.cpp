#include "core/widget_tree.h"
#include "widgets/document_tab_view.h"

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
        std::cerr << "document_tab_view_tests failed: " << message << '\n';
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
            clip, color, 17, 0.0F});
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

lotui::Point center(lotui::Rect rect) {
    return {rect.x + rect.width * 0.5F,
        rect.y + rect.height * 0.5F};
}

void click(lotui::WidgetTree& tree, lotui::Rect rect) {
    require(lotui::hasArea(rect), "click target must be visible");
    tree.pointerPressed(center(rect), lotui::PointerButton::Primary);
    tree.pointerReleased(center(rect), lotui::PointerButton::Primary);
}

void controlledSelectionAndActions() {
    auto view = std::make_unique<lotui::DocumentTabView>(
        std::make_shared<Engine>());
    auto* observed = view.get();
    int activated = 0;
    int closed = 0;
    int added = 0;
    std::string lastId;
    view->setOnActivate([&](std::string_view id) {
        ++activated; lastId = id;
    });
    view->setOnClose([&](std::string_view id) {
        ++closed; lastId = id;
    });
    view->setOnAdd([&] { ++added; });
    view->setTabs({{"a", "Drawing A", false, true},
        {"b", "Drawing B", true, true},
        {"c", "Reference", false, false}});
    lotui::WidgetTree tree(std::move(view));
    tree.layout({0.0F, 0.0F, 480.0F, 36.0F});
    require(observed->selectedId() == "a", "first tab must be selected");
    require(observed->setSelectedId("b") && activated == 0,
        "engine-driven selection must not call activation callback");
    click(tree, observed->tabBounds(0));
    require(observed->selectedId() == "a" && activated == 1 &&
        lastId == "a", "click must request activation by stable id");
    click(tree, observed->closeBounds(1));
    require(closed == 1 && lastId == "b" && observed->tabs().size() == 3,
        "close must request without mutating the owner model");
    click(tree, observed->addBounds());
    require(added == 1, "add control must request a new document");
    require(!lotui::hasArea(observed->closeBounds(2)),
        "non-closable tab must not expose a close target");

    observed->setTabs({{"b", "Drawing B", false, true},
        {"c", "Reference", false, false}});
    require(observed->selectedId() == "b" && activated == 1,
        "removing selected tab must choose a fallback without callback");
    std::vector<lotui::PaintCommand> commands;
    tree.paint(commands);
    require(commands.size() > 5, "tab strip must paint its controls");
}

void overflowAndKeyboard() {
    auto view = std::make_unique<lotui::DocumentTabView>(
        std::make_shared<Engine>());
    auto* observed = view.get();
    int activated = 0;
    view->setOnActivate([&](std::string_view) { ++activated; });
    view->setTabs({{"a", "Alpha"}, {"b", "Bravo"},
        {"c", "Charlie"}, {"d", "Delta"}});
    lotui::WidgetTree tree(std::move(view));
    tree.layout({0.0F, 0.0F, 230.0F, 36.0F});
    require(lotui::hasArea(observed->nextBounds()) &&
        !lotui::hasArea(observed->tabBounds(3)),
        "overflow must reserve navigation controls");
    click(tree, observed->nextBounds());
    require(lotui::hasArea(observed->tabBounds(1)),
        "next control must reveal a later tab");
    require(observed->setSelectedId("d") && activated == 0 &&
        lotui::hasArea(observed->tabBounds(3)),
        "external selection must scroll selected tab into view silently");
    click(tree, observed->tabBounds(3));
    tree.keyPressed(lotui::KeyCode::Left);
    require(observed->selectedId() == "c" && activated == 1,
        "keyboard navigation must request activation");
    tree.layout({0.0F, 0.0F, 700.0F, 36.0F});
    require(!lotui::hasArea(observed->nextBounds()) &&
        lotui::hasArea(observed->tabBounds(0)),
        "expanding the view must restore all tabs");
}

void rejectsDuplicateIds() {
    lotui::DocumentTabView view(std::make_shared<Engine>());
    try {
        view.setTabs({{"same", "A"}, {"same", "B"}});
    } catch (const std::invalid_argument&) {
        return;
    }
    require(false, "duplicate document ids must be rejected");
}

} // namespace

int main() {
    controlledSelectionAndActions();
    overflowAndKeyboard();
    rejectsDuplicateIds();
    std::cout << "document_tab_view_tests passed\n";
    return EXIT_SUCCESS;
}
