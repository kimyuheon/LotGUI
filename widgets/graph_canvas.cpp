#include "widgets/graph_canvas.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace lotui {
namespace {

bool finite(Point p) noexcept {
    return std::isfinite(p.x) && std::isfinite(p.y);
}

bool finite(Size s) noexcept {
    return std::isfinite(s.width) && std::isfinite(s.height);
}

const GraphNode* findNode(const std::vector<GraphNode>& nodes,
    std::string_view id) noexcept {
    const auto it = std::find_if(nodes.begin(), nodes.end(),
        [id](const GraphNode& node) { return node.id == id; });
    return it == nodes.end() ? nullptr : &*it;
}

} // namespace

GraphCanvas::GraphCanvas(std::shared_ptr<const TextEngine> textEngine,
    Size preferredSize, GraphCanvasStyle style, TextStyle textStyle)
    : textEngine_(std::move(textEngine)), preferredSize_(preferredSize),
      style_(style), textStyle_(std::move(textStyle)) {
    if (!textEngine_ || !isValidTextStyle(textStyle_) ||
        !finite(preferredSize_) || preferredSize_.width <= 0.0F ||
        preferredSize_.height <= 0.0F)
        throw std::invalid_argument("invalid GraphCanvas configuration");
}

Size GraphCanvas::measure(const LayoutConstraints& constraints) const {
    return constraints.constrain(preferredSize_);
}

void GraphCanvas::setGraph(std::vector<GraphNode> nodes,
    std::vector<GraphLink> links) {
    std::unordered_map<std::string, const GraphNode*> byId;
    for (const auto& node : nodes) {
        const std::size_t rows = std::max(node.inputs.size(), node.outputs.size());
        if (node.id.empty() || !finite(node.position) || !finite(node.size) ||
            node.size.width < 80.0F ||
            node.size.height < 44.0F + 24.0F * rows ||
            !byId.emplace(node.id, &node).second)
            throw std::invalid_argument("invalid GraphCanvas node");
    }
    std::unordered_set<std::string> linkIds;
    for (const auto& link : links) {
        const auto from = byId.find(link.fromNode);
        const auto to = byId.find(link.toNode);
        if (link.id.empty() || !linkIds.insert(link.id).second ||
            from == byId.end() || to == byId.end() ||
            link.fromPort >= from->second->outputs.size() ||
            link.toPort >= to->second->inputs.size())
            throw std::invalid_argument("invalid GraphCanvas link");
    }
    nodes_ = std::move(nodes);
    links_ = std::move(links);
    drag_ = DragMode::None;
    if (selected_ && !findNode(nodes_, *selected_)) select(std::nullopt);
}

const std::vector<GraphNode>& GraphCanvas::nodes() const noexcept {
    return nodes_;
}

const std::vector<GraphLink>& GraphCanvas::links() const noexcept {
    return links_;
}

std::optional<std::string> GraphCanvas::selectedNode() const {
    return selected_;
}

void GraphCanvas::select(std::optional<std::string> id) {
    if (selected_ == id) return;
    selected_ = std::move(id);
    if (onSelectionChanged_) onSelectionChanged_(selected_);
}

bool GraphCanvas::setSelectedNode(std::string_view id) {
    if (!findNode(nodes_, id)) return false;
    select(std::string(id));
    return true;
}

Point GraphCanvas::screen(Point world) const noexcept {
    return {bounds().x + offset_.x + world.x * zoom_,
        bounds().y + offset_.y + world.y * zoom_};
}

Rect GraphCanvas::nodeBounds(std::string_view id) const noexcept {
    const GraphNode* node = findNode(nodes_, id);
    if (!node) return {};
    const Point origin = screen(node->position);
    return {origin.x, origin.y, node->size.width * zoom_,
        node->size.height * zoom_};
}

Point GraphCanvas::port(const GraphNode& node, bool output,
    std::size_t index) const noexcept {
    return screen({node.position.x + (output ? node.size.width : 0.0F),
        node.position.y + 47.0F + static_cast<float>(index) * 24.0F});
}

Point GraphCanvas::offset() const noexcept { return offset_; }
float GraphCanvas::zoomLevel() const noexcept { return zoom_; }

