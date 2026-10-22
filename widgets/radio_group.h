#pragma once

#include "core/widget.h"
#include "text/text_layout.h"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace lotui {

struct RadioOption {
    std::string id;
    std::string label;
    bool enabled{true};
};

struct RadioGroupStyle {
    Color background{0.10F, 0.13F, 0.17F, 1.0F};
    Color hovered{0.18F, 0.24F, 0.29F, 1.0F};
    Color selected{0.18F, 0.31F, 0.36F, 1.0F};
    Color indicator{0.35F, 0.78F, 0.69F, 1.0F};
    Color outline{0.55F, 0.61F, 0.65F, 1.0F};
    Color text{0.91F, 0.94F, 0.94F, 1.0F};
    Color disabledText{0.51F, 0.56F, 0.58F, 1.0F};
    Color focusLine{0.35F, 0.78F, 0.69F, 1.0F};
    float rowHeight{26.0F};
    float indicatorSize{14.0F};
    float padding{8.0F};
    float spacing{8.0F};
};

class RadioGroup final : public Widget {
public:
    using ChangedHandler = std::function<void(std::string_view)>;

    RadioGroup(
        std::shared_ptr<const TextEngine> textEngine,
        std::vector<RadioOption> options = {},
        std::optional<std::string> selectedId = std::nullopt,
        ChangedHandler onChanged = {},
        RadioGroupStyle style = {},
        TextStyle textStyle = {});

    Size measure(const LayoutConstraints& constraints) const override;
    void setOptions(std::vector<RadioOption> options);
    const std::vector<RadioOption>& options() const noexcept;
    bool setSelectedId(std::string_view id);
    std::optional<std::string> selectedId() const;
    void setOnChanged(ChangedHandler onChanged);
    void setEnabled(bool enabled) noexcept;
    bool isEnabled() const noexcept;
    void setStyle(RadioGroupStyle style) noexcept;
    Rect optionBounds(std::string_view id) const noexcept;

protected:
    void onPaint(std::vector<PaintCommand>& commands) const override;
    bool acceptsPointerEvents() const noexcept override;
    bool onPointerEvent(const WidgetPointerEvent& event) override;
    bool acceptsFocus() const noexcept override;
    bool onFocusChanged(bool focused) override;
    bool onKeyEvent(const WidgetKeyEvent& event) override;

private:
    std::optional<std::size_t> optionAt(Point position) const noexcept;
    std::optional<std::size_t> indexOf(std::string_view id) const noexcept;
    bool select(std::size_t index, bool notify);
    bool moveSelection(int direction);

    std::shared_ptr<const TextEngine> textEngine_;
    std::vector<RadioOption> options_;
    std::optional<std::string> selectedId_;
    ChangedHandler onChanged_{};
    RadioGroupStyle style_{};
    TextStyle textStyle_{};
    std::optional<std::size_t> hovered_;
    std::optional<std::size_t> pressed_;
    bool enabled_{true};
    bool focused_{false};
};

} // namespace lotui
