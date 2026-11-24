#include "widgets/code_editor.h"

#include "widgets/conversation_list.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace lotui {
namespace {

bool continuation(unsigned char c) noexcept {
    return (c & 0xC0U) == 0x80U;
}

std::size_t previousPoint(std::string_view text, std::size_t offset) noexcept {
    if (offset == 0) return 0;
    std::size_t result = std::min(offset, text.size()) - 1;
    while (result > 0 && continuation(static_cast<unsigned char>(text[result])))
        --result;
    return result;
}

std::size_t nextPoint(std::string_view text, std::size_t offset) noexcept {
    if (offset >= text.size()) return text.size();
    std::size_t result = offset + 1;
    while (result < text.size() &&
           continuation(static_cast<unsigned char>(text[result]))) ++result;
    return result;
}

std::string normalizeNewlines(std::string_view text) {
    std::string result;
    result.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\r') {
            result.push_back('\n');
            if (i + 1 < text.size() && text[i + 1] == '\n') ++i;
        } else {
            result.push_back(text[i]);
        }
    }
    return result;
}

char closingBracket(char opening) noexcept {
    switch (opening) {
    case '(': return ')';
    case '[': return ']';
    case '{': return '}';
    default: return '\0';
    }
}

Color tokenColor(const CodeEditorStyle& style, CodeTokenKind kind) noexcept {
    switch (kind) {
    case CodeTokenKind::Keyword: return style.keyword;
    case CodeTokenKind::String: return style.string;
    case CodeTokenKind::Number: return style.number;
    case CodeTokenKind::Comment: return style.comment;
    case CodeTokenKind::Plain: return style.text;
    }
    return style.text;
}

} // namespace

CodeEditor::CodeEditor(std::shared_ptr<const TextEngine> textEngine,
    std::string text, std::string language, Size preferredSize,
    ChangedHandler onChanged, CodeEditorStyle style, TextStyle textStyle)
    : textEngine_(std::move(textEngine)), language_(std::move(language)),
      onChanged_(std::move(onChanged)), style_(style),
      textStyle_(std::move(textStyle)), preferredSize_(preferredSize) {
    if (!textEngine_ || !isValidTextStyle(textStyle_) ||
        !std::isfinite(style_.lineHeight) || style_.lineHeight <= 0.0F ||
        !std::isfinite(style_.gutterWidth) || style_.gutterWidth < 0.0F ||
        !std::isfinite(style_.padding) || style_.padding < 0.0F ||
        !std::isfinite(style_.caretWidth) || style_.caretWidth <= 0.0F)
        throw std::invalid_argument("CodeEditor requires valid text and style");
    setText(std::move(text));
}

Size CodeEditor::measure(const LayoutConstraints& constraints) const {
    return constraints.constrain(preferredSize_);
}

void CodeEditor::setText(std::string text) {
    const bool hadBreakpoints = !breakpoints_.empty();
    text_ = normalizeNewlines(text);
    breakpoints_.clear();
    cursor_ = anchor_ = 0;
    undoStack_.clear();
    redoStack_.clear();
    composition_.clear();
    scrollX_ = scrollY_ = 0.0F;
    preferredColumnX_ = -1.0F;
    rebuildLines();
    if (hadBreakpoints && onBreakpointsChanged_)
        onBreakpointsChanged_(breakpoints_);
}