void GraphCanvas::fitToContent() noexcept {
    if (nodes_.empty() || !hasArea(bounds())) {
        offset_ = {40.0F, 40.0F};
        zoom_ = 1.0F;
        return;
    }
    float left = nodes_.front().position.x;
    float top = nodes_.front().position.y;
    float right = left + nodes_.front().size.width;
    float bottom = top + nodes_.front().size.height;
    for (const auto& node : nodes_) {
        left = std::min(left, node.position.x);
        top = std::min(top, node.position.y);
        right = std::max(right, node.position.x + node.size.width);
        bottom = std::max(bottom, node.position.y + node.size.height);
    }
    const Rect area = bounds();
    zoom_ = std::clamp(std::min((area.width - 80.0F) /
        std::max(1.0F, right - left), (area.height - 80.0F) /
        std::max(1.0F, bottom - top)), 0.35F, 1.5F);
    offset_ = {(area.width - (left + right) * zoom_) * 0.5F,
        (area.height - (top + bottom) * zoom_) * 0.5F};
}

void GraphCanvas::setOnSelectionChanged(SelectionHandler callback) {
    onSelectionChanged_ = std::move(callback);
}

void GraphCanvas::setOnNodeMoved(NodeMovedHandler callback) {
    onNodeMoved_ = std::move(callback);
}

void GraphCanvas::label(std::vector<PaintCommand>& commands,
    std::string_view value, Point origin, Rect visible, Color color) const {
    auto layout = textEngine_->createLayout(value, textStyle_, {});
    if (layout) layout->appendPaintCommands(origin, visible, color, commands);
}

void GraphCanvas::onPaint(std::vector<PaintCommand>& commands) const {
    const Rect area = bounds();
    const Rect visible = intersect(area, clip());
    if (!hasArea(visible)) return;
    commands.push_back({area, visible, style_.background,
        invalidTextureId, 4.0F});
    const float step = 48.0F * zoom_;
    const float startX = area.x + std::fmod(std::fmod(offset_.x, step) + step, step);
    const float startY = area.y + std::fmod(std::fmod(offset_.y, step) + step, step);
    for (float x = startX; x < area.x + area.width; x += step)
        commands.push_back(linePaintCommand({x, area.y},
            {x, area.y + area.height}, 1.0F, visible, style_.grid));
    for (float y = startY; y < area.y + area.height; y += step)
        commands.push_back(linePaintCommand({area.x, y},
            {area.x + area.width, y}, 1.0F, visible, style_.grid));

    for (const auto& link : links_) {
        const GraphNode* from = findNode(nodes_, link.fromNode);
        const GraphNode* to = findNode(nodes_, link.toNode);
        if (!from || !to) continue;
        const Point start = port(*from, true, link.fromPort);
        const Point end = port(*to, false, link.toPort);
        const float bend = std::max(36.0F * zoom_,
            std::abs(end.x - start.x) * 0.5F);
        const Point c1{start.x + bend, start.y};
        const Point c2{end.x - bend, end.y};
        Point previous = start;
        for (int i = 1; i <= 24; ++i) {
            const float t = static_cast<float>(i) / 24.0F;
            const float u = 1.0F - t;
            const Point next{
                u*u*u*start.x + 3*u*u*t*c1.x + 3*u*t*t*c2.x + t*t*t*end.x,
                u*u*u*start.y + 3*u*u*t*c1.y + 3*u*t*t*c2.y + t*t*t*end.y};
            commands.push_back(linePaintCommand(previous, next,
                2.0F, visible, link.color));
            previous = next;
        }
    }

    for (const auto& node : nodes_) {
        const Rect card = nodeBounds(node.id);
        const Rect cardClip = intersect(card, visible);
        if (!hasArea(cardClip)) continue;
        if (selected_ && *selected_ == node.id) {
            commands.push_back({{card.x - 2.0F, card.y - 2.0F,
                card.width + 4.0F, card.height + 4.0F}, visible,
                style_.selection, invalidTextureId, 7.0F * zoom_});
        }
        commands.push_back({card, cardClip, style_.card,
            invalidTextureId, 6.0F * zoom_});
        const Rect header{card.x, card.y, card.width, 31.0F * zoom_};
        commands.push_back({header, cardClip, style_.header,
            invalidTextureId, 4.0F * zoom_});
        commands.push_back({{card.x, card.y, 4.0F * zoom_,
            31.0F * zoom_}, cardClip, node.accent,
            invalidTextureId, 1.0F});
        label(commands, node.title,
            {card.x + 13.0F * zoom_, card.y + 7.0F * zoom_},
            intersect(header, cardClip), style_.text);
        const auto paintPorts = [&](const std::vector<std::string>& ports,
            bool output) {
            for (std::size_t i = 0; i < ports.size(); ++i) {
                const Point p = port(node, output, i);
                const float radius = 4.5F * zoom_;
                commands.push_back({{p.x - radius, p.y - radius,
                    radius * 2.0F, radius * 2.0F}, visible,
                    node.accent, invalidTextureId, radius});
                const Point origin{output
                    ? card.x + card.width - 70.0F * zoom_
                    : card.x + 12.0F * zoom_,
                    p.y - 8.0F * zoom_};
                label(commands, ports[i], origin, cardClip,
                    style_.mutedText);
            }
        };
        paintPorts(node.inputs, false);
        paintPorts(node.outputs, true);
    }
}

