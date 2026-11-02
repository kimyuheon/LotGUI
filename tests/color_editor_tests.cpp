#include "core/widget_tree.h"
#include "widgets/color_editor.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "color_editor_tests failed: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

bool near(float a, float b) { return std::abs(a - b) < 0.005F; }

class StubLayout final : public lotui::TextLayout {
public:
    explicit StubLayout(float width) : width_(width) {}
    lotui::Size size() const noexcept override { return {width_, 16.0F}; }
    float baseline() const noexcept override { return 12.0F; }
    void appendPaintCommands(lotui::Point origin, lotui::Rect clip,
        lotui::Color color,
        std::vector<lotui::PaintCommand>& commands) const override {
        commands.push_back({{origin.x, origin.y, width_, 16.0F}, clip,
            color, lotui::invalidTextureId, 0.0F});
    }
private:
    float width_{};
};

class StubEngine final : public lotui::TextEngine {
public:
    std::unique_ptr<lotui::TextLayout> createLayout(
        std::string_view text, const lotui::TextStyle&,
        const lotui::TextLayoutOptions&) const override {
        requested.emplace_back(text);
        return std::make_unique<StubLayout>(text.size() * 7.0F);
    }
    mutable std::vector<std::string> requested;
};

void dragAndUndoTransaction() {
    auto engine = std::make_shared<StubEngine>();
    int starts = 0;
    int finishes = 0;
    int changes = 0;
    auto editor = std::make_unique<lotui::ColorEditor>(engine,
        lotui::Color{0.2F, 0.4F, 0.6F, 0.75F},
        [&changes](lotui::Color) { ++changes; });
    auto* observed = editor.get();
    editor->setOnEditStarted([&starts]() { ++starts; });
    editor->setOnEditFinished([&finishes]() { ++finishes; });
    lotui::WidgetTree tree(std::move(editor));
    tree.layout({10.0F, 20.0F, 240.0F, 100.0F});
    const auto track = observed->channelTrackBounds(1);
    const float y = track.y + track.height * 0.5F;
    tree.pointerPressed({track.x + track.width * 0.5F, y},
        lotui::PointerButton::Primary);
    require(near(observed->color().green, 0.5F) && starts == 1 &&
            finishes == 0 && observed->activeChannel() == 1,
        "press must begin one transaction and update the chosen channel");
    tree.pointerMoved({track.x + track.width + 30.0F, y});
    require(near(observed->color().green, 1.0F),
        "drag outside must clamp the channel");
    tree.pointerReleased({track.x, y}, lotui::PointerButton::Primary);
    require(near(observed->color().green, 0.0F) &&
            near(observed->color().alpha, 0.75F) &&
            starts == 1 && finishes == 1 && changes == 3,
        "release must finish once and preserve alpha");

    observed->setColor({0.2F, 0.3F, 0.6F, 0.75F});
    require(changes == 3 &&
            std::find(engine->requested.begin(), engine->requested.end(),
                "77") != engine->requested.end(),
        "programmatic updates must refresh channel text without callbacks");
    tree.keyPressed(lotui::KeyCode::Tab);
    require(tree.focusedWidget() == observed,
        "Tab must focus the color editor");
    tree.keyPressed(lotui::KeyCode::Right);
    require(starts == 2 && finishes == 2 && changes == 4,
        "keyboard edits must form one complete transaction");
    observed->setEnabled(false);
    tree.keyPressed(lotui::KeyCode::End);
    tree.pointerPressed({track.x + track.width, y},
        lotui::PointerButton::Primary);
    require(changes == 4, "disabled editor must ignore input");
}

void rejectsInvalidColor() {
    auto engine = std::make_shared<StubEngine>();
    lotui::ColorEditor editor(engine);
    bool rejected = false;
    try {
        editor.setColor({std::numeric_limits<float>::quiet_NaN(),
            0.0F, 0.0F, 1.0F});
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    require(rejected, "non-finite color values must be rejected");
}

} // namespace

int main() {
    dragAndUndoTransaction();
    rejectsInvalidColor();
    std::cout << "color_editor_tests passed\n";
    return EXIT_SUCCESS;
}
