#pragma once

#include "widgets/label.h"
#include "widgets/single_child_widget.h"

#include <functional>
#include <memory>
#include <string>

namespace lotui {

struct DisclosurePanelStyle {
    Color normal{0.18F, 0.20F, 0.23F, 1.0F};
    Color hovered{0.23F, 0.26F, 0.29F, 1.0F};
    Color pressed{0.14F, 0.16F, 0.19F, 1.0F};
    Color indicator{0.80F, 0.84F, 0.86F, 1.0F};
    Color focusLine{0.32F, 0.73F, 0.72F, 1.0F};
    float headerHeight{28.0F};
    float headerPadding{9.0F};
    float indicatorSize{8.0F};
    float labelSpacing{8.0F};
    float contentIndent{16.0F};
    float contentSpacing{3.0F};
};

class DisclosurePanel final : public SingleChildWidget {
public:
    using ChangedHandler = std::function<void(bool)>;

    DisclosurePanel(
        std::shared_ptr<const TextEngine> textEngine,
        std::string title,
        std::unique_ptr<Widget> content = {},
        bool expanded = true,
        ChangedHandler onChanged = {},
        DisclosurePanelStyle style = {},
        TextStyle titleStyle = {});

    Size measure(const LayoutConstraints& constraints) const override;

    void setTitle(std::string title);
    const std::string& title() const noexcept;
    void setContent(std::unique_ptr<Widget> content);
    Widget* content() noexcept;
    const Widget* content() const noexcept;

    void setExpanded(bool expanded);
    bool isExpanded() const noexcept;
    void toggle();
    void setEnabled(bool enabled) noexcept;
    bool isEnabled() const noexcept;
    void setOnChanged(ChangedHandler onChanged);
    void setStyle(DisclosurePanelStyle style) noexcept;
    const DisclosurePanelStyle& style() const noexcept;

protected:
    void onArrange() override;
    void onPaint(std::vector<PaintCommand>& commands) const override;
    void paintChildren(std::vector<PaintCommand>& commands) const override;
    void collectChildHitTestEntries(
        std::vector<HitTestEntry>& entries) const override;
    void collectChildFocusTargets(
        std::vector<PointerTargetId>& targets) const override;
    Widget* findChildByPointerTarget(
        PointerTargetId target) noexcept override;
    bool acceptsPointerEvents() const noexcept override;
    bool onPointerEvent(const WidgetPointerEvent& event) override;
    bool acceptsFocus() const noexcept override;
    bool onFocusChanged(bool focused) override;
    bool onKeyEvent(const WidgetKeyEvent& event) override;

private:
    Rect headerBounds() const noexcept;
    void activate();

    Label titleLabel_;
    ChangedHandler onChanged_{};
    DisclosurePanelStyle style_{};
    bool expanded_{true};
    bool enabled_{true};
    bool hovered_{false};
    bool pressed_{false};
    bool focused_{false};
    bool keyboardPressed_{false};
};

} // namespace lotui
