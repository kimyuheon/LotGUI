#include "core/widget_tree.h"
#include "widgets/graph_canvas.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string_view>

namespace {
void check(bool okay, const char* message) {
    if (!okay) { std::cerr << message << '\n'; std::exit(1); }
}
class StubLayout final : public lotui::TextLayout {
public:
    lotui::Size size() const noexcept override { return {20.0F, 12.0F}; }
    float baseline() const noexcept override { return 9.0F; }
    void appendPaintCommands(lotui::Point, lotui::Rect,
        lotui::Color, std::vector<lotui::PaintCommand>&) const override {}
};
class StubText final : public lotui::TextEngine {
public:
    std::unique_ptr<lotui::TextLayout> createLayout(std::string_view,
        const lotui::TextStyle&, const lotui::TextLayoutOptions&) const override {
        return std::make_unique<StubLayout>();
    }
};
}

int main() {
    auto canvas = std::make_unique<lotui::GraphCanvas>(
        std::make_shared<StubText>(), lotui::Size{600.0F, 360.0F});
    auto* graph = canvas.get();
    graph->setGraph({
        {"a", "Input", {0.0F, 0.0F}, {120.0F, 90.0F}, {}, {"out"}},
        {"b", "Output", {240.0F, 40.0F}, {120.0F, 90.0F}, {"in"}, {}}
    }, {{"link", "a", 0, "b", 0}});
    lotui::WidgetTree tree(std::move(canvas));
    tree.layout({0.0F, 0.0F, 600.0F, 360.0F});
    graph->fitToContent();
    std::vector<lotui::PaintCommand> commands;
    tree.paint(commands);
    check(commands.size() > 30, "graph must draw cards and links");
    bool line = false;
    for (const auto& command : commands)
        line |= command.primitive == lotui::PaintPrimitive::Line;
    check(line, "graph must emit line paint commands");

    int selections = 0, moves = 0;
    graph->setOnSelectionChanged([&](auto) { ++selections; });
    graph->setOnNodeMoved([&](auto, auto) { ++moves; });
    const auto node = graph->nodeBounds("a");
    const lotui::Point point{node.x + 20.0F, node.y + 15.0F};
    tree.pointerPressed(point, lotui::PointerButton::Primary);
    tree.pointerMoved({point.x + 25.0F, point.y + 10.0F});
    tree.pointerReleased({point.x + 25.0F, point.y + 10.0F},
        lotui::PointerButton::Primary);
    check(graph->selectedNode() == "a" && selections == 1 && moves == 1 &&
        graph->nodes()[0].position.x > 0.0F,
        "pointer drag must select and move the node");
    const float beforeZoom = graph->zoomLevel();
    check(tree.scroll({300.0F, 180.0F}, {0.0F, 48.0F}).handled &&
        graph->zoomLevel() > beforeZoom,
        "wheel must zoom graph inside canvas");
    const float beforeKey = graph->nodes()[0].position.x;
    tree.keyPressed(lotui::KeyCode::Right);
    check(graph->nodes()[0].position.x > beforeKey && moves == 2,
        "arrow key must move selected node");
    tree.keyPressed(lotui::KeyCode::Escape);
    check(!graph->selectedNode() && selections == 2,
        "Escape must clear selection");

    bool rejected = false;
    try {
        graph->setGraph({{"bad", "Bad", {}, {100.0F, 80.0F},
            {}, {"out"}}}, {{"broken", "bad", 0, "missing", 0}});
    } catch (const std::invalid_argument&) { rejected = true; }
    check(rejected && graph->nodes().size() == 2,
        "invalid links must be rejected without changing the graph");
    std::cout << "graph_canvas_tests passed\n";
}