const std::string& CodeEditor::text() const noexcept { return text_; }
void CodeEditor::setLanguage(std::string language) { language_ = std::move(language); }
const std::string& CodeEditor::language() const noexcept { return language_; }
void CodeEditor::setOnChanged(ChangedHandler handler) {
    onChanged_ = std::move(handler);
}
void CodeEditor::setClipboardHandlers(ClipboardWriteHandler write,
    ClipboardReadHandler read) {
    clipboardWrite_ = std::move(write);
    clipboardRead_ = std::move(read);
}
void CodeEditor::setBreakpoints(std::vector<std::size_t> lines) {
    lines.erase(std::remove_if(lines.begin(), lines.end(), [this](std::size_t line) {
        return line == 0 || line > lineCount();
    }), lines.end());
    std::sort(lines.begin(), lines.end());
    lines.erase(std::unique(lines.begin(), lines.end()), lines.end());
    if (lines == breakpoints_) return;
    breakpoints_ = std::move(lines);
    if (onBreakpointsChanged_) onBreakpointsChanged_(breakpoints_);
}
const std::vector<std::size_t>& CodeEditor::breakpoints() const noexcept {
    return breakpoints_;
}
bool CodeEditor::hasBreakpoint(std::size_t line) const noexcept {
    return std::binary_search(breakpoints_.begin(), breakpoints_.end(), line);
}
void CodeEditor::toggleBreakpoint(std::size_t line) {
    if (line == 0 || line > lineCount()) return;
    auto it = std::lower_bound(breakpoints_.begin(), breakpoints_.end(), line);
    if (it != breakpoints_.end() && *it == line)
        breakpoints_.erase(it);
    else
        breakpoints_.insert(it, line);
    if (onBreakpointsChanged_) onBreakpointsChanged_(breakpoints_);
}
void CodeEditor::setOnBreakpointsChanged(BreakpointsChangedHandler handler) {
    onBreakpointsChanged_ = std::move(handler);
}
void CodeEditor::setStyle(CodeEditorStyle style) {
    if (!std::isfinite(style.lineHeight) || style.lineHeight <= 0.0F ||
        !std::isfinite(style.gutterWidth) || style.gutterWidth < 0.0F ||
        !std::isfinite(style.padding) || style.padding < 0.0F ||
        !std::isfinite(style.caretWidth) || style.caretWidth <= 0.0F)
        throw std::invalid_argument("CodeEditor style is invalid");
    style_ = style;
    revealCursor();
}
const CodeEditorStyle& CodeEditor::style() const noexcept { return style_; }
std::size_t CodeEditor::lineCount() const noexcept { return lineStarts_.size(); }
std::size_t CodeEditor::cursorByteOffset() const noexcept { return cursor_; }
std::string CodeEditor::selectedText() const {
    return text_.substr(selectionStart(), selectionEnd() - selectionStart());
}
void CodeEditor::selectAll() noexcept { anchor_ = 0; cursor_ = text_.size(); }
bool CodeEditor::canUndo() const noexcept { return !undoStack_.empty(); }
bool CodeEditor::canRedo() const noexcept { return !redoStack_.empty(); }
bool CodeEditor::isFocused() const noexcept { return focused_; }
const std::string& CodeEditor::composition() const noexcept { return composition_; }
float CodeEditor::scrollOffset() const noexcept { return scrollY_; }

bool CodeEditor::findNext(std::string_view query) {
    if (query.empty()) return false;
    std::size_t found = text_.find(query, selectionEnd());
    if (found == std::string::npos)
        found = text_.find(query);
    if (found == std::string::npos) return false;
    anchor_ = found;
    cursor_ = found + query.size();
    revealCursor();
    return true;
}

bool CodeEditor::undo() {
    if (!canUndo() || !composition_.empty()) return false;
    redoStack_.push_back({text_, cursor_, anchor_});
    Snapshot snapshot = std::move(undoStack_.back());
    undoStack_.pop_back();
    restore(std::move(snapshot));
    return true;
}

bool CodeEditor::redo() {
    if (!canRedo() || !composition_.empty()) return false;
    undoStack_.push_back({text_, cursor_, anchor_});
    Snapshot snapshot = std::move(redoStack_.back());
    redoStack_.pop_back();
    restore(std::move(snapshot));
    return true;
}

void CodeEditor::scrollToLine(std::size_t line) noexcept {
    const float viewport = std::max(0.0F, bounds().height -
        style_.padding * 2.0F);
    const float maximum = std::max(0.0F,
        static_cast<float>(lineCount()) * style_.lineHeight - viewport);
    scrollY_ = std::clamp(static_cast<float>(line) * style_.lineHeight,
        0.0F, maximum);
}

