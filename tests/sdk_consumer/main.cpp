#include "core/widget_tree.h"
#include "renderer/vulkan/vulkan_embedded_renderer.h"
#include "widgets/button.h"

#include <memory>
#include <vector>

int main() {
    int clicks = 0;
    auto button = std::make_unique<lotui::Button>(
        lotui::Size{96.0F, 32.0F}, [&clicks] { ++clicks; });
    lotui::WidgetTree tree(std::move(button));
    tree.layout({0.0F, 0.0F, 96.0F, 32.0F});
    tree.pointerPressed({16.0F, 16.0F}, lotui::PointerButton::Primary);
    tree.pointerReleased({16.0F, 16.0F}, lotui::PointerButton::Primary);
    std::vector<lotui::PaintCommand> commands;
    tree.paint(commands);

    lotui::VulkanEmbeddedRenderer embedded;
    return clicks == 1 && !commands.empty() ? 0 : 1;
}
