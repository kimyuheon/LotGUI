#include "widgets/numeric_input.h"

#include "widgets/label.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace lotui {
namespace {

float dimension(float value) noexcept {
    return std::isfinite(value) ? std::max(0.0F, value) : 0.0F;
}

void sanitize(NumericInputStyle& style) noexcept {
    style.contentPadding.left = dimension(style.contentPadding.left);
    style.contentPadding.top = dimension(style.contentPadding.top);
    style.contentPadding.right = dimension(style.contentPadding.right);
    style.contentPadding.bottom = dimension(style.contentPadding.bottom);
    style.stepButtonWidth = dimension(style.stepButtonWidth);
    style.cornerRadius = dimension(style.cornerRadius);
    style.focusRingWidth = dimension(style.focusRingWidth);
}

void validateOptions(const NumericInputOptions& options) {
    if (!std::isfinite(options.value) ||
        !std::isfinite(options.minimum) ||
        !std::isfinite(options.maximum) ||
        options.minimum > options.maximum ||
        !std::isfinite(options.step) || options.step <= 0.0 ||
        options.decimalPlaces < 0 || options.decimalPlaces > 9) {
        throw std::invalid_argument("NumericInput options are invalid");
    }
}

} // namespace

NumericInput::NumericInput(
    std::shared_ptr<const TextEngine> textEngine,
    NumericInputOptions options,
    ChangedHandler onChanged,
    NumericInputStyle style,
    TextStyle textStyle)
    : value_(options.value),
      minimum_(options.minimum),
      maximum_(options.maximum),
      step_(options.step),
      decimalPlaces_(options.decimalPlaces),
      preferredSize_(options.preferredSize),
      onChanged_(std::move(onChanged)),
      style_(style) {
    validateOptions(options);
    sanitize(style_);
    preferredSize_.width = dimension(preferredSize_.width);
    preferredSize_.height = dimension(preferredSize_.height);
    value_ = std::clamp(value_, minimum_, maximum_);

    auto label = std::make_unique<Label>(
        std::move(textEngine), std::string{}, std::move(textStyle));
    label->setColor(style_.text);
    label->setVerticalAlignment(VerticalTextAlignment::Center);
    setChild(std::move(label));
    refreshLabel();
}

Size NumericInput::measure(const LayoutConstraints& constraints) const {
    const Widget* label = child();
    Size desired = preferredSize_;
    if (label) {
        const float chrome = style_.contentPadding.horizontal() +
            style_.stepButtonWidth;
        const float availableWidth = std::isfinite(constraints.maximum.width)
            ? std::max(0.0F, constraints.maximum.width - chrome)
            : unboundedLayoutSize;
        const Size labelSize = label->measure(LayoutConstraints::loose({
            availableWidth,
            std::isfinite(constraints.maximum.height)
                ? std::max(
                    0.0F,
                    constraints.maximum.height -
                        style_.contentPadding.vertical())
                : unboundedLayoutSize,
        }));
        desired.width = std::max(desired.width, labelSize.width + chrome);
        desired.height = std::max(
            desired.height,
            labelSize.height + style_.contentPadding.vertical());
    }
    return constraints.constrain(desired);
}

void NumericInput::setValue(double value) {
    if (!std::isfinite(value)) {
        throw std::invalid_argument("NumericInput value must be finite");
    }
    const double next = std::clamp(value, minimum_, maximum_);
    if (next == value_) {
        return;
    }
    value_ = next;
    refreshLabel();
}

double NumericInput::value() const noexcept {
    return value_;
}

void NumericInput::setRange(double minimum, double maximum) {
    if (!std::isfinite(minimum) || !std::isfinite(maximum) ||
        minimum > maximum) {
        throw std::invalid_argument("NumericInput range is invalid");
    }
    minimum_ = minimum;
    maximum_ = maximum;
    setValue(value_);
}

double NumericInput::minimum() const noexcept {
    return minimum_;
}

double NumericInput::maximum() const noexcept {
    return maximum_;
}

void NumericInput::setStep(double step) {
    if (!std::isfinite(step) || step <= 0.0) {
        throw std::invalid_argument("NumericInput step must be positive");
    }
    step_ = step;
}

double NumericInput::step() const noexcept {
    return step_;
}

