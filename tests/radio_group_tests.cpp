#include "core/widget_tree.h"
#include "widgets/radio_group.h"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "radio_group_tests failed: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

void pointerAndKeyboardSelection() {
    int changes = 0;
    std::string last;
    auto group = std::make_unique<lotui::RadioGroup>(
        nullptr,
        std::vector<lotui::RadioOption>{
            {"union", "Union"},
            {"subtract", "Subtract", false},
            {"intersect", "Intersect"},
        },
        std::optional<std::string>{"union"},
        [&changes, &last](std::string_view id) {
            ++changes;
            last = id;
        });
    auto* observed = group.get();
    lotui::WidgetTree tree(std::move(group));
    tree.layout({0.0F, 0.0F, 180.0F, 78.0F});

    tree.pointerPressed({20.0F, 39.0F}, lotui::PointerButton::Primary);
    tree.pointerReleased({20.0F, 39.0F}, lotui::PointerButton::Primary);
    require(observed->selectedId() == "union" && changes == 0,
        "disabled option must not be selectable");
    tree.pointerPressed({20.0F, 65.0F}, lotui::PointerButton::Primary);
    tree.pointerReleased({20.0F, 65.0F}, lotui::PointerButton::Primary);
    require(observed->selectedId() == "intersect" && changes == 1 &&
            last == "intersect", "pointer must choose enabled option once");

    tree.keyPressed(lotui::KeyCode::Tab);
    tree.keyPressed(lotui::KeyCode::Up);
    require(observed->selectedId() == "union" && changes == 2,
        "Up must skip disabled option");
    tree.keyPressed(lotui::KeyCode::Home);
    require(changes == 2, "unchanged keyboard choice must not notify");
    tree.keyPressed(lotui::KeyCode::End);
    require(observed->selectedId() == "intersect" && changes == 3,
        "End must choose final enabled option");
    observed->setSelectedId("union");
    require(changes == 3, "programmatic selection must not notify");
    observed->setEnabled(false);
    tree.keyPressed(lotui::KeyCode::Down);
    tree.pointerPressed({20.0F, 65.0F}, lotui::PointerButton::Primary);
    tree.pointerReleased({20.0F, 65.0F}, lotui::PointerButton::Primary);
    require(observed->selectedId() == "union" && changes == 3,
        "disabled group must ignore input");
}

void validatesAndPreservesSelection() {
    lotui::RadioGroup group(nullptr, {{"a", "A"}, {"b", "B"}}, "b");
    require(group.selectedId() == "b", "constructor must set selection");
    group.setOptions({{"a", "A"}, {"b", "B"}, {"c", "C"}});
    require(group.selectedId() == "b", "refresh must retain stable ID");
    group.setOptions({{"a", "A"}, {"b", "B", false}});
    require(!group.selectedId(), "disabled selection must be cleared");
    require(!group.setSelectedId("missing"), "unknown ID must be rejected");
    try {
        group.setOptions({{"a", "A"}, {"a", "duplicate"}});
        require(false, "duplicate IDs must be rejected");
    } catch (const std::invalid_argument&) {
        require(group.options().size() == 2,
            "invalid update must keep existing options");
    }
}

} // namespace

int main() {
    pointerAndKeyboardSelection();
    validatesAndPreservesSelection();
    std::cout << "radio_group_tests passed\n";
    return EXIT_SUCCESS;
}
