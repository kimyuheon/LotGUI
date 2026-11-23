#pragma once

#include "core/widget.h"

#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace lotui {

enum class DockSide { Top, Bottom, Left, Right };

struct DockWorkspaceStyle {
    float horizontalThickness{40.0F};
    float verticalThickness{48.0F};
    float minimumContentWidth{120.0F};
    float minimumContentHeight{100.0F};
    float dropZone{72.0F};
    Color toolbarBackground{0.16F, 0.18F, 0.21F, 1.0F};
    Color gripColor{0.50F, 0.56F, 0.62F, 1.0F};
    Color dockPreview{0.31F, 0.51F, 0.72F, 0.36F};
};

// The host owns command behavior. Commands remain the same widgets on every side.
class DockToolbar final : public Widget {
public:
    Widget& addCommand(std::unique_ptr<Widget> command, float extent = 44.0F);
    std::size_t commandCount() const noexcept;
    Widget& commandAt(std::size_t index);
    const Widget& commandAt(std::size_t index) const;
    Rect gripBounds() const noexcept;
    bool vertical() const noexcept;
    Size measure(const LayoutConstraints& constraints) const override;

protected:
    void onArrange() override;
    void onPaint(std::vector<PaintCommand>& commands) const override;
    void paintChildren(std::vector<PaintCommand>& commands) const override;
    void collectChildHitTestEntries(std::vector<HitTestEntry>& entries) const override;
    void collectChildFocusTargets(std::vector<PointerTargetId>& targets) const override;
    Widget* findChildByPointerTarget(PointerTargetId target) noexcept override;
    bool acceptsPointerEvents() const noexcept override;
    bool onPointerEvent(const WidgetPointerEvent& event) override;

private:
    friend class DockWorkspace;
    struct Command {
        std::unique_ptr<Widget> widget;
        float extent{44.0F};
    };
    void setVertical(bool vertical) noexcept;
    void setStyle(const DockWorkspaceStyle& style) noexcept;
    void setDragHandler(std::function<void(Point, bool)> handler);

    std::vector<Command> commands_;
    DockWorkspaceStyle style_{};
    std::function<void(Point, bool)> onDrag_{};
    bool vertical_{false};
    bool dragging_{false};
};

// One movable toolbar around an arbitrary retained content tree.
class DockWorkspace final : public Widget {
public:
    DockWorkspace(std::unique_ptr<Widget> content,
        std::unique_ptr<DockToolbar> toolbar,
        DockSide side = DockSide::Top,
        DockWorkspaceStyle style = {});
    Size measure(const LayoutConstraints& constraints) const override;
    void setSide(DockSide side);
    DockSide side() const noexcept;
    std::optional<DockSide> previewSide() const noexcept;
    DockToolbar& toolbar() noexcept;
    const DockToolbar& toolbar() const noexcept;
    Widget& content() noexcept;
    const Widget& content() const noexcept;

protected:
    void onArrange() override;
    void paintChildren(std::vector<PaintCommand>& commands) const override;
    void collectChildHitTestEntries(std::vector<HitTestEntry>& entries) const override;
    void collectChildFocusTargets(std::vector<PointerTargetId>& targets) const override;
    Widget* findChildByPointerTarget(PointerTargetId target) noexcept override;

private:
    std::optional<DockSide> targetAt(Point point) const noexcept;
    void onToolbarDrag(Point point, bool released);
    Rect dockRect(DockSide side) const noexcept;

    std::unique_ptr<Widget> content_;
    std::unique_ptr<DockToolbar> toolbar_;
    DockSide side_{DockSide::Top};
    std::optional<DockSide> preview_{};
    DockWorkspaceStyle style_{};
};

} // namespace lotui
