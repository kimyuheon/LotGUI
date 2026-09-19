#include "widgets/popup.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace lotui {
namespace {

float finite(float value) noexcept {
    return std::isfinite(value) ? value : 0.0F;
}

float nonNegative(float value) noexcept {
    return std::max(0.0F, finite(value));
}

Rect normalized(Rect value) noexcept {
    value.x = finite(value.x);
    value.y = finite(value.y);
    value.width = nonNegative(value.width);
    value.height = nonNegative(value.height);
    return value;
}

} // namespace

PopupHost::PopupHost(std::unique_ptr<Widget> content)
    : content_(std::move(content)) {
    if (!content_) {
        throw std::invalid_argument("PopupHost content must not be null");
    }
}

Size PopupHost::measure(const LayoutConstraints& constraints) const {
    return content_->measure(constraints);
}

void PopupHost::setContent(std::unique_ptr<Widget> content) {
    if (!content) {
        throw std::invalid_argument("PopupHost content must not be null");
    }
    content_ = std::move(content);
    if (hasArea(bounds())) {
        content_->arrange(bounds(), clip());
    }
}

Widget* PopupHost::content() noexcept {
    return content_.get();
}

const Widget* PopupHost::content() const noexcept {
    return content_.get();
}

PopupId PopupHost::showPopup(
    std::unique_ptr<Widget> popupWidget,
    Rect popupAnchor,
    PopupOptions options,
    PopupClosedHandler onClosed) {
    if (!popupWidget) {
        throw std::invalid_argument("PopupHost popup must not be null");
    }
    if (popup_) {
        closePopup(popupId_, PopupCloseReason::Replaced);
    }
    PopupId id = nextPopupId_++;
    if (id == invalidPopupId) {
        id = nextPopupId_++;
    }
    options.gap = nonNegative(options.gap);
    popup_ = std::move(popupWidget);
    popupId_ = id;
    anchor_ = normalized(popupAnchor);
    options_ = options;
    onClosed_ = std::move(onClosed);
    arrangePopup();
    return id;
}

bool PopupHost::acceptPopup(PopupId id) {
    return closePopup(id, PopupCloseReason::Accepted);
}

bool PopupHost::dismissPopup(PopupId id) {
    return closePopup(id, PopupCloseReason::Dismissed);
}

std::unique_ptr<Widget> PopupHost::takePopup() noexcept {
    onClosed_ = {};
    popupId_ = invalidPopupId;
    return std::move(popup_);
}

Widget* PopupHost::popup() noexcept {
    return popup_.get();
}

const Widget* PopupHost::popup() const noexcept {
    return popup_.get();
}

PopupId PopupHost::popupId() const noexcept {
    return popupId_;
}

bool PopupHost::hasPopup() const noexcept {
    return popup_ != nullptr;
}

Rect PopupHost::anchor() const noexcept {
    return anchor_;
}

Rect PopupHost::popupBounds() const noexcept {
    return popup_ ? popup_->bounds() : Rect{};
}

void PopupHost::onArrange() {
    content_->arrange(bounds(), clip());
    arrangePopup();
}

void PopupHost::paintChildren(
    std::vector<PaintCommand>& commands) const {
    dismissedPopup_.reset();
    content_->paint(commands);
    if (popup_) {
        popup_->paint(commands);
    }
}

void PopupHost::collectChildHitTestEntries(
    std::vector<HitTestEntry>& entries) const {
    if (popup_) {
        popup_->collectHitTestEntries(entries);
    } else {
        content_->collectHitTestEntries(entries);
    }
}

void PopupHost::collectChildFocusTargets(
    std::vector<PointerTargetId>& targets) const {
    if (popup_) {
        popup_->collectFocusTargets(targets);
    } else {
        content_->collectFocusTargets(targets);
    }
}

Widget* PopupHost::findChildByPointerTarget(
    PointerTargetId target) noexcept {
    if (popup_) {
        if (Widget* result = popup_->findByPointerTarget(target)) {
            return result;
        }
    }
    if (Widget* result = content_->findByPointerTarget(target)) {
        return result;
    }
    return dismissedPopup_
        ? dismissedPopup_->findByPointerTarget(target)
        : nullptr;
}