void NumericInput::setDecimalPlaces(int decimalPlaces) {
    if (decimalPlaces < 0 || decimalPlaces > 9) {
        throw std::invalid_argument(
            "NumericInput decimal places must be between 0 and 9");
    }
    if (decimalPlaces_ != decimalPlaces) {
        decimalPlaces_ = decimalPlaces;
        refreshLabel();
    }
}

int NumericInput::decimalPlaces() const noexcept {
    return decimalPlaces_;
}

void NumericInput::increment() {
    if (enabled_) {
        applyUserValue(value_ + step_);
    }
}

void NumericInput::decrement() {
    if (enabled_) {
        applyUserValue(value_ - step_);
    }
}

void NumericInput::setEnabled(bool enabled) noexcept {
    enabled_ = enabled;
    if (!enabled_) {
        focused_ = false;
        hoveredRegion_ = StepRegion::None;
        pressedRegion_ = StepRegion::None;
    }
}

bool NumericInput::isEnabled() const noexcept {
    return enabled_;
}

bool NumericInput::isFocused() const noexcept {
    return focused_;
}

void NumericInput::setOnChanged(ChangedHandler onChanged) {
    onChanged_ = std::move(onChanged);
}

void NumericInput::setStyle(NumericInputStyle style) noexcept {
    sanitize(style);
    style_ = style;
    if (auto* label = dynamic_cast<Label*>(child())) {
        label->setColor(style_.text);
    }
}

const NumericInputStyle& NumericInput::style() const noexcept {
    return style_;
}

void NumericInput::setPreferredSize(Size preferredSize) noexcept {
    preferredSize_ = {
        dimension(preferredSize.width),
        dimension(preferredSize.height),
    };
}

Size NumericInput::preferredSize() const noexcept {
    return preferredSize_;
}

void NumericInput::onArrange() {
    Widget* label = child();
    if (!label) {
        return;
    }
    const float availableWidth = std::max(
        0.0F,
        bounds().width - style_.stepButtonWidth -
            style_.contentPadding.horizontal());
    const float availableHeight = std::max(
        0.0F, bounds().height - style_.contentPadding.vertical());
    label->arrange(
        {
            bounds().x + style_.contentPadding.left,
            bounds().y + style_.contentPadding.top,
            availableWidth,
            availableHeight,
        },
        clip());
}

void NumericInput::onPaint(std::vector<PaintCommand>& commands) const {
    Rect field = bounds();
    const float ringWidth = std::min(
        style_.focusRingWidth,
        std::min(field.width, field.height) * 0.5F);
    if (focused_ && ringWidth > 0.0F) {
        commands.push_back({
            field, clip(), style_.focusRing, invalidTextureId,
            style_.cornerRadius});
        field = {
            field.x + ringWidth,
            field.y + ringWidth,
            std::max(0.0F, field.width - ringWidth * 2.0F),
            std::max(0.0F, field.height - ringWidth * 2.0F),
        };
    }
    commands.push_back({
        field,
        clip(),
        enabled_
            ? (hoveredRegion_ == StepRegion::None
                ? style_.normal
                : style_.hovered)
            : style_.disabled,
        invalidTextureId,
        std::max(0.0F, style_.cornerRadius - ringWidth),
    });

    if (enabled_) {
        for (const StepRegion region :
             {StepRegion::Increment, StepRegion::Decrement}) {
            const Rect regionBounds = stepRegionBounds(region);
            commands.push_back({
                regionBounds,
                clip(),
                pressedRegion_ == region
                    ? style_.stepButtonPressed
                    : style_.stepButton,
                invalidTextureId,
                0.0F,
            });

            const float lineWidth = std::min(10.0F, regionBounds.width * 0.4F);
            const float lineHeight = 2.0F;
            const float centerX = regionBounds.x + regionBounds.width * 0.5F;
            const float centerY = regionBounds.y + regionBounds.height * 0.5F;
            commands.push_back({
                {centerX - lineWidth * 0.5F, centerY - lineHeight * 0.5F,
                    lineWidth, lineHeight},
                clip(), style_.indicator, invalidTextureId, 1.0F,
            });
            if (region == StepRegion::Increment) {
                commands.push_back({
                    {centerX - lineHeight * 0.5F,
                        centerY - lineWidth * 0.5F,
                        lineHeight, lineWidth},
                    clip(), style_.indicator, invalidTextureId, 1.0F,
                });
            }
        }
    }
}

bool NumericInput::acceptsPointerEvents() const noexcept {
    return enabled_;
}