void CodeEditor::onArrange() {
    const float viewport = std::max(0.0F,
        bounds().height - style_.padding * 2.0F);
    scrollY_ = std::clamp(scrollY_, 0.0F, std::max(0.0F,
        static_cast<float>(lineCount()) * style_.lineHeight - viewport));
}

void CodeEditor::onPaint(std::vector<PaintCommand>& commands) const {
    const Rect area = bounds();
    commands.push_back({area, clip(), style_.background,
        invalidTextureId, 0.0F});
    const Rect gutter{area.x, area.y, std::min(area.width,
        style_.gutterWidth), area.height};
    commands.push_back({gutter, clip(), style_.gutter,
        invalidTextureId, 0.0F});
    const Rect content = contentClip();
    if (area.height <= 0.0F || content.width <= 0.0F) return;
    const std::size_t first = std::min(lineCount(), static_cast<std::size_t>(
        std::max(0.0F, std::floor(scrollY_ / style_.lineHeight))));
    const std::size_t visible = static_cast<std::size_t>(
        std::ceil(area.height / style_.lineHeight)) + 2;
    const std::size_t last = std::min(lineCount(), first + visible);
    const std::size_t currentLine = lineOf(cursor_);
    for (std::size_t line = first; line < last; ++line) {
        const float y = textY(line);
        if (line == currentLine) {
            commands.push_back({
                {content.x, y, content.width, style_.lineHeight},
                content, style_.currentLine, invalidTextureId, 0.0F});
        }
        if (hasSelection() && selectionStart() <= lineEnd(line) &&
            selectionEnd() >= lineStarts_[line]) {
            const std::size_t left = std::clamp(selectionStart(),
                lineStarts_[line], lineEnd(line));
            const std::size_t right = std::clamp(selectionEnd(),
                lineStarts_[line], lineEnd(line));
            const float x1 = textX() + prefixWidth(line, left);
            const float x2 = textX() + prefixWidth(line, right);
            if (x2 > x1) commands.push_back({
                {x1, y, x2 - x1, style_.lineHeight}, content,
                style_.selection, invalidTextureId, 0.0F});
        }
        const std::string number = std::to_string(line + 1);
        auto numberLayout = textEngine_->createLayout(number, textStyle_, {});
        numberLayout->appendPaintCommands({
            gutter.x + gutter.width - style_.padding - numberLayout->size().width,
            y + (style_.lineHeight - numberLayout->size().height) * 0.5F},
            intersect(gutter, clip()), style_.lineNumber, commands);
        if (hasBreakpoint(line + 1)) {
            const float diameter = std::max(0.0F,
                std::min(13.0F, style_.lineHeight - 4.0F));
            if (diameter > 0.0F) commands.push_back({
                {gutter.x + 5.0F, y + (style_.lineHeight - diameter) * 0.5F,
                    diameter, diameter},
                intersect(gutter, clip()), style_.breakpoint,
                invalidTextureId, diameter * 0.5F});
        }

        float x = textX();
        for (const auto& token : tokenizeCodeLine(language_, lineText(line))) {
            auto layout = textEngine_->createLayout(token.text, textStyle_, {});
            layout->appendPaintCommands({x, y +
                (style_.lineHeight - layout->size().height) * 0.5F},
                content, tokenColor(style_, token.kind), commands);
            x += layout->size().width;
        }
    }
    if (focused_) {
        const Rect caret = textInputRect();
        commands.push_back({caret, content, style_.caret,
            invalidTextureId, 0.0F});
        if (!composition_.empty()) {
            auto layout = textEngine_->createLayout(composition_, textStyle_, {});
            layout->appendPaintCommands({caret.x, caret.y}, content,
                style_.composition, commands);
            commands.push_back({
                {caret.x, caret.y + style_.lineHeight - 2.0F,
                    layout->size().width, 1.0F},
                content, style_.composition, invalidTextureId, 0.0F});
        }
    }
}