bool PopupHost::acceptsPointerEvents() const noexcept {
    return popup_ != nullptr;
}

bool PopupHost::onPointerEvent(const WidgetPointerEvent& event) {
    if (!popup_) {
        return false;
    }
    if (event.type == WidgetPointerEventType::Press &&
        event.button == PointerButton::Primary &&
        options_.dismissOnOutsidePress) {
        dismissPopup();
    }
    return true;
}

bool PopupHost::onPreviewKeyEvent(const WidgetKeyEvent& event) {
    if (!popup_ || !options_.dismissOnEscape ||
        event.type != WidgetKeyEventType::Press ||
        event.key != KeyCode::Escape) {
        return false;
    }
    closePopup(popupId_, PopupCloseReason::Escape);
    return true;
}

bool PopupHost::onUnhandledKeyEvent(const WidgetKeyEvent& event) {
    return popup_ && popup_->dispatchUnhandledKeyEvent(event);
}

PointerTargetId PopupHost::activeFocusScopeTarget() const noexcept {
    return popup_ ? popup_->pointerTargetId() : content_->pointerTargetId();
}

bool PopupHost::closePopup(PopupId id, PopupCloseReason reason) {
    if (!popup_ || (id != invalidPopupId && id != popupId_)) {
        return false;
    }
    dismissedPopup_ = std::move(popup_);
    popupId_ = invalidPopupId;
    PopupClosedHandler callback = std::move(onClosed_);
    onClosed_ = {};
    if (callback) {
        callback(reason);
    }
    return true;
}

void PopupHost::arrangePopup() {
    if (!popup_ || !hasArea(bounds())) {
        return;
    }
    const float gap = options_.gap;
    const float below = std::max(
        0.0F, bounds().y + bounds().height -
            (anchor_.y + anchor_.height + gap));
    const float above = std::max(0.0F, anchor_.y - gap - bounds().y);
    float maximumHeight = std::max(below, above);
    if (options_.placement == PopupPlacement::BelowStart ||
        options_.placement == PopupPlacement::BelowEnd) {
        maximumHeight = below;
    } else if (options_.placement == PopupPlacement::AboveStart ||
        options_.placement == PopupPlacement::AboveEnd) {
        maximumHeight = above;
    } else if (options_.placement == PopupPlacement::Anchor) {
        maximumHeight = bounds().height;
    }
    Size measured = popup_->measure(LayoutConstraints::loose({
        bounds().width,
        maximumHeight,
    }));
    measured.width = std::min(bounds().width, nonNegative(measured.width));
    measured.height = std::min(maximumHeight, nonNegative(measured.height));
    if (options_.exactAnchorWidth) {
        measured.width = std::min(bounds().width, anchor_.width);
    } else if (options_.matchAnchorWidth) {
        measured.width = std::min(
            bounds().width, std::max(measured.width, anchor_.width));
    }
    if (options_.exactAnchorHeight) {
        measured.height = std::min(bounds().height, anchor_.height);
    }

    bool placeAbove = false;
    bool alignEnd = false;
    bool placeOverAnchor = false;
    switch (options_.placement) {
    case PopupPlacement::Auto:
        placeAbove = measured.height > below && above > below;
        break;
    case PopupPlacement::Anchor:
        placeOverAnchor = true;
        break;
    case PopupPlacement::BelowStart:
        break;
    case PopupPlacement::BelowEnd:
        alignEnd = true;
        break;
    case PopupPlacement::AboveStart:
        placeAbove = true;
        break;
    case PopupPlacement::AboveEnd:
        placeAbove = true;
        alignEnd = true;
        break;
    }

    float x = alignEnd
        ? anchor_.x + anchor_.width - measured.width
        : anchor_.x;
    x = std::clamp(
        x,
        bounds().x,
        bounds().x + std::max(0.0F, bounds().width - measured.width));
    float y = placeOverAnchor
        ? anchor_.y
        : (placeAbove
            ? anchor_.y - gap - measured.height
            : anchor_.y + anchor_.height + gap);
    y = std::clamp(
        y,
        bounds().y,
        bounds().y + std::max(0.0F, bounds().height - measured.height));
    popup_->arrange({x, y, measured.width, measured.height}, clip());
}

} // namespace lotui
