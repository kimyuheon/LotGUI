#include "core/widget_tree.h"
#include "widgets/tree_view.h"

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
        std::cerr << "tree_view_tests failed: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

class TestTextLayout final : public lotui::TextLayout {
public:
    lotui::Size size() const noexcept override { return {40.0F, 12.0F}; }
    float baseline() const noexcept override { return 10.0F; }
    void appendPaintCommands(lotui::Point origin, lotui::Rect clip,
        lotui::Color color,
        std::vector<lotui::PaintCommand>& commands) const override {
        commands.push_back({{origin.x, origin.y, 40.0F, 12.0F},
            clip, color, 17, 0.0F});
    }
};

class TestTextEngine final : public lotui::TextEngine {
public:
    std::unique_ptr<lotui::TextLayout> createLayout(
        std::string_view, const lotui::TextStyle&,
        const lotui::TextLayoutOptions&) const override {
        ++layouts;
        return std::make_unique<TestTextLayout>();
    }
    mutable int layouts{0};
};

void expansionSelectionAndKeyboard() {
    auto engine = std::make_shared<TestTextEngine>();
    int selections = 0;
    int expansions = 0;
    int activations = 0;
    auto view = std::make_unique<lotui::TreeView>(engine,
        std::vector<lotui::TreeNode>{
            {"root", "Root", 0, true},
            {"child", "Child", 1},
            {"branch", "Branch", 1, false},
            {"grandchild", "Grandchild", 2},
            {"peer", "Peer", 0},
        });
    auto* observed = view.get();
    view->setOnSelectionChanged([&selections](auto) { ++selections; });
    view->setOnExpansionChanged([&expansions](auto, bool) { ++expansions; });
    view->setOnActivated([&activations](auto) { ++activations; });
    lotui::WidgetTree tree(std::move(view));
    tree.layout({0.0F, 0.0F, 200.0F, 104.0F});
    require(observed->visibleRowCount() == 4 &&
            !lotui::hasArea(observed->rowBounds("grandchild")),
        "collapsed descendants must not have visible rows");
    require(observed->setSelectedId("child") && selections == 0,
        "programmatic selection must not notify");
    observed->setExpanded("root", false);
    require(observed->visibleRowCount() == 2 &&
            observed->selectedId() == "root",
        "collapse must move hidden selection to parent");

    tree.pointerPressed({12.0F, 13.0F}, lotui::PointerButton::Primary);
    tree.pointerReleased({12.0F, 13.0F}, lotui::PointerButton::Primary);
    require(observed->isExpanded("root") && expansions == 1,
        "disclosure click must expand root");
    tree.keyPressed(lotui::KeyCode::Tab);
    tree.keyPressed(lotui::KeyCode::Down);
    require(observed->selectedId() == "child" && selections == 1,
        "Down must select next visible row");
    tree.keyPressed(lotui::KeyCode::Left);
    require(observed->selectedId() == "root" && selections == 2,
        "Left must select parent of leaf");
    tree.keyPressed(lotui::KeyCode::Enter);
    require(activations == 1, "Enter must activate selected node");
    tree.keyPressed(lotui::KeyCode::End);
    require(observed->selectedId() == "peer",
        "End must choose final visible row");
    observed->setNodes({
        {"root", "Renamed root", 0, false},
        {"child", "Child", 1},
        {"branch", "Branch", 1, false},
        {"grandchild", "Grandchild", 2},
        {"peer", "Peer", 0},
    });
    require(observed->isExpanded("root") &&
            observed->selectedId() == "peer" && selections == 3,
        "model refresh must retain expansion and selection by stable ID");
}

void virtualizesAndScrolls() {
    auto engine = std::make_shared<TestTextEngine>();
    std::vector<lotui::TreeNode> nodes;
    for (int index = 0; index < 1000; ++index) {
        nodes.push_back({"node-" + std::to_string(index),
            "Node " + std::to_string(index)});
    }
    auto view = std::make_unique<lotui::TreeView>(engine, std::move(nodes));
    auto* observed = view.get();
    lotui::WidgetTree tree(std::move(view));
    tree.layout({0.0F, 0.0F, 200.0F, 100.0F});
    observed->setScrollOffset(2600.0F);
    std::vector<lotui::PaintCommand> commands;
    tree.paint(commands);
    require(observed->visibleRowCount() == 1000 && engine->layouts <= 5,
        "painting must shape only viewport rows");
    const int previousLayouts = engine->layouts;
    commands.clear();
    tree.paint(commands);
    require(engine->layouts == previousLayouts,
        "visible text layouts must be cached between paints");
    observed->setSelectedId("node-999");
    observed->ensureVisible("node-999");
    require(observed->scrollOffset() > 2600.0F,
        "ensureVisible must scroll distant selection into view");
    const float previousOffset = observed->scrollOffset();
    tree.scroll({10.0F, 10.0F}, {0.0F, -26.0F},
        lotui::ScrollDeltaMode::Pixel);
    require(observed->scrollOffset() < previousOffset,
        "wheel input must change tree scroll offset");
}

