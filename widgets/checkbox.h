#pragma once

#include "widgets/single_child_widget.h"

#include <functional>
#include <memory>

namespace lotui {

struct CheckboxStyle {
    Color unchecked{0.16F, 0.19F, 0.25F, 1.0F};
    Color hovered{0.22F, 0.27F, 0.36F, 1.0F};
    Color checked{0.15F, 0.47F, 0.94F, 1.0F};
    Color checkmark{0.95F, 0.97F, 1.0F, 1.0F};
    Color disabled{0.25F, 0.27F, 0.31F, 1.0F};
    Color focusRing{0.96F, 0.82F, 0.32F, 1.0F};
    float boxSize{22.0F};
    float spacing{10.0F};
    float cornerRadius{5.0F};
    float focusRingWidth{2.0F};
};

class Checkbox final : public SingleChildWidget {
public:
    using ChangedHandler = std::function<void(bool)>;

    explicit Checkbox(
        bool checked = false,
        ChangedHandler onChanged = {},
        CheckboxStyle style = {});
    Checkbox(
        std::unique_ptr<Widget> content,
        bool checked = false,
        ChangedHandler onChanged = {},
        CheckboxStyle style = {});

    Size measure(const LayoutConstraints& constraints) const override;

    void setChecked(bool checked) noexcept;
    bool isChecked() const noexcept;
    void toggle();

    void setEnabled(bool enabled) noexcept;
    bool isEnabled() const noexcept;
    bool isHovered() const noexcept;
    bool isPressed() const noexcept;
    bool isFocused() const noexcept;

    void setOnChanged(ChangedHandler onChanged);
    void setContent(std::unique_ptr<Widget> content);
    Widget* content() noexcept;
    const Widget* content() const noexcept;

    void setStyle(CheckboxStyle style) noexcept;
    const CheckboxStyle& style() const noexcept;

protected:
    void onArrange() override;
    void onPaint(std::vector<PaintCommand>& commands) const override;
    bool acceptsPointerEvents() const noexcept override;
    bool onPointerEvent(const WidgetPointerEvent& event) override;
    bool acceptsFocus() const noexcept override;
    bool onFocusChanged(bool focused) override;
    bool onKeyEvent(const WidgetKeyEvent& event) override;

private:
    void activate();
    Rect checkboxBounds() const noexcept;

    bool checked_{false};
    ChangedHandler onChanged_{};
    CheckboxStyle style_{};
    bool enabled_{true};
    bool hovered_{false};
    bool pressed_{false};
    bool focused_{false};
    bool keyboardPressed_{false};
};

} // namespace lotui
