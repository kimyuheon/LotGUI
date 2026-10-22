#include "widgets/tree_view.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace lotui {
namespace {

float dimension(float value) noexcept {
    return std::isfinite(value) ? std::max(0.0F, value) : 0.0F;
}

void sanitize(TreeViewStyle& style) noexcept {
    style.rowHeight = std::max(1.0F, dimension(style.rowHeight));
    style.indent = dimension(style.indent);
    style.padding = dimension(style.padding);
    style.disclosureSize = dimension(style.disclosureSize);
    style.labelSpacing = dimension(style.labelSpacing);
}

} // namespace

TreeView::TreeView(
    std::shared_ptr<const TextEngine> textEngine,
    std::vector<TreeNode> nodes,
    Size preferredSize,
    TreeViewStyle style,
    TextStyle textStyle)
    : textEngine_(std::move(textEngine)),
      preferredSize_(preferredSize),
      style_(style),
      textStyle_(std::move(textStyle)) {
    sanitize(style_);
    preferredSize_.width = dimension(preferredSize_.width);
    preferredSize_.height = dimension(preferredSize_.height);
    setNodes(std::move(nodes));
}

Size TreeView::measure(const LayoutConstraints& constraints) const {
    return constraints.constrain(preferredSize_);
}

void TreeView::setNodes(std::vector<TreeNode> nodes) {
    std::unordered_set<std::string> ids;
    for (std::size_t index = 0; index < nodes.size(); ++index) {
        const TreeNode& node = nodes[index];
        if (node.id.empty() || !ids.insert(node.id).second ||
            (index == 0 && node.depth != 0) ||
            (index > 0 && node.depth > nodes[index - 1].depth + 1)) {
            throw std::invalid_argument(
                "TreeView nodes need unique IDs and preorder depths");
        }
    }
    std::unordered_map<std::string, bool> expansion;
    for (const TreeNode& node : nodes_) expansion.emplace(node.id, node.expanded);
    for (TreeNode& node : nodes) {
        const auto previous = expansion.find(node.id);
        if (previous != expansion.end()) node.expanded = previous->second;
    }
    nodes_ = std::move(nodes);
    layouts_.clear();
    rebuildVisible();
    if (selectedId_) {
        const auto selected = nodeIndex(*selectedId_);
        if (!selected || !nodes_[*selected].selectable ||
            !visibleIndex(*selectedId_)) selectedId_.reset();
    }
    hoveredRow_.reset();
    pressedRow_.reset();
    clampScroll();
}

const std::vector<TreeNode>& TreeView::nodes() const noexcept {
    return nodes_;
}

std::size_t TreeView::visibleRowCount() const noexcept {
    return visibleRows_.size();
}

bool TreeView::setSelectedId(std::string_view id) {
    const auto index = nodeIndex(id);
    return index && visibleIndex(id) && select(*index, false);
}

std::optional<std::string> TreeView::selectedId() const {
    return selectedId_;
}

bool TreeView::setExpanded(std::string_view id, bool expanded) {
    const auto index = nodeIndex(id);
    return index && expand(*index, expanded, false);
}

bool TreeView::isExpanded(std::string_view id) const noexcept {
    const auto index = nodeIndex(id);
    return index && nodes_[*index].expanded;
}

Rect TreeView::rowBounds(std::string_view id) const noexcept {
    const auto row = visibleIndex(id);
    if (!row) return {};
    return {bounds().x,
        bounds().y + static_cast<float>(*row) * style_.rowHeight -
            scrollOffset_,
        bounds().width, style_.rowHeight};
}

void TreeView::ensureVisible(std::string_view id) noexcept {
    const auto row = visibleIndex(id);
    if (!row) return;
    const float top = static_cast<float>(*row) * style_.rowHeight;
    if (top < scrollOffset_) {
        setScrollOffset(top);
    } else if (top + style_.rowHeight > scrollOffset_ + bounds().height) {
        setScrollOffset(top + style_.rowHeight - bounds().height);
    }
}

void TreeView::setScrollOffset(float offset) noexcept {
    scrollOffset_ = dimension(offset);
    clampScroll();
}

