#include "widgets/dialog.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace lotui {
namespace {

float nonNegative(float value) noexcept {
    return std::isfinite(value) ? std::max(0.0F, value) : 0.0F;
}

EdgeInsets normalized(EdgeInsets value) noexcept {
    value.left = nonNegative(value.left);
    value.top = nonNegative(value.top);
    value.right = nonNegative(value.right);
    value.bottom = nonNegative(value.bottom);
    return value;
}

Size contentMaximum(Size maximum, const EdgeInsets& padding) noexcept {
    return {
        std::max(0.0F, maximum.width - padding.horizontal()),
        std::max(0.0F, maximum.height - padding.vertical()),
    };
}

} // namespace

Dialog::Dialog(
    std::unique_ptr<Widget> content,
    Size preferredSize,
    DialogStyle style)
    : SingleChildWidget(std::move(content)),
      preferredSize_(preferredSize),
      style_(style) {
    setPreferredSize(preferredSize_);
    setStyle(style_);
}

Size Dialog::measure(const LayoutConstraints& constraints) const {
    Size desired = preferredSize_;
    if (child()) {
        Size childLimit = contentMaximum(constraints.maximum,
            style_.contentPadding);
        if (preferredSize_.width > 0.0F) {
            childLimit.width = std::min(
                childLimit.width,
                std::max(0.0F, preferredSize_.width -
                    style_.contentPadding.horizontal()));
        }
        if (preferredSize_.height > 0.0F) {
            childLimit.height = std::min(
                childLimit.height,
                std::max(0.0F, preferredSize_.height -
                    style_.contentPadding.vertical()));
        }
        const Size childSize = child()->measure(
            LayoutConstraints::loose(childLimit));
        desired.width = std::max(
            desired.width,
            childSize.width + style_.contentPadding.horizontal());
        desired.height = std::max(
            desired.height,
            childSize.height + style_.contentPadding.vertical());
    }
    return constraints.constrain(desired);
}

void Dialog::setContent(std::unique_ptr<Widget> content) {
    setChild(std::move(content));
}

std::unique_ptr<Widget> Dialog::takeContent() noexcept {
    return takeChild();
}

Widget* Dialog::content() noexcept {
    return child();
}

const Widget* Dialog::content() const noexcept {
    return child();
}

void Dialog::setPreferredSize(Size preferredSize) noexcept {
    preferredSize_.width = nonNegative(preferredSize.width);
    preferredSize_.height = nonNegative(preferredSize.height);
}

Size Dialog::preferredSize() const noexcept {
    return preferredSize_;
}

void Dialog::setStyle(DialogStyle style) noexcept {
    style.cornerRadius = nonNegative(style.cornerRadius);
    style.contentPadding = normalized(style.contentPadding);
    style_ = style;
}

const DialogStyle& Dialog::style() const noexcept {
    return style_;
}

void Dialog::onArrange() {
    if (!child()) {
        return;
    }
    const EdgeInsets& padding = style_.contentPadding;
    child()->arrange({
        bounds().x + padding.left,
        bounds().y + padding.top,
        std::max(0.0F, bounds().width - padding.horizontal()),
        std::max(0.0F, bounds().height - padding.vertical()),
    }, clip());
}

void Dialog::onPaint(std::vector<PaintCommand>& commands) const {
    commands.push_back({
        bounds(), clip(), style_.background, invalidTextureId,
        style_.cornerRadius});
}

DialogHost::DialogHost(
    std::unique_ptr<Widget> content,
    DialogHostStyle style)
    : content_(std::move(content)), style_(style) {
    if (!content_) {
        throw std::invalid_argument("dialog host content must not be null");
    }
    setStyle(style_);
}

Size DialogHost::measure(const LayoutConstraints& constraints) const {
    return content_->measure(constraints);
}

void DialogHost::setContent(std::unique_ptr<Widget> content) {
    if (!content) {
        throw std::invalid_argument("dialog host content must not be null");
    }
    content_ = std::move(content);
    if (hasArea(bounds())) {
        content_->arrange(bounds(), clip());
    }
}

Widget* DialogHost::content() noexcept {
    return content_.get();
}

const Widget* DialogHost::content() const noexcept {
    return content_.get();
}

