#pragma once

#include "text/text_layout.h"
#include "widgets/single_child_widget.h"

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace lotui {

struct RibbonGroupStyle {
    Color background{0.14F, 0.17F, 0.23F, 1.0F};
    Color titleColor{0.72F, 0.77F, 0.86F, 1.0F};
    Color separator{0.28F, 0.32F, 0.40F, 1.0F};
    EdgeInsets contentPadding{8.0F, 8.0F, 8.0F, 6.0F};
    float titleHeight{22.0F};
    float separatorWidth{1.0F};
    float cornerRadius{5.0F};
};

class RibbonGroup final : public SingleChildWidget {
public:
    RibbonGroup(
        std::shared_ptr<const TextEngine> textEngine,
        std::string title,
        std::unique_ptr<Widget> content = {},
        RibbonGroupStyle style = {},
        TextStyle titleStyle = {});

    Size measure(const LayoutConstraints& constraints) const override;

    const std::string& title() const noexcept;
    void setContent(std::unique_ptr<Widget> content);
    Widget* content() noexcept;
    const Widget* content() const noexcept;
    void setStyle(RibbonGroupStyle style) noexcept;
    const RibbonGroupStyle& style() const noexcept;

protected:
    void onArrange() override;
    void onPaint(std::vector<PaintCommand>& commands) const override;

private:
    const TextLayout& titleLayout() const;

    std::shared_ptr<const TextEngine> textEngine_;
    std::string title_;
    RibbonGroupStyle style_{};
    TextStyle titleStyle_{};
    mutable std::unique_ptr<TextLayout> titleLayout_;
};

class RibbonTab final : public SingleChildWidget {
public:
    RibbonTab(
        std::string id,
        std::string title,
        std::unique_ptr<Widget> content = {});

    Size measure(const LayoutConstraints& constraints) const override;

    const std::string& id() const noexcept;
    const std::string& title() const noexcept;
    void setContent(std::unique_ptr<Widget> content);
    Widget* content() noexcept;
    const Widget* content() const noexcept;

protected:
    void onArrange() override;

private:
    std::string id_;
    std::string title_;
};

struct RibbonStyle {
    Color background{0.09F, 0.11F, 0.16F, 1.0F};
    Color tabBar{0.12F, 0.15F, 0.21F, 1.0F};
    Color tabNormal{0.12F, 0.15F, 0.21F, 1.0F};
    Color tabHovered{0.20F, 0.25F, 0.34F, 1.0F};
    Color tabPressed{0.15F, 0.20F, 0.29F, 1.0F};
    Color tabSelected{0.18F, 0.23F, 0.32F, 1.0F};
    Color text{0.84F, 0.88F, 0.95F, 1.0F};
    Color selectedText{0.98F, 0.99F, 1.0F, 1.0F};
    Color focusRing{0.96F, 0.82F, 0.32F, 1.0F};
    EdgeInsets contentPadding{8.0F, 8.0F, 8.0F, 8.0F};
    float tabBarHeight{38.0F};
    float tabHorizontalPadding{16.0F};
    float tabSpacing{2.0F};
    float minimumTabWidth{72.0F};
    float tabCornerRadius{6.0F};
    float focusRingWidth{2.0F};
    float preferredHeight{150.0F};
};

class Ribbon final : public SingleChildWidget {
public:
    using TabChangedHandler =
        std::function<void(std::size_t, std::string_view)>;

    Ribbon(
        std::shared_ptr<const TextEngine> textEngine,
        RibbonStyle style = {},
        TextStyle tabTextStyle = {},
        TabChangedHandler onTabChanged = {});

    Size measure(const LayoutConstraints& constraints) const override;

    RibbonTab& addTab(std::unique_ptr<RibbonTab> tab);
    RibbonTab& addTab(std::unique_ptr<Widget> tab);
    std::size_t tabCount() const noexcept;
    RibbonTab& tabAt(std::size_t index);
    const RibbonTab& tabAt(std::size_t index) const;
    std::size_t selectedIndex() const noexcept;
    std::string_view selectedId() const noexcept;
    bool selectTab(std::size_t index);
    bool selectTab(std::string_view id);
    Rect tabHeaderBounds(std::size_t index) const;

    void setEnabled(bool enabled) noexcept;
    bool isEnabled() const noexcept;
    bool isFocused() const noexcept;
    void setStyle(RibbonStyle style) noexcept;
    const RibbonStyle& style() const noexcept;
    void setOnTabChanged(TabChangedHandler onTabChanged);

protected:
    void onArrange() override;
    void onPaint(std::vector<PaintCommand>& commands) const override;
    bool acceptsPointerEvents() const noexcept override;
    bool onPointerEvent(const WidgetPointerEvent& event) override;
    bool acceptsFocus() const noexcept override;
    bool onFocusChanged(bool focused) override;
    bool onKeyEvent(const WidgetKeyEvent& event) override;
    void collectChildHitTestEntries(
        std::vector<HitTestEntry>& entries) const override;
    void collectChildFocusTargets(
        std::vector<PointerTargetId>& targets) const override;

private:
    struct TabSlot {
        std::unique_ptr<Widget> widget;
        mutable std::unique_ptr<TextLayout> titleLayout;
        Rect headerBounds{};
    };

    static constexpr std::size_t noTab = static_cast<std::size_t>(-1);

    RibbonTab* tabPointer(std::size_t index) noexcept;
    const RibbonTab* tabPointer(std::size_t index) const noexcept;
    const TextLayout& titleLayout(std::size_t index) const;
    float tabWidth(std::size_t index) const;
    std::size_t tabAtPoint(Point point) const noexcept;
    void arrangeActiveContent();
    void invalidateTitleLayouts() noexcept;

    std::shared_ptr<const TextEngine> textEngine_;
    RibbonStyle style_{};
    TextStyle tabTextStyle_{};
    TabChangedHandler onTabChanged_{};
    std::vector<TabSlot> tabs_;
    std::size_t selectedIndex_{noTab};
    std::size_t hoveredIndex_{noTab};
    std::size_t pressedIndex_{noTab};
    bool enabled_{true};
    bool focused_{false};
    Rect contentBounds_{};
};

} // namespace lotui
