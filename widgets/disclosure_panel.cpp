#include "widgets/disclosure_panel.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace lotui {
namespace {

float dimension(float value) noexcept {
    return std::isfinite(value) ? std::max(0.0F, value) : 0.0F;
}

void sanitize(DisclosurePanelStyle& style) noexcept {
    style.headerHeight = dimension(style.headerHeight);
    style.headerPadding = dimension(style.headerPadding);
    style.indicatorSize = dimension(style.indicatorSize);
    style.labelSpacing = dimension(style.labelSpacing);
    style.contentIndent = dimension(style.contentIndent);
    style.contentSpacing = dimension(style.contentSpacing);
}

float remaining(float value, float used) noexcept {
    return std::isfinite(value)
        ? std::max(0.0F, value - used)
        : unboundedLayoutSize;
}

} // namespace

DisclosurePanel::DisclosurePanel(
    std::shared_ptr<const TextEngine> textEngine,
    std::string title,
    std::unique_ptr<Widget> content,
    bool expanded,
    ChangedHandler onChanged,
    DisclosurePanelStyle style,
    TextStyle titleStyle)
    : SingleChildWidget(std::move(content)),
      titleLabel_(std::move(textEngine), std::move(title),
          std::move(titleStyle)),
      onChanged_(std::move(onChanged)),
      style_(style),
      expanded_(expanded) {
    sanitize(style_);
    titleLabel_.setVerticalAlignment(VerticalTextAlignment::Center);
}

Size DisclosurePanel::measure(
    const LayoutConstraints& constraints) const {
    const float titleOffset = style_.headerPadding +
        style_.indicatorSize + style_.labelSpacing;
    const Size titleSize = titleLabel_.measure(LayoutConstraints::loose({
        remaining(constraints.maximum.width,
            titleOffset + style_.headerPadding),
        style_.headerHeight,
    }));
    Size desired{
        titleOffset + titleSize.width + style_.headerPadding,
        style_.headerHeight,
    };
    if (expanded_ && child()) {
        const Size childSize = child()->measure(LayoutConstraints::loose({
            remaining(constraints.maximum.width, style_.contentIndent),
            remaining(constraints.maximum.height,
                style_.headerHeight + style_.contentSpacing),
        }));
        desired.width = std::max(
            desired.width, style_.contentIndent + childSize.width);
        desired.height += style_.contentSpacing + childSize.height;
    }
    return constraints.constrain(desired);
}

void DisclosurePanel::setTitle(std::string title) {
    titleLabel_.setText(std::move(title));
}

const std::string& DisclosurePanel::title() const noexcept {
    return titleLabel_.text();
}

void DisclosurePanel::setContent(std::unique_ptr<Widget> content) {
    setChild(std::move(content));
    if (hasArea(bounds())) onArrange();
}

Widget* DisclosurePanel::content() noexcept {
    return child();
}

const Widget* DisclosurePanel::content() const noexcept {
    return child();
}

void DisclosurePanel::setExpanded(bool expanded) {
    if (expanded_ == expanded) return;
    expanded_ = expanded;
    pressed_ = false;
    keyboardPressed_ = false;
    if (hasArea(bounds())) onArrange();
}

bool DisclosurePanel::isExpanded() const noexcept {
    return expanded_;
}

void DisclosurePanel::toggle() {
    if (enabled_) activate();
}

void DisclosurePanel::setEnabled(bool enabled) noexcept {
    enabled_ = enabled;
    if (!enabled_) {
        hovered_ = false;
        pressed_ = false;
        focused_ = false;
        keyboardPressed_ = false;
    }
}

bool DisclosurePanel::isEnabled() const noexcept {
    return enabled_;
}

void DisclosurePanel::setOnChanged(ChangedHandler onChanged) {
    onChanged_ = std::move(onChanged);
}

void DisclosurePanel::setStyle(DisclosurePanelStyle style) noexcept {
    sanitize(style);
    style_ = style;
    if (hasArea(bounds())) onArrange();
}

const DisclosurePanelStyle& DisclosurePanel::style() const noexcept {
    return style_;
}

void DisclosurePanel::onArrange() {
    const Rect header = headerBounds();
    const float titleX = header.x + style_.headerPadding +
        style_.indicatorSize + style_.labelSpacing;
    titleLabel_.arrange({
        titleX, header.y,
        std::max(0.0F, header.x + header.width -
            titleX - style_.headerPadding),
        header.height,
    }, header);

    if (!child()) return;
    if (!expanded_) {
        child()->arrange({}, {});
        return;
    }
    const float contentY = header.y + header.height +
        style_.contentSpacing;
    const Size maximum{
        remaining(bounds().width, style_.contentIndent),
        remaining(bounds().y + bounds().height, contentY),
    };
    const Size size = child()->measure(LayoutConstraints::loose(maximum));
    child()->arrange({
        bounds().x + style_.contentIndent,
        contentY,
        maximum.width,
        std::min(size.height, maximum.height),
    }, clip());
}