Widget& DialogHost::showModal(
    std::unique_ptr<Widget> modal,
    DialogClosedHandler onClosed) {
    if (!modal) {
        throw std::invalid_argument("dialog host modal must not be null");
    }
    if (modal_) {
        throw std::logic_error("dialog host already has an active modal");
    }
    modal_ = std::move(modal);
    onModalClosed_ = std::move(onClosed);
    arrangeModal();
    return *modal_;
}

void DialogHost::acceptModal() {
    closeModal(DialogResult::Accepted);
}

void DialogHost::cancelModal() {
    closeModal(DialogResult::Cancelled);
}

void DialogHost::dismissModal() {
    closeModal(DialogResult::Dismissed);
}

std::unique_ptr<Widget> DialogHost::takeModal() noexcept {
    onModalClosed_ = {};
    return std::move(modal_);
}

Widget* DialogHost::modal() noexcept {
    return modal_.get();
}

const Widget* DialogHost::modal() const noexcept {
    return modal_.get();
}

bool DialogHost::hasModal() const noexcept {
    return modal_ != nullptr;
}

void DialogHost::setStyle(DialogHostStyle style) noexcept {
    style.modalMargin = normalized(style.modalMargin);
    style_ = style;
    arrangeModal();
}

const DialogHostStyle& DialogHost::style() const noexcept {
    return style_;
}

void DialogHost::onArrange() {
    content_->arrange(bounds(), clip());
    arrangeModal();
}

void DialogHost::onPaint(std::vector<PaintCommand>& commands) const {
    (void)commands;
}

void DialogHost::paintChildren(
    std::vector<PaintCommand>& commands) const {
    dismissedModal_.reset();
    content_->paint(commands);
    if (modal_) {
        commands.push_back({
            bounds(), clip(), style_.scrim, invalidTextureId, 0.0F});
        modal_->paint(commands);
    }
}

void DialogHost::collectChildHitTestEntries(
    std::vector<HitTestEntry>& entries) const {
    if (modal_) {
        modal_->collectHitTestEntries(entries);
    } else {
        content_->collectHitTestEntries(entries);
    }
}

void DialogHost::collectChildFocusTargets(
    std::vector<PointerTargetId>& targets) const {
    if (modal_) {
        modal_->collectFocusTargets(targets);
    } else {
        content_->collectFocusTargets(targets);
    }
}

Widget* DialogHost::findChildByPointerTarget(
    PointerTargetId target) noexcept {
    if (modal_) {
        if (Widget* result = modal_->findByPointerTarget(target)) {
            return result;
        }
    }
    if (Widget* result = content_->findByPointerTarget(target)) {
        return result;
    }
    return dismissedModal_
        ? dismissedModal_->findByPointerTarget(target)
        : nullptr;
}

bool DialogHost::acceptsPointerEvents() const noexcept {
    return modal_ != nullptr;
}

bool DialogHost::onPointerEvent(const WidgetPointerEvent&) {
    return modal_ != nullptr;
}

bool DialogHost::onPreviewKeyEvent(const WidgetKeyEvent& event) {
    if (!modal_ || !style_.cancelOnEscape ||
        event.type != WidgetKeyEventType::Press ||
        event.key != KeyCode::Escape) {
        return false;
    }
    cancelModal();
    return true;
}

PointerTargetId DialogHost::activeFocusScopeTarget() const noexcept {
    return modal_ ? modal_->pointerTargetId() : content_->pointerTargetId();
}

void DialogHost::arrangeModal() {
    if (!modal_ || !hasArea(bounds())) {
        return;
    }
    const EdgeInsets& margin = style_.modalMargin;
    const Size available{
        std::max(0.0F, bounds().width - margin.horizontal()),
        std::max(0.0F, bounds().height - margin.vertical()),
    };
    const Size modalSize = modal_->measure(
        LayoutConstraints::loose(available));
    const Rect modalBounds{
        bounds().x + (bounds().width - modalSize.width) * 0.5F,
        bounds().y + (bounds().height - modalSize.height) * 0.5F,
        modalSize.width,
        modalSize.height,
    };
    modal_->arrange(modalBounds, clip());
}

void DialogHost::closeModal(DialogResult result) {
    if (!modal_) {
        return;
    }
    dismissedModal_ = std::move(modal_);
    DialogClosedHandler onClosed = std::move(onModalClosed_);
    onModalClosed_ = {};
    if (onClosed) {
        onClosed(result);
    }
}

} // namespace lotui
