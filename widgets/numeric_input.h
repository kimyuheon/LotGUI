#pragma once

#include "text/text_layout.h"
#include "widgets/single_child_widget.h"

#include <functional>
#include <memory>

namespace lotui {

struct NumericInputOptions {
    double value{0.0};
    double minimum{0.0};
    double maximum{100.0};
    double step{1.0};
    int decimalPlaces{0};
    Size preferredSize{160.0F, 40.0F};
};

struct NumericInputStyle {
    Color normal{0.12F, 0.15F, 0.21F, 1.0F};
    Color hovered{0.16F, 0.20F, 0.28F, 1.0F};
    Color stepButton{0.20F, 0.26F, 0.36F, 1.0F};
    Color stepButtonPressed{0.10F, 0.36F, 0.72F, 1.0F};
    Color indicator{0.91F, 0.94F, 1.0F, 1.0F};
    Color text{0.93F, 0.95F, 1.0F, 1.0F};
    Color disabled{0.22F, 0.24F, 0.28F, 1.0F};
    Color focusRing{0.96F, 0.82F, 0.32F, 1.0F};
    EdgeInsets contentPadding{12.0F, 8.0F, 8.0F, 8.0F};
    float stepButtonWidth{28.0F};
    float cornerRadius{7.0F};
    float focusRingWidth{2.0F};
};

class NumericInput final : public SingleChildWidget {
public:
    using ChangedHandler = std::function<void(double)>;

    NumericInput(
        std::shared_ptr<const TextEngine> textEngine,
        NumericInputOptions options = {},
        ChangedHandler onChanged = {},
        NumericInputStyle style = {},
        TextStyle textStyle = {});

    Size measure(const LayoutConstraints& constraints) const override;

    void setValue(double value);
    double value() const noexcept;
    void setRange(double minimum, double maximum);
    double minimum() const noexcept;
    double maximum() const noexcept;
    void setStep(double step);
    double step() const noexcept;
    void setDecimalPlaces(int decimalPlaces);
    int decimalPlaces() const noexcept;

    void increment();
    void decrement();

    void setEnabled(bool enabled) noexcept;
    bool isEnabled() const noexcept;
    bool isFocused() const noexcept;

    void setOnChanged(ChangedHandler onChanged);
    void setStyle(NumericInputStyle style) noexcept;
    const NumericInputStyle& style() const noexcept;
    void setPreferredSize(Size preferredSize) noexcept;
    Size preferredSize() const noexcept;

protected:
    void onArrange() override;
    void onPaint(std::vector<PaintCommand>& commands) const override;
    bool acceptsPointerEvents() const noexcept override;
    bool onPointerEvent(const WidgetPointerEvent& event) override;
    bool acceptsFocus() const noexcept override;
    bool onFocusChanged(bool focused) override;
    bool onKeyEvent(const WidgetKeyEvent& event) override;

private:
    enum class StepRegion {
        None,
        Increment,
        Decrement,
    };

    void applyUserValue(double value);
    void refreshLabel();
    StepRegion stepRegion(Point position) const noexcept;
    Rect stepRegionBounds(StepRegion region) const noexcept;

    double value_{0.0};
    double minimum_{0.0};
    double maximum_{100.0};
    double step_{1.0};
    int decimalPlaces_{0};
    Size preferredSize_{160.0F, 40.0F};
    ChangedHandler onChanged_{};
    NumericInputStyle style_{};
    bool enabled_{true};
    bool focused_{false};
    StepRegion hoveredRegion_{StepRegion::None};
    StepRegion pressedRegion_{StepRegion::None};
};

} // namespace lotui
