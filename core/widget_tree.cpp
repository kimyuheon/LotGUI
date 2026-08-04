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
    root_ = std::move(root);
    syncHitTests();
}

void WidgetTree::layout(Rect bounds) {
    layout(bounds, bounds);
}

void WidgetTree::layout(Rect bounds, Rect clip) {
    root_->arrange(bounds, clip);
    syncHitTests();
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
    const PointerRoute route = pointerRouter_.pointerPressed(position, button);
    WidgetPointerUpdate update;
    update.captureStarted = route.captureStarted;
    update.needsRepaint = transitionHover(route.target, position);
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

void WidgetTree::syncHitTests() {
    std::vector<HitTestEntry> entries;
    root_->collectHitTestEntries(entries);
    pointerRouter_.setHitTestEntries(std::move(entries));
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
