#pragma once

#include "core/widget.h"
#include "text/text_layout.h"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace lotui {

struct GraphNode {
    std::string id;
    std::string title;
    Point position{}; // Canvas coordinates, independent of the window.
    Size size{176.0F, 112.0F};
    std::vector<std::string> inputs;
    std::vector<std::string> outputs;
    Color accent{0.31F, 0.75F, 0.96F, 1.0F};
};

struct GraphLink {
    std::string id;
    std::string fromNode;
    std::size_t fromPort{0};
    std::string toNode;
    std::size_t toPort{0};
    Color color{0.47F, 0.62F, 0.72F, 1.0F};
};

struct GraphCanvasStyle {
    Color background{0.015F, 0.021F, 0.032F, 1.0F};
    Color grid{0.10F, 0.14F, 0.19F, 1.0F};
    Color card{0.085F, 0.11F, 0.15F, 1.0F};
    Color header{0.12F, 0.16F, 0.21F, 1.0F};
    Color text{0.88F, 0.91F, 0.95F, 1.0F};
    Color mutedText{0.64F, 0.71F, 0.78F, 1.0F};
    Color selection{0.98F, 0.72F, 0.31F, 1.0F};
};

class GraphCanvas final : public Widget {
public:
    using SelectionHandler = std::function<void(std::optional<std::string>)>;
    using NodeMovedHandler = std::function<void(std::string_view, Point)>;

    explicit GraphCanvas(std::shared_ptr<const TextEngine> textEngine,
        Size preferredSize = {720.0F, 420.0F},
        GraphCanvasStyle style = {}, TextStyle textStyle = {});
    Size measure(const LayoutConstraints& constraints) const override;
    void setGraph(std::vector<GraphNode> nodes, std::vector<GraphLink> links);
    const std::vector<GraphNode>& nodes() const noexcept;
    const std::vector<GraphLink>& links() const noexcept;
    std::optional<std::string> selectedNode() const;
    bool setSelectedNode(std::string_view id);
    Rect nodeBounds(std::string_view id) const noexcept;
    Point offset() const noexcept;
    float zoomLevel() const noexcept;
    void fitToContent() noexcept;
    void setOnSelectionChanged(SelectionHandler callback);
    void setOnNodeMoved(NodeMovedHandler callback);

protected:
    void onPaint(std::vector<PaintCommand>& commands) const override;
    bool acceptsPointerEvents() const noexcept override;
    bool onPointerEvent(const WidgetPointerEvent& event) override;
    bool onScrollEvent(const WidgetScrollEvent& event) override;
    bool acceptsFocus() const noexcept override;
    bool onKeyEvent(const WidgetKeyEvent& event) override;

private:
    enum class DragMode { None, Pan, Node };
    Point screen(Point world) const noexcept;
    Point port(const GraphNode& node, bool output,
        std::size_t index) const noexcept;
    void select(std::optional<std::string> id);
    void label(std::vector<PaintCommand>& commands, std::string_view value,
        Point origin, Rect clip, Color color) const;

    std::shared_ptr<const TextEngine> textEngine_;
    Size preferredSize_{};
    GraphCanvasStyle style_{};
    TextStyle textStyle_{};
    std::vector<GraphNode> nodes_;
    std::vector<GraphLink> links_;
    std::optional<std::string> selected_;
    Point offset_{40.0F, 40.0F};
    float zoom_{1.0F};
    DragMode drag_{DragMode::None};
    std::size_t draggedNode_{0};
    Point lastPointer_{};
    SelectionHandler onSelectionChanged_{};
    NodeMovedHandler onNodeMoved_{};
};

} // namespace lotui
