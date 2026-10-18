#include "core/widget_tree.h"
#include "renderer/vulkan/vulkan_embedded_renderer.h"
#include "widgets/button.h"
#include "widgets/disclosure_panel.h"

#include <memory>
#include <string_view>
#include <vector>

namespace {

class StubTextLayout final : public lotui::TextLayout {
public:
    lotui::Size size() const noexcept override { return {60.0F, 18.0F}; }
    float baseline() const noexcept override { return 14.0F; }
    void appendPaintCommands(lotui::Point, lotui::Rect, lotui::Color,
        std::vector<lotui::PaintCommand>&) const override {}
};

class StubTextEngine final : public lotui::TextEngine {
public:
    std::unique_ptr<lotui::TextLayout> createLayout(
        std::string_view, const lotui::TextStyle&,
        const lotui::TextLayoutOptions&) const override {
        return std::make_unique<StubTextLayout>();
    }
};

} // namespace

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

    auto disclosure = std::make_unique<lotui::DisclosurePanel>(
        std::make_shared<StubTextEngine>(), "Properties",
        std::make_unique<lotui::Button>());
    lotui::DisclosurePanel* panel = disclosure.get();
    lotui::WidgetTree disclosureTree(std::move(disclosure));
    disclosureTree.layout({0.0F, 0.0F, 180.0F, 70.0F});
    disclosureTree.pointerPressed(
        {12.0F, 12.0F}, lotui::PointerButton::Primary);
    disclosureTree.pointerReleased(
        {12.0F, 12.0F}, lotui::PointerButton::Primary);

    lotui::VulkanEmbeddedRenderer embedded;
    return clicks == 1 && !commands.empty() && !panel->isExpanded()
        ? 0 : 1;
}