bool CodeEditor::acceptsPointerEvents() const noexcept { return true; }
bool CodeEditor::onPointerEvent(const WidgetPointerEvent& event) {
    switch (event.type) {
    case WidgetPointerEventType::Press:
        if (event.button != PointerButton::Primary) return false;
        if (event.position.x >= bounds().x &&
            event.position.x < bounds().x +
                std::min(23.0F, style_.gutterWidth)) {
            const float relative = event.position.y - bounds().y -
                style_.padding + scrollY_;
            if (relative >= 0.0F &&
                relative < static_cast<float>(lineCount()) *
                    style_.lineHeight) {
                toggleBreakpoint(static_cast<std::size_t>(
                    std::floor(relative / style_.lineHeight)) + 1);
                breakpointPressed_ = true;
                return true;
            }
        }
        composition_.clear();
        cursor_ = offsetAt(event.position);
        if (!event.modifiers.shift || !focused_) anchor_ = cursor_;
        dragging_ = true;
        preferredColumnX_ = -1.0F;
        return true;
    case WidgetPointerEventType::Move:
        if (!dragging_) return false;
        cursor_ = offsetAt(event.position);
        revealCursor();
        return true;
    case WidgetPointerEventType::Release:
        if (breakpointPressed_ && event.button == PointerButton::Primary) {
            breakpointPressed_ = false;
            return true;
        }
        if (event.button != PointerButton::Primary || !dragging_) return false;
        cursor_ = offsetAt(event.position);
        dragging_ = false;
        revealCursor();
        return true;
    case WidgetPointerEventType::Cancel:
        breakpointPressed_ = false;
        dragging_ = false;
        return false;
    case WidgetPointerEventType::Enter:
    case WidgetPointerEventType::Leave:
        return false;
    }
    return false;
}

bool CodeEditor::onScrollEvent(const WidgetScrollEvent& event) {
    const float multiplier = event.mode == ScrollDeltaMode::Line ?
        style_.lineHeight : 1.0F;
    const float before = scrollY_;
    const float viewport = std::max(0.0F,
        bounds().height - style_.padding * 2.0F);
    const float maximum = std::max(0.0F,
        static_cast<float>(lineCount()) * style_.lineHeight - viewport);
    if (std::isfinite(event.delta.y))
        scrollY_ = std::clamp(scrollY_ + event.delta.y * multiplier,
            0.0F, maximum);
    return before != scrollY_;
}

bool CodeEditor::acceptsFocus() const noexcept { return true; }
bool CodeEditor::onFocusChanged(bool focused) {
    const bool changed = focused_ != focused;
    focused_ = focused;
    if (!focused) {
        dragging_ = false;
        breakpointPressed_ = false;
        composition_.clear();
    }
    return changed;
}