void connectorsAndCheckboxes() {
    lotui::TreeViewStyle style;
    style.rowHeight = 24.0F;
    style.connector = {0.83F, 0.17F, 0.11F, 1.0F};
    int checks = 0;
    bool lastChecked = false;
    auto view = std::make_unique<lotui::TreeView>(nullptr,
        std::vector<lotui::TreeNode>{
            {"root", "Root", 0, true},
            {"first", "First", 1, true, true, true},
            {"grandchild", "Grandchild", 2, false, true, true},
            {"last", "Last", 1, false, true, true},
            {"peer", "Peer", 0},
        }, lotui::Size{200.0F, 120.0F}, style);
    auto* observed = view.get();
    view->setOnCheckedChanged([&checks, &lastChecked](
            std::string_view id, bool checked) {
        require(id == "first", "check callback must report node ID");
        lastChecked = checked;
        ++checks;
    });
    lotui::WidgetTree tree(std::move(view));
    tree.layout({0.0F, 0.0F, 200.0F, 120.0F});
    require(observed->setSelectedId("first"), "first node must be selectable");
    tree.pointerPressed({45.0F, 36.0F}, lotui::PointerButton::Primary);
    tree.pointerReleased({45.0F, 36.0F}, lotui::PointerButton::Primary);
    require(observed->isChecked("first") && checks == 1 && lastChecked &&
            observed->selectedId() == "first" && observed->isExpanded("first"),
        "checkbox click must not change selection or expansion");
    tree.keyPressed(lotui::KeyCode::Tab);
    tree.keyPressed(lotui::KeyCode::Space);
    require(!observed->isChecked("first") && checks == 2 &&
            !lastChecked && observed->isExpanded("first"),
        "Space must toggle a selected checkbox without collapsing its node");
    require(observed->setChecked("first", true) && checks == 2,
        "programmatic check must not emit user callback");

    std::vector<lotui::PaintCommand> commands;
    tree.paint(commands);
    const auto hasConnector = [&](float x, float y, float height) {
        for (const auto& command : commands) {
            if (command.color.red == style.connector.red &&
                command.bounds.x == x && command.bounds.y == y &&
                command.bounds.width == style.connectorWidth &&
                command.bounds.height == height) return true;
        }
        return false;
    };
    require(hasConnector(13.5F, 12.0F, 12.0F),
        "parent trunk must start below disclosure");
    require(hasConnector(13.5F, 24.0F, 24.0F) &&
            hasConnector(13.5F, 48.0F, 24.0F),
        "ancestor trunk must continue through nested branch");
    require(hasConnector(13.5F, 72.0F, 12.0F),
        "last sibling trunk must stop at its midpoint");

    observed->setNodes({
        {"root", "Root", 0, false},
        {"first", "Renamed", 1, false, true, true},
        {"grandchild", "Grandchild", 2},
        {"last", "Last", 1},
    });
    require(observed->isChecked("first") && observed->isExpanded("first"),
        "model refresh must keep check and expansion state by ID");
    require(!observed->setChecked("root", true) &&
            !observed->setChecked("missing", true),
        "only existing checkable nodes accept check updates");
    observed->setExpanded("root", false);
    commands.clear();
    tree.paint(commands);
    for (const auto& command : commands) {
        require(command.color.red != style.connector.red,
            "collapsed descendants must not leave connector lines");
    }
}

void rejectsInvalidPreorder() {
    lotui::TreeView view(nullptr);
    try {
        view.setNodes({{"child", "Child", 1}});
        require(false, "first node must be a root");
    } catch (const std::invalid_argument&) {}
    try {
        view.setNodes({{"root", "Root"}, {"deep", "Deep", 2}});
        require(false, "depth jumps must be rejected");
    } catch (const std::invalid_argument&) {}
    try {
        view.setNodes({{"root", "Root"}, {"root", "Duplicate"}});
        require(false, "duplicate IDs must be rejected");
    } catch (const std::invalid_argument&) {}
}

} // namespace

int main() {
    expansionSelectionAndKeyboard();
    virtualizesAndScrolls();
    connectorsAndCheckboxes();
    rejectsInvalidPreorder();
    std::cout << "tree_view_tests passed\n";
    return EXIT_SUCCESS;
}
