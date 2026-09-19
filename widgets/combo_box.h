#pragma once

#include "text/text_layout.h"
#include "widgets/popup.h"

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace lotui {

struct ComboBoxStyle {
    Color normal{0.13F, 0.16F, 0.22F, 1.0F};
    Color hovered{0.17F, 0.21F, 0.29F, 1.0F};
    Color pressed{0.10F, 0.13F, 0.19F, 1.0F};
    Color disabled{0.22F, 0.24F, 0.28F, 1.0F};
    Color popupBackground{0.11F, 0.14F, 0.20F, 1.0F};
    Color popupHovered{0.19F, 0.25F, 0.35F, 1.0F};
    Color popupSelected{0.14F, 0.43F, 0.82F, 1.0F};
    Color text{0.93F, 0.95F, 1.0F, 1.0F};
    Color disabledText{0.58F, 0.61F, 0.67F, 1.0F};
    Color arrow{0.52F, 0.72F, 1.0F, 1.0F};
    Color focusRing{0.96F, 0.82F, 0.32F, 1.0F};
    EdgeInsets contentPadding{9.0F, 4.0F, 7.0F, 4.0F};
    EdgeInsets popupPadding{4.0F, 4.0F, 4.0F, 4.0F};
    float arrowAreaWidth{24.0F};
    float itemHeight{26.0F};
    std::size_t maximumVisibleItems{8};
    float cornerRadius{5.0F};
    float popupCornerRadius{5.0F};
    float focusRingWidth{1.5F};
};

class ComboBox final : public Widget {
public:
    using SelectionChangedHandler = std::function<void(std::size_t)>;

    ComboBox(
        std::shared_ptr<const TextEngine> textEngine,
        std::vector<std::string> options = {},
        std::optional<std::size_t> selectedIndex = std::nullopt,
        Size preferredSize = {180.0F, 32.0F},
        SelectionChangedHandler onSelectionChanged = {},
        ComboBoxStyle style = {},
        TextStyle textStyle = {});

    Size measure(const LayoutConstraints& constraints) const override;

    void setPopupHost(PopupHost* host) noexcept;
    PopupHost* popupHost() const noexcept;
    bool openPopup();
    bool closePopup();
    bool isPopupOpen() const noexcept;

    void setOptions(std::vector<std::string> options);
    const std::vector<std::string>& options() const noexcept;
    void setSelectedIndex(
        std::optional<std::size_t> index,
        bool notify = false);
    std::optional<std::size_t> selectedIndex() const noexcept;
    const std::string* selectedText() const noexcept;

    void setEnabled(bool enabled) noexcept;
    bool isEnabled() const noexcept;
    bool isFocused() const noexcept;
    void setPreferredSize(Size preferredSize) noexcept;
    void setOnSelectionChanged(SelectionChangedHandler handler);
    void setStyle(ComboBoxStyle style) noexcept;
    const ComboBoxStyle& style() const noexcept;

    static PopupId showPopupAt(
        PopupHost& host,
        std::shared_ptr<const TextEngine> textEngine,
        Rect anchor,
        std::vector<std::string> options,
        std::optional<std::size_t> selectedIndex,
        SelectionChangedHandler onSelectionChanged,
        ComboBoxStyle style = {},
        TextStyle textStyle = {},
        PopupClosedHandler onClosed = {});

protected:
    void onPaint(std::vector<PaintCommand>& commands) const override;
    bool acceptsPointerEvents() const noexcept override;
    bool onPointerEvent(const WidgetPointerEvent& event) override;
    bool acceptsFocus() const noexcept override;
    bool onFocusChanged(bool focused) override;
    bool onKeyEvent(const WidgetKeyEvent& event) override;

private:
    struct CallbackState;

    void select(std::size_t index, bool notify);
    void invalidateLayout() const noexcept;
    const TextLayout* ensureLayout(float maximumWidth) const;
    Color currentColor() const noexcept;

    std::shared_ptr<const TextEngine> textEngine_;
    std::vector<std::string> options_;
    std::optional<std::size_t> selectedIndex_;
    Size preferredSize_{180.0F, 32.0F};
    SelectionChangedHandler onSelectionChanged_{};
    ComboBoxStyle style_{};
    TextStyle textStyle_{};
    PopupHost* popupHost_{nullptr};
    PopupId popupId_{invalidPopupId};
    bool enabled_{true};
    bool focused_{false};
    bool hovered_{false};
    bool pressed_{false};
    std::shared_ptr<CallbackState> callbackState_;
    mutable std::unique_ptr<TextLayout> layout_;
    mutable float layoutWidth_{-1.0F};
};

} // namespace lotui
