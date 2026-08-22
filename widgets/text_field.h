#pragma once

#include "text/text_layout.h"
#include "core/widget.h"

#include <functional>
#include <memory>
#include <string>

namespace lotui {

struct TextFieldStyle {
    Color normal{0.12F, 0.15F, 0.21F, 1.0F};
    Color hovered{0.16F, 0.20F, 0.28F, 1.0F};
    Color disabled{0.22F, 0.24F, 0.28F, 1.0F};
    Color text{0.93F, 0.95F, 1.0F, 1.0F};
    Color composition{0.36F, 0.68F, 1.0F, 1.0F};
    Color caret{0.96F, 0.97F, 1.0F, 1.0F};
    Color focusRing{0.96F, 0.82F, 0.32F, 1.0F};
    EdgeInsets contentPadding{12.0F, 8.0F, 12.0F, 8.0F};
    float cornerRadius{7.0F};
    float focusRingWidth{2.0F};
    float caretWidth{1.5F};
};

class TextField final : public Widget {
public:
    using ChangedHandler = std::function<void(const std::string&)>;
    using SubmittedHandler = std::function<void(const std::string&)>;

    TextField(
        std::shared_ptr<const TextEngine> textEngine,
        std::string text = {},
        Size preferredSize = {220.0F, 40.0F},
        ChangedHandler onChanged = {},
        SubmittedHandler onSubmitted = {},
        TextFieldStyle style = {},
        TextStyle textStyle = {});

    Size measure(const LayoutConstraints& constraints) const override;

    void setText(std::string text);
    const std::string& text() const noexcept;
    const std::string& composition() const noexcept;
    std::size_t cursorByteOffset() const noexcept;

    void setEnabled(bool enabled) noexcept;
    bool isEnabled() const noexcept;
    bool isFocused() const noexcept;
    void setPreferredSize(Size preferredSize) noexcept;
    Size preferredSize() const noexcept;
    void setOnChanged(ChangedHandler onChanged);
    void setOnSubmitted(SubmittedHandler onSubmitted);
    void setStyle(TextFieldStyle style) noexcept;
    const TextFieldStyle& style() const noexcept;

protected:
    void onPaint(std::vector<PaintCommand>& commands) const override;
    bool acceptsPointerEvents() const noexcept override;
    bool onPointerEvent(const WidgetPointerEvent& event) override;
    bool acceptsFocus() const noexcept override;
    bool onFocusChanged(bool focused) override;
    bool onKeyEvent(const WidgetKeyEvent& event) override;
    bool acceptsTextInput() const noexcept override;
    Rect textInputRect() const noexcept override;
    bool onTextInputEvent(const TextInputEvent& event) override;

private:
    std::string displayText() const;
    float prefixWidth(std::size_t committedByteOffset) const;
    float compositionWidth() const;
    float contentY(float textHeight) const noexcept;
    void insertCommitted(std::string text);
    void erasePrevious();
    void eraseNext();
    void moveCursorLeft() noexcept;
    void moveCursorRight() noexcept;
    void clearComposition() noexcept;
    void notifyChanged();
    void invalidateLayouts() const noexcept;
    const TextLayout& ensureDisplayLayout(float maximumWidth) const;
    float measureTextWidth(std::string_view text) const;

    std::shared_ptr<const TextEngine> textEngine_;
    std::string text_;
    std::string composition_;
    std::size_t cursorByteOffset_{0};
    std::size_t compositionSelectionStart_{0};
    std::size_t compositionSelectionLength_{0};
    Size preferredSize_{220.0F, 40.0F};
    ChangedHandler onChanged_{};
    SubmittedHandler onSubmitted_{};
    TextFieldStyle style_{};
    TextStyle textStyle_{};
    bool enabled_{true};
    bool focused_{false};
    bool hovered_{false};
    mutable std::unique_ptr<TextLayout> displayLayout_;
    mutable float displayLayoutWidth_{-1.0F};
    mutable float caretOffset_{0.0F};
};

} // namespace lotui