float TreeView::scrollOffset() const noexcept { return scrollOffset_; }

void TreeView::setEnabled(bool enabled) noexcept {
    enabled_ = enabled;
    if (!enabled_) {
        hoveredRow_.reset();
        pressedRow_.reset();
        focused_ = false;
    }
}

bool TreeView::isEnabled() const noexcept { return enabled_; }

void TreeView::setOnSelectionChanged(SelectionChangedHandler handler) {
    onSelectionChanged_ = std::move(handler);
}

void TreeView::setOnExpansionChanged(ExpansionChangedHandler handler) {
    onExpansionChanged_ = std::move(handler);
}

void TreeView::setOnActivated(ActivatedHandler handler) {
    onActivated_ = std::move(handler);
}

void TreeView::onArrange() { clampScroll(); }

void TreeView::onPaint(std::vector<PaintCommand>& commands) const {
    commands.push_back({bounds(), clip(), style_.background,
        invalidTextureId, 0.0F});
    if (visibleRows_.empty()) return;
    const std::size_t first = std::min(visibleRows_.size(),
        static_cast<std::size_t>(scrollOffset_ / style_.rowHeight));
    const std::size_t count = static_cast<std::size_t>(
        std::ceil(bounds().height / style_.rowHeight)) + 1;
    const std::size_t last = std::min(visibleRows_.size(), first + count);
    for (std::size_t row = first; row < last; ++row) {
        const TreeNode& node = nodes_[visibleRows_[row]];
        const Rect rectangle = rowBounds(node.id);
        const Rect rowClip = intersect(rectangle, clip());
        if (!hasArea(rowClip)) continue;
        const bool selected = selectedId_ && *selectedId_ == node.id;
        if (selected || hoveredRow_ == row) {
            commands.push_back({rectangle, rowClip,
                selected ? style_.selected : style_.hovered,
                invalidTextureId, 0.0F});
        }
        const float iconX = rectangle.x + style_.padding +
            static_cast<float>(node.depth) * style_.indent;
        const float iconY = rectangle.y +
            (rectangle.height - style_.disclosureSize) * 0.5F;
        if (hasChildren(visibleRows_[row])) {
            const float middle = iconY + style_.disclosureSize * 0.5F;
            commands.push_back({{iconX + 2.0F, middle - 1.0F,
                std::max(0.0F, style_.disclosureSize - 4.0F), 2.0F},
                rowClip, style_.disclosure, invalidTextureId, 0.0F});
            if (!node.expanded) {
                commands.push_back({{iconX + style_.disclosureSize * 0.5F - 1.0F,
                    iconY + 2.0F, 2.0F,
                    std::max(0.0F, style_.disclosureSize - 4.0F)},
                    rowClip, style_.disclosure, invalidTextureId, 0.0F});
            }
        }
        if (const TextLayout* layout = layoutFor(node)) {
            const Point origin{iconX + style_.disclosureSize +
                    style_.labelSpacing,
                rectangle.y + std::max(0.0F,
                    (rectangle.height - layout->size().height) * 0.5F)};
            layout->appendPaintCommands(origin, rowClip,
                node.selectable ? style_.text : style_.disabledText,
                commands);
        }
    }
    if (focused_) {
        commands.push_back({{bounds().x, bounds().y + bounds().height - 1.0F,
            bounds().width, 1.0F}, clip(), style_.focusLine,
            invalidTextureId, 0.0F});
    }
}

bool TreeView::acceptsPointerEvents() const noexcept { return enabled_; }

