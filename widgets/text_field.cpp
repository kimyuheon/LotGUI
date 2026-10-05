#include "widgets/text_field.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace lotui {
namespace {

float dimension(float value) noexcept {
    return std::isfinite(value) ? std::max(0.0F, value) : 0.0F;
}

void sanitize(TextFieldStyle& style) noexcept {
    style.contentPadding.left = dimension(style.contentPadding.left);
    style.contentPadding.top = dimension(style.contentPadding.top);
    style.contentPadding.right = dimension(style.contentPadding.right);
    style.contentPadding.bottom = dimension(style.contentPadding.bottom);
    style.cornerRadius = dimension(style.cornerRadius);
    style.focusRingWidth = dimension(style.focusRingWidth);
    style.caretWidth = dimension(style.caretWidth);
}

bool continuation(unsigned char value) noexcept {
    return (value & 0xC0U) == 0x80U;
}

std::size_t previousCodePoint(
    const std::string& text,
    std::size_t offset) noexcept {
    if (offset == 0) {
        return 0;
    }
    std::size_t result = std::min(offset, text.size()) - 1;
    while (result > 0 &&
           continuation(static_cast<unsigned char>(text[result]))) {
        --result;
    }
    return result;
}

std::size_t nextCodePoint(
    const std::string& text,
    std::size_t offset) noexcept {
    if (offset >= text.size()) {
        return text.size();
    }
    std::size_t result = offset + 1;
    while (result < text.size() &&
           continuation(static_cast<unsigned char>(text[result]))) {
        ++result;
    }
    return result;
}

} // namespace

TextField::TextField(
    std::shared_ptr<const TextEngine> textEngine,
    std::string text,
    Size preferredSize,
    ChangedHandler onChanged,
    SubmittedHandler onSubmitted,
    TextFieldStyle style,
    TextStyle textStyle)
    : textEngine_(std::move(textEngine)),
      text_(std::move(text)),
      cursorByteOffset_(text_.size()),
      selectionAnchorByteOffset_(cursorByteOffset_),
      preferredSize_(preferredSize),
      onChanged_(std::move(onChanged)),
      onSubmitted_(std::move(onSubmitted)),
      style_(style),
      textStyle_(std::move(textStyle)) {
    if (!textEngine_) {
        throw std::invalid_argument("TextField requires a TextEngine");
    }
    if (!isValidTextStyle(textStyle_)) {
        throw std::invalid_argument("TextField text style is invalid");
    }
    sanitize(style_);
    setPreferredSize(preferredSize_);
}

Size TextField::measure(const LayoutConstraints& constraints) const {
    const float availableWidth = std::isfinite(constraints.maximum.width)
        ? std::max(
            0.0F,
            constraints.maximum.width - style_.contentPadding.horizontal())
        : unboundedLayoutSize;
    const Size textSize = ensureDisplayLayout(availableWidth).size();
    return constraints.constrain({
        std::max(
            preferredSize_.width,
            textSize.width + style_.contentPadding.horizontal()),
        std::max(
            preferredSize_.height,
            textSize.height + style_.contentPadding.vertical()),
    });
}

void TextField::setText(std::string text) {
    if (text_ == text) {
        return;
    }
    text_ = std::move(text);
    cursorByteOffset_ = text_.size();
    selectionAnchorByteOffset_ = cursorByteOffset_;
    horizontalOffset_ = 0.0F;
    clearComposition();
    invalidateLayouts();
}

const std::string& TextField::text() const noexcept {
    return text_;
}

const std::string& TextField::composition() const noexcept {
    return composition_;
}

std::size_t TextField::cursorByteOffset() const noexcept {
    return cursorByteOffset_;
}

std::string TextField::selectedText() const {
    return text_.substr(selectionStart(), selectionEnd() - selectionStart());
}

void TextField::setEnabled(bool enabled) noexcept {
    enabled_ = enabled;
    if (!enabled_) {
        focused_ = false;
        hovered_ = false;
        draggingSelection_ = false;
        clearComposition();
    }
}

bool TextField::isEnabled() const noexcept {
    return enabled_;
}

