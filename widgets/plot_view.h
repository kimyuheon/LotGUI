#pragma once

#include "core/widget.h"
#include "text/text_layout.h"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace lotui {

struct PlotPoint { double x{0.0}; double y{0.0}; };
struct PlotRange { double xMin{0.0}, xMax{1.0}, yMin{0.0}, yMax{1.0}; };
struct PlotSeries {
    std::string id;
    std::string label;
    std::vector<PlotPoint> points;
    Color color{0.30F, 0.75F, 0.96F, 1.0F};
    bool visible{true};
};
struct PlotHover {
    std::string seriesId;
    PlotPoint point{};
    std::size_t index{0};
};
struct PlotViewStyle {
    Color background{0.015F, 0.021F, 0.032F, 1.0F};
    Color grid{0.13F, 0.17F, 0.22F, 1.0F};
    Color axis{0.39F, 0.45F, 0.52F, 1.0F};
    Color text{0.80F, 0.85F, 0.90F, 1.0F};
    Color crosshair{0.72F, 0.77F, 0.82F, 0.7F};
    float lineWidth{2.0F};
};

class PlotView final : public Widget {
public:
    using ViewportHandler = std::function<void(PlotRange)>;
    using HoverHandler = std::function<void(std::optional<PlotHover>)>;

    explicit PlotView(std::shared_ptr<const TextEngine> textEngine,
        Size preferredSize = {640.0F, 320.0F},
        PlotViewStyle style = {}, TextStyle textStyle = {});
    Size measure(const LayoutConstraints& constraints) const override;
    void setSeries(std::vector<PlotSeries> series);
    const std::vector<PlotSeries>& series() const noexcept;
    void setRange(PlotRange range);
    void resetAutoRange() noexcept;
    bool autoRange() const noexcept;
    PlotRange viewRange() const noexcept;
    Rect plotBounds() const noexcept;
    std::optional<PlotHover> hovered() const;
    void setOnViewportChanged(ViewportHandler callback);
    void setOnHoverChanged(HoverHandler callback);

protected:
    void onPaint(std::vector<PaintCommand>& commands) const override;
    bool acceptsPointerEvents() const noexcept override;
    bool onPointerEvent(const WidgetPointerEvent& event) override;
    bool onScrollEvent(const WidgetScrollEvent& event) override;
    bool acceptsFocus() const noexcept override;
    bool onKeyEvent(const WidgetKeyEvent& event) override;

private:
    void pan(float dx, float dy);
    void zoom(Point center, double factor);
    void updateHover(Point position);
    void notifyViewport();
    void label(std::vector<PaintCommand>& commands, std::string value,
        Point origin, Rect clip) const;

    std::shared_ptr<const TextEngine> textEngine_;
    Size preferredSize_{};
    PlotViewStyle style_{};
    TextStyle textStyle_{};
    std::vector<PlotSeries> series_{};
    PlotRange range_{};
    bool autoRange_{true};
    bool dragging_{false};
    Point lastPointer_{};
    std::optional<PlotHover> hover_{};
    ViewportHandler onViewportChanged_{};
    HoverHandler onHoverChanged_{};
};

} // namespace lotui
