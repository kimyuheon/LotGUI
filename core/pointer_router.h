#pragma once

#include "core/geometry.h"
#include "core/pointer_event.h"

#include <cstdint>
#include <vector>

namespace lotui {

using PointerTargetId = std::uint32_t;
inline constexpr PointerTargetId invalidPointerTarget = 0;

struct HitTestEntry {
    PointerTargetId target{invalidPointerTarget};
    Rect bounds{};
    Rect clip{};
    bool enabled{true};
};

struct PointerRoute {
    PointerTargetId target{invalidPointerTarget};
    bool inside{false};
    bool captureStarted{false};
    bool captureEnded{false};

    explicit operator bool() const noexcept {
        return target != invalidPointerTarget;
    }
};

class PointerRouter {
public:
    void setHitTestEntries(std::vector<HitTestEntry> entries);

    PointerTargetId hitTest(Point position) const noexcept;
    PointerRoute pointerMoved(Point position) noexcept;
    PointerRoute pointerPressed(
        Point position,
        PointerButton button) noexcept;
    PointerRoute pointerReleased(
        Point position,
        PointerButton button) noexcept;
    PointerRoute cancelPointer() noexcept;

    PointerTargetId hoverTarget() const noexcept;
    PointerTargetId capturedTarget() const noexcept;

private:
    bool isInsideTarget(
        PointerTargetId target,
        Point position) const noexcept;

    std::vector<HitTestEntry> entries_;
    PointerTargetId hoverTarget_{invalidPointerTarget};
    PointerTargetId capturedTarget_{invalidPointerTarget};
    PointerButton capturedButton_{PointerButton::Unspecified};
};

} // namespace lotui