bool TextField::isFocused() const noexcept {
    return focused_;
}

void TextField::setPreferredSize(Size preferredSize) noexcept {
    preferredSize_ = {
        dimension(preferredSize.width),
        dimension(preferredSize.height),
    };
}

Size TextField::preferredSize() const noexcept {
    return preferredSize_;
}

void TextField::setOnChanged(ChangedHandler onChanged) {
    onChanged_ = std::move(onChanged);
}

void TextField::setOnSubmitted(SubmittedHandler onSubmitted) {
    onSubmitted_ = std::move(onSubmitted);
}

void TextField::setClipboardHandlers(
    ClipboardWriteHandler write,
    ClipboardReadHandler read) {
    clipboardWrite_ = std::move(write);
    clipboardRead_ = std::move(read);
}

void TextField::setStyle(TextFieldStyle style) noexcept {
    sanitize(style);
    style_ = style;
}

const TextFieldStyle& TextField::style() const noexcept {
    return style_;
}

void TextField::onPaint(std::vector<PaintCommand>& commands) const {
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
        enabled_ ? (hovered_ ? style_.hovered : style_.normal)
                 : style_.disabled,
        invalidTextureId,
        std::max(0.0F, style_.cornerRadius - ringWidth),
    });

    const TextLayout& layout = ensureDisplayLayout(unboundedLayoutSize);
    updateHorizontalOffset();
    const Rect content = intersect(clip(), {
        bounds().x + style_.contentPadding.left,
        bounds().y + style_.contentPadding.top,
        std::max(0.0F, bounds().width - style_.contentPadding.horizontal()),
        std::max(0.0F, bounds().height - style_.contentPadding.vertical()),
    });
    const float x = bounds().x + style_.contentPadding.left -
        horizontalOffset_;
    const float y = contentY(layout.size().height);
    if (focused_ && hasSelection() && composition_.empty()) {
        const float left = prefixWidth(selectionStart());
        const float right = prefixWidth(selectionEnd());
        commands.push_back({
            {x + left, y, std::max(0.0F, right - left),
             layout.size().height},
            content, style_.selection, invalidTextureId, 0.0F,
        });
    }
    layout.appendPaintCommands({x, y}, content, style_.text, commands);

    const float prefix = prefixWidth(
        !composition_.empty() && hasSelection()
            ? selectionStart() : cursorByteOffset_);
    if (!composition_.empty()) {
        const float width = compositionWidth();
        commands.push_back({
            {x + prefix, y + layout.size().height - 1.5F, width, 1.5F},
            content, style_.composition, invalidTextureId, 0.0F,
        });
    }
    if (focused_) {
        caretOffset_ = caretPosition();
        commands.push_back({
            {
                x + caretOffset_,
                y,
                style_.caretWidth,
                layout.size().height,
            },
            content, style_.caret, invalidTextureId, 0.0F,
        });
    }
}

bool TextField::acceptsPointerEvents() const noexcept {
    return enabled_;
}

bool TextField::onPointerEvent(const WidgetPointerEvent& event) {
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
    case WidgetPointerEventType::Move: {
        const bool changed = hovered_ != event.inside;
        hovered_ = event.inside;
        if (draggingSelection_) {
            cursorByteOffset_ = byteOffsetAt(event.position.x);
            invalidateLayouts();
            return true;
        }
        return changed;
    }
    case WidgetPointerEventType::Press:
        if (event.button != PointerButton::Primary) {
            return false;
        }
        clearComposition();
        cursorByteOffset_ = byteOffsetAt(event.position.x);
        if (!event.modifiers.shift || !focused_) {
            selectionAnchorByteOffset_ = cursorByteOffset_;
        }
        draggingSelection_ = true;
        invalidateLayouts();
        return true;
    case WidgetPointerEventType::Release:
        if (event.button == PointerButton::Primary && draggingSelection_) {
            cursorByteOffset_ = byteOffsetAt(event.position.x);
            draggingSelection_ = false;
            invalidateLayouts();
            return true;
        }
        return false;
    case WidgetPointerEventType::Cancel:
        draggingSelection_ = false;
        return false;
    }
    return false;
}