bool TreeView::onPointerEvent(const WidgetPointerEvent& event) {
    if (!enabled_) return false;
    const auto row = visibleAt(event.position);
    switch (event.type) {
    case WidgetPointerEventType::Enter:
    case WidgetPointerEventType::Move:
        hoveredRow_ = row;
        return true;
    case WidgetPointerEventType::Leave:
        hoveredRow_.reset();
        return true;
    case WidgetPointerEventType::Press:
        if (event.button != PointerButton::Primary || !row) return false;
        pressedRow_ = row;
        hoveredRow_ = row;
        {
            const TreeNode& node = nodes_[visibleRows_[*row]];
            const float iconX = bounds().x + style_.padding +
                static_cast<float>(node.depth) * style_.indent;
            pressedDisclosure_ = hasChildren(visibleRows_[*row]) &&
                event.position.x >= iconX &&
                event.position.x < iconX + style_.disclosureSize +
                    style_.labelSpacing;
        }
        return true;
    case WidgetPointerEventType::Release:
        if (event.button != PointerButton::Primary || !pressedRow_) return false;
        if (row == pressedRow_) {
            const std::size_t index = visibleRows_[*row];
            if (pressedDisclosure_) {
                expand(index, !nodes_[index].expanded, true);
            } else {
                select(index, true);
                ensureVisible(nodes_[index].id);
            }
        }
        pressedRow_.reset();
        pressedDisclosure_ = false;
        return true;
    case WidgetPointerEventType::Cancel:
        pressedRow_.reset();
        pressedDisclosure_ = false;
        hoveredRow_.reset();
        return true;
    }
    return false;
}

bool TreeView::acceptsFocus() const noexcept { return enabled_; }

bool TreeView::onFocusChanged(bool focused) {
    const bool changed = focused_ != focused;
    focused_ = focused;
    return changed;
}

bool TreeView::onKeyEvent(const WidgetKeyEvent& event) {
    if (!enabled_ || event.type != WidgetKeyEventType::Press) return false;
    switch (event.key) {
    case KeyCode::Up:
        moveSelection(-1);
        return true;
    case KeyCode::Down:
        moveSelection(1);
        return true;
    case KeyCode::Home:
    case KeyCode::End: {
        const int direction = event.key == KeyCode::Home ? 1 : -1;
        int row = direction > 0 ? 0 :
            static_cast<int>(visibleRows_.size()) - 1;
        while (row >= 0 && row < static_cast<int>(visibleRows_.size())) {
            const std::size_t index = visibleRows_[static_cast<std::size_t>(row)];
            if (nodes_[index].selectable) {
                select(index, true);
                ensureVisible(nodes_[index].id);
                break;
            }
            row += direction;
        }
        return true;
    }
    case KeyCode::Right:
    case KeyCode::Left: {
        if (!selectedId_) return true;
        const auto index = nodeIndex(*selectedId_);
        if (!index) return true;
        TreeNode& node = nodes_[*index];
        if (event.key == KeyCode::Right) {
            if (hasChildren(*index) && !node.expanded) {
                expand(*index, true, true);
            } else if (hasChildren(*index)) {
                const auto row = visibleIndex(node.id);
                if (row && *row + 1 < visibleRows_.size()) {
                    select(visibleRows_[*row + 1], true);
                    ensureVisible(nodes_[visibleRows_[*row + 1]].id);
                }
            }
        } else if (hasChildren(*index) && node.expanded) {
            expand(*index, false, true);
        } else if (node.depth > 0) {
            for (std::size_t parent = *index; parent > 0; --parent) {
                if (nodes_[parent - 1].depth == node.depth - 1) {
                    select(parent - 1, true);
                    ensureVisible(nodes_[parent - 1].id);
                    break;
                }
            }
        }
        return true;
    }
    case KeyCode::Space:
        if (selectedId_) {
            const auto index = nodeIndex(*selectedId_);
            if (index && hasChildren(*index)) {
                expand(*index, !nodes_[*index].expanded, true);
            }
        }
        return true;
    case KeyCode::Enter:
        if (selectedId_ && onActivated_) {
            const std::string id = *selectedId_;
            ActivatedHandler callback = onActivated_;
            callback(id);
        }
        return true;
    default:
        return false;
    }
}

bool TreeView::onScrollEvent(const WidgetScrollEvent& event) {
    if (!enabled_) return false;
    const float delta = std::isfinite(event.delta.y) ?
        event.delta.y * (event.mode == ScrollDeltaMode::Line ?
            style_.rowHeight : 1.0F) : 0.0F;
    const float previous = scrollOffset_;
    setScrollOffset(scrollOffset_ + delta);
    return previous != scrollOffset_;
}