bool GraphCanvas::acceptsPointerEvents() const noexcept { return true; }

bool GraphCanvas::onPointerEvent(const WidgetPointerEvent& event) {
    if (event.type == WidgetPointerEventType::Press &&
        event.button == PointerButton::Primary &&
        contains(bounds(), event.position)) {
        lastPointer_ = event.position;
        for (std::size_t i = nodes_.size(); i > 0; --i) {
            if (contains(nodeBounds(nodes_[i - 1].id), event.position)) {
                draggedNode_ = i - 1;
                drag_ = DragMode::Node;
                select(nodes_[draggedNode_].id);
                return true;
            }
        }
        select(std::nullopt);
        drag_ = DragMode::Pan;
        return true;
    }
    if (event.type == WidgetPointerEventType::Move &&
        drag_ != DragMode::None) {
        const Point delta{event.position.x - lastPointer_.x,
            event.position.y - lastPointer_.y};
        lastPointer_ = event.position;
        if (drag_ == DragMode::Pan) {
            offset_.x += delta.x;
            offset_.y += delta.y;
        } else {
            GraphNode& node = nodes_[draggedNode_];
            node.position.x += delta.x / zoom_;
            node.position.y += delta.y / zoom_;
            if (onNodeMoved_) onNodeMoved_(node.id, node.position);
        }
        return true;
    }
    if (event.type == WidgetPointerEventType::Release ||
        event.type == WidgetPointerEventType::Cancel) {
        const bool wasDragging = drag_ != DragMode::None;
        drag_ = DragMode::None;
        return wasDragging;
    }
    return false;
}

bool GraphCanvas::onScrollEvent(const WidgetScrollEvent& event) {
    if (!contains(bounds(), event.position) || event.delta.y == 0.0F)
        return false;
    const float steps = std::clamp(event.delta.y /
        (event.mode == ScrollDeltaMode::Line ? 1.0F : 48.0F),
        -8.0F, 8.0F);
    const float next = std::clamp(zoom_ * std::pow(1.12F, steps),
        0.35F, 2.5F);
    const Point anchor{event.position.x - bounds().x,
        event.position.y - bounds().y};
    offset_ = {anchor.x - (anchor.x - offset_.x) * next / zoom_,
        anchor.y - (anchor.y - offset_.y) * next / zoom_};
    zoom_ = next;
    return true;
}

bool GraphCanvas::acceptsFocus() const noexcept { return true; }

bool GraphCanvas::onKeyEvent(const WidgetKeyEvent& event) {
    if (event.type != WidgetKeyEventType::Press) return false;
    if (event.key == KeyCode::Home) { fitToContent(); return true; }
    if (event.key == KeyCode::Escape) {
        select(std::nullopt);
        return true;
    }
    Point step{};
    switch (event.key) {
    case KeyCode::Left: step.x = -8.0F; break;
    case KeyCode::Right: step.x = 8.0F; break;
    case KeyCode::Up: step.y = -8.0F; break;
    case KeyCode::Down: step.y = 8.0F; break;
    default: return false;
    }
    if (selected_) {
        for (GraphNode& node : nodes_) {
            if (node.id != *selected_) continue;
            node.position.x += step.x / zoom_;
            node.position.y += step.y / zoom_;
            if (onNodeMoved_) onNodeMoved_(node.id, node.position);
            return true;
        }
    }
    offset_.x -= step.x;
    offset_.y -= step.y;
    return true;
}

} // namespace lotui
