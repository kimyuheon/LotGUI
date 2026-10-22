#pragma once

#include "core/widget.h"
#include "text/text_layout.h"

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace lotui {

// Nodes are supplied in preorder. A child immediately follows its parent
// with depth one greater; IDs remain stable across model refreshes.
struct TreeNode {
    std::string id;
    std::string label;
    std::size_t depth{0};
    bool expanded{false};
    bool selectable{true};
};

struct TreeViewStyle {
    Color background{0.10F, 0.13F, 0.17F, 1.0F};
    Color hovered{0.18F, 0.24F, 0.29F, 1.0F};
    Color selected{0.16F, 0.36F, 0.43F, 1.0F};
    Color text{0.91F, 0.94F, 0.94F, 1.0F};
    Color disabledText{0.52F, 0.58F, 0.60F, 1.0F};
    Color disclosure{0.69F, 0.79F, 0.80F, 1.0F};
    Color focusLine{0.35F, 0.78F, 0.69F, 1.0F};
    float rowHeight{26.0F};
    float indent{16.0F};
    float padding{8.0F};
    float disclosureSize{12.0F};
    float labelSpacing{6.0F};
};

class TreeView final : public Widget {
public:
    using SelectionChangedHandler =
        std::function<void(std::optional<std::string_view>)>;
    using ExpansionChangedHandler =
        std::function<void(std::string_view, bool)>;
    using ActivatedHandler = std::function<void(std::string_view)>;

    TreeView(
        std::shared_ptr<const TextEngine> textEngine,
        std::vector<TreeNode> nodes = {},
        Size preferredSize = {260.0F, 240.0F},
        TreeViewStyle style = {},
        TextStyle textStyle = {});

    Size measure(const LayoutConstraints& constraints) const override;
    void setNodes(std::vector<TreeNode> nodes);
    const std::vector<TreeNode>& nodes() const noexcept;
    std::size_t visibleRowCount() const noexcept;
    bool setSelectedId(std::string_view id);
    std::optional<std::string> selectedId() const;
    bool setExpanded(std::string_view id, bool expanded);
    bool isExpanded(std::string_view id) const noexcept;
    Rect rowBounds(std::string_view id) const noexcept;
    void ensureVisible(std::string_view id) noexcept;
    void setScrollOffset(float offset) noexcept;
    float scrollOffset() const noexcept;
    void setEnabled(bool enabled) noexcept;
    bool isEnabled() const noexcept;
    void setOnSelectionChanged(SelectionChangedHandler handler);
    void setOnExpansionChanged(ExpansionChangedHandler handler);
    void setOnActivated(ActivatedHandler handler);

protected:
    void onArrange() override;
    void onPaint(std::vector<PaintCommand>& commands) const override;
    bool acceptsPointerEvents() const noexcept override;
    bool onPointerEvent(const WidgetPointerEvent& event) override;
    bool acceptsFocus() const noexcept override;
    bool onFocusChanged(bool focused) override;
    bool onKeyEvent(const WidgetKeyEvent& event) override;
    bool onScrollEvent(const WidgetScrollEvent& event) override;

private:
    std::optional<std::size_t> nodeIndex(std::string_view id) const noexcept;
    std::optional<std::size_t> visibleIndex(std::string_view id) const noexcept;
    std::optional<std::size_t> visibleAt(Point position) const noexcept;
    bool hasChildren(std::size_t nodeIndex) const noexcept;
    void rebuildVisible();
    void clampScroll() noexcept;
    bool select(std::size_t nodeIndex, bool notify);
    bool expand(std::size_t nodeIndex, bool value, bool notify);
    bool moveSelection(int direction);
    const TextLayout* layoutFor(const TreeNode& node) const;

    std::shared_ptr<const TextEngine> textEngine_;
    std::vector<TreeNode> nodes_;
    std::vector<std::size_t> visibleRows_;
    mutable std::unordered_map<std::string, std::unique_ptr<TextLayout>> layouts_;
    Size preferredSize_{};
    TreeViewStyle style_{};
    TextStyle textStyle_{};
    std::optional<std::string> selectedId_;
    std::optional<std::size_t> hoveredRow_;
    std::optional<std::size_t> pressedRow_;
    bool pressedDisclosure_{false};
    float scrollOffset_{0.0F};
    bool enabled_{true};
    bool focused_{false};
    SelectionChangedHandler onSelectionChanged_{};
    ExpansionChangedHandler onExpansionChanged_{};
    ActivatedHandler onActivated_{};
};

} // namespace lotui