bool CodeEditor::onKeyEvent(const WidgetKeyEvent& event) {
    if (event.type == WidgetKeyEventType::Cancel) {
        const bool changed = !composition_.empty();
        composition_.clear();
        return changed;
    }
    const bool shortcut = (event.modifiers.control || event.modifiers.meta) &&
        !event.modifiers.alt;
    if (event.type != WidgetKeyEventType::Press) {
        if (shortcut && (event.key == KeyCode::A || event.key == KeyCode::C ||
            event.key == KeyCode::V || event.key == KeyCode::X ||
            event.key == KeyCode::Y || event.key == KeyCode::Z)) return true;
        switch (event.key) {
        case KeyCode::Tab:
        case KeyCode::Enter:
        case KeyCode::Backspace:
        case KeyCode::Delete:
        case KeyCode::Left:
        case KeyCode::Right:
        case KeyCode::Up:
        case KeyCode::Down:
        case KeyCode::Home:
        case KeyCode::End:
        case KeyCode::PageUp:
        case KeyCode::PageDown: return true;
        default: return false;
        }
    }
    if (event.key == KeyCode::Tab && shortcut) return false;
    if (shortcut) {
        switch (event.key) {
        case KeyCode::A: selectAll(); revealCursor(); return true;
        case KeyCode::C:
        case KeyCode::X:
            if (clipboardWrite_ && hasSelection() &&
                clipboardWrite_(selectedText()) && event.key == KeyCode::X)
                replaceSelection({});
            return true;
        case KeyCode::V:
            if (clipboardRead_) {
                const auto value = clipboardRead_();
                if (value) replaceSelection(normalizeNewlines(*value));
            }
            return true;
        case KeyCode::Z:
            if (event.modifiers.shift) redo(); else undo();
            return true;
        case KeyCode::Y: redo(); return true;
        case KeyCode::Home: moveTo(0, event.modifiers.shift); return true;
        case KeyCode::End: moveTo(text_.size(), event.modifiers.shift); return true;
        default: break;
        }
    }
    if (!composition_.empty()) return true;
    switch (event.key) {
    case KeyCode::Left:
        moveTo(!event.modifiers.shift && hasSelection() ? selectionStart() :
            previousPoint(text_, cursor_), event.modifiers.shift);
        return true;
    case KeyCode::Right:
        moveTo(!event.modifiers.shift && hasSelection() ? selectionEnd() :
            nextPoint(text_, cursor_), event.modifiers.shift);
        return true;
    case KeyCode::Up: moveVertical(-1, event.modifiers.shift); return true;
    case KeyCode::Down: moveVertical(1, event.modifiers.shift); return true;
    case KeyCode::PageUp:
    case KeyCode::PageDown: {
        const int pages = std::max(1, static_cast<int>(
            bounds().height / style_.lineHeight) - 1);
        moveVertical(event.key == KeyCode::PageUp ? -pages : pages,
            event.modifiers.shift);
        return true;
    }
    case KeyCode::Home:
        moveTo(lineStarts_[lineOf(cursor_)], event.modifiers.shift);
        return true;
    case KeyCode::End:
        moveTo(lineEnd(lineOf(cursor_)), event.modifiers.shift);
        return true;
    case KeyCode::Backspace:
        if (!hasSelection() && cursor_ > 0 && cursor_ < text_.size() &&
            closingBracket(text_[cursor_ - 1]) == text_[cursor_]) {
            anchor_ = cursor_ - 1;
            ++cursor_;
            replaceSelection({});
            return true;
        }
        if (!hasSelection() && cursor_ > 0)
            anchor_ = previousPoint(text_, cursor_);
        replaceSelection({});
        return true;
    case KeyCode::Delete:
        if (!hasSelection() && cursor_ < text_.size())
            anchor_ = nextPoint(text_, cursor_);
        replaceSelection({});
        return true;
    case KeyCode::Enter: {
        const std::size_t position = selectionStart();
        const std::size_t start = lineStarts_[lineOf(position)];
        const std::size_t end = lineEnd(lineOf(position));
        std::size_t indent = start;
        while (indent < end && (text_[indent] == ' ' || text_[indent] == '\t'))
            ++indent;
        const std::string base = text_.substr(start, indent - start);
        std::size_t preceding = position;
        while (preceding > start &&
            (text_[preceding - 1] == ' ' || text_[preceding - 1] == '\t'))
            --preceding;
        const bool opensBlock = preceding > start && text_[preceding - 1] == '{';
        const std::string inner = base + (opensBlock ? "    " : "");
        const bool splitsPair = opensBlock && selectionEnd() < text_.size() &&
            text_[selectionEnd()] == '}';
        replaceSelection("\n" + inner +
            (splitsPair ? "\n" + base : ""));
        if (splitsPair) moveTo(position + 1 + inner.size(), false);
        return true;
    }
    case KeyCode::Tab:
        if (event.modifiers.shift ||
            (hasSelection() && lineOf(selectionStart()) != lineOf(selectionEnd())))
            indentSelection(event.modifiers.shift);
        else replaceSelection("    ");
        return true;
    default: return false;
    }
}

