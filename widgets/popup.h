#pragma once

#include "core/widget.h"

#include <cstdint>
#include <functional>
#include <memory>

namespace lotui {

using PopupId = std::uint64_t;
inline constexpr PopupId invalidPopupId = 0;

enum class PopupPlacement {
    Auto,
    Anchor,
    BelowStart,
    BelowEnd,
    AboveStart,
    AboveEnd,
};

enum class PopupCloseReason {
    Accepted,
    Dismissed,
    Escape,
    Replaced,
};

struct PopupOptions {
    PopupPlacement placement{PopupPlacement::Auto};
    float gap{3.0F};
    bool matchAnchorWidth{true};
    bool exactAnchorWidth{false};
    bool exactAnchorHeight{false};
    bool dismissOnOutsidePress{true};
    bool dismissOnEscape{true};
};

using PopupClosedHandler = std::function<void(PopupCloseReason)>;

class PopupHost final : public Widget {
public:
    explicit PopupHost(std::unique_ptr<Widget> content);

    Size measure(const LayoutConstraints& constraints) const override;

    void setContent(std::unique_ptr<Widget> content);
    Widget* content() noexcept;
    const Widget* content() const noexcept;

    PopupId showPopup(
        std::unique_ptr<Widget> popup,
        Rect anchor,
        PopupOptions options = {},
        PopupClosedHandler onClosed = {});
    bool acceptPopup(PopupId id = invalidPopupId);
    bool dismissPopup(PopupId id = invalidPopupId);
    std::unique_ptr<Widget> takePopup() noexcept;
    Widget* popup() noexcept;
    const Widget* popup() const noexcept;
    PopupId popupId() const noexcept;
    bool hasPopup() const noexcept;
    Rect anchor() const noexcept;
    Rect popupBounds() const noexcept;

protected:
    void onArrange() override;
    void paintChildren(std::vector<PaintCommand>& commands) const override;
    void collectChildHitTestEntries(
        std::vector<HitTestEntry>& entries) const override;
    void collectChildFocusTargets(
        std::vector<PointerTargetId>& targets) const override;
    Widget* findChildByPointerTarget(
        PointerTargetId target) noexcept override;
    bool acceptsPointerEvents() const noexcept override;
    bool onPointerEvent(const WidgetPointerEvent& event) override;
    bool onPreviewKeyEvent(const WidgetKeyEvent& event) override;
    bool onUnhandledKeyEvent(const WidgetKeyEvent& event) override;
    PointerTargetId activeFocusScopeTarget() const noexcept override;

private:
    bool closePopup(PopupId id, PopupCloseReason reason);
    void arrangePopup();

    std::unique_ptr<Widget> content_;
    std::unique_ptr<Widget> popup_;
    mutable std::unique_ptr<Widget> dismissedPopup_;
    PopupId popupId_{invalidPopupId};
    PopupId nextPopupId_{1};
    Rect anchor_{};
    PopupOptions options_{};
    PopupClosedHandler onClosed_{};
};

} // namespace lotui
