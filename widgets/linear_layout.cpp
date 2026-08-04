#include "widgets/linear_layout.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace lotui {
namespace {

float limited(float value, float limit) noexcept {
    return std::isfinite(limit) ? std::min(value, limit) : value;
}

LayoutConstraints childConstraints(
    const ChildLayout& slot,
    const LayoutConstraints& parent,
    const LinearLayoutOptions& options) noexcept {
    const float availableWidth = std::max(
        0.0F, parent.maximum.width - options.padding.horizontal());
    const float availableHeight = std::max(
        0.0F, parent.maximum.height - options.padding.vertical());
    const float maximumWidth = std::max(
        slot.minimum.width,
        limited(slot.maximum.width, availableWidth));
    const float maximumHeight = std::max(
        slot.minimum.height,
        limited(slot.maximum.height, availableHeight));
    return {
        slot.minimum,
        {maximumWidth, maximumHeight},
    };
}

} // namespace

LinearLayout::LinearLayout(bool horizontal)
    : horizontal_(horizontal) {
}

Widget& LinearLayout::addChild(
    std::unique_ptr<Widget> child,
    ChildLayout layout) {
    if (!child) {
        throw std::invalid_argument("layout child must not be null");
    }
    Widget& result = *child;
    children_.push_back({std::move(child), layout});
    return result;
}

std::size_t LinearLayout::childCount() const noexcept {
    return children_.size();
}

Widget& LinearLayout::childAt(std::size_t index) {
    return *children_.at(index).widget;
}

const Widget& LinearLayout::childAt(std::size_t index) const {
    return *children_.at(index).widget;
}

void LinearLayout::setOptions(LinearLayoutOptions options) {
    options_ = options;
}

const LinearLayoutOptions& LinearLayout::options() const noexcept {
    return options_;
}

void LinearLayout::setDecoration(
    std::optional<BoxDecoration> decoration) noexcept {
    if (decoration) {
        decoration->cornerRadius = std::max(
            0.0F, decoration->cornerRadius);
    }
    decoration_ = decoration;
}

const std::optional<BoxDecoration>& LinearLayout::decoration() const noexcept {
    return decoration_;
}

Size LinearLayout::measure(const LayoutConstraints& constraints) const {
    return performLayout(constraints).size;
}

void LinearLayout::onArrange() {
    const LayoutResult result = performLayout(
        LayoutConstraints::tight({bounds().width, bounds().height}));
    for (std::size_t index = 0; index < children_.size(); ++index) {
        children_[index].widget->arrange(
            translated(result.children[index], {bounds().x, bounds().y}),
            clip());
    }
}

void LinearLayout::onPaint(
    std::vector<PaintCommand>& commands) const {
    if (decoration_) {
        commands.push_back({
            bounds(), clip(), decoration_->color, invalidTextureId,
            decoration_->cornerRadius});
    }
}

void LinearLayout::paintChildren(
    std::vector<PaintCommand>& commands) const {
    for (const Slot& child : children_) {
        child.widget->paint(commands);
    }
}

void LinearLayout::collectChildHitTestEntries(
    std::vector<HitTestEntry>& entries) const {
    for (const Slot& child : children_) {
        child.widget->collectHitTestEntries(entries);
    }
}

Widget* LinearLayout::findChildByPointerTarget(
    PointerTargetId target) noexcept {
    for (const Slot& child : children_) {
        if (Widget* result = child.widget->findByPointerTarget(target)) {
            return result;
        }
    }
    return nullptr;
}

std::vector<LayoutItem> LinearLayout::makeLayoutItems(
    const LayoutConstraints& constraints) const {
    std::vector<LayoutItem> items;
    items.reserve(children_.size());
    for (const Slot& child : children_) {
        const Size preferred = child.widget->measure(
            childConstraints(child.layout, constraints, options_));
        items.push_back({
            preferred,
            child.layout.minimum,
            child.layout.maximum,
            child.layout.flex,
        });
    }
    return items;
}

LayoutResult LinearLayout::performLayout(
    const LayoutConstraints& constraints) const {
    const std::vector<LayoutItem> items = makeLayoutItems(constraints);
    return horizontal_
        ? layoutRow(constraints, items, options_)
        : layoutColumn(constraints, items, options_);
}

Row::Row()
    : LinearLayout(true) {
}

Column::Column()
    : LinearLayout(false) {
}

} // namespace lotui