bool CodeEditor::acceptsTextInput() const noexcept { return focused_; }
Rect CodeEditor::textInputRect() const {
    const std::size_t line = lineOf(cursor_);
    return {textX() + prefixWidth(line, cursor_), textY(line),
        style_.caretWidth, style_.lineHeight};
}
bool CodeEditor::onTextInputEvent(const TextInputEvent& event) {
    if (!focused_) return false;
    switch (event.type) {
    case TextInputEventType::Commit:
        composition_.clear();
        if (!event.text.empty()) {
            const std::string value = normalizeNewlines(event.text);
            if (value.size() == 1) {
                const char character = value.front();
                const char close = closingBracket(character);
                if (close != '\0') {
                    const std::size_t start = selectionStart();
                    const std::size_t selected = selectionEnd() - start;
                    const std::string inside = selectedText();
                    replaceSelection(std::string(1, character) + inside + close);
                    anchor_ = start + 1;
                    cursor_ = anchor_ + selected;
                    revealCursor();
                    return true;
                }
                if (!hasSelection() && cursor_ < text_.size() &&
                    text_[cursor_] == character &&
                    (character == ')' || character == ']' || character == '}')) {
                    moveTo(cursor_ + 1, false);
                    return true;
                }
                if (character == '}' && !hasSelection()) {
                    const std::size_t start = lineStarts_[lineOf(cursor_)];
                    if (text_.find_first_not_of(" \t", start) >= cursor_) {
                        if (cursor_ < text_.size() && text_[cursor_] == '\n') {
                            const std::size_t nextStart = cursor_ + 1;
                            const std::size_t nextEnd = lineEnd(lineOf(nextStart));
                            std::size_t closeOffset = nextStart;
                            while (closeOffset < nextEnd &&
                                (text_[closeOffset] == ' ' ||
                                    text_[closeOffset] == '\t'))
                                ++closeOffset;
                            if (closeOffset + 1 == nextEnd &&
                                text_[closeOffset] == '}') {
                                anchor_ = start;
                                cursor_ = nextStart;
                                replaceSelection({});
                                moveTo(start + closeOffset - nextStart + 1,
                                    false);
                                return true;
                            }
                        }
                        const std::size_t spaces = cursor_ - start;
                        const std::size_t remove = spaces > 0 &&
                            text_[cursor_ - 1] == '\t' ? 1 :
                            std::min<std::size_t>(4, spaces);
                        anchor_ = cursor_ - remove;
                        replaceSelection("}");
                        return true;
                    }
                }
            }
            replaceSelection(value);
        }
        return true;
    case TextInputEventType::Composition:
        composition_ = event.text;
        return true;
    case TextInputEventType::CompositionEnd:
        composition_.clear();
        return true;
    }
    return false;
}

