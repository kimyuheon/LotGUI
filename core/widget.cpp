#include "core/widget.h"

#include <atomic>

namespace lotui {
namespace {

PointerTargetId nextPointerTargetId() noexcept {
    static std::atomic<PointerTargetId> next{1};
    PointerTargetId id = next.fetch_add(1, std::memory_order_relaxed);
    if (id == invalidPointerTarget) {
        id = next.fetch_add(1, std::memory_order_relaxed);
    }
    return id;
}

} // namespace

Widget::Widget()
    : pointerTargetId_(nextPointerTargetId()) {
}

void Widget::arrange(Rect bounds, Rect parentClip) {
    bounds_ = bounds;
    clip_ = intersect(bounds, parentClip);
    onArrange();
}

void Widget::paint(std::vector<PaintCommand>& commands) const {
    if (!hasArea(clip_)) {
        return;
    }
    onPaint(commands);
    paintChildren(commands);
}

Rect Widget::bounds() const noexcept {
    return bounds_;
}

Rect Widget::clip() const noexcept {
    return clip_;
}

PointerTargetId Widget::pointerTargetId() const noexcept {
    return pointerTargetId_;
}

void Widget::collectHitTestEntries(
    std::vector<HitTestEntry>& entries) const {
    if (!hasArea(clip_)) {
        return;
    }
    if (acceptsPointerEvents()) {
        entries.push_back({pointerTargetId_, bounds_, clip_, true});
    }
    collectChildHitTestEntries(entries);
}

Widget* Widget::findByPointerTarget(PointerTargetId target) noexcept {
    if (pointerTargetId_ == target) {
        return this;
    }
    return findChildByPointerTarget(target);
}

bool Widget::dispatchPointerEvent(const WidgetPointerEvent& event) {
    return onPointerEvent(event);
}

void Widget::onArrange() {
}

void Widget::onPaint(std::vector<PaintCommand>&) const {
}

void Widget::paintChildren(std::vector<PaintCommand>&) const {
}

void Widget::collectChildHitTestEntries(
    std::vector<HitTestEntry>&) const {
}

Widget* Widget::findChildByPointerTarget(PointerTargetId) noexcept {
    return nullptr;
}

bool Widget::acceptsPointerEvents() const noexcept {
    return false;
}

bool Widget::onPointerEvent(const WidgetPointerEvent&) {
    return false;
}

} // namespace lotui
