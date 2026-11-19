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

struct CodeEditorStyle {
    Color background{0.11F, 0.13F, 0.16F, 1.0F};
    Color gutter{0.09F, 0.11F, 0.14F, 1.0F};
    Color currentLine{0.15F, 0.18F, 0.22F, 1.0F};
    Color text{0.90F, 0.92F, 0.94F, 1.0F};
    Color lineNumber{0.55F, 0.61F, 0.67F, 1.0F};
    Color keyword{0.56F, 0.75F, 1.00F, 1.0F};
    Color string{0.75F, 0.88F, 0.62F, 1.0F};
    Color number{0.95F, 0.72F, 0.53F, 1.0F};
    Color comment{0.59F, 0.67F, 0.70F, 1.0F};
    Color selection{0.25F, 0.39F, 0.55F, 0.85F};
    Color caret{0.96F, 0.97F, 0.99F, 1.0F};
    Color composition{0.46F, 0.75F, 1.0F, 1.0F};
    float lineHeight{23.0F};
    float gutterWidth{54.0F};
    float padding{10.0F};
    float caretWidth{1.5F};
};

// Editable UTF-8 source text. Positions are byte offsets at code point boundaries.
// The language names accepted by tokenizeCodeLine are shared with ConversationList.
class CodeEditor final : public Widget {
public:
    using ChangedHandler = std::function<void(const std::string&)>;
    using ClipboardWriteHandler = std::function<bool(std::string)>;
    using ClipboardReadHandler = std::function<std::optional<std::string>()>;

    CodeEditor(std::shared_ptr<const TextEngine> textEngine,
        std::string text = {}, std::string language = "cpp",
        Size preferredSize = {720.0F, 420.0F},
        ChangedHandler onChanged = {}, CodeEditorStyle style = {},
        TextStyle textStyle = {});

    Size measure(const LayoutConstraints& constraints) const override;
    void setText(std::string text);
    const std::string& text() const noexcept;
    void setLanguage(std::string language);
    const std::string& language() const noexcept;
    void setOnChanged(ChangedHandler handler);
    void setClipboardHandlers(ClipboardWriteHandler write,
        ClipboardReadHandler read);
    void setStyle(CodeEditorStyle style);
    const CodeEditorStyle& style() const noexcept;
    std::size_t lineCount() const noexcept;
    std::size_t cursorByteOffset() const noexcept;
    std::string selectedText() const;
    void selectAll() noexcept;
    bool findNext(std::string_view query);
    bool canUndo() const noexcept;
    bool canRedo() const noexcept;
    bool undo();
    bool redo();
    void scrollToLine(std::size_t line) noexcept;
    float scrollOffset() const noexcept;
    bool isFocused() const noexcept;
    const std::string& composition() const noexcept;

protected:
    void onArrange() override;
    void onPaint(std::vector<PaintCommand>& commands) const override;
    bool acceptsPointerEvents() const noexcept override;
    bool onPointerEvent(const WidgetPointerEvent& event) override;
    bool onScrollEvent(const WidgetScrollEvent& event) override;
    bool acceptsFocus() const noexcept override;
    bool onFocusChanged(bool focused) override;
    bool onKeyEvent(const WidgetKeyEvent& event) override;
    bool acceptsTextInput() const noexcept override;
    Rect textInputRect() const override;
    bool onTextInputEvent(const TextInputEvent& event) override;

private:
    struct Snapshot {
        std::string text;
        std::size_t cursor{0};
        std::size_t anchor{0};
    };

    void rebuildLines();
    std::size_t lineOf(std::size_t offset) const noexcept;
    std::size_t lineEnd(std::size_t line) const noexcept;
    std::string_view lineText(std::size_t line) const noexcept;
    std::string expanded(std::string_view text) const;
    float textWidth(std::string_view text) const;
    float prefixWidth(std::size_t line, std::size_t offset) const;
    std::size_t offsetAt(Point point) const;
    std::size_t offsetAtX(std::size_t line, float x) const;
    float textX() const noexcept;
    float textY(std::size_t line) const noexcept;
    Rect contentClip() const noexcept;
    void revealCursor() noexcept;
    void moveVertical(int lines, bool select);
    void moveTo(std::size_t offset, bool select, bool keepColumn = false);
    void replaceSelection(std::string value);
    void indentSelection(bool reverse);
    void recordEdit();
    void restore(Snapshot snapshot);
    void notifyChanged();
    std::size_t selectionStart() const noexcept;
    std::size_t selectionEnd() const noexcept;
    bool hasSelection() const noexcept;

    std::shared_ptr<const TextEngine> textEngine_;
    std::string text_;
    std::string language_;
    std::vector<std::size_t> lineStarts_;
    std::size_t cursor_{0};
    std::size_t anchor_{0};
    std::vector<Snapshot> undoStack_;
    std::vector<Snapshot> redoStack_;
    std::string composition_;
    ChangedHandler onChanged_;
    ClipboardWriteHandler clipboardWrite_;
    ClipboardReadHandler clipboardRead_;
    CodeEditorStyle style_;
    TextStyle textStyle_;
    Size preferredSize_;
    float scrollY_{0.0F};
    float scrollX_{0.0F};
    float preferredColumnX_{-1.0F};
    bool focused_{false};
    bool dragging_{false};
};

} // namespace lotui
