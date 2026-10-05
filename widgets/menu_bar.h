#pragma once

#include "text/text_layout.h"
#include "widgets/popup.h"

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace lotui {

enum class MenuEntryKind {
    Command,
    Toggle,
    Submenu,
    Separator,
};

struct MenuEntry {
    MenuEntryKind kind{MenuEntryKind::Command};
    std::string id;
    std::string label;
    std::string shortcut;
    bool enabled{true};
    bool checked{false};
    std::vector<MenuEntry> children;
};

struct Menu {
    std::string id;
    std::string label;
    std::vector<MenuEntry> entries;
};

struct MenuBarStyle {
    Color background{0.12F, 0.15F, 0.19F, 1.0F};
    Color popupBackground{0.15F, 0.18F, 0.22F, 1.0F};
    Color hovered{0.23F, 0.29F, 0.35F, 1.0F};
    Color selected{0.25F, 0.33F, 0.40F, 1.0F};
    Color text{0.93F, 0.95F, 0.97F, 1.0F};
    Color muted{0.65F, 0.70F, 0.75F, 1.0F};
    Color separator{0.34F, 0.39F, 0.44F, 1.0F};
    float barHeight{28.0F};
    float rowHeight{27.0F};
    float separatorHeight{9.0F};
    float popupWidth{230.0F};
    float horizontalPadding{11.0F};
    float popupPadding{4.0F};
};

class MenuBar final : public Widget {
public:
    using ActionHandler = std::function<void(std::string_view)>;

    explicit MenuBar(
        std::shared_ptr<const TextEngine> textEngine,
        std::vector<Menu> menus = {},
        ActionHandler onAction = {},
        MenuBarStyle style = {},
        TextStyle textStyle = {});

    Size measure(const LayoutConstraints& constraints) const override;
    void setPopupHost(PopupHost* host) noexcept;
    void setMenus(std::vector<Menu> menus);
    const std::vector<Menu>& menus() const noexcept;
    void setOnAction(ActionHandler onAction);
    bool isOpen() const noexcept;
    Rect menuBounds(std::size_t index) const;

protected:
    void onArrange() override;
    void onPaint(std::vector<PaintCommand>& commands) const override;
    bool acceptsPointerEvents() const noexcept override;
    bool onPointerEvent(const WidgetPointerEvent& event) override;
    bool acceptsFocus() const noexcept override;
    bool onFocusChanged(bool focused) override;
    bool onKeyEvent(const WidgetKeyEvent& event) override;

private:
    const TextLayout& labelLayout(std::size_t index) const;
    std::size_t menuAt(Point point) const noexcept;
    void open(std::size_t index);

    std::shared_ptr<const TextEngine> textEngine_;
    std::vector<Menu> menus_;
    std::vector<Rect> menuBounds_;
    mutable std::vector<std::unique_ptr<TextLayout>> labels_;
    ActionHandler onAction_{};
    MenuBarStyle style_{};
    TextStyle textStyle_{};
    PopupHost* host_{nullptr};
    PopupId popupId_{invalidPopupId};
    std::size_t openIndex_{static_cast<std::size_t>(-1)};
    std::size_t hoveredIndex_{static_cast<std::size_t>(-1)};
    bool focused_{false};
};

} // namespace lotui
