#pragma once

#include "core/widget.h"

#include <memory>
#include <vector>

namespace lotui {

struct WidgetPointerUpdate {
    bool captureStarted{false};
    bool captureEnded{false};
    bool needsRepaint{false};
};

class WidgetTree {
public:
    explicit WidgetTree(std::unique_ptr<Widget> root);

    WidgetTree(const WidgetTree&) = delete;
    WidgetTree& operator=(const WidgetTree&) = delete;
    WidgetTree(WidgetTree&&) noexcept = default;
    WidgetTree& operator=(WidgetTree&&) noexcept = default;

    Widget& root() noexcept;
    const Widget& root() const noexcept;

    void setRoot(std::unique_ptr<Widget> root);
    void layout(Rect bounds);
    void layout(Rect bounds, Rect clip);
    void paint(std::vector<PaintCommand>& commands) const;

    WidgetPointerUpdate pointerMoved(Point position);
    WidgetPointerUpdate pointerPressed(
        Point position,
        PointerButton button);
    WidgetPointerUpdate pointerReleased(
        Point position,
        PointerButton button);
    WidgetPointerUpdate cancelPointer();

private:
    void syncHitTests();
    bool dispatch(
        PointerTargetId target,
        WidgetPointerEventType type,
        Point position,
        PointerButton button,
        bool inside);
    bool transitionHover(PointerTargetId target, Point position);

    std::unique_ptr<Widget> root_;
    PointerRouter pointerRouter_;
    PointerTargetId visualHoverTarget_{invalidPointerTarget};
};

} // namespace lotui
