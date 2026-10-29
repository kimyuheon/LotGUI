#include "widgets/conversation_list.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace lotui {
namespace {

std::string lower(std::string_view value) {
    std::string result(value);
    std::transform(result.begin(), result.end(), result.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (result == "c++" || result == "cc" || result == "hpp") return "cpp";
    if (result == "c#") return "csharp";
    if (result == "py") return "python";
    if (result == "js") return "javascript";
    if (result == "ts") return "typescript";
    if (result == "sh" || result == "shell") return "bash";
    return result;
}

bool keyword(std::string_view language, std::string_view word) {
    static const std::unordered_set<std::string_view> cpp{
        "auto", "bool", "break", "case", "class", "const", "constexpr",
        "continue", "else", "enum", "false", "float", "for", "if",
        "include", "int", "namespace", "nullptr", "private", "public",
        "return", "static", "struct", "template", "true", "using", "void",
        "while"};
    static const std::unordered_set<std::string_view> python{
        "and", "as", "async", "await", "class", "def", "elif", "else",
        "False", "for", "from", "if", "import", "in", "is", "lambda",
        "None", "not", "or", "pass", "return", "True", "try", "while",
        "with", "yield"};
    static const std::unordered_set<std::string_view> csharp{
        "async", "await", "bool", "class", "else", "false", "for",
        "if", "int", "new", "null", "public", "return", "string",
        "true", "using", "var", "void", "while"};
    static const std::unordered_set<std::string_view> script{
        "async", "await", "class", "const", "else", "export", "false",
        "for", "function", "if", "import", "interface", "let", "new",
        "null", "return", "true", "type", "var", "while"};
    static const std::unordered_set<std::string_view> sql{
        "AND", "AS", "BY", "DELETE", "FROM", "GROUP", "INSERT", "INTO",
        "JOIN", "LIMIT", "ORDER", "SELECT", "UPDATE", "VALUES", "WHERE"};
    static const std::unordered_set<std::string_view> shell{
        "case", "do", "done", "echo", "else", "export", "fi", "for", "if",
        "in", "then", "while"};
    static const std::unordered_set<std::string_view> json{
        "true", "false", "null"};
    if (language == "cpp" || language == "c")
        return cpp.count(word) != 0;
    if (language == "csharp") return csharp.count(word) != 0;
    if (language == "python") return python.count(word) != 0;
    if (language == "javascript" || language == "typescript")
        return script.count(word) != 0;
    if (language == "sql") return sql.count(word) != 0;
    if (language == "bash") return shell.count(word) != 0;
    if (language == "json") return json.count(word) != 0;
    return false;
}

void appendBlock(std::vector<MessageBlock>& blocks, MessageBlockKind kind,
    std::string& text, std::string language = {}) {
    if (text.empty()) return;
    if (text.back() == '\n') text.pop_back();
    blocks.push_back({kind, std::move(text), std::move(language)});
    text.clear();
}

std::unique_ptr<TextLayout> layout(const TextEngine& engine,
    std::string_view text, const TextStyle& style,
    float width = unboundedLayoutSize) {
    TextLayoutOptions options;
    options.maximumWidth = width;
    options.wrap = std::isfinite(width);
    auto result = engine.createLayout(text, style, options);
    if (!result) throw std::runtime_error("TextEngine returned a null layout");
    return result;
}

void validateMessages(const std::vector<ConversationMessage>& messages) {
    std::unordered_set<std::string_view> ids;
    for (const auto& message : messages) {
        if (message.id.empty() || !ids.insert(message.id).second)
            throw std::invalid_argument(
                "ConversationMessage IDs must be unique and nonempty");
    }
}

} // namespace

std::vector<MessageBlock> parseMessageBlocks(std::string_view markdown) {
    std::vector<MessageBlock> blocks;
    std::string buffer;
    std::string language;
    bool inCode = false;
    std::size_t start = 0;
    while (start < markdown.size()) {
        const auto end = markdown.find('\n', start);
        const bool newline = end != std::string_view::npos;
        std::string_view line = markdown.substr(start,
            newline ? end - start : markdown.size() - start);
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
        if (line.substr(0, 3) == "```") {
            appendBlock(blocks, inCode ? MessageBlockKind::Code :
                MessageBlockKind::Text, buffer, language);
            inCode = !inCode;
            language = inCode ? lower(line.substr(3)) : std::string{};
        } else {
            buffer.append(line);
            buffer.push_back('\n');
        }
        start = newline ? end + 1 : markdown.size();
    }
    appendBlock(blocks, inCode ? MessageBlockKind::Code :
        MessageBlockKind::Text, buffer, language);
    return blocks;
}

std::vector<CodeToken> tokenizeCodeLine(
    std::string_view language, std::string_view line) {
    const std::string name = lower(language);
    std::vector<CodeToken> tokens;
    std::size_t i = 0;
    while (i < line.size()) {
        const std::size_t begin = i;
        CodeTokenKind kind = CodeTokenKind::Plain;
        const unsigned char c = static_cast<unsigned char>(line[i]);
        const bool comment =
            ((name == "python" || name == "bash") && line[i] == '#') ||
            (name == "sql" && line.substr(i, 2) == "--") ||
            ((name == "cpp" || name == "c" || name == "csharp" ||
              name == "javascript" || name == "typescript") &&
                line.substr(i, 2) == "//");
        if (comment) {
            kind = CodeTokenKind::Comment;
            i = line.size();
        } else if (c == '\'' || c == '"' || c == '`') {
            kind = CodeTokenKind::String;
            const char quote = line[i++];
            while (i < line.size()) {
                if (line[i] == '\\' && i + 1 < line.size()) {
                    i += 2;
                } else if (line[i++] == quote) {
                    break;
                }
            }
        } else if (std::isdigit(c)) {
            kind = CodeTokenKind::Number;
            while (i < line.size() &&
                    (std::isalnum(static_cast<unsigned char>(line[i])) ||
                     line[i] == '.')) ++i;
        } else if (std::isalpha(c) || c == '_') {
            ++i;
            while (i < line.size() &&
                    (std::isalnum(static_cast<unsigned char>(line[i])) ||
                     line[i] == '_')) ++i;
            if (keyword(name, line.substr(begin, i - begin)))
                kind = CodeTokenKind::Keyword;
        } else if (std::isspace(c)) {
            ++i;
            while (i < line.size() &&
                    std::isspace(static_cast<unsigned char>(line[i]))) ++i;
        } else if (c >= 0x80) {
            ++i;
            while (i < line.size() &&
                    static_cast<unsigned char>(line[i]) >= 0x80) ++i;
        } else {
            ++i;
        }
        tokens.push_back({std::string(line.substr(begin, i - begin)), kind});
    }
    return tokens;
}

ConversationList::ConversationList(
    std::shared_ptr<const TextEngine> textEngine,
    std::vector<ConversationMessage> messages,
    Size preferredSize, ConversationListStyle style)
    : textEngine_(std::move(textEngine)), messages_(std::move(messages)),
      preferredSize_(preferredSize), style_(style) {
    if (!textEngine_) throw std::invalid_argument(
        "ConversationList requires a TextEngine");
    if (!std::isfinite(style_.maximumBubbleWidth) ||
        style_.maximumBubbleWidth <= 0.0F ||
        !std::isfinite(style_.lineHeight) || style_.lineHeight <= 0.0F)
        throw std::invalid_argument("ConversationList style is invalid");
    validateMessages(messages_);
}

Size ConversationList::measure(const LayoutConstraints& constraints) const {
    return constraints.constrain(preferredSize_);
}

void ConversationList::setMessages(std::vector<ConversationMessage> messages) {
    validateMessages(messages);
    messages_ = std::move(messages);
    if (selectedId_ && std::none_of(messages_.begin(), messages_.end(),
            [this](const auto& message) { return message.id == *selectedId_; }))
        selectedId_.reset();
    invalidate();
    clampScroll();
}

void ConversationList::appendMessage(ConversationMessage message) {
    if (message.id.empty() || std::any_of(messages_.begin(), messages_.end(),
            [&message](const auto& existing) {
                return existing.id == message.id;
            }))
        throw std::invalid_argument(
            "ConversationMessage IDs must be unique and nonempty");
    const bool atEnd = scrollOffset_ >=
        std::max(0.0F, contentHeight() - bounds().height - 2.0F);
    messages_.push_back(std::move(message));
    invalidate();
    if (atEnd) scrollToEnd();
}

const std::vector<ConversationMessage>& ConversationList::messages() const noexcept {
    return messages_;
}

void ConversationList::setScrollOffset(float offset) noexcept {
    scrollOffset_ = std::isfinite(offset) ? offset : 0.0F;
    clampScroll();
}

float ConversationList::scrollOffset() const noexcept { return scrollOffset_; }

void ConversationList::scrollToEnd() noexcept {
    try {
        rebuild();
        scrollOffset_ = std::max(0.0F, contentHeight_ - bounds().height);
    } catch (...) { scrollOffset_ = 0.0F; }
}

float ConversationList::contentHeight() const {
    rebuild();
    return contentHeight_;
}

Rect ConversationList::messageBounds(std::string_view id) const {
    rebuild();
    for (std::size_t i = 0; i < messages_.size(); ++i) {
        if (messages_[i].id != id) continue;
        const float width = bubbleWidth();
        return {messages_[i].outgoing ?
                bounds().x + bounds().width - style_.sideMargin - width :
                bounds().x + style_.sideMargin,
            bounds().y + drawMessages_[i].y - scrollOffset_,
            width, drawMessages_[i].height};
    }
    return {};
}

std::optional<std::string> ConversationList::selectedId() const {
    return selectedId_;
}

void ConversationList::setOnSelectionChanged(SelectionHandler handler) {
    onSelectionChanged_ = std::move(handler);
}

void ConversationList::onArrange() { invalidate(); clampScroll(); }

void ConversationList::onPaint(std::vector<PaintCommand>& commands) const {
    const Rect area = intersect(bounds(), clip());
    if (!hasArea(area)) return;
    commands.push_back({bounds(), area, style_.background,
        invalidTextureId, 0.0F});
    rebuild();
    const float width = bubbleWidth();
    const float padding = style_.bubblePadding;
    for (std::size_t i = 0; i < drawMessages_.size(); ++i) {
        const auto& drawn = drawMessages_[i];
        const auto& message = messages_[i];
        const Rect bubble = messageBounds(message.id);
        const Rect bubbleClip = intersect(bubble, area);
        if (!hasArea(bubbleClip)) continue;
        commands.push_back({bubble, bubbleClip,
            message.outgoing ? style_.outgoingBubble : style_.incomingBubble,
            invalidTextureId, 10.0F});
        if (selectedId_ && *selectedId_ == message.id) {
            commands.push_back({{bubble.x, bubble.y, bubble.width, 2.0F},
                bubbleClip, style_.selectedOutline, invalidTextureId, 0.0F});
        }
        const float x = bubble.x + padding;
        float y = bubble.y + padding;
        drawn.author->appendPaintCommands({x, y}, bubbleClip,
            style_.text, commands);
        drawn.time->appendPaintCommands(
            {bubble.x + width - padding - drawn.time->size().width, y},
            bubbleClip, style_.secondaryText, commands);
        y += std::max(drawn.author->size().height,
            drawn.time->size().height) + 10.0F;
        for (const auto& block : drawn.blocks) {
            if (block.kind == MessageBlockKind::Text) {
                block.text->appendPaintCommands({x, y}, bubbleClip,
                    style_.text, commands);
            } else {
                const Rect code{x, y, width - 2.0F * padding,
                    block.height};
                const Rect codeClip = intersect(code, bubbleClip);
                commands.push_back({code, codeClip, style_.codeBackground,
                    invalidTextureId, 5.0F});
                block.languageLabel->appendPaintCommands(
                    {x + 10.0F, y + 5.0F}, codeClip,
                    style_.secondaryText, commands);
                float lineY = y + 28.0F;
                for (const auto& line : block.lines) {
                    float runX = x + 10.0F;
                    for (const auto& run : line) {
                        run.layout->appendPaintCommands({runX, lineY},
                            codeClip, tokenColor(run.kind), commands);
                        runX += run.layout->size().width;
                    }
                    lineY += style_.lineHeight;
                }
            }
            y += block.height + 8.0F;
        }
    }
}

bool ConversationList::acceptsPointerEvents() const noexcept { return true; }

bool ConversationList::onPointerEvent(const WidgetPointerEvent& event) {
    if (event.type == WidgetPointerEventType::Cancel) {
        pressedMessage_.reset();
        return true;
    }
    if (event.button != PointerButton::Primary) return false;
    if (event.type == WidgetPointerEventType::Press) {
        pressedMessage_ = hitMessage(event.position);
        return pressedMessage_.has_value();
    }
    if (event.type == WidgetPointerEventType::Release) {
        const auto released = hitMessage(event.position);
        if (pressedMessage_ && pressedMessage_ == released) {
            const std::string id = messages_[*pressedMessage_].id;
            selectedId_ = id;
            if (onSelectionChanged_) onSelectionChanged_(id);
        }
        pressedMessage_.reset();
        return released.has_value();
    }
    return false;
}

bool ConversationList::onScrollEvent(const WidgetScrollEvent& event) {
    const float delta = event.delta.y *
        (event.mode == ScrollDeltaMode::Line ? 36.0F : 1.0F);
    const float before = scrollOffset_;
    setScrollOffset(scrollOffset_ + delta);
    return before != scrollOffset_;
}

void ConversationList::rebuild() const {
    if (cachedWidth_ == bounds().width &&
        drawMessages_.size() == messages_.size()) return;
    drawMessages_.clear();
    cachedWidth_ = bounds().width;
    const float textWidth = std::max(1.0F,
        bubbleWidth() - style_.bubblePadding * 2.0F);
    TextStyle body;
    body.fontFamilies = {"Noto Sans KR"};
    body.fontSize = 14.0F;
    TextStyle meta = body;
    meta.fontSize = 11.0F;
    TextStyle code = body;
    code.fontSize = 14.0F;
    float y = style_.messageGap;
    for (const auto& message : messages_) {
        DrawMessage drawn;
        drawn.y = y;
        drawn.author = layout(*textEngine_, message.author, body);
        drawn.time = layout(*textEngine_, message.time, meta);
        float height = style_.bubblePadding * 2.0F +
            std::max(drawn.author->size().height,
                drawn.time->size().height) + 10.0F;
        for (const auto& block : message.blocks) {
            DrawBlock out;
            out.kind = block.kind;
            out.language = block.language;
            if (block.kind == MessageBlockKind::Text) {
                out.text = layout(*textEngine_, block.text, body, textWidth);
                out.height = out.text->size().height;
            } else {
                out.languageLabel = layout(*textEngine_,
                    block.language.empty() ? "CODE" : block.language, meta);
                std::size_t start = 0;
                while (start <= block.text.size()) {
                    const auto end = block.text.find('\n', start);
                    const auto line = std::string_view(block.text).substr(start,
                        end == std::string::npos ? std::string::npos :
                            end - start);
                    std::vector<DrawRun> runs;
                    for (auto& token : tokenizeCodeLine(block.language, line)) {
                        runs.push_back({layout(*textEngine_, token.text, code),
                            token.kind});
                    }
                    out.lines.push_back(std::move(runs));
                    if (end == std::string::npos) break;
                    start = end + 1;
                }
                out.height = 34.0F + style_.lineHeight *
                    static_cast<float>(out.lines.size());
            }
            height += out.height + 8.0F;
            drawn.blocks.push_back(std::move(out));
        }
        drawn.height = height;
        y += height + style_.messageGap;
        drawMessages_.push_back(std::move(drawn));
    }
    contentHeight_ = y;
}

void ConversationList::invalidate() noexcept {
    cachedWidth_ = -1.0F;
    drawMessages_.clear();
}

void ConversationList::clampScroll() noexcept {
    try {
        rebuild();
        scrollOffset_ = std::clamp(scrollOffset_, 0.0F,
            std::max(0.0F, contentHeight_ - bounds().height));
    } catch (...) { scrollOffset_ = 0.0F; }
}

float ConversationList::bubbleWidth() const noexcept {
    return std::max(1.0F, std::min(style_.maximumBubbleWidth,
        std::min(bounds().width * 0.78F,
            bounds().width - 2.0F * style_.sideMargin)));
}

Color ConversationList::tokenColor(CodeTokenKind kind) const noexcept {
    switch (kind) {
    case CodeTokenKind::Keyword: return style_.keyword;
    case CodeTokenKind::String: return style_.string;
    case CodeTokenKind::Number: return style_.number;
    case CodeTokenKind::Comment: return style_.comment;
    case CodeTokenKind::Plain: return style_.text;
    }
    return style_.text;
}

std::optional<std::size_t> ConversationList::hitMessage(Point point) const {
    if (!contains(clip(), point)) return std::nullopt;
    rebuild();
    for (std::size_t i = 0; i < messages_.size(); ++i) {
        if (contains(messageBounds(messages_[i].id), point)) return i;
    }
    return std::nullopt;
}

} // namespace lotui
