#include "widgets/checkbox.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace lotui {
namespace {

float dimension(float value) noexcept {
    return std::isfinite(value) ? std::max(0.0F, value) : 0.0F;
}

void sanitize(CheckboxStyle& style) noexcept {
    style.boxSize = dimension(style.boxSize);
    style.spacing = dimension(style.spacing);
    style.cornerRadius = dimension(style.cornerRadius);
    style.focusRingWidth = dimension(style.focusRingWidth);
}

} // namespace

Checkbox::Checkbox(
    bool checked,
    ChangedHandler onChanged,
    CheckboxStyle style)
    : checked_(checked),
      onChanged_(std::move(onChanged)),
      style_(style) {
    sanitize(style_);
}

Checkbox::Checkbox(
    std::unique_ptr<Widget> content,
    bool checked,
    ChangedHandler onChanged,
    CheckboxStyle style)
    : SingleChildWidget(std::move(content)),
      checked_(checked),
      onChanged_(std::move(onChanged)),
      style_(style) {
    sanitize(style_);
}

Size Checkbox::measure(const LayoutConstraints& constraints) const {
    Size desired{style_.boxSize, style_.boxSize};
    if (const Widget* contentWidget = child()) {
        const float availableWidth = std::isfinite(constraints.maximum.width)
            ? std::max(
                0.0F,
                constraints.maximum.width - style_.boxSize - style_.spacing)
            : unboundedLayoutSize;
        const Size contentSize = contentWidget->measure(
            LayoutConstraints::loose(
                {availableWidth, constraints.maximum.height}));
        desired.width += style_.spacing + contentSize.width;
        desired.height = std::max(desired.height, contentSize.height);
    }
    return constraints.constrain(desired);
}

void Checkbox::setChecked(bool checked) noexcept {
    checked_ = checked;
}

bool Checkbox::isChecked() const noexcept {
    return checked_;
}

void Checkbox::toggle() {
    if (enabled_) {
        activate();
    }
}

void Checkbox::setEnabled(bool enabled) noexcept {
    enabled_ = enabled;
    if (!enabled_) {
        hovered_ = false;
        pressed_ = false;
        focused_ = false;
        keyboardPressed_ = false;
    }
}

bool Checkbox::isEnabled() const noexcept {
    return enabled_;
}

bool Checkbox::isHovered() const noexcept {
    return hovered_;
}

bool Checkbox::isPressed() const noexcept {
    return pressed_ || keyboardPressed_;
}

bool Checkbox::isFocused() const noexcept {
    return focused_;
}

void Checkbox::setOnChanged(ChangedHandler onChanged) {
    onChanged_ = std::move(onChanged);
}

void Checkbox::setContent(std::unique_ptr<Widget> content) {
    setChild(std::move(content));
}

Widget* Checkbox::content() noexcept {
    return child();
}

const Widget* Checkbox::content() const noexcept {
    return child();
}

void Checkbox::setStyle(CheckboxStyle style) noexcept {
    sanitize(style);
    style_ = style;
}

const CheckboxStyle& Checkbox::style() const noexcept {
    return style_;
}

void Checkbox::onArrange() {
    Widget* contentWidget = child();
    if (!contentWidget) {
        return;
    }
    const float contentX = bounds().x + style_.boxSize + style_.spacing;
    const float availableWidth = std::max(
        0.0F, bounds().width - style_.boxSize - style_.spacing);
    const Size contentSize = contentWidget->measure(
        LayoutConstraints::loose({availableWidth, bounds().height}));
    contentWidget->arrange(
        {
            contentX,
            bounds().y +
                std::max(0.0F, bounds().height - contentSize.height) * 0.5F,
            contentSize.width,
            contentSize.height,
        },
        clip());
}

