#include "core/pointer_router.h"

#include <algorithm>
#include <utility>

namespace lotui {

void PointerRouter::setHitTestEntries(std::vector<HitTestEntry> entries) {
    entries_ = std::move(entries);

    const auto stillExists = [this](PointerTargetId target) {
        return target == invalidPointerTarget ||
            std::any_of(
                entries_.begin(), entries_.end(),
                [target](const HitTestEntry& entry) {
                    return entry.enabled && entry.target == target;
                });
    };

    if (!stillExists(hoverTarget_)) {
        hoverTarget_ = invalidPointerTarget;
    }
    if (!stillExists(capturedTarget_)) {
        capturedTarget_ = invalidPointerTarget;
        capturedButton_ = PointerButton::Unspecified;
    }
}

PointerTargetId PointerRouter::hitTest(Point position) const noexcept {
    for (auto entry = entries_.rbegin(); entry != entries_.rend(); ++entry) {
        if (entry->enabled && entry->target != invalidPointerTarget &&
            contains(entry->bounds, position) &&
            contains(entry->clip, position)) {
            return entry->target;
        }
    }
    return invalidPointerTarget;
}

PointerRoute PointerRouter::pointerMoved(Point position) noexcept {
    hoverTarget_ = hitTest(position);
    const PointerTargetId target = capturedTarget_ != invalidPointerTarget
        ? capturedTarget_
        : hoverTarget_;
    return {target, isInsideTarget(target, position), false, false};
}

PointerRoute PointerRouter::pointerPressed(
    Point position,
    PointerButton button) noexcept {
    hoverTarget_ = hitTest(position);
    if (capturedTarget_ != invalidPointerTarget) {
        return {
            capturedTarget_,
            isInsideTarget(capturedTarget_, position),
            false,
            false,
        };
    }
    if (button == PointerButton::Unspecified ||
        hoverTarget_ == invalidPointerTarget) {
        return {};
    }

    capturedTarget_ = hoverTarget_;
    capturedButton_ = button;
    return {capturedTarget_, true, true, false};
}

PointerRoute PointerRouter::pointerReleased(
    Point position,
    PointerButton button) noexcept {
    hoverTarget_ = hitTest(position);
    if (capturedTarget_ == invalidPointerTarget) {
        return {
            hoverTarget_,
            hoverTarget_ != invalidPointerTarget,
            false,
            false,
        };
    }

    const PointerTargetId target = capturedTarget_;
    const bool inside = isInsideTarget(target, position);
    const bool endsCapture = button == capturedButton_;
    if (endsCapture) {
        capturedTarget_ = invalidPointerTarget;
        capturedButton_ = PointerButton::Unspecified;
    }
    return {target, inside, false, endsCapture};
}

PointerRoute PointerRouter::cancelPointer() noexcept {
    const PointerTargetId target = capturedTarget_;
    capturedTarget_ = invalidPointerTarget;
    capturedButton_ = PointerButton::Unspecified;
    hoverTarget_ = invalidPointerTarget;
    return {
        target,
        false,
        false,
        target != invalidPointerTarget,
    };
}

PointerTargetId PointerRouter::hoverTarget() const noexcept {
    return hoverTarget_;
}

PointerTargetId PointerRouter::capturedTarget() const noexcept {
    return capturedTarget_;
}

bool PointerRouter::isInsideTarget(
    PointerTargetId target,
    Point position) const noexcept {
    if (target == invalidPointerTarget) {
        return false;
    }
    const auto entry = std::find_if(
        entries_.begin(), entries_.end(),
        [target](const HitTestEntry& candidate) {
            return candidate.enabled && candidate.target == target;
        });
    return entry != entries_.end() &&
        contains(entry->bounds, position) &&
        contains(entry->clip, position);
}

} // namespace lotui
