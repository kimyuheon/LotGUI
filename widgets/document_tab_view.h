#pragma once

#include "core/widget.h"
#include "text/text_layout.h"

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace lotui {

struct DocumentTab {
    std::string id;
    std::string title;
    bool dirty{false};
    bool closable{true};
};

struct DocumentTabStyle {
    Color background{0.12F, 0.14F, 0.17F, 1.0F};
    Color normal{0.17F, 0.19F, 0.22F, 1.0F};
    Color hovered{0.24F, 0.27F, 0.31F, 1.0F};
    Color selected{0.25F, 0.30F, 0.36F, 1.0F};
    Color accent{0.42F, 0.65F, 0.85F, 1.0F};
    Color text{0.91F, 0.93F, 0.95F, 1.0F};
    Color muted{0.67F, 0.72F, 0.77F, 1.0F};
    float height{36.0F};
    float minimumTabWidth{82.0F};
    float maximumTabWidth{180.0F};
    float horizontalPadding{12.0F};
    float controlWidth{30.0F};
    float spacing{2.0F};
};

class DocumentTabView final : public Widget {
public:
    using TabHandler = std::function<void(std::string_view)>;
    using AddHandler = std::function<void()>;

    explicit DocumentTabView(
        std::shared_ptr<const TextEngine> textEngine,
        DocumentTabStyle style = {},
        TextStyle textStyle = {});

    Size measure(const LayoutConstraints& constraints) const override;

    void setTabs(std::vector<DocumentTab> tabs);
    const std::vector<DocumentTab>& tabs() const noexcept;
    bool setSelectedId(std::string_view id);
    std::string_view selectedId() const noexcept;
    void setOnActivate(TabHandler handler);
    void setOnClose(TabHandler handler);
    void setOnAdd(AddHandler handler);
    Rect tabBounds(std::size_t index) const;
    Rect closeBounds(std::size_t index) const;
    Rect addBounds() const noexcept;
    Rect previousBounds() const noexcept;
    Rect nextBounds() const noexcept;

protected:
    void onArrange() override;
    void onPaint(std::vector<PaintCommand>& commands) const override;
    bool acceptsPointerEvents() const noexcept override;
    bool onPointerEvent(const WidgetPointerEvent& event) override;
    bool acceptsFocus() const noexcept override;
    bool onFocusChanged(bool focused) override;
    bool onKeyEvent(const WidgetKeyEvent& event) override;

private:
    enum class Part { None, Tab, Close, Add, Previous, Next };
    struct Target {
        Part part{Part::None};
        std::size_t index{0};
        bool operator==(const Target& other) const noexcept {
            return part == other.part && index == other.index;
        }
    };
    struct Slot {
        Rect tab{};
        Rect close{};
        mutable std::unique_ptr<TextLayout> title;
    };

    const TextLayout& titleLayout(std::size_t index) const;
    float widthFor(std::size_t index) const;
    Target hit(Point point) const noexcept;
    void activate(std::size_t index);
    void ensureSelectedVisible();

    std::shared_ptr<const TextEngine> textEngine_;
    DocumentTabStyle style_{};
    TextStyle textStyle_{};
    std::vector<DocumentTab> tabs_;
    std::vector<Slot> slots_;
    std::string selectedId_;
    std::size_t firstVisible_{0};
    std::size_t visibleEnd_{0};
    Rect add_{};
    Rect previous_{};
    Rect next_{};
    TabHandler onActivate_{};
    TabHandler onClose_{};
    AddHandler onAdd_{};
    Target hovered_{};
    Target pressed_{};
    bool focused_{false};
};

} // namespace lotui