void CodeEditor::rebuildLines() {
    lineStarts_.clear();
    lineStarts_.push_back(0);
    for (std::size_t i = 0; i < text_.size(); ++i)
        if (text_[i] == '\n') lineStarts_.push_back(i + 1);
}
std::size_t CodeEditor::lineOf(std::size_t offset) const noexcept {
    const auto it = std::upper_bound(lineStarts_.begin(), lineStarts_.end(),
        std::min(offset, text_.size()));
    return static_cast<std::size_t>(it - lineStarts_.begin() - 1);
}
std::size_t CodeEditor::lineEnd(std::size_t line) const noexcept {
    return line + 1 < lineCount() ? lineStarts_[line + 1] - 1 : text_.size();
}
std::string_view CodeEditor::lineText(std::size_t line) const noexcept {
    return std::string_view(text_).substr(lineStarts_[line],
        lineEnd(line) - lineStarts_[line]);
}
std::string CodeEditor::expanded(std::string_view text) const {
    std::string result;
    result.reserve(text.size());
    std::size_t column = 0;
    for (const unsigned char c : text) {
        if (c == '\t') {
            const std::size_t spaces = 4 - column % 4;
            result.append(spaces, ' ');
            column += spaces;
        } else {
            result.push_back(static_cast<char>(c));
            if (!continuation(c)) ++column;
        }
    }
    return result;
}
float CodeEditor::textWidth(std::string_view text) const {
    if (text.empty()) return 0.0F;
    return textEngine_->createLayout(text, textStyle_, {})->size().width;
}
float CodeEditor::prefixWidth(std::size_t line, std::size_t offset) const {
    const std::size_t end = std::clamp(offset, lineStarts_[line], lineEnd(line));
    return textWidth(expanded(std::string_view(text_).substr(
        lineStarts_[line], end - lineStarts_[line])));
}
std::size_t CodeEditor::offsetAt(Point point) const {
    const float relative = point.y - bounds().y - style_.padding + scrollY_;
    const auto index = static_cast<std::size_t>(std::clamp(
        std::floor(relative / style_.lineHeight), 0.0F,
        static_cast<float>(lineCount() - 1)));
    return offsetAtX(index, point.x - textX());
}
std::size_t CodeEditor::offsetAtX(std::size_t line, float x) const {
    if (x <= 0.0F) return lineStarts_[line];
    const std::size_t end = lineEnd(line);
    std::size_t previous = lineStarts_[line];
    float previousWidth = 0.0F;
    while (previous < end) {
        const std::size_t next = nextPoint(text_, previous);
        const float width = prefixWidth(line, next);
        if (x < (previousWidth + width) * 0.5F) return previous;
        previous = next;
        previousWidth = width;
    }
    return end;
}
float CodeEditor::textX() const noexcept {
    return bounds().x + style_.gutterWidth + style_.padding - scrollX_;
}
float CodeEditor::textY(std::size_t line) const noexcept {
    return bounds().y + style_.padding +
        static_cast<float>(line) * style_.lineHeight - scrollY_;
}
Rect CodeEditor::contentClip() const noexcept {
    return intersect(clip(), {bounds().x + style_.gutterWidth,
        bounds().y, std::max(0.0F, bounds().width - style_.gutterWidth),
        bounds().height});
}
void CodeEditor::revealCursor() noexcept {
    const float viewport = std::max(0.0F, bounds().height -
        style_.padding * 2.0F);
    const float lineY = static_cast<float>(lineOf(cursor_)) * style_.lineHeight;
    if (lineY < scrollY_) scrollY_ = lineY;
    else if (lineY + style_.lineHeight > scrollY_ + viewport)
        scrollY_ = lineY + style_.lineHeight - viewport;
    const float maximum = std::max(0.0F,
        static_cast<float>(lineCount()) * style_.lineHeight - viewport);
    scrollY_ = std::clamp(scrollY_, 0.0F, maximum);
    const float x = prefixWidth(lineOf(cursor_), cursor_);
    const float width = std::max(0.0F, bounds().width -
        style_.gutterWidth - style_.padding * 2.0F);
    if (x < scrollX_) scrollX_ = x;
    else if (x + style_.caretWidth > scrollX_ + width)
        scrollX_ = x + style_.caretWidth - width;
}
void CodeEditor::moveVertical(int lines, bool select) {
    const std::size_t current = lineOf(cursor_);
    if (preferredColumnX_ < 0.0F)
        preferredColumnX_ = prefixWidth(current, cursor_);
    const auto target = static_cast<std::size_t>(std::clamp(
        static_cast<long long>(current) + lines, 0LL,
        static_cast<long long>(lineCount() - 1)));
    moveTo(offsetAtX(target, preferredColumnX_), select, true);
}
void CodeEditor::moveTo(std::size_t offset, bool select, bool keepColumn) {
    cursor_ = std::min(offset, text_.size());
    if (!select) anchor_ = cursor_;
    if (!keepColumn) preferredColumnX_ = -1.0F;
    revealCursor();
}
void CodeEditor::replaceSelection(std::string value) {
    if (value.empty() && !hasSelection()) return;
    const std::size_t inserted = value.size();
    const std::size_t start = selectionStart();
    const std::size_t removed = selectionEnd() - start;
    std::vector<std::size_t> breakpointOffsets;
    breakpointOffsets.reserve(breakpoints_.size());
    for (const std::size_t line : breakpoints_)
        breakpointOffsets.push_back(lineStarts_[line - 1]);
    recordEdit();
    text_.replace(start, removed, std::move(value));
    cursor_ = anchor_ = start + inserted;
    rebuildLines();
    rebaseBreakpoints(breakpointOffsets, start, start + removed, inserted);
    preferredColumnX_ = -1.0F;
    revealCursor();
    notifyChanged();
}
void CodeEditor::rebaseBreakpoints(const std::vector<std::size_t>& offsets,
    std::size_t start, std::size_t end, std::size_t inserted) {
    std::vector<std::size_t> lines;
    lines.reserve(offsets.size());
    for (const std::size_t offset : offsets) {
        if (offset > start && offset < end) continue;
        const std::size_t mapped = offset < start ||
            (offset == start && start != end) ? offset :
            start + inserted + (offset - end);
        lines.push_back(lineOf(mapped) + 1);
    }
    setBreakpoints(std::move(lines));
}
void CodeEditor::indentSelection(bool reverse) {
    const std::size_t first = lineOf(selectionStart());
    std::size_t last = lineOf(selectionEnd());
    if (hasSelection() && selectionEnd() == lineStarts_[last] && last > first)
        --last;
    recordEdit();
    std::size_t newCursor = cursor_;
    std::size_t newAnchor = anchor_;
    for (std::size_t line = last + 1; line-- > first;) {
        const std::size_t start = lineStarts_[line];
        std::size_t removed = 0;
        if (reverse) {
            if (start < text_.size() && text_[start] == '\t') removed = 1;
            else while (removed < 4 && start + removed < text_.size() &&
                text_[start + removed] == ' ') ++removed;
            text_.erase(start, removed);
        } else {
            text_.insert(start, "    ");
        }
        const auto adjust = [&](std::size_t& offset) {
            if (offset < start) return;
            if (reverse) offset -= std::min(removed, offset - start);
            else offset += 4;
        };
        adjust(newCursor);
        adjust(newAnchor);
    }
    cursor_ = newCursor;
    anchor_ = newAnchor;
    rebuildLines();
    revealCursor();
    notifyChanged();
}
void CodeEditor::recordEdit() {
    if (undoStack_.size() >= 100) undoStack_.erase(undoStack_.begin());
    undoStack_.push_back({text_, cursor_, anchor_});
    redoStack_.clear();
}
void CodeEditor::restore(Snapshot snapshot) {
    std::vector<std::size_t> breakpointOffsets;
    breakpointOffsets.reserve(breakpoints_.size());
    for (const std::size_t line : breakpoints_)
        breakpointOffsets.push_back(lineStarts_[line - 1]);
    std::size_t start = 0;
    while (start < text_.size() && start < snapshot.text.size() &&
        text_[start] == snapshot.text[start]) ++start;
    std::size_t suffix = 0;
    while (suffix < text_.size() - start &&
        suffix < snapshot.text.size() - start &&
        text_[text_.size() - suffix - 1] ==
            snapshot.text[snapshot.text.size() - suffix - 1]) ++suffix;
    const std::size_t oldEnd = text_.size() - suffix;
    const std::size_t inserted = snapshot.text.size() - start - suffix;
    text_ = std::move(snapshot.text);
    cursor_ = snapshot.cursor;
    anchor_ = snapshot.anchor;
    rebuildLines();
    rebaseBreakpoints(breakpointOffsets, start, oldEnd, inserted);
    revealCursor();
    notifyChanged();
}
void CodeEditor::notifyChanged() {
    if (onChanged_) onChanged_(text_);
}
std::size_t CodeEditor::selectionStart() const noexcept {
    return std::min(cursor_, anchor_);
}
std::size_t CodeEditor::selectionEnd() const noexcept {
    return std::max(cursor_, anchor_);
}
bool CodeEditor::hasSelection() const noexcept { return cursor_ != anchor_; }

} // namespace lotui
