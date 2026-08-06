#include "core/focus_manager.h"

#include <cstdlib>
#include <iostream>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "focus_manager_tests failed: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

void traversesAndWrapsTargets() {
    lotui::FocusManager focus;
    focus.setTargets({11, 22, 33});

    require(focus.moveFocus(false).current == 11,
        "forward traversal must start at the first target");
    require(focus.moveFocus(false).current == 22,
        "forward traversal must advance");
    require(focus.moveFocus(false).current == 33,
        "forward traversal must reach the last target");
    require(focus.moveFocus(false).current == 11,
        "forward traversal must wrap");
    require(focus.moveFocus(true).current == 33,
        "reverse traversal must wrap");
}

void preservesOrClearsFocusWhenTargetsChange() {
    lotui::FocusManager focus;
    focus.setTargets({11, 22});
    focus.focus(22);
    require(!focus.setTargets({22, 33}).changed(),
        "an existing target must keep focus");
    const auto removed = focus.setTargets({11, 33});
    require(removed.previous == 22 &&
            removed.current == lotui::invalidPointerTarget,
        "removing the focused target must clear focus");
    require(!focus.focus(99).changed(),
        "an unknown target must not receive focus");
}

} // namespace

int main() {
    traversesAndWrapsTargets();
    preservesOrClearsFocusWhenTargetsChange();
    std::cout << "focus_manager_tests passed\n";
    return EXIT_SUCCESS;
}
