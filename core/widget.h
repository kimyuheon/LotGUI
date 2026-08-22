#pragma once

#include "core/layout.h"
#include "core/key_event.h"
#include "core/paint_command.h"
#include "core/pointer_router.h"
#include "core/text_input_event.h"

#include <vector>

namespace lotui {

enum class WidgetPointerEventType {
    Enter,
    Leave,
    Move,
    Press,
    Release,
    Cancel,
};

struct WidgetPointerEvent {
    WidgetPointerEventType type{WidgetPointerEventType::Move};
    Point position{};
    PointerButton button{PointerButton::Unspecified};
    bool inside{false};
};

class Widget {
public:
    Widget();
    virtual ~Widget() = default;

    Widget(const Widget&) = delete;
    Widget& operator=(const Widget&) = delete;
    Widget(Widget&&) = delete;
    Widget& operator=(Widget&&) = delete;

    virtual Size measure(const LayoutConstraints& constraints) const = 0;

    void arrange(Rect bounds, Rect parentClip);
    void paint(std::vector<PaintCommand>& commands) const;

    Rect bounds() const noexcept;
    Rect clip() const noexcept;
    PointerTargetId pointerTargetId() const noexcept;

private:
    friend class WidgetTree;
    friend class LinearLayout;
    friend class SingleChildWidget;

    void collectHitTestEntries(std::vector<HitTestEntry>& entries) const;
    void collectFocusTargets(std::vector<PointerTargetId>& targets) const;
    Widget* findByPointerTarget(PointerTargetId target) noexcept;
    bool dispatchPointerEvent(const WidgetPointerEvent& event);
    bool dispatchFocusChanged(bool focused);
    bool dispatchKeyEvent(const WidgetKeyEvent& event);
    bool dispatchTextInputEvent(const TextInputEvent& event);
    bool requestsTextInput() const noexcept;
    Rect requestedTextInputRect() const noexcept;

protected:
    virtual void onArrange();
    virtual void onPaint(std::vector<PaintCommand>& commands) const;
    virtual void paintChildren(std::vector<PaintCommand>& commands) const;
    virtual void collectChildHitTestEntries(
        std::vector<HitTestEntry>& entries) const;
    virtual void collectChildFocusTargets(
        std::vector<PointerTargetId>& targets) const;
    virtual Widget* findChildByPointerTarget(
        PointerTargetId target) noexcept;
    virtual bool acceptsPointerEvents() const noexcept;
    virtual bool onPointerEvent(const WidgetPointerEvent& event);
    virtual bool acceptsFocus() const noexcept;
    virtual bool onFocusChanged(bool focused);
    virtual bool onKeyEvent(const WidgetKeyEvent& event);
    virtual bool acceptsTextInput() const noexcept;
    virtual Rect textInputRect() const noexcept;
    virtual bool onTextInputEvent(const TextInputEvent& event);

private:
    PointerTargetId pointerTargetId_{invalidPointerTarget};
    Rect bounds_{};
    Rect clip_{};
};

} // namespace lotui
