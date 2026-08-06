#include "core/focus_manager.h"

#include <algorithm>
#include <iterator>
#include <utility>

namespace lotui {

FocusChange FocusManager::setTargets(
    std::vector<PointerTargetId> targets) {
    targets.erase(
        std::remove(targets.begin(), targets.end(), invalidPointerTarget),
        targets.end());
    targets.erase(std::unique(targets.begin(), targets.end()), targets.end());
    targets_ = std::move(targets);

    const PointerTargetId previous = focusedTarget_;
    if (std::find(targets_.begin(), targets_.end(), focusedTarget_) ==
        targets_.end()) {
        focusedTarget_ = invalidPointerTarget;
    }
    return {previous, focusedTarget_};
}

FocusChange FocusManager::focus(PointerTargetId target) noexcept {
    const PointerTargetId previous = focusedTarget_;
    if (std::find(targets_.begin(), targets_.end(), target) != targets_.end()) {
        focusedTarget_ = target;
    }
    return {previous, focusedTarget_};
}

FocusChange FocusManager::moveFocus(bool reverse) noexcept {
    const PointerTargetId previous = focusedTarget_;
    if (targets_.empty()) {
        focusedTarget_ = invalidPointerTarget;
        return {previous, focusedTarget_};
    }

    const auto current = std::find(
        targets_.begin(), targets_.end(), focusedTarget_);
    if (current == targets_.end()) {
        focusedTarget_ = reverse ? targets_.back() : targets_.front();
    } else if (reverse) {
        focusedTarget_ = current == targets_.begin()
            ? targets_.back()
            : *std::prev(current);
    } else {
        const auto next = std::next(current);
        focusedTarget_ = next == targets_.end() ? targets_.front() : *next;
    }
    return {previous, focusedTarget_};
}

FocusChange FocusManager::clear() noexcept {
    const PointerTargetId previous = focusedTarget_;
    focusedTarget_ = invalidPointerTarget;
    return {previous, focusedTarget_};
}

PointerTargetId FocusManager::focusedTarget() const noexcept {
    return focusedTarget_;
}

const std::vector<PointerTargetId>& FocusManager::targets() const noexcept {
    return targets_;
}

} // namespace lotui
