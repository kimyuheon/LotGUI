#include "widgets/dock_workspace.h"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <utility>

namespace lotui {

Widget& DockToolbar::addCommand(std::unique_ptr<Widget> command, float extent) {
    if (!command || extent <= 0.0F)
        throw std::invalid_argument("toolbar command and extent are required");
    Widget& result = *command;
    commands_.push_back({std::move(command), extent});
    if (hasArea(bounds())) onArrange();
    return result;
}

std::size_t DockToolbar::commandCount() const noexcept {
    return commands_.size();
}

Widget& DockToolbar::commandAt(std::size_t index) {
    return *commands_.at(index).widget;
}

const Widget& DockToolbar::commandAt(std::size_t index) const {
    return *commands_.at(index).widget;
}

Rect DockToolbar::gripBounds() const noexcept {
    const Rect area = bounds();
    return vertical_
        ? Rect{area.x, area.y, area.width, std::min(18.0F, area.height)}
        : Rect{area.x, area.y, std::min(18.0F, area.width), area.height};
}

bool DockToolbar::vertical() const noexcept {
    return vertical_;
}

Size DockToolbar::measure(const LayoutConstraints& constraints) const {
    float extent = 22.0F;
    for (const auto& command : commands_) extent += command.extent + 3.0F;
    return constraints.constrain(vertical_
        ? Size{style_.verticalThickness, extent}
        : Size{extent, style_.horizontalThickness});
}

void DockToolbar::setVertical(bool vertical) noexcept {
    vertical_ = vertical;
}

void DockToolbar::setStyle(const DockWorkspaceStyle& style) noexcept {
    style_ = style;
}

void DockToolbar::setDragHandler(
    std::function<void(Point, bool)> handler) {
    onDrag_ = std::move(handler);
}

void DockToolbar::onArrange() {
    const Rect area = bounds();
    float cursor = vertical_ ? area.y + gripBounds().height + 3.0F
                             : area.x + gripBounds().width + 3.0F;
    for (const auto& command : commands_) {
        const float available = vertical_
            ? std::max(0.0F, area.y + area.height - cursor - 3.0F)
            : std::max(0.0F, area.x + area.width - cursor - 3.0F);
        const float extent = std::min(command.extent, available);
        const Rect commandBounds = vertical_
            ? Rect{area.x + 3.0F, cursor,
                std::max(0.0F, area.width - 6.0F), extent}
            : Rect{cursor, area.y + 3.0F, extent,
                std::max(0.0F, area.height - 6.0F)};
        command.widget->arrange(commandBounds, clip());
        cursor += extent + 3.0F;
    }
}

void DockToolbar::onPaint(std::vector<PaintCommand>& commands) const {
    commands.push_back({bounds(), clip(), style_.toolbarBackground,
        invalidTextureId, 0.0F});
    const Rect grip = gripBounds();
    for (int index = 0; index < 3; ++index) {
        const float offset = (static_cast<float>(index) - 1.0F) * 4.0F;
        const Rect dot = vertical_
            ? Rect{grip.x + grip.width * 0.5F + offset - 1.0F,
                grip.y + grip.height * 0.5F - 1.0F, 2.0F, 2.0F}
            : Rect{grip.x + grip.width * 0.5F - 1.0F,
                grip.y + grip.height * 0.5F + offset - 1.0F, 2.0F, 2.0F};
        commands.push_back({dot, clip(), style_.gripColor,
            invalidTextureId, 1.0F});
    }
}

void DockToolbar::paintChildren(std::vector<PaintCommand>& commands) const {
    for (const auto& command : commands_) command.widget->paint(commands);
}

void DockToolbar::collectChildHitTestEntries(
    std::vector<HitTestEntry>& entries) const {
    for (const auto& command : commands_)
        command.widget->collectHitTestEntries(entries);
}

void DockToolbar::collectChildFocusTargets(
    std::vector<PointerTargetId>& targets) const {
    for (const auto& command : commands_)
        command.widget->collectFocusTargets(targets);
}

Widget* DockToolbar::findChildByPointerTarget(
    PointerTargetId target) noexcept {
    for (const auto& command : commands_)
        if (Widget* child = command.widget->findByPointerTarget(target))
            return child;
    return nullptr;
}

bool DockToolbar::acceptsPointerEvents() const noexcept {
    return true;
}

bool DockToolbar::onPointerEvent(const WidgetPointerEvent& event) {
    if (event.type == WidgetPointerEventType::Press &&
            event.button == PointerButton::Primary &&
            contains(gripBounds(), event.position)) {
        dragging_ = true;
        return true;
    }
    if (event.type == WidgetPointerEventType::Move && dragging_) {
        if (onDrag_) onDrag_(event.position, false);
        return true;
    }
    if (event.type == WidgetPointerEventType::Release && dragging_) {
        dragging_ = false;
        if (onDrag_) onDrag_(event.position, true);
        return true;
    }
    if (event.type == WidgetPointerEventType::Cancel && dragging_) {
        dragging_ = false;
        if (onDrag_) onDrag_({}, true);
        return true;
    }
    return false;
}

DockWorkspace::DockWorkspace(std::unique_ptr<Widget> content,
        std::unique_ptr<DockToolbar> toolbar, DockSide side,
        DockWorkspaceStyle style)
    : content_(std::move(content)), toolbar_(std::move(toolbar)),
      side_(side), style_(style) {
    if (!content_ || !toolbar_)
        throw std::invalid_argument("dock content and toolbar are required");
    style_.horizontalThickness = std::max(1.0F, style_.horizontalThickness);
    style_.verticalThickness = std::max(1.0F, style_.verticalThickness);
    style_.dropZone = std::max(1.0F, style_.dropZone);
    toolbar_->setStyle(style_);
    toolbar_->setDragHandler([this](Point point, bool released) {
        onToolbarDrag(point, released);
    });
}

Size DockWorkspace::measure(const LayoutConstraints& constraints) const {
    const Size preferred = content_->measure(constraints);
    return constraints.constrain({
        preferred.width + style_.verticalThickness,
        preferred.height + style_.horizontalThickness});
}

void DockWorkspace::setSide(DockSide side) {
    side_ = side;
    preview_.reset();
    if (hasArea(bounds())) onArrange();
}

DockSide DockWorkspace::side() const noexcept { return side_; }

std::optional<DockSide> DockWorkspace::previewSide() const noexcept {
    return preview_;
}

DockToolbar& DockWorkspace::toolbar() noexcept { return *toolbar_; }
const DockToolbar& DockWorkspace::toolbar() const noexcept {
    return *toolbar_;
}
Widget& DockWorkspace::content() noexcept { return *content_; }
const Widget& DockWorkspace::content() const noexcept { return *content_; }

Rect DockWorkspace::dockRect(DockSide side) const noexcept {
    const Rect area = bounds();
    if (side == DockSide::Top || side == DockSide::Bottom) {
        const float thickness = std::min(style_.horizontalThickness,
            std::max(0.0F, area.height - style_.minimumContentHeight));
        return {area.x,
            side == DockSide::Top ? area.y : area.y + area.height - thickness,
            area.width, thickness};
    }
    const float thickness = std::min(style_.verticalThickness,
        std::max(0.0F, area.width - style_.minimumContentWidth));
    return {side == DockSide::Left ? area.x
            : area.x + area.width - thickness,
        area.y, thickness, area.height};
}

void DockWorkspace::onArrange() {
    const Rect area = bounds();
    const Rect bar = dockRect(side_);
    Rect content = area;
    if (side_ == DockSide::Top) {
        content.y += bar.height;
        content.height -= bar.height;
    } else if (side_ == DockSide::Bottom) {
        content.height -= bar.height;
    } else if (side_ == DockSide::Left) {
        content.x += bar.width;
        content.width -= bar.width;
    } else {
        content.width -= bar.width;
    }
    content_->arrange(content, clip());
    toolbar_->setVertical(side_ == DockSide::Left || side_ == DockSide::Right);
    toolbar_->arrange(bar, clip());
}

void DockWorkspace::paintChildren(
        std::vector<PaintCommand>& commands) const {
    content_->paint(commands);
    toolbar_->paint(commands);
    if (preview_) {
        commands.push_back({dockRect(*preview_), clip(),
            style_.dockPreview, invalidTextureId, 0.0F});
    }
}

void DockWorkspace::collectChildHitTestEntries(
        std::vector<HitTestEntry>& entries) const {
    content_->collectHitTestEntries(entries);
    toolbar_->collectHitTestEntries(entries);
}

void DockWorkspace::collectChildFocusTargets(
        std::vector<PointerTargetId>& targets) const {
    content_->collectFocusTargets(targets);
    toolbar_->collectFocusTargets(targets);
}

Widget* DockWorkspace::findChildByPointerTarget(
        PointerTargetId target) noexcept {
    if (Widget* child = toolbar_->findByPointerTarget(target)) return child;
    return content_->findByPointerTarget(target);
}

std::optional<DockSide> DockWorkspace::targetAt(Point point) const noexcept {
    const Rect area = bounds();
    if (!contains(area, point)) return std::nullopt;
    const std::array<std::pair<float, DockSide>, 4> distances{{
        {point.y - area.y, DockSide::Top},
        {area.y + area.height - point.y, DockSide::Bottom},
        {point.x - area.x, DockSide::Left},
        {area.x + area.width - point.x, DockSide::Right},
    }};
    const auto nearest = std::min_element(distances.begin(), distances.end(),
        [](const auto& a, const auto& b) { return a.first < b.first; });
    return nearest->first <= style_.dropZone
        ? std::optional<DockSide>{nearest->second} : std::nullopt;
}

void DockWorkspace::onToolbarDrag(Point point, bool released) {
    const auto target = targetAt(point);
    if (released) {
        if (target) setSide(*target);
        preview_.reset();
    } else {
        preview_ = target;
    }
}

} // namespace lotui