std::optional<std::size_t> TreeView::nodeIndex(
    std::string_view id) const noexcept {
    for (std::size_t index = 0; index < nodes_.size(); ++index) {
        if (nodes_[index].id == id) return index;
    }
    return std::nullopt;
}

std::optional<std::size_t> TreeView::visibleIndex(
    std::string_view id) const noexcept {
    for (std::size_t row = 0; row < visibleRows_.size(); ++row) {
        if (nodes_[visibleRows_[row]].id == id) return row;
    }
    return std::nullopt;
}

std::optional<std::size_t> TreeView::visibleAt(
    Point position) const noexcept {
    if (!contains(clip(), position)) return std::nullopt;
    const auto row = static_cast<std::size_t>(
        (position.y - bounds().y + scrollOffset_) / style_.rowHeight);
    return row < visibleRows_.size() ? std::optional{row} : std::nullopt;
}

bool TreeView::hasChildren(std::size_t index) const noexcept {
    return index + 1 < nodes_.size() &&
        nodes_[index + 1].depth > nodes_[index].depth;
}

void TreeView::rebuildVisible() {
    visibleRows_.clear();
    std::vector<bool> descendantVisible;
    for (std::size_t index = 0; index < nodes_.size(); ++index) {
        const TreeNode& node = nodes_[index];
        const bool visible = node.depth == 0 ||
            descendantVisible[node.depth - 1];
        descendantVisible.resize(node.depth + 1);
        descendantVisible[node.depth] = visible && node.expanded;
        if (visible) visibleRows_.push_back(index);
    }
}

void TreeView::clampScroll() noexcept {
    scrollOffset_ = std::clamp(dimension(scrollOffset_), 0.0F,
        std::max(0.0F, static_cast<float>(visibleRows_.size()) *
            style_.rowHeight - bounds().height));
}

bool TreeView::select(std::size_t index, bool notify) {
    if (index >= nodes_.size() || !nodes_[index].selectable ||
        selectedId_ == nodes_[index].id) return false;
    selectedId_ = nodes_[index].id;
    if (notify && onSelectionChanged_) {
        const std::string id = *selectedId_;
        SelectionChangedHandler callback = onSelectionChanged_;
        callback(id);
    }
    return true;
}

bool TreeView::expand(std::size_t index, bool value, bool notify) {
    if (!hasChildren(index) || nodes_[index].expanded == value) return false;
    const std::string id = nodes_[index].id;
    nodes_[index].expanded = value;
    rebuildVisible();
    if (selectedId_ && !visibleIndex(*selectedId_)) {
        if (!select(index, notify)) {
            selectedId_.reset();
            if (notify && onSelectionChanged_) {
                SelectionChangedHandler callback = onSelectionChanged_;
                callback(std::nullopt);
            }
        }
    }
    hoveredRow_.reset();
    pressedRow_.reset();
    clampScroll();
    if (notify && onExpansionChanged_) {
        ExpansionChangedHandler callback = onExpansionChanged_;
        callback(id, value);
    }
    return true;
}

bool TreeView::moveSelection(int direction) {
    if (visibleRows_.empty()) return false;
    int row = selectedId_ ? static_cast<int>(*visibleIndex(*selectedId_)) :
        (direction > 0 ? -1 : static_cast<int>(visibleRows_.size()));
    row += direction;
    while (row >= 0 && row < static_cast<int>(visibleRows_.size())) {
        const std::size_t index = visibleRows_[static_cast<std::size_t>(row)];
        if (nodes_[index].selectable) {
            const bool changed = select(index, true);
            ensureVisible(nodes_[index].id);
            return changed;
        }
        row += direction;
    }
    return false;
}

const TextLayout* TreeView::layoutFor(const TreeNode& node) const {
    if (!textEngine_) return nullptr;
    const auto found = layouts_.find(node.id);
    if (found != layouts_.end()) return found->second.get();
    auto layout = textEngine_->createLayout(node.label, textStyle_, {});
    const TextLayout* result = layout.get();
    layouts_.emplace(node.id, std::move(layout));
    return result;
}

} // namespace lotui