bool TextField::acceptsFocus() const noexcept {
    return enabled_;
}

bool TextField::onFocusChanged(bool focused) {
    const bool changed = focused_ != focused ||
        (!focused && !composition_.empty());
    focused_ = focused;
    if (!focused_) {
        draggingSelection_ = false;
        clearComposition();
        invalidateLayouts();
    }
    return changed;
}

bool TextField::onKeyEvent(const WidgetKeyEvent& event) {
    if (!enabled_) {
        return false;
    }
    if (event.type == WidgetKeyEventType::Cancel) {
        const bool changed = !composition_.empty();
        clearComposition();
        invalidateLayouts();
        return changed;
    }
    const bool selectAll = (event.modifiers.control ||
        event.modifiers.meta) && !event.modifiers.alt &&
        event.key == KeyCode::A;
    const bool primaryShortcut = (event.modifiers.control ||
        event.modifiers.meta) && !event.modifiers.alt &&
        composition_.empty();
    const bool copyKey = primaryShortcut && event.key == KeyCode::C &&
        static_cast<bool>(clipboardWrite_);
    const bool cutKey = primaryShortcut && event.key == KeyCode::X &&
        static_cast<bool>(clipboardWrite_);
    const bool pasteKey = primaryShortcut && event.key == KeyCode::V &&
        static_cast<bool>(clipboardRead_);
    if (event.type != WidgetKeyEventType::Press) {
        if (selectAll || copyKey || cutKey || pasteKey) {
            return true;
        }
        switch (event.key) {
        case KeyCode::Backspace:
        case KeyCode::Delete:
        case KeyCode::Left:
        case KeyCode::Right:
        case KeyCode::Home:
        case KeyCode::End:
        case KeyCode::Enter:
            return true;
        default:
            return false;
        }
    }

    if (selectAll) {
        if (composition_.empty()) {
            selectionAnchorByteOffset_ = 0;
            cursorByteOffset_ = text_.size();
            invalidateLayouts();
        }
        return true;
    }
    if (copyKey || cutKey) {
        std::string selection = selectedText();
        if (!selection.empty()) {
            ClipboardWriteHandler write = clipboardWrite_;
            const bool copied = write(std::move(selection));
            if (cutKey && copied && eraseSelection()) {
                notifyChanged();
            }
        }
        return true;
    }
    if (pasteKey) {
        ClipboardReadHandler read = clipboardRead_;
        const std::optional<std::string> text = read();
        if (text && (!text->empty() || hasSelection())) {
            insertCommitted(*text);
        }
        return true;
    }

    switch (event.key) {
    case KeyCode::Backspace:
        if (composition_.empty()) {
            erasePrevious();
        }
        return true;
    case KeyCode::Delete:
        if (composition_.empty()) {
            eraseNext();
        }
        return true;
    case KeyCode::Left:
        if (composition_.empty()) {
            if (!event.modifiers.shift && hasSelection()) {
                cursorByteOffset_ = selectionStart();
            } else {
                moveCursorLeft();
            }
            if (!event.modifiers.shift) {
                selectionAnchorByteOffset_ = cursorByteOffset_;
            }
            invalidateLayouts();
        }
        return true;
    case KeyCode::Right:
        if (composition_.empty()) {
            if (!event.modifiers.shift && hasSelection()) {
                cursorByteOffset_ = selectionEnd();
            } else {
                moveCursorRight();
            }
            if (!event.modifiers.shift) {
                selectionAnchorByteOffset_ = cursorByteOffset_;
            }
            invalidateLayouts();
        }
        return true;
    case KeyCode::Home:
        if (composition_.empty()) {
            cursorByteOffset_ = 0;
            if (!event.modifiers.shift) {
                selectionAnchorByteOffset_ = cursorByteOffset_;
            }
            invalidateLayouts();
        }
        return true;
    case KeyCode::End:
        if (composition_.empty()) {
            cursorByteOffset_ = text_.size();
            if (!event.modifiers.shift) {
                selectionAnchorByteOffset_ = cursorByteOffset_;
            }
            invalidateLayouts();
        }
        return true;
    case KeyCode::Enter:
        if (composition_.empty() && onSubmitted_) {
            SubmittedHandler callback = onSubmitted_;
            callback(text_);
        }
        return true;
    default:
        return false;
    }
}