bool NumericInput::onPointerEvent(const WidgetPointerEvent& event) {
    if (!enabled_) {
        return false;
    }
    const StepRegion region = event.inside
        ? stepRegion(event.position)
        : StepRegion::None;
    switch (event.type) {
    case WidgetPointerEventType::Enter:
    case WidgetPointerEventType::Move: {
        const bool changed = hoveredRegion_ != region;
        hoveredRegion_ = region;
        return changed;
    }
    case WidgetPointerEventType::Leave:
        if (hoveredRegion_ == StepRegion::None) {
            return false;
        }
        hoveredRegion_ = StepRegion::None;
        return true;
    case WidgetPointerEventType::Press:
        if (event.button != PointerButton::Primary) {
            return false;
        }
        pressedRegion_ = region;
        hoveredRegion_ = region;
        return true;
    case WidgetPointerEventType::Release: {
        if (event.button != PointerButton::Primary) {
            return false;
        }
        const StepRegion pressed = pressedRegion_;
        pressedRegion_ = StepRegion::None;
        hoveredRegion_ = region;
        if (pressed != StepRegion::None && pressed == region) {
            applyUserValue(value_ +
                (pressed == StepRegion::Increment ? step_ : -step_));
        }
        return true;
    }
    case WidgetPointerEventType::Cancel:
        if (pressedRegion_ == StepRegion::None &&
            hoveredRegion_ == StepRegion::None) {
            return false;
        }
        pressedRegion_ = StepRegion::None;
        hoveredRegion_ = StepRegion::None;
        return true;
    }
    return false;
}

bool NumericInput::acceptsFocus() const noexcept {
    return enabled_;
}

bool NumericInput::onFocusChanged(bool focused) {
    const bool changed = focused_ != focused;
    focused_ = focused;
    return changed;
}

bool NumericInput::onKeyEvent(const WidgetKeyEvent& event) {
    if (!enabled_) {
        return false;
    }
    if (event.type == WidgetKeyEventType::Cancel) {
        const bool changed = pressedRegion_ != StepRegion::None;
        pressedRegion_ = StepRegion::None;
        return changed;
    }
    const double largeStep = step_ * 10.0;
    if (event.type == WidgetKeyEventType::Press) {
        switch (event.key) {
        case KeyCode::Up:
            applyUserValue(value_ + step_);
            return true;
        case KeyCode::Down:
            applyUserValue(value_ - step_);
            return true;
        case KeyCode::PageUp:
            applyUserValue(value_ + largeStep);
            return true;
        case KeyCode::PageDown:
            applyUserValue(value_ - largeStep);
            return true;
        case KeyCode::Home:
            applyUserValue(minimum_);
            return true;
        case KeyCode::End:
            applyUserValue(maximum_);
            return true;
        default:
            return false;
        }
    }
    switch (event.key) {
    case KeyCode::Up:
    case KeyCode::Down:
    case KeyCode::PageUp:
    case KeyCode::PageDown:
    case KeyCode::Home:
    case KeyCode::End:
        return true;
    default:
        return false;
    }
}

void NumericInput::applyUserValue(double value) {
    const double previous = value_;
    setValue(value);
    if (value_ != previous && onChanged_) {
        ChangedHandler callback = onChanged_;
        callback(value_);
    }
}

void NumericInput::refreshLabel() {
    auto* label = dynamic_cast<Label*>(child());
    if (!label) {
        return;
    }
    double display = value_;
    const double threshold = 0.5 * std::pow(10.0, -decimalPlaces_);
    if (std::abs(display) < threshold) {
        display = 0.0;
    }
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(decimalPlaces_) << display;
    label->setText(stream.str());
}

NumericInput::StepRegion NumericInput::stepRegion(
    Point position) const noexcept {
    if (!contains(bounds(), position) || style_.stepButtonWidth <= 0.0F ||
        position.x < bounds().x + bounds().width - style_.stepButtonWidth) {
        return StepRegion::None;
    }
    return position.y < bounds().y + bounds().height * 0.5F
        ? StepRegion::Increment
        : StepRegion::Decrement;
}

Rect NumericInput::stepRegionBounds(StepRegion region) const noexcept {
    const float width = std::min(style_.stepButtonWidth, bounds().width);
    const float halfHeight = bounds().height * 0.5F;
    return {
        bounds().x + bounds().width - width,
        bounds().y +
            (region == StepRegion::Decrement ? halfHeight : 0.0F),
        width,
        halfHeight,
    };
}

} // namespace lotui
