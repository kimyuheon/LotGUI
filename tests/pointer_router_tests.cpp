#include "core/pointer_router.h"

#include <cstdlib>
#include <iostream>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "pointer_router_tests failed: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

lotui::PointerRouter makeRouter() {
    lotui::PointerRouter router;
    router.setHitTestEntries({
        {1, {0.0F, 0.0F, 100.0F, 100.0F},
            {0.0F, 0.0F, 100.0F, 100.0F}, true},
        {2, {25.0F, 25.0F, 100.0F, 100.0F},
            {50.0F, 50.0F, 50.0F, 50.0F}, true},
    });
    return router;
}

void choosesTopmostClippedTarget() {
    auto router = makeRouter();
    require(router.hitTest({60.0F, 60.0F}) == 2,
            "the last matching entry must be topmost");
    require(router.hitTest({30.0F, 30.0F}) == 1,
            "a point outside the top entry clip must hit the lower entry");
    require(router.hitTest({150.0F, 150.0F}) == lotui::invalidPointerTarget,
            "a point outside all entries must miss");
}

void routesOutsideMovementToCapturedTarget() {
    auto router = makeRouter();
    const auto press = router.pointerPressed(
        {10.0F, 10.0F}, lotui::PointerButton::Primary);
    require(press.target == 1 && press.captureStarted,
            "press must capture the hit target");

    const auto move = router.pointerMoved({180.0F, 180.0F});
    require(move.target == 1 && !move.inside,
            "captured movement must stay routed to its target");

    const auto release = router.pointerReleased(
        {180.0F, 180.0F}, lotui::PointerButton::Primary);
    require(release.target == 1 && release.captureEnded && !release.inside,
            "matching release must end capture without activating outside");
    require(router.capturedTarget() == lotui::invalidPointerTarget,
            "capture must be cleared after release");
}

void ignoresNonMatchingReleaseButton() {
    auto router = makeRouter();
    router.pointerPressed({10.0F, 10.0F}, lotui::PointerButton::Primary);
    const auto otherRelease = router.pointerReleased(
        {10.0F, 10.0F}, lotui::PointerButton::Secondary);
    require(!otherRelease.captureEnded && router.capturedTarget() == 1,
            "a different button must not end capture");

    const auto cancel = router.cancelPointer();
    require(cancel.target == 1 && cancel.captureEnded,
            "cancel must report and clear the captured target");
}

} // namespace

int main() {
    choosesTopmostClippedTarget();
    routesOutsideMovementToCapturedTarget();
    ignoresNonMatchingReleaseButton();
    std::cout << "pointer_router_tests passed\n";
    return EXIT_SUCCESS;
}
