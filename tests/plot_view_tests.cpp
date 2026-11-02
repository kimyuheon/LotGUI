#include "core/widget_tree.h"
#include "widgets/plot_view.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <vector>

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
    auto plot = std::make_unique<lotui::PlotView>(
        std::make_shared<StubText>(), lotui::Size{320.0F, 180.0F});
    auto* observed = plot.get();
    std::vector<lotui::PlotPoint> points;
    for (int i = 0; i < 10000; ++i)
        points.push_back({static_cast<double>(i), i == 5000 ? 20.0 : 0.0});
    observed->setSeries({{"spike", "Spike", std::move(points),
        {0.2F, 0.8F, 0.9F, 1.0F}, true}});
    check(observed->autoRange() && observed->viewRange().yMax > 20.0,
        "auto range must retain a one-sample spike");
    lotui::WidgetTree tree(std::move(plot));
    tree.layout({0.0F, 0.0F, 320.0F, 180.0F});
    std::vector<lotui::PaintCommand> commands;
    tree.paint(commands);
    check(commands.size() < 1500,
        "dense series must be bounded by visible screen columns");
    bool line = false;
    for (const auto& command : commands)
        line |= command.primitive == lotui::PaintPrimitive::Line;
    check(line, "plot must emit line paint commands");
    const auto area = observed->plotBounds();
    const lotui::Point center{area.x + area.width * 0.5F,
        area.y + area.height * 0.5F};
    check(tree.scroll(center, {0.0F, 48.0F}).handled &&
        !observed->autoRange() &&
        observed->viewRange().xMax - observed->viewRange().xMin < 10000.0,
        "wheel must zoom only within plot bounds");
    const auto beforePan = observed->viewRange();
    tree.pointerPressed(center, lotui::PointerButton::Primary);
    tree.pointerMoved({center.x + 20.0F, center.y});
    tree.pointerReleased({center.x + 20.0F, center.y},
        lotui::PointerButton::Primary);
    check(observed->viewRange().xMin < beforePan.xMin,
        "drag must pan the viewport");
    tree.keyPressed(lotui::KeyCode::Home);
    check(observed->autoRange(), "Home must restore auto range");
    bool rejected = false;
    try { observed->setRange({1.0, 1.0, 0.0, 1.0}); }
    catch (const std::invalid_argument&) { rejected = true; }
    check(rejected, "invalid range must be rejected");
    observed->setSeries({{"gap", "Gap",
        {{0.0, 1.0}, {1.0, std::numeric_limits<double>::quiet_NaN()},
         {2.0, 2.0}}, {}, true}});
    commands.clear();
    tree.paint(commands);
    check(commands.size() < 40, "invalid Y must break the series");
    std::cout << "plot_view_tests passed\n";
}