void DisclosurePanel::onPaint(
    std::vector<PaintCommand>& commands) const {
    const Rect header = headerBounds();
    if (!hasArea(header)) return;
    const Color background = pressed_ || keyboardPressed_
        ? style_.pressed : hovered_ ? style_.hovered : style_.normal;
    commands.push_back({
        header, clip(), background, invalidTextureId, 0.0F});

    const float step = style_.indicatorSize * 0.25F;
    const float iconX = header.x + style_.headerPadding;
    const float iconY = header.y +
        (header.height - style_.indicatorSize) * 0.5F;
    for (int index = 0; index < 4; ++index) {
        const float offset = static_cast<float>(index) * step;
        const Rect bar = expanded_
            ? Rect{iconX + offset * 0.5F, iconY + offset,
                style_.indicatorSize - offset, step}
            : Rect{iconX + offset, iconY + offset * 0.5F,
                step, style_.indicatorSize - offset};
        commands.push_back({
            bar, clip(), style_.indicator, invalidTextureId, 0.0F});
    }
    titleLabel_.paint(commands);
    if (focused_) {
        commands.push_back({
            {header.x, header.y + header.height - 1.0F,
                header.width, 1.0F},
            clip(), style_.focusLine, invalidTextureId, 0.0F});
    }
}

void DisclosurePanel::paintChildren(
    std::vector<PaintCommand>& commands) const {
    if (expanded_) SingleChildWidget::paintChildren(commands);
}

void DisclosurePanel::collectChildHitTestEntries(
    std::vector<HitTestEntry>& entries) const {
    if (expanded_ && enabled_) {
        SingleChildWidget::collectChildHitTestEntries(entries);
    }
}

void DisclosurePanel::collectChildFocusTargets(
    std::vector<PointerTargetId>& targets) const {
    if (expanded_ && enabled_) {
        SingleChildWidget::collectChildFocusTargets(targets);
    }
}

Widget* DisclosurePanel::findChildByPointerTarget(
    PointerTargetId target) noexcept {
    return expanded_ && enabled_
        ? SingleChildWidget::findChildByPointerTarget(target) : nullptr;
}

bool DisclosurePanel::acceptsPointerEvents() const noexcept {
    return enabled_;
}

bool DisclosurePanel::onPointerEvent(
    const WidgetPointerEvent& event) {
    if (!enabled_) return false;
    const bool inHeader = contains(headerBounds(), event.position);
    switch (event.type) {
    case WidgetPointerEventType::Enter:
    case WidgetPointerEventType::Move:
        hovered_ = inHeader;
        return true;
    case WidgetPointerEventType::Leave:
        hovered_ = false;
        return true;
    case WidgetPointerEventType::Press:
        if (event.button != PointerButton::Primary || !inHeader) return false;
        pressed_ = true;
        hovered_ = true;
        return true;
    case WidgetPointerEventType::Release:
        if (event.button != PointerButton::Primary || !pressed_) return false;
        pressed_ = false;
        hovered_ = inHeader;
        if (inHeader) activate();
        return true;
    case WidgetPointerEventType::Cancel:
        pressed_ = false;
        hovered_ = false;
        return true;
    }
    return false;
}

bool DisclosurePanel::acceptsFocus() const noexcept {
    return enabled_;
}

bool DisclosurePanel::onFocusChanged(bool focused) {
    const bool changed = focused_ != focused ||
        (!focused && keyboardPressed_);
    focused_ = focused;
    if (!focused) keyboardPressed_ = false;
    return changed;
}

bool DisclosurePanel::onKeyEvent(const WidgetKeyEvent& event) {
    if (!enabled_) return false;
    if (event.type == WidgetKeyEventType::Cancel) {
        const bool changed = keyboardPressed_;
        keyboardPressed_ = false;
        return changed;
    }
    if (event.key != KeyCode::Space && event.key != KeyCode::Enter &&
        event.key != KeyCode::Left && event.key != KeyCode::Right) {
        return false;
    }
    if (event.type == WidgetKeyEventType::Press) {
        if (event.key == KeyCode::Left && expanded_) {
            activate();
        } else if (event.key == KeyCode::Right && !expanded_) {
            activate();
        } else if ((event.key == KeyCode::Space ||
            event.key == KeyCode::Enter) && !event.repeat) {
            keyboardPressed_ = true;
        }
        return true;
    }
    if (event.type == WidgetKeyEventType::Release && keyboardPressed_ &&
        (event.key == KeyCode::Space || event.key == KeyCode::Enter)) {
        keyboardPressed_ = false;
        activate();
        return true;
    }
    return true;
}

Rect DisclosurePanel::headerBounds() const noexcept {
    return {bounds().x, bounds().y, bounds().width,
        std::min(bounds().height, style_.headerHeight)};
}

void DisclosurePanel::activate() {
    setExpanded(!expanded_);
    if (onChanged_) {
        ChangedHandler callback = onChanged_;
        callback(expanded_);
    }
}

} // namespace lotui
