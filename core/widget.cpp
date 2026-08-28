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

void Widget::collectFocusTargets(
    std::vector<PointerTargetId>& targets) const {
    if (acceptsFocus()) {
        targets.push_back(pointerTargetId_);
    }
    collectChildFocusTargets(targets);
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

bool Widget::dispatchFocusChanged(bool focused) {
    return onFocusChanged(focused);
}

bool Widget::dispatchPreviewKeyEvent(const WidgetKeyEvent& event) {
    return onPreviewKeyEvent(event);
}

bool Widget::dispatchKeyEvent(const WidgetKeyEvent& event) {
    return onKeyEvent(event);
}

bool Widget::dispatchUnhandledKeyEvent(const WidgetKeyEvent& event) {
    return onUnhandledKeyEvent(event);
}

bool Widget::dispatchTextInputEvent(const TextInputEvent& event) {
    return onTextInputEvent(event);
}

bool Widget::requestsTextInput() const noexcept {
    return acceptsTextInput();
}

Rect Widget::requestedTextInputRect() const noexcept {
    return textInputRect();
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

void Widget::collectChildFocusTargets(
    std::vector<PointerTargetId>&) const {
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

bool Widget::acceptsFocus() const noexcept {
    return false;
}

bool Widget::onFocusChanged(bool) {
    return false;
}

bool Widget::onPreviewKeyEvent(const WidgetKeyEvent&) {
    return false;
}

bool Widget::onKeyEvent(const WidgetKeyEvent&) {
    return false;
}

bool Widget::onUnhandledKeyEvent(const WidgetKeyEvent&) {
    return false;
}

bool Widget::acceptsTextInput() const noexcept {
    return false;
}

Rect Widget::textInputRect() const noexcept {
    return bounds_;
}

bool Widget::onTextInputEvent(const TextInputEvent&) {
    return false;
}

PointerTargetId Widget::activeFocusScopeTarget() const noexcept {
    return pointerTargetId_;
}

} // namespace lotui