bool TextField::acceptsTextInput() const noexcept {
    return enabled_ && focused_;
}

Rect TextField::textInputRect() const {
    updateHorizontalOffset();
    const float x = bounds().x + style_.contentPadding.left +
        caretOffset_ - horizontalOffset_;
    return {
        x,
        bounds().y + style_.contentPadding.top,
        std::max(1.0F, style_.caretWidth),
        std::max(
            1.0F,
            bounds().height - style_.contentPadding.vertical()),
    };
}

bool TextField::onTextInputEvent(const TextInputEvent& event) {
    if (!enabled_ || !focused_) {
        return false;
    }
    switch (event.type) {
    case TextInputEventType::Commit:
        clearComposition();
        if (!event.text.empty()) {
            insertCommitted(event.text);
        } else {
            invalidateLayouts();
        }
        return true;
    case TextInputEventType::Composition:
        composition_ = event.text;
        compositionSelectionStart_ = std::min(
            event.selectionStart, composition_.size());
        compositionSelectionLength_ = std::min(
            event.selectionLength,
            composition_.size() - compositionSelectionStart_);
        invalidateLayouts();
        return true;
    case TextInputEventType::CompositionEnd:
        clearComposition();
        invalidateLayouts();
        return true;
    }
    return false;
}

std::string TextField::displayText() const {
    std::string result;
    result.reserve(text_.size() + composition_.size());
    const bool replacesSelection = !composition_.empty() && hasSelection();
    const std::size_t insertion = replacesSelection
        ? selectionStart() : cursorByteOffset_;
    result.append(text_, 0, insertion);
    result += composition_;
    result.append(text_, replacesSelection ? selectionEnd() : insertion,
        std::string::npos);
    return result;
}

float TextField::prefixWidth(std::size_t committedByteOffset) const {
    return measureTextWidth(text_.substr(
        0, std::min(committedByteOffset, text_.size())));
}

float TextField::compositionWidth() const {
    return measureTextWidth(composition_);
}

float TextField::contentY(float textHeight) const noexcept {
    return bounds().y + style_.contentPadding.top +
        std::max(
            0.0F,
            bounds().height - style_.contentPadding.vertical() - textHeight) *
            0.5F;
}

std::size_t TextField::byteOffsetAt(float x) const {
    const float position = x - bounds().x - style_.contentPadding.left +
        horizontalOffset_;
    if (position <= 0.0F) {
        return 0;
    }
    std::size_t previous = 0;
    float previousWidth = 0.0F;
    while (previous < text_.size()) {
        const std::size_t next = nextCodePoint(text_, previous);
        const float nextWidth = prefixWidth(next);
        if (position < (previousWidth + nextWidth) * 0.5F) {
            return previous;
        }
        previous = next;
        previousWidth = nextWidth;
    }
    return text_.size();
}

std::size_t TextField::selectionStart() const noexcept {
    return std::min(selectionAnchorByteOffset_, cursorByteOffset_);
}

std::size_t TextField::selectionEnd() const noexcept {
    return std::max(selectionAnchorByteOffset_, cursorByteOffset_);
}

bool TextField::hasSelection() const noexcept {
    return selectionAnchorByteOffset_ != cursorByteOffset_;
}

float TextField::caretPosition() const {
    const std::size_t start = std::min(
        compositionSelectionStart_, composition_.size());
    std::size_t compositionCaretOffset = start + std::min(
        compositionSelectionLength_, composition_.size() - start);
    if (compositionCaretOffset == 0 && !composition_.empty()) {
        compositionCaretOffset = composition_.size();
    }
    return prefixWidth(!composition_.empty() && hasSelection()
            ? selectionStart() : cursorByteOffset_) +
        (composition_.empty() ? 0.0F : measureTextWidth(
            std::string_view(composition_).substr(0, compositionCaretOffset)));
}