void Checkbox::onPaint(std::vector<PaintCommand>& commands) const {
    Rect box = checkboxBounds();
    const float ringWidth = std::min(
        style_.focusRingWidth,
        std::min(box.width, box.height) * 0.5F);
    if (focused_ && ringWidth > 0.0F) {
        commands.push_back({
            box, clip(), style_.focusRing, invalidTextureId,
            style_.cornerRadius});
        box = {
            box.x + ringWidth,
            box.y + ringWidth,
            std::max(0.0F, box.width - ringWidth * 2.0F),
            std::max(0.0F, box.height - ringWidth * 2.0F),
        };
    }

    Color background = style_.unchecked;
    if (!enabled_) {
        background = style_.disabled;
    } else if (checked_) {
        background = style_.checked;
    } else if (hovered_ || pressed_ || keyboardPressed_) {
        background = style_.hovered;
    }
    commands.push_back({
        box, clip(), background, invalidTextureId,
        std::max(0.0F, style_.cornerRadius - ringWidth)});

    if (checked_) {
        const float inset = std::min(box.width, box.height) * 0.28F;
        commands.push_back({
            {
                box.x + inset,
                box.y + inset,
                std::max(0.0F, box.width - inset * 2.0F),
                std::max(0.0F, box.height - inset * 2.0F),
            },
            clip(),
            style_.checkmark,
            invalidTextureId,
            std::max(0.0F, style_.cornerRadius * 0.35F),
        });
    }
}

bool Checkbox::acceptsPointerEvents() const noexcept {
    return enabled_;
}

bool Checkbox::onPointerEvent(const WidgetPointerEvent& event) {
    if (!enabled_) {
        return false;
    }
    switch (event.type) {
    case WidgetPointerEventType::Enter:
        hovered_ = true;
        return true;
    case WidgetPointerEventType::Leave:
        hovered_ = false;
        return true;
    case WidgetPointerEventType::Move: {
        const bool changed = hovered_ != event.inside;
        hovered_ = event.inside;
        return changed;
    }
    case WidgetPointerEventType::Press:
        if (event.button != PointerButton::Primary) {
            return false;
        }
        pressed_ = true;
        hovered_ = event.inside;
        return true;
    case WidgetPointerEventType::Release:
        if (event.button != PointerButton::Primary || !pressed_) {
            return false;
        }
        pressed_ = false;
        hovered_ = event.inside;
        if (event.inside) {
            activate();
        }
        return true;
    case WidgetPointerEventType::Cancel:
        if (!pressed_ && !hovered_) {
            return false;
        }
        pressed_ = false;
        hovered_ = false;
        return true;
    }
    return false;
}

bool Checkbox::acceptsFocus() const noexcept {
    return enabled_;
}

bool Checkbox::onFocusChanged(bool focused) {
    const bool changed = focused_ != focused ||
        (!focused && keyboardPressed_);
    focused_ = focused;
    if (!focused_) {
        keyboardPressed_ = false;
    }
    return changed;
}

bool Checkbox::onKeyEvent(const WidgetKeyEvent& event) {
    if (!enabled_) {
        return false;
    }
    if (event.type == WidgetKeyEventType::Cancel) {
        const bool changed = keyboardPressed_;
        keyboardPressed_ = false;
        return changed;
    }
    if (event.key != KeyCode::Space && event.key != KeyCode::Enter) {
        return false;
    }
    if (event.type == WidgetKeyEventType::Press) {
        if (!event.repeat) {
            keyboardPressed_ = true;
        }
        return true;
    }
    if (event.type == WidgetKeyEventType::Release && keyboardPressed_) {
        keyboardPressed_ = false;
        activate();
        return true;
    }
    return false;
}

void Checkbox::activate() {
    checked_ = !checked_;
    if (onChanged_) {
        ChangedHandler callback = onChanged_;
        callback(checked_);
    }
}

Rect Checkbox::checkboxBounds() const noexcept {
    const float size = std::min(
        style_.boxSize,
        std::min(bounds().width, bounds().height));
    return {
        bounds().x,
        bounds().y + std::max(0.0F, bounds().height - size) * 0.5F,
        size,
        size,
    };
}

} // namespace lotui
