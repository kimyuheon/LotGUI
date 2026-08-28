#include "core/widget_tree.h"

#include <stdexcept>
#include <utility>

namespace lotui {

WidgetTree::WidgetTree(std::unique_ptr<Widget> root) {
    setRoot(std::move(root));
}

Widget& WidgetTree::root() noexcept {
    return *root_;
}

const Widget& WidgetTree::root() const noexcept {
    return *root_;
}

void WidgetTree::setRoot(std::unique_ptr<Widget> root) {
    if (!root) {
        throw std::invalid_argument("WidgetTree root must not be null");
    }
    pointerRouter_.cancelPointer();
    visualHoverTarget_ = invalidPointerTarget;
    activeFocusScope_ = invalidPointerTarget;
    scopeFocusHistory_.clear();
    if (root_) {
        applyFocusChange(focusManager_.clear());
    }
    root_ = std::move(root);
    syncHitTests();
    syncFocusTargets();
}

void WidgetTree::layout(Rect bounds) {
    layout(bounds, bounds);
}

void WidgetTree::layout(Rect bounds, Rect clip) {
    root_->arrange(bounds, clip);
    syncHitTests();
    syncFocusTargets();
}

void WidgetTree::paint(std::vector<PaintCommand>& commands) const {
    root_->paint(commands);
}

WidgetPointerUpdate WidgetTree::pointerMoved(Point position) {
    syncHitTests();
    const PointerRoute route = pointerRouter_.pointerMoved(position);
    const PointerTargetId hover =
        pointerRouter_.capturedTarget() != invalidPointerTarget
        ? (route.inside ? route.target : invalidPointerTarget)
        : pointerRouter_.hoverTarget();

    WidgetPointerUpdate update;
    update.needsRepaint = transitionHover(hover, position);
    update.needsRepaint = dispatch(
        route.target,
        WidgetPointerEventType::Move,
        position,
        PointerButton::Unspecified,
        route.inside) || update.needsRepaint;
    return update;
}

WidgetPointerUpdate WidgetTree::pointerPressed(
    Point position,
    PointerButton button) {
    syncHitTests();
    const bool focusTargetsChanged = syncFocusTargets();
    const PointerRoute route = pointerRouter_.pointerPressed(position, button);
    WidgetPointerUpdate update;
    update.captureStarted = route.captureStarted;
    update.needsRepaint = focusTargetsChanged ||
        transitionHover(route.target, position);
    if (button == PointerButton::Primary) {
        const FocusChange focus = focusManager_.focus(route.target);
        update.focusChanged = focus.changed();
        update.needsRepaint = applyFocusChange(focus) ||
            update.needsRepaint;
    }
    update.needsRepaint = dispatch(
        route.target,
        WidgetPointerEventType::Press,
        position,
        button,
        route.inside) || update.needsRepaint;
    return update;
}

WidgetPointerUpdate WidgetTree::pointerReleased(
    Point position,
    PointerButton button) {
    syncHitTests();
    const PointerRoute route = pointerRouter_.pointerReleased(position, button);
    WidgetPointerUpdate update;
    update.captureEnded = route.captureEnded;
    update.needsRepaint = dispatch(
        route.target,
        WidgetPointerEventType::Release,
        position,
        button,
        route.inside);

    const PointerTargetId hover =
        pointerRouter_.capturedTarget() != invalidPointerTarget
        ? (route.inside ? route.target : invalidPointerTarget)
        : pointerRouter_.hoverTarget();
    update.needsRepaint = transitionHover(hover, position) ||
        update.needsRepaint;
    return update;
}

WidgetPointerUpdate WidgetTree::cancelPointer() {
    const PointerRoute route = pointerRouter_.cancelPointer();
    WidgetPointerUpdate update;
    update.captureEnded = route.captureEnded;
    update.needsRepaint = dispatch(
        route.target,
        WidgetPointerEventType::Cancel,
        {},
        PointerButton::Unspecified,
        false);
    update.needsRepaint = transitionHover(
        invalidPointerTarget, {}) || update.needsRepaint;
    return update;
}

WidgetKeyUpdate WidgetTree::keyPressed(
    KeyCode key,
    KeyModifiers modifiers,
    bool repeat) {
    WidgetKeyUpdate update;
    update.needsRepaint = syncFocusTargets();
    const PointerTargetId focusBeforePreview =
        focusManager_.focusedTarget();
    if (root_->dispatchPreviewKeyEvent({
            WidgetKeyEventType::Press, key, modifiers, repeat})) {
        update.handled = true;
        syncFocusTargets();
        update.needsRepaint = true;
        update.focusChanged = focusBeforePreview !=
            focusManager_.focusedTarget();
        return update;
    }
    if (key == KeyCode::Tab && !modifiers.control &&
        !modifiers.alt && !modifiers.meta) {
        const FocusChange change = focusManager_.moveFocus(modifiers.shift);
        update.handled = true;
        update.focusChanged = change.changed();
        update.needsRepaint = applyFocusChange(change) ||
            update.needsRepaint;
        return update;
    }

    Widget* focused = focusedWidget();
    if (focused != nullptr) {
        update.handled = focused->dispatchKeyEvent({
            WidgetKeyEventType::Press, key, modifiers, repeat});
        update.needsRepaint = update.handled || update.needsRepaint;
    }
    return update;
}

WidgetKeyUpdate WidgetTree::keyReleased(
    KeyCode key,
    KeyModifiers modifiers) {
    WidgetKeyUpdate update;
    update.needsRepaint = syncFocusTargets();
    Widget* focused = focusedWidget();
    if (focused != nullptr) {
        update.handled = focused->dispatchKeyEvent({
            WidgetKeyEventType::Release, key, modifiers, false});
        update.needsRepaint = update.handled || update.needsRepaint;
    }
    return update;
}

WidgetKeyUpdate WidgetTree::cancelKeyboard() {
    WidgetKeyUpdate update;
    Widget* focused = focusedWidget();
    if (focused != nullptr) {
        update.handled = focused->dispatchKeyEvent({
            WidgetKeyEventType::Cancel, KeyCode::Unknown, {}, false});
        update.needsRepaint = update.handled;
    }
    return update;
}

WidgetTextInputUpdate WidgetTree::textInput(
    const TextInputEvent& event) {
    Widget* focused = focusedWidget();
    if (focused == nullptr || !focused->requestsTextInput()) {
        return {};
    }
    const bool handled = focused->dispatchTextInputEvent(event);
    return {handled, handled};
}

TextInputState WidgetTree::textInputState() const noexcept {
    const Widget* focused = focusedWidget();
    if (focused == nullptr || !focused->requestsTextInput()) {
        return {};
    }
    return {true, focused->requestedTextInputRect()};
}

WidgetKeyUpdate WidgetTree::moveFocus(bool reverse) {
    WidgetKeyUpdate update;
    update.needsRepaint = syncFocusTargets();
    const FocusChange change = focusManager_.moveFocus(reverse);
    update.handled = !focusManager_.targets().empty();
    update.focusChanged = change.changed();
    update.needsRepaint = applyFocusChange(change) || update.needsRepaint;
    return update;
}

WidgetKeyUpdate WidgetTree::clearFocus() {
    const FocusChange change = focusManager_.clear();
    return {
        change.changed(),
        change.changed(),
        applyFocusChange(change),
    };
}

Widget* WidgetTree::focusedWidget() noexcept {
    return root_->findByPointerTarget(focusManager_.focusedTarget());
}

const Widget* WidgetTree::focusedWidget() const noexcept {
    return const_cast<WidgetTree*>(this)->focusedWidget();
}

void WidgetTree::syncHitTests() {
    std::vector<HitTestEntry> entries;
    root_->collectHitTestEntries(entries);
    pointerRouter_.setHitTestEntries(std::move(entries));
}

bool WidgetTree::syncFocusTargets() {
    std::vector<PointerTargetId> targets;
    root_->collectFocusTargets(targets);
    const PointerTargetId nextScope = root_->activeFocusScopeTarget();
    const bool scopeChanged = nextScope != activeFocusScope_;
    const PointerTargetId previouslyFocused = focusManager_.focusedTarget();
    if (scopeChanged && activeFocusScope_ != invalidPointerTarget &&
        previouslyFocused != invalidPointerTarget) {
        scopeFocusHistory_[activeFocusScope_] = previouslyFocused;
    }

    bool needsRepaint = applyFocusChange(
        focusManager_.setTargets(std::move(targets)));
    if (!scopeChanged) {
        return needsRepaint;
    }

    const PointerTargetId previousScope = activeFocusScope_;
    activeFocusScope_ = nextScope;
    if (previousScope == invalidPointerTarget ||
        focusManager_.focusedTarget() != invalidPointerTarget) {
        return needsRepaint;
    }

    const auto saved = scopeFocusHistory_.find(activeFocusScope_);
    FocusChange restored;
    if (saved != scopeFocusHistory_.end()) {
        restored = focusManager_.focus(saved->second);
        scopeFocusHistory_.erase(saved);
    }
    if (focusManager_.focusedTarget() == invalidPointerTarget) {
        restored = focusManager_.moveFocus(false);
    }
    return applyFocusChange(restored) || needsRepaint;
}

bool WidgetTree::applyFocusChange(const FocusChange& change) {
    if (!change.changed()) {
        return false;
    }
    bool needsRepaint = false;
    if (change.previous != invalidPointerTarget) {
        if (Widget* previous = root_->findByPointerTarget(change.previous)) {
            needsRepaint = previous->dispatchFocusChanged(false);
        }
    }
    if (change.current != invalidPointerTarget) {
        if (Widget* current = root_->findByPointerTarget(change.current)) {
            needsRepaint = current->dispatchFocusChanged(true) || needsRepaint;
        }
    }
    return needsRepaint;
}

bool WidgetTree::dispatch(
    PointerTargetId target,
    WidgetPointerEventType type,
    Point position,
    PointerButton button,
    bool inside) {
    if (target == invalidPointerTarget) {
        return false;
    }
    Widget* widget = root_->findByPointerTarget(target);
    return widget != nullptr && widget->dispatchPointerEvent(
        {type, position, button, inside});
}

bool WidgetTree::transitionHover(
    PointerTargetId target,
    Point position) {
    if (target == visualHoverTarget_) {
        return false;
    }

    bool needsRepaint = dispatch(
        visualHoverTarget_,
        WidgetPointerEventType::Leave,
        position,
        PointerButton::Unspecified,
        false);
    visualHoverTarget_ = target;
    needsRepaint = dispatch(
        visualHoverTarget_,
        WidgetPointerEventType::Enter,
        position,
        PointerButton::Unspecified,
        true) || needsRepaint;
    return needsRepaint;
}

} // namespace lotui