void TextField::updateHorizontalOffset() const {
    if (!focused_) {
        horizontalOffset_ = 0.0F;
        return;
    }
    caretOffset_ = caretPosition();
    const float available = std::max(
        0.0F, bounds().width - style_.contentPadding.horizontal());
    if (available <= 0.0F) {
        horizontalOffset_ = 0.0F;
    } else if (caretOffset_ < horizontalOffset_) {
        horizontalOffset_ = caretOffset_;
    } else if (caretOffset_ + style_.caretWidth >
               horizontalOffset_ + available) {
        horizontalOffset_ = caretOffset_ + style_.caretWidth - available;
    }
}

void TextField::insertCommitted(std::string text) {
    eraseSelection();
    text_.insert(cursorByteOffset_, text);
    cursorByteOffset_ += text.size();
    selectionAnchorByteOffset_ = cursorByteOffset_;
    invalidateLayouts();
    notifyChanged();
}

bool TextField::eraseSelection() {
    if (!hasSelection()) {
        return false;
    }
    const std::size_t start = selectionStart();
    text_.erase(start, selectionEnd() - start);
    cursorByteOffset_ = start;
    selectionAnchorByteOffset_ = start;
    invalidateLayouts();
    return true;
}

void TextField::erasePrevious() {
    if (eraseSelection()) {
        notifyChanged();
        return;
    }
    if (cursorByteOffset_ == 0) {
        return;
    }
    const std::size_t previous = previousCodePoint(text_, cursorByteOffset_);
    text_.erase(previous, cursorByteOffset_ - previous);
    cursorByteOffset_ = previous;
    selectionAnchorByteOffset_ = cursorByteOffset_;
    invalidateLayouts();
    notifyChanged();
}

void TextField::eraseNext() {
    if (eraseSelection()) {
        notifyChanged();
        return;
    }
    if (cursorByteOffset_ >= text_.size()) {
        return;
    }
    const std::size_t next = nextCodePoint(text_, cursorByteOffset_);
    text_.erase(cursorByteOffset_, next - cursorByteOffset_);
    selectionAnchorByteOffset_ = cursorByteOffset_;
    invalidateLayouts();
    notifyChanged();
}

void TextField::moveCursorLeft() noexcept {
    cursorByteOffset_ = previousCodePoint(text_, cursorByteOffset_);
}

void TextField::moveCursorRight() noexcept {
    cursorByteOffset_ = nextCodePoint(text_, cursorByteOffset_);
}

void TextField::clearComposition() noexcept {
    composition_.clear();
    compositionSelectionStart_ = 0;
    compositionSelectionLength_ = 0;
}

void TextField::notifyChanged() {
    if (onChanged_) {
        ChangedHandler callback = onChanged_;
        callback(text_);
    }
}

void TextField::invalidateLayouts() const noexcept {
    displayLayout_.reset();
    displayLayoutWidth_ = -1.0F;
}

const TextLayout& TextField::ensureDisplayLayout(float maximumWidth) const {
    const float normalizedWidth = std::isfinite(maximumWidth)
        ? std::max(0.0F, maximumWidth)
        : unboundedLayoutSize;
    if (!displayLayout_ || displayLayoutWidth_ != normalizedWidth) {
        TextLayoutOptions options;
        options.maximumWidth = normalizedWidth;
        displayLayout_ = textEngine_->createLayout(
            displayText(), textStyle_, options);
        if (!displayLayout_) {
            throw std::runtime_error("TextEngine returned a null layout");
        }
        displayLayoutWidth_ = normalizedWidth;
    }
    return *displayLayout_;
}

float TextField::measureTextWidth(std::string_view text) const {
    TextLayoutOptions options;
    std::unique_ptr<TextLayout> layout = textEngine_->createLayout(
        text, textStyle_, options);
    if (!layout) {
        throw std::runtime_error("TextEngine returned a null layout");
    }
    return layout->size().width;
}

} // namespace lotui
