#pragma once

#include "widgets/single_child_widget.h"

#include <functional>
#include <memory>

namespace lotui {

struct ButtonStyle {
    Color normal{0.15F, 0.47F, 0.94F, 1.0F};
    Color hovered{0.23F, 0.58F, 1.0F, 1.0F};
    Color pressed{0.08F, 0.35F, 0.78F, 1.0F};
    Color disabled{0.22F, 0.24F, 0.28F, 1.0F};
    float cornerRadius{8.0F};
    EdgeInsets contentPadding{12.0F, 8.0F, 12.0F, 8.0F};
};

class Button final : public SingleChildWidget {
public:
    using ClickHandler = std::function<void()>;

    explicit Button(
        Size preferredSize = {120.0F, 40.0F},
        ClickHandler onClick = {},
        ButtonStyle style = {});
    Button(
        std::unique_ptr<Widget> content,
        Size minimumSize = {120.0F, 40.0F},
        ClickHandler onClick = {},
        ButtonStyle style = {});

    Size measure(const LayoutConstraints& constraints) const override;

    void setPreferredSize(Size preferredSize) noexcept;
    Size preferredSize() const noexcept;

    void setStyle(ButtonStyle style) noexcept;
    const ButtonStyle& style() const noexcept;

    void setOnClick(ClickHandler onClick);
    void setContent(std::unique_ptr<Widget> content);
    std::unique_ptr<Widget> takeContent() noexcept;
    Widget* content() noexcept;
    const Widget* content() const noexcept;

    void setEnabled(bool enabled) noexcept;
    bool isEnabled() const noexcept;
    bool isHovered() const noexcept;
    bool isPressed() const noexcept;

protected:
    void onArrange() override;
    void onPaint(std::vector<PaintCommand>& commands) const override;
    bool acceptsPointerEvents() const noexcept override;
    bool onPointerEvent(const WidgetPointerEvent& event) override;

private:
    Color currentColor() const noexcept;

    Size preferredSize_{};
    ClickHandler onClick_{};
    ButtonStyle style_{};
    bool enabled_{true};
    bool hovered_{false};
    bool pressed_{false};
};

} // namespace lotui
