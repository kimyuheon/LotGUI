#include "widgets/button.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace lotui {
namespace {

float sanitizedDimension(float value) noexcept {
    return std::isfinite(value) ? std::max(0.0F, value) : 0.0F;
}

void sanitize(ButtonStyle& style) noexcept {
    style.cornerRadius = sanitizedDimension(style.cornerRadius);
    style.focusRingWidth = sanitizedDimension(style.focusRingWidth);
    style.contentPadding.left = sanitizedDimension(style.contentPadding.left);
    style.contentPadding.top = sanitizedDimension(style.contentPadding.top);
    style.contentPadding.right = sanitizedDimension(style.contentPadding.right);
    style.contentPadding.bottom = sanitizedDimension(style.contentPadding.bottom);
}

float available(float maximum, float padding) noexcept {
    return std::isfinite(maximum)
        ? std::max(0.0F, maximum - padding)
        : unboundedLayoutSize;
}

} // namespace

Button::Button(
    Size preferredSize,
    ClickHandler onClick,
    ButtonStyle style)
    : preferredSize_(preferredSize),
      onClick_(std::move(onClick)),
      style_(style) {
    sanitize(style_);
}

Button::Button(
    std::unique_ptr<Widget> content,
    Size minimumSize,
    ClickHandler onClick,
    ButtonStyle style)
    : SingleChildWidget(std::move(content)),
      preferredSize_(minimumSize),
      onClick_(std::move(onClick)),
      style_(style) {
    sanitize(style_);
}

Size Button::measure(const LayoutConstraints& constraints) const {
    Size desired = preferredSize_;
    if (const Widget* contentWidget = child()) {
        const LayoutConstraints contentConstraints{
            {},
            {
                available(
                    constraints.maximum.width,
                    style_.contentPadding.horizontal()),
                available(
                    constraints.maximum.height,
                    style_.contentPadding.vertical()),
            },
        };
        const Size contentSize = contentWidget->measure(contentConstraints);
        desired.width = std::max(
            desired.width,
            contentSize.width + style_.contentPadding.horizontal());
        desired.height = std::max(
            desired.height,
            contentSize.height + style_.contentPadding.vertical());
    }
    return constraints.constrain(desired);
}

void Button::setPreferredSize(Size preferredSize) noexcept {
    preferredSize_ = preferredSize;
}

Size Button::preferredSize() const noexcept {
    return preferredSize_;
}

void Button::setStyle(ButtonStyle style) noexcept {
    sanitize(style);
    style_ = style;
}

const ButtonStyle& Button::style() const noexcept {
    return style_;
}

void Button::setOnClick(ClickHandler onClick) {
    onClick_ = std::move(onClick);
}

void Button::setContent(std::unique_ptr<Widget> content) {
    setChild(std::move(content));
}

std::unique_ptr<Widget> Button::takeContent() noexcept {
    return takeChild();
}

Widget* Button::content() noexcept {
    return child();
}

const Widget* Button::content() const noexcept {
    return child();
}

void Button::setEnabled(bool enabled) noexcept {
    enabled_ = enabled;
    if (!enabled_) {
        hovered_ = false;
        pressed_ = false;
        focused_ = false;
        keyboardPressed_ = false;
        keyboardActivationKey_ = KeyCode::Unknown;
    }
}

bool Button::isEnabled() const noexcept {
    return enabled_;
}

bool Button::isHovered() const noexcept {
    return hovered_;
}

bool Button::isPressed() const noexcept {
    return pressed_ || keyboardPressed_;
}

bool Button::isFocused() const noexcept {
    return focused_;
}

void Button::onArrange() {
    Widget* contentWidget = child();
    if (!contentWidget) {
        return;
    }

    const float availableWidth = std::max(
        0.0F, bounds().width - style_.contentPadding.horizontal());
    const float availableHeight = std::max(
        0.0F, bounds().height - style_.contentPadding.vertical());
    const Size contentSize = contentWidget->measure(
        LayoutConstraints::loose({availableWidth, availableHeight}));
    contentWidget->arrange(
        {
            bounds().x + style_.contentPadding.left +
                std::max(0.0F, availableWidth - contentSize.width) * 0.5F,
            bounds().y + style_.contentPadding.top +
                std::max(0.0F, availableHeight - contentSize.height) * 0.5F,
            contentSize.width,
            contentSize.height,
        },
        clip());
}

void Button::onPaint(std::vector<PaintCommand>& commands) const {
    const float ringWidth = std::min(
        style_.focusRingWidth,
        std::min(bounds().width, bounds().height) * 0.5F);
    if (focused_ && ringWidth > 0.0F) {
        commands.push_back({
            bounds(), clip(), style_.focusRing, invalidTextureId,
            style_.cornerRadius});
        commands.push_back({
            {
                bounds().x + ringWidth,
                bounds().y + ringWidth,
                std::max(0.0F, bounds().width - ringWidth * 2.0F),
                std::max(0.0F, bounds().height - ringWidth * 2.0F),
            },
            clip(),
            currentColor(),
            invalidTextureId,
            std::max(0.0F, style_.cornerRadius - ringWidth),
        });
    } else {
        commands.push_back({
            bounds(), clip(), currentColor(), invalidTextureId,
            style_.cornerRadius});
    }
}

bool Button::acceptsPointerEvents() const noexcept {
    return enabled_;
}

bool Button::acceptsFocus() const noexcept {
    return enabled_;
}

bool Button::onFocusChanged(bool focused) {
    const bool changed = focused_ != focused ||
        (!focused && keyboardPressed_);
    focused_ = focused;
    if (!focused_) {
        keyboardPressed_ = false;
        keyboardActivationKey_ = KeyCode::Unknown;
    }
    return changed;
}

bool Button::onKeyEvent(const WidgetKeyEvent& event) {
    if (!enabled_) {
        return false;
    }
    if (event.type == WidgetKeyEventType::Cancel) {
        if (!keyboardPressed_) {
            return false;
        }
        keyboardPressed_ = false;
        keyboardActivationKey_ = KeyCode::Unknown;
        return true;
    }
    if (event.key != KeyCode::Enter && event.key != KeyCode::Space) {
        return false;
    }
    if (event.type == WidgetKeyEventType::Press) {
        if (!event.repeat && !keyboardPressed_) {
            keyboardPressed_ = true;
            keyboardActivationKey_ = event.key;
        }
        return true;
    }
    if (event.type == WidgetKeyEventType::Release &&
        keyboardPressed_ && keyboardActivationKey_ == event.key) {
        keyboardPressed_ = false;
        keyboardActivationKey_ = KeyCode::Unknown;
        if (onClick_) {
            ClickHandler callback = onClick_;
            callback();
        }
        return true;
    }
    return false;
}

bool Button::onPointerEvent(const WidgetPointerEvent& event) {
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
    case WidgetPointerEventType::Release: {
        if (event.button != PointerButton::Primary || !pressed_) {
            return false;
        }
        pressed_ = false;
        hovered_ = event.inside;
        if (event.inside && onClick_) {
            ClickHandler callback = onClick_;
            callback();
        }
        return true;
    }
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

Color Button::currentColor() const noexcept {
    if (!enabled_) {
        return style_.disabled;
    }
    if ((pressed_ && hovered_) || keyboardPressed_) {
        return style_.pressed;
    }
    return hovered_ ? style_.hovered : style_.normal;
}

} // namespace lotui
