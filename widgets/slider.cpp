#include "widgets/slider.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace lotui {
namespace {

float dimension(float value) noexcept {
    return std::isfinite(value) ? std::max(0.0F, value) : 0.0F;
}

void sanitize(SliderStyle& style) noexcept {
    style.trackHeight = dimension(style.trackHeight);
    style.thumbSize = dimension(style.thumbSize);
    style.focusRingWidth = dimension(style.focusRingWidth);
}

void validateRange(double minimum, double maximum) {
    if (!std::isfinite(minimum) || !std::isfinite(maximum) ||
        minimum >= maximum) {
        throw std::invalid_argument("slider range must be finite and increasing");
    }
}

void validateStep(double step) {
    if (!std::isfinite(step) || step <= 0.0) {
        throw std::invalid_argument("slider step must be finite and positive");
    }
}

} // namespace

Slider::Slider(
    SliderOptions options,
    ChangedHandler onChanged,
    SliderStyle style)
    : minimum_(options.minimum),
      maximum_(options.maximum),
      step_(options.step),
      preferredSize_(options.preferredSize),
      onChanged_(std::move(onChanged)),
      style_(style) {
    validateRange(minimum_, maximum_);
    validateStep(step_);
    setPreferredSize(preferredSize_);
    sanitize(style_);
    setValue(options.value);
}

Size Slider::measure(const LayoutConstraints& constraints) const {
    return constraints.constrain(preferredSize_);
}

void Slider::setValue(double value) {
    if (!std::isfinite(value)) {
        throw std::invalid_argument("slider value must be finite");
    }
    value_ = std::clamp(value, minimum_, maximum_);
}

double Slider::value() const noexcept { return value_; }

void Slider::setRange(double minimum, double maximum) {
    validateRange(minimum, maximum);
    minimum_ = minimum;
    maximum_ = maximum;
    value_ = std::clamp(value_, minimum_, maximum_);
}

double Slider::minimum() const noexcept { return minimum_; }
double Slider::maximum() const noexcept { return maximum_; }

void Slider::setStep(double step) {
    validateStep(step);
    step_ = step;
}

double Slider::step() const noexcept { return step_; }

void Slider::setPreferredSize(Size size) noexcept {
    preferredSize_ = {dimension(size.width), dimension(size.height)};
}

Size Slider::preferredSize() const noexcept { return preferredSize_; }

void Slider::setEnabled(bool enabled) noexcept {
    enabled_ = enabled;
    if (!enabled_) {
        hovered_ = false;
        dragging_ = false;
        focused_ = false;
    }
}

bool Slider::isEnabled() const noexcept { return enabled_; }
bool Slider::isFocused() const noexcept { return focused_; }
bool Slider::isDragging() const noexcept { return dragging_; }

void Slider::setOnChanged(ChangedHandler onChanged) {
    onChanged_ = std::move(onChanged);
}

void Slider::setStyle(SliderStyle style) noexcept {
    sanitize(style);
    style_ = style;
}

const SliderStyle& Slider::style() const noexcept { return style_; }

void Slider::onPaint(std::vector<PaintCommand>& commands) const {
    const Rect area = bounds();
    const float thumb = std::min(style_.thumbSize,
        std::min(area.width, area.height));
    const float trackHeight = std::min(style_.trackHeight, area.height);
    const float trackY = area.y + (area.height - trackHeight) * 0.5F;
    const float start = area.x + thumb * 0.5F;
    const float travel = std::max(0.0F, area.width - thumb);
    const float centerX = start + travel *
        static_cast<float>(normalizedValue());
    const Rect track{start, trackY, travel, trackHeight};
    const float radius = trackHeight * 0.5F;
    commands.push_back({track, clip(), style_.track,
        invalidTextureId, radius});
    if (centerX > start) {
        commands.push_back({{start, trackY, centerX - start, trackHeight},
            clip(), enabled_ ? style_.filled : style_.disabled,
            invalidTextureId, radius});
    }
    const Rect thumbBounds{centerX - thumb * 0.5F,
        area.y + (area.height - thumb) * 0.5F, thumb, thumb};
    if (focused_ && style_.focusRingWidth > 0.0F) {
        const float width = style_.focusRingWidth;
        commands.push_back({
            {thumbBounds.x - width, thumbBounds.y - width,
             thumbBounds.width + width * 2.0F,
             thumbBounds.height + width * 2.0F},
            clip(), style_.focusRing, invalidTextureId,
            (thumb + width * 2.0F) * 0.5F});
    }
    commands.push_back({thumbBounds, clip(),
        !enabled_ ? style_.disabled :
            (hovered_ || dragging_ ? style_.thumbHovered : style_.thumb),
        invalidTextureId, thumb * 0.5F});
}

bool Slider::acceptsPointerEvents() const noexcept { return enabled_; }

bool Slider::onPointerEvent(const WidgetPointerEvent& event) {
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
    case WidgetPointerEventType::Move:
        hovered_ = event.inside;
        if (dragging_) {
            applyPointer(event.position.x);
            return true;
        }
        return true;
    case WidgetPointerEventType::Press:
        if (event.button != PointerButton::Primary) {
            return false;
        }
        dragging_ = true;
        applyPointer(event.position.x);
        return true;
    case WidgetPointerEventType::Release:
        if (event.button != PointerButton::Primary || !dragging_) {
            return false;
        }
        applyPointer(event.position.x);
        dragging_ = false;
        hovered_ = event.inside;
        return true;
    case WidgetPointerEventType::Cancel:
        if (!dragging_ && !hovered_) {
            return false;
        }
        dragging_ = false;
        hovered_ = false;
        return true;
    }
    return false;
}

bool Slider::acceptsFocus() const noexcept { return enabled_; }

bool Slider::onFocusChanged(bool focused) {
    const bool changed = focused_ != focused;
    focused_ = focused;
    return changed;
}

bool Slider::onKeyEvent(const WidgetKeyEvent& event) {
    if (!enabled_ || event.type != WidgetKeyEventType::Press) {
        return false;
    }
    switch (event.key) {
    case KeyCode::Left:
    case KeyCode::Down:
        applyUserValue(value_ - step_);
        return true;
    case KeyCode::Right:
    case KeyCode::Up:
        applyUserValue(value_ + step_);
        return true;
    case KeyCode::PageDown:
        applyUserValue(value_ - step_ * 10.0);
        return true;
    case KeyCode::PageUp:
        applyUserValue(value_ + step_ * 10.0);
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

void Slider::applyUserValue(double value) {
    const double clamped = std::clamp(value, minimum_, maximum_);
    if (clamped == value_) {
        return;
    }
    value_ = clamped;
    if (onChanged_) {
        ChangedHandler callback = onChanged_;
        callback(value_);
    }
}

void Slider::applyPointer(float x) {
    const float thumb = std::min(style_.thumbSize,
        std::min(bounds().width, bounds().height));
    const double travel = std::max(0.0F, bounds().width - thumb);
    if (travel <= 0.0) {
        return;
    }
    const double start = bounds().x + thumb * 0.5F;
    const double fraction = std::clamp((x - start) / travel, 0.0, 1.0);
    const double raw = minimum_ + (maximum_ - minimum_) * fraction;
    const double steps = std::round((raw - minimum_) / step_);
    applyUserValue(minimum_ + steps * step_);
}

double Slider::normalizedValue() const noexcept {
    return (value_ - minimum_) / (maximum_ - minimum_);
}

} // namespace lotui
