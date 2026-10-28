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
    style.connectorWidth = dimension(style.connectorWidth);
    style.checkboxSize = dimension(style.checkboxSize);
    style.checkboxSpacing = dimension(style.checkboxSpacing);
}

void appendLine(std::vector<PaintCommand>& commands, Rect bounds,
    Rect clip, Color color) {
    if (hasArea(bounds)) {
        commands.push_back({bounds, clip, color, invalidTextureId, 0.0F,
            {0.0F, 0.0F, 1.0F, 1.0F}, true});
    }
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
    struct NodeState { bool expanded; bool checkable; bool checked; bool mixed; };
    std::unordered_map<std::string, NodeState> state;
    for (const TreeNode& node : nodes_) {
        state.emplace(node.id,
            NodeState{node.expanded, node.checkable, node.checked, node.mixed});
    }
    for (TreeNode& node : nodes) {
        const auto previous = state.find(node.id);
        if (previous != state.end()) {
            node.expanded = previous->second.expanded;
            if (node.checkable && previous->second.checkable) {
                node.checked = previous->second.checked;
                node.mixed = previous->second.mixed;
            }
        }
        if (node.mixed) node.checked = false;
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
    pressedDisclosure_ = false;
    pressedCheckbox_ = false;
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

bool TreeView::setChecked(std::string_view id, bool checked) {
    const auto index = nodeIndex(id);
    return index && check(*index, checked, false);
}

bool TreeView::isChecked(std::string_view id) const noexcept {
    const auto index = nodeIndex(id);
    return index && nodes_[*index].checkable && nodes_[*index].checked &&
        !nodes_[*index].mixed;
}

bool TreeView::setCheckState(std::string_view id, TreeCheckState state) {
    const auto index = nodeIndex(id);
    if (!index || !nodes_[*index].checkable) return false;
    TreeNode& node = nodes_[*index];
    const bool checked = state == TreeCheckState::Checked;
    const bool mixed = state == TreeCheckState::Mixed;
    if (node.checked == checked && node.mixed == mixed) return false;
    node.checked = checked;
    node.mixed = mixed;
    return true;
}

TreeCheckState TreeView::checkState(std::string_view id) const noexcept {
    const auto index = nodeIndex(id);
    if (!index || !nodes_[*index].checkable) return TreeCheckState::Unchecked;
    if (nodes_[*index].mixed) return TreeCheckState::Mixed;
    return nodes_[*index].checked ?
        TreeCheckState::Checked : TreeCheckState::Unchecked;
}

bool TreeView::setLabel(std::string_view id, std::string label) {
    const auto index = nodeIndex(id);
    if (!index || nodes_[*index].label == label) return false;
    nodes_[*index].label = std::move(label);
    layouts_.erase(nodes_[*index].id);
    return true;
}

Rect TreeView::rowBounds(std::string_view id) const noexcept {
    const auto row = visibleIndex(id);
    if (!row) return {};
    return {bounds().x,
        bounds().y + static_cast<float>(*row) * style_.rowHeight -
            scrollOffset_,
        bounds().width, style_.rowHeight};
}

Rect TreeView::labelBounds(std::string_view id) const noexcept {
    const auto index = nodeIndex(id);
    if (!index || !visibleIndex(id)) return {};
    const Rect row = rowBounds(id);
    const TreeNode& node = nodes_[*index];
    const float x = row.x + style_.padding +
        static_cast<float>(node.depth) * style_.indent +
        style_.disclosureSize + style_.labelSpacing +
        (node.checkable ? style_.checkboxSize + style_.checkboxSpacing : 0.0F);
    return {x, row.y, std::max(0.0F,
        row.x + row.width - style_.padding - x), row.height};
}

bool TreeView::requestRename(std::string_view id) {
    const auto index = nodeIndex(id);
    if (!enabled_ || !onRenameRequested_ || !index ||
        !nodes_[*index].selectable || !visibleIndex(id)) return false;
    ensureVisible(id);
    const Rect anchor = labelBounds(id);
    if (!hasArea(intersect(anchor, clip()))) return false;
    const std::string stableId = nodes_[*index].id;
    RenameRequestedHandler callback = onRenameRequested_;
    callback(stableId, anchor);
    return true;
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
        pressedDisclosure_ = false;
        pressedCheckbox_ = false;
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

void TreeView::setOnCheckedChanged(CheckedChangedHandler handler) {
    onCheckedChanged_ = std::move(handler);
}

void TreeView::setOnRenameRequested(RenameRequestedHandler handler) {
    onRenameRequested_ = std::move(handler);
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
        const std::size_t index = visibleRows_[row];
        const TreeNode& node = nodes_[index];
        const Rect rectangle{bounds().x,
            bounds().y + static_cast<float>(row) * style_.rowHeight -
                scrollOffset_,
            bounds().width, style_.rowHeight};
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
        const float middle = rectangle.y + rectangle.height * 0.5F;
        if (style_.connectorWidth > 0.0F) {
            std::size_t branch = index;
            auto ancestor = parentIndices_[branch];
            bool directParent = true;
            while (ancestor) {
                const TreeNode& parent = nodes_[*ancestor];
                const float x = rectangle.x + style_.padding +
                    static_cast<float>(parent.depth) * style_.indent +
                    style_.disclosureSize * 0.5F;
                if (directParent || followingSibling_[branch]) {
                    const float bottom = !directParent ||
                        followingSibling_[branch] ?
                        rectangle.y + rectangle.height : middle;
                    appendLine(commands,
                        {x - style_.connectorWidth * 0.5F, rectangle.y,
                            style_.connectorWidth, bottom - rectangle.y},
                        rowClip, style_.connector);
                }
                if (directParent) {
                    const float end = hasChildren(index) ? iconX :
                        iconX + style_.disclosureSize * 0.5F;
                    appendLine(commands,
                        {x, middle - style_.connectorWidth * 0.5F,
                            std::max(0.0F, end - x), style_.connectorWidth},
                        rowClip, style_.connector);
                }
                branch = *ancestor;
                ancestor = parentIndices_[branch];
                directParent = false;
            }
            if (node.expanded && hasChildren(index)) {
                const float x = iconX + style_.disclosureSize * 0.5F;
                appendLine(commands,
                    {x - style_.connectorWidth * 0.5F, middle,
                        style_.connectorWidth,
                        rectangle.y + rectangle.height - middle},
                    rowClip, style_.connector);
            }
        }
        if (hasChildren(index)) {
            const Rect box{iconX, iconY, style_.disclosureSize,
                style_.disclosureSize};
            commands.push_back({box, rowClip, style_.disclosure,
                invalidTextureId, 2.0F});
            commands.push_back({{box.x + 1.0F, box.y + 1.0F,
                std::max(0.0F, box.width - 2.0F),
                std::max(0.0F, box.height - 2.0F)}, rowClip,
                style_.background, invalidTextureId, 1.0F});
            const float iconMiddle = iconY + style_.disclosureSize * 0.5F;
            commands.push_back({{iconX + 2.0F, iconMiddle - 1.0F,
                std::max(0.0F, style_.disclosureSize - 4.0F), 2.0F},
                rowClip, style_.disclosure, invalidTextureId, 0.0F});
            if (!node.expanded) {
                commands.push_back({{iconX + style_.disclosureSize * 0.5F - 1.0F,
                    iconY + 2.0F, 2.0F,
                    std::max(0.0F, style_.disclosureSize - 4.0F)},
                    rowClip, style_.disclosure, invalidTextureId, 0.0F});
            }
        } else if (node.depth > 0) {
            commands.push_back({{iconX + style_.disclosureSize * 0.5F - 1.5F,
                middle - 1.5F, 3.0F, 3.0F}, rowClip,
                style_.connector, invalidTextureId, 1.5F});
        }
        if (node.checkable) {
            const Rect box = checkboxBounds(index, rectangle);
            commands.push_back({box, rowClip, style_.checkboxOutline,
                invalidTextureId, 2.0F});
            commands.push_back({{box.x + 1.0F, box.y + 1.0F,
                std::max(0.0F, box.width - 2.0F),
                std::max(0.0F, box.height - 2.0F)}, rowClip,
                (node.checked || node.mixed) ? style_.checkboxChecked :
                    style_.checkboxBackground,
                invalidTextureId, 1.0F});
            if (node.mixed && box.width >= 10.0F) {
                commands.push_back({{box.x + (box.width - 8.0F) * 0.5F,
                    box.y + (box.height - 2.0F) * 0.5F, 8.0F, 2.0F},
                    rowClip, style_.checkboxMark,
                    invalidTextureId, 0.0F});
            } else if (node.checked && box.width >= 10.0F) {
                const float x = box.x + (box.width - 8.0F) * 0.5F;
                const float y = box.y + (box.height - 8.0F) * 0.5F;
                for (const Point mark : {Point{x, y + 3.0F},
                         Point{x + 2.0F, y + 5.0F},
                         Point{x + 4.0F, y + 3.0F},
                         Point{x + 6.0F, y + 1.0F}}) {
                    commands.push_back({{mark.x, mark.y, 2.0F, 2.0F},
                        rowClip, style_.checkboxMark,
                        invalidTextureId, 0.0F});
                }
            }
        }
        if (const TextLayout* layout = layoutFor(node)) {
            const Point origin{iconX + style_.disclosureSize +
                    style_.labelSpacing + (node.checkable ?
                        style_.checkboxSize + style_.checkboxSpacing : 0.0F),
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
            pressedCheckbox_ = !pressedDisclosure_ && node.checkable &&
                contains(checkboxBounds(visibleRows_[*row],
                    rowBounds(node.id)), event.position);
        }
        return true;
    case WidgetPointerEventType::Release:
        if (event.button != PointerButton::Primary || !pressedRow_) return false;
        if (row == pressedRow_) {
            const std::size_t index = visibleRows_[*row];
            if (pressedDisclosure_) {
                expand(index, !nodes_[index].expanded, true);
            } else if (pressedCheckbox_) {
                if (contains(checkboxBounds(index, rowBounds(nodes_[index].id)),
                        event.position)) {
                    check(index, !nodes_[index].checked, true);
                }
            } else {
                select(index, true);
                ensureVisible(nodes_[index].id);
            }
        }
        pressedRow_.reset();
        pressedDisclosure_ = false;
        pressedCheckbox_ = false;
        return true;
    case WidgetPointerEventType::Cancel:
        pressedRow_.reset();
        pressedDisclosure_ = false;
        pressedCheckbox_ = false;
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
            if (index && nodes_[*index].checkable) {
                check(*index, !nodes_[*index].checked, true);
            } else if (index && hasChildren(*index)) {
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
    case KeyCode::F2:
        return selectedId_ && requestRename(*selectedId_);
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
    parentIndices_.assign(nodes_.size(), std::nullopt);
    followingSibling_.assign(nodes_.size(), false);
    std::vector<bool> descendantVisible;
    std::vector<std::size_t> previousAtDepth;
    for (std::size_t index = 0; index < nodes_.size(); ++index) {
        const TreeNode& node = nodes_[index];
        previousAtDepth.resize(node.depth + 1, nodes_.size());
        if (previousAtDepth[node.depth] != nodes_.size()) {
            followingSibling_[previousAtDepth[node.depth]] = true;
        }
        if (node.depth > 0) {
            parentIndices_[index] = previousAtDepth[node.depth - 1];
        }
        previousAtDepth[node.depth] = index;
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

bool TreeView::check(std::size_t index, bool checked, bool notify) {
    if (index >= nodes_.size() || !nodes_[index].checkable ||
        (nodes_[index].checked == checked && !nodes_[index].mixed)) return false;
    nodes_[index].checked = checked;
    nodes_[index].mixed = false;
    if (notify && onCheckedChanged_) {
        const std::string id = nodes_[index].id;
        CheckedChangedHandler callback = onCheckedChanged_;
        callback(id, checked);
    }
    return true;
}

Rect TreeView::checkboxBounds(std::size_t index, Rect row) const noexcept {
    const float size = std::min(style_.checkboxSize, row.height);
    const float x = row.x + style_.padding +
        static_cast<float>(nodes_[index].depth) * style_.indent +
        style_.disclosureSize + style_.labelSpacing;
    return {x, row.y + (row.height - size) * 0.5F, size, size};
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
