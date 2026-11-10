#include "core/widget_tree.h"
#include "widgets/timeline_view.h"

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
    lotui::Size size() const noexcept override { return {30.0F, 12.0F}; }
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
    auto timeline = std::make_unique<lotui::TimelineView>(
        std::make_shared<StubText>(), lotui::Size{800.0F, 280.0F});
    auto* view = timeline.get();
    view->setDuration(12.0);
    view->setTracks({
        {"position", "Position", {{"start", 1.0}, {"end", 8.0}}},
        {"rotation", "Rotation", {{"turn", 4.0}}}
    });
    lotui::WidgetTree tree(std::move(timeline));
    tree.layout({0.0F, 0.0F, 800.0F, 280.0F});
    std::vector<lotui::PaintCommand> commands;
    tree.paint(commands);
    check(commands.size() > 15, "timeline must draw tracks, ruler and keys");

    int selects = 0, moves = 0, seeks = 0, plays = 0;
    double committed = -1.0;
    view->setOnKeySelected([&](auto) { ++selects; });
    view->setOnKeyMoved([&](const auto& key, double time) {
        check(key.trackId == "position" && key.keyId == "start",
            "key move must preserve host IDs");
        ++moves;
        committed = time;
    });
    view->setOnTimeChanged([&](double) { ++seeks; });
    view->setOnPlayChanged([&](bool) { ++plays; });

    const auto marker = view->keyBounds("position", "start");
    const lotui::Point key{marker.x + 6.0F, marker.y + 6.0F};
    tree.pointerPressed(key, lotui::PointerButton::Primary);
    tree.pointerMoved({key.x + 60.0F, key.y});
    check(moves == 0 && view->tracks()[0].keys[0].time > 1.0,
        "key drag must preview without committing");
    tree.pointerReleased({key.x + 60.0F, key.y},
        lotui::PointerButton::Primary);
    check(selects == 1 && moves == 1 && committed > 1.0 &&
        view->selectedKey() == lotui::TimelineKeyRef{"position", "start"},
        "release must commit one key move");

    const auto ruler = view->rulerBounds();
    const lotui::Point scrub{ruler.x + ruler.width * 0.5F, ruler.y + 12.0F};
    tree.pointerPressed(scrub, lotui::PointerButton::Primary);
    tree.pointerMoved({scrub.x + 20.0F, scrub.y});
    tree.pointerReleased({scrub.x + 20.0F, scrub.y},
        lotui::PointerButton::Primary);
    check(seeks > 0 && view->time() > 5.0 && !view->selectedKey(),
        "ruler scrub must seek and clear key selection");

    tree.pointerPressed({20.0F, 15.0F}, lotui::PointerButton::Primary);
    tree.pointerReleased({20.0F, 15.0F}, lotui::PointerButton::Primary);
    check(view->playing() && plays == 1, "transport must toggle playback");
    const double before = view->viewDuration();
    check(tree.scroll({400.0F, 90.0F}, {0.0F, 48.0F}).handled &&
        view->viewDuration() < before, "wheel must zoom around pointer");
    check(!tree.scroll({900.0F, 90.0F}, {0.0F, 48.0F}).handled,
        "outside wheel must pass through");

    bool rejected = false;
    try { view->setTracks({{"bad", "Bad", {{"same", 1.0}, {"same", 2.0}}}}); }
    catch (const std::invalid_argument&) { rejected = true; }
    check(rejected && view->tracks().size() == 2,
        "invalid data must leave the previous model intact");
    std::cout << "timeline_view_tests passed\n";
}
