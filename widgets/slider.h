#pragma once

#include "core/widget.h"

#include <functional>

namespace lotui {

struct SliderOptions {
    double value{0.0};
    double minimum{0.0};
    double maximum{100.0};
    double step{1.0};
    Size preferredSize{160.0F, 24.0F};
};

struct SliderStyle {
    Color track{0.27F, 0.31F, 0.36F, 1.0F};
    Color filled{0.10F, 0.55F, 0.47F, 1.0F};
    Color thumb{0.95F, 0.96F, 0.97F, 1.0F};
    Color thumbHovered{1.0F, 1.0F, 1.0F, 1.0F};
    Color disabled{0.40F, 0.42F, 0.44F, 1.0F};
    Color focusRing{0.96F, 0.82F, 0.32F, 1.0F};
    float trackHeight{4.0F};
    float thumbSize{14.0F};
    float focusRingWidth{2.0F};
};

class Slider final : public Widget {
public:
    using ChangedHandler = std::function<void(double)>;

    explicit Slider(
        SliderOptions options = {},
        ChangedHandler onChanged = {},
        SliderStyle style = {});

    Size measure(const LayoutConstraints& constraints) const override;

    void setValue(double value);
    double value() const noexcept;
    void setRange(double minimum, double maximum);
    double minimum() const noexcept;
    double maximum() const noexcept;
    void setStep(double step);
    double step() const noexcept;
    void setPreferredSize(Size size) noexcept;
    Size preferredSize() const noexcept;
    void setEnabled(bool enabled) noexcept;
    bool isEnabled() const noexcept;
    bool isFocused() const noexcept;
    bool isDragging() const noexcept;
    void setOnChanged(ChangedHandler onChanged);
    void setStyle(SliderStyle style) noexcept;
    const SliderStyle& style() const noexcept;

protected:
    void onPaint(std::vector<PaintCommand>& commands) const override;
    bool acceptsPointerEvents() const noexcept override;
    bool onPointerEvent(const WidgetPointerEvent& event) override;
    bool acceptsFocus() const noexcept override;
    bool onFocusChanged(bool focused) override;
    bool onKeyEvent(const WidgetKeyEvent& event) override;

private:
    void applyUserValue(double value);
    void applyPointer(float x);
    double normalizedValue() const noexcept;

    double value_{0.0};
    double minimum_{0.0};
    double maximum_{100.0};
    double step_{1.0};
    Size preferredSize_{160.0F, 24.0F};
    ChangedHandler onChanged_{};
    SliderStyle style_{};
    bool enabled_{true};
    bool hovered_{false};
    bool dragging_{false};
    bool focused_{false};
};

} // namespace lotui
