#include "core/widget_tree.h"
#include "widgets/button.h"
#include "widgets/dock_workspace.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace {

class Content final : public lotui::Widget {
public:
    lotui::Size measure(const lotui::LayoutConstraints& constraints) const override {
        return constraints.constrain({300.0F, 200.0F});
    }
};

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void checkGeometry(const lotui::DockWorkspace& dock) {
    const auto outer = dock.bounds();
    const auto bar = dock.toolbar().bounds();
    const auto center = dock.content().bounds();
    const bool horizontal = dock.side() == lotui::DockSide::Top ||
        dock.side() == lotui::DockSide::Bottom;
    require(dock.toolbar().vertical() != horizontal,
        "toolbar orientation does not match dock side");
    require(center.width >= 120.0F && center.height >= 100.0F,
        "dock squeezed content below its minimum");
    require(std::abs(bar.width + center.width - outer.width) < 0.1F ||
            std::abs(bar.height + center.height - outer.height) < 0.1F,
        "toolbar and content do not fill workspace");
    require(bar.x >= outer.x && bar.y >= outer.y &&
            bar.x + bar.width <= outer.x + outer.width + 0.1F &&
            bar.y + bar.height <= outer.y + outer.height + 0.1F,
        "toolbar escaped workspace");
}

} // namespace

int main() {
    try {
        int commandClicks = 0;
        auto bar = std::make_unique<lotui::DockToolbar>();
        auto* button = &bar->addCommand(std::make_unique<lotui::Button>(
            lotui::Size{40.0F, 32.0F}, [&commandClicks] { ++commandClicks; }),
            42.0F);
        auto dock = std::make_unique<lotui::DockWorkspace>(
            std::make_unique<Content>(), std::move(bar));
        auto* workspace = dock.get();
        lotui::WidgetTree tree(std::move(dock));
        tree.layout({0.0F, 0.0F, 500.0F, 360.0F});

        const auto clickCommand = [&] {
            const auto rect = button->bounds();
            const lotui::Point point{rect.x + rect.width * 0.5F,
                rect.y + rect.height * 0.5F};
            tree.pointerPressed(point, lotui::PointerButton::Primary);
            tree.pointerReleased(point, lotui::PointerButton::Primary);
        };
        const auto drag = [&](lotui::Point point, lotui::DockSide side) {
            const auto grip = workspace->toolbar().gripBounds();
            const lotui::Point start{grip.x + grip.width * 0.5F,
                grip.y + grip.height * 0.5F};
            tree.pointerPressed(start, lotui::PointerButton::Primary);
            tree.pointerMoved(point);
            require(workspace->previewSide() == side,
                "drag did not preview target edge");
            tree.pointerReleased(point, lotui::PointerButton::Primary);
            require(workspace->side() == side &&
                    !workspace->previewSide(),
                "toolbar did not dock at target edge");
            checkGeometry(*workspace);
            clickCommand();
        };
        checkGeometry(*workspace);
        clickCommand();
        drag({250.0F, 355.0F}, lotui::DockSide::Bottom);
        drag({4.0F, 180.0F}, lotui::DockSide::Left);
        drag({495.0F, 180.0F}, lotui::DockSide::Right);
        drag({250.0F, 4.0F}, lotui::DockSide::Top);
        require(commandClicks == 5,
            "command stopped receiving clicks after docking");

        const auto grip = workspace->toolbar().gripBounds();
        const lotui::Point start{grip.x + grip.width * 0.5F,
            grip.y + grip.height * 0.5F};
        tree.pointerPressed(start, lotui::PointerButton::Primary);
        tree.pointerMoved({250.0F, 180.0F});
        require(!workspace->previewSide(),
            "center should not preview a dock edge");
        tree.pointerReleased({250.0F, 180.0F}, lotui::PointerButton::Primary);
        require(workspace->side() == lotui::DockSide::Top,
            "center release changed dock side");

        tree.layout({0.0F, 0.0F, 280.0F, 220.0F});
        for (const auto side : {lotui::DockSide::Top, lotui::DockSide::Bottom,
                lotui::DockSide::Left, lotui::DockSide::Right}) {
            workspace->setSide(side);
            checkGeometry(*workspace);
        }
        std::cout << "dock_workspace_tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
