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

enum class MessageBlockKind { Text, Code };

struct MessageBlock {
    MessageBlockKind kind{MessageBlockKind::Text};
    std::string text;
    std::string language;
};

struct ConversationMessage {
    std::string id;
    std::string author;
    std::string time;
    bool outgoing{false};
    std::vector<MessageBlock> blocks;
};

enum class CodeTokenKind { Plain, Keyword, String, Number, Comment };

struct CodeToken {
    std::string text;
    CodeTokenKind kind{CodeTokenKind::Plain};
};

// Fences at the start of a line become code blocks; ordinary lines stay text.
std::vector<MessageBlock> parseMessageBlocks(std::string_view markdown);
std::vector<CodeToken> tokenizeCodeLine(
    std::string_view language, std::string_view line);

struct ConversationListStyle {
    Color background{0.10F, 0.13F, 0.18F, 1.0F};
    Color incomingBubble{0.19F, 0.24F, 0.31F, 1.0F};
    Color outgoingBubble{0.17F, 0.37F, 0.44F, 1.0F};
    Color codeBackground{0.08F, 0.11F, 0.16F, 1.0F};
    Color text{0.95F, 0.96F, 0.97F, 1.0F};
    Color secondaryText{0.70F, 0.76F, 0.80F, 1.0F};
    Color keyword{0.53F, 0.72F, 1.00F, 1.0F};
    Color string{0.74F, 0.88F, 0.60F, 1.0F};
    Color number{0.94F, 0.69F, 0.50F, 1.0F};
    Color comment{0.57F, 0.64F, 0.67F, 1.0F};
    Color selectedOutline{0.42F, 0.81F, 0.86F, 1.0F};
    float maximumBubbleWidth{540.0F};
    float sideMargin{18.0F};
    float messageGap{14.0F};
    float bubblePadding{14.0F};
    float lineHeight{21.0F};
};

class ConversationList final : public Widget {
public:
    using SelectionHandler = std::function<void(std::string_view)>;

    ConversationList(std::shared_ptr<const TextEngine> textEngine,
        std::vector<ConversationMessage> messages = {},
        Size preferredSize = {720.0F, 480.0F},
        ConversationListStyle style = {});

    Size measure(const LayoutConstraints& constraints) const override;
    void setMessages(std::vector<ConversationMessage> messages);
    void appendMessage(ConversationMessage message);
    const std::vector<ConversationMessage>& messages() const noexcept;
    void setScrollOffset(float offset) noexcept;
    float scrollOffset() const noexcept;
    void scrollToEnd() noexcept;
    float contentHeight() const;
    Rect messageBounds(std::string_view id) const;
    std::optional<std::string> selectedId() const;
    void setOnSelectionChanged(SelectionHandler handler);

protected:
    void onArrange() override;
    void onPaint(std::vector<PaintCommand>& commands) const override;
    bool acceptsPointerEvents() const noexcept override;
    bool onPointerEvent(const WidgetPointerEvent& event) override;
    bool onScrollEvent(const WidgetScrollEvent& event) override;

private:
    struct DrawRun {
        std::unique_ptr<TextLayout> layout;
        CodeTokenKind kind{CodeTokenKind::Plain};
    };
    struct DrawBlock {
        MessageBlockKind kind{MessageBlockKind::Text};
        std::string language;
        std::unique_ptr<TextLayout> text;
        std::unique_ptr<TextLayout> languageLabel;
        std::vector<std::vector<DrawRun>> lines;
        float height{0.0F};
    };
    struct DrawMessage {
        std::unique_ptr<TextLayout> author;
        std::unique_ptr<TextLayout> time;
        std::vector<DrawBlock> blocks;
        float y{0.0F};
        float height{0.0F};
    };

    void rebuild() const;
    void invalidate() noexcept;
    void clampScroll() noexcept;
    float bubbleWidth() const noexcept;
    Color tokenColor(CodeTokenKind kind) const noexcept;
    std::optional<std::size_t> hitMessage(Point point) const;

    std::shared_ptr<const TextEngine> textEngine_;
    std::vector<ConversationMessage> messages_;
    Size preferredSize_{};
    ConversationListStyle style_{};
    mutable std::vector<DrawMessage> drawMessages_;
    mutable float cachedWidth_{-1.0F};
    mutable float contentHeight_{0.0F};
    float scrollOffset_{0.0F};
    std::optional<std::string> selectedId_;
    std::optional<std::size_t> pressedMessage_;
    SelectionHandler onSelectionChanged_{};
};

} // namespace lotui
