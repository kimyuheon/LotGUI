#include "widgets/single_child_widget.h"

#include <utility>

namespace lotui {

SingleChildWidget::SingleChildWidget(std::unique_ptr<Widget> child)
    : child_(std::move(child)) {
}

void SingleChildWidget::setChild(std::unique_ptr<Widget> child) {
    child_ = std::move(child);
}

std::unique_ptr<Widget> SingleChildWidget::takeChild() noexcept {
    return std::move(child_);
}

Widget* SingleChildWidget::child() noexcept {
    return child_.get();
}

const Widget* SingleChildWidget::child() const noexcept {
    return child_.get();
}

void SingleChildWidget::paintChildren(
    std::vector<PaintCommand>& commands) const {
    if (child_) {
        child_->paint(commands);
    }
}

void SingleChildWidget::collectChildHitTestEntries(
    std::vector<HitTestEntry>& entries) const {
    if (child_) {
        child_->collectHitTestEntries(entries);
    }
}

Widget* SingleChildWidget::findChildByPointerTarget(
    PointerTargetId target) noexcept {
    return child_ ? child_->findByPointerTarget(target) : nullptr;
}

} // namespace lotui
