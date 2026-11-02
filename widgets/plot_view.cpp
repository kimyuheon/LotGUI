#include "widgets/plot_view.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <utility>

namespace lotui {
namespace {

bool valid(PlotRange r) noexcept {
    return std::isfinite(r.xMin) && std::isfinite(r.xMax) &&
        std::isfinite(r.yMin) && std::isfinite(r.yMax) &&
        r.xMin < r.xMax && r.yMin < r.yMax;
}

bool finite(PlotPoint p) noexcept {
    return std::isfinite(p.x) && std::isfinite(p.y);
}

std::string number(double value) {
    char buffer[32]{};
    std::snprintf(buffer, sizeof(buffer), "%.3g", value);
    return buffer;
}

} // namespace

PlotView::PlotView(std::shared_ptr<const TextEngine> textEngine,
    Size preferredSize, PlotViewStyle style, TextStyle textStyle)
    : textEngine_(std::move(textEngine)), preferredSize_(preferredSize),
      style_(style), textStyle_(std::move(textStyle)) {
    if (!textEngine_ || !isValidTextStyle(textStyle_) ||
        !std::isfinite(preferredSize.width) ||
        !std::isfinite(preferredSize.height) ||
        preferredSize.width <= 0.0F || preferredSize.height <= 0.0F ||
        !std::isfinite(style_.lineWidth) || style_.lineWidth <= 0.0F)
        throw std::invalid_argument("invalid PlotView configuration");
}

Size PlotView::measure(const LayoutConstraints& constraints) const {
    return constraints.constrain(preferredSize_);
}

void PlotView::setSeries(std::vector<PlotSeries> series) {
    for (const auto& entry : series) {
        double lastX = -std::numeric_limits<double>::infinity();
        for (const auto& point : entry.points) {
            if (!std::isfinite(point.x) || point.x < lastX)
                throw std::invalid_argument("PlotView X values must be finite and sorted");
            lastX = point.x;
        }
    }
    series_ = std::move(series);
    if (autoRange_) resetAutoRange();
    hover_.reset();
}

const std::vector<PlotSeries>& PlotView::series() const noexcept { return series_; }

void PlotView::setRange(PlotRange range) {
    if (!valid(range)) throw std::invalid_argument("invalid PlotView range");
    range_ = range;
    autoRange_ = false;
}

void PlotView::resetAutoRange() noexcept {
    autoRange_ = true;
    double minX = std::numeric_limits<double>::infinity();
    double maxX = -minX;
    double minY = minX;
    double maxY = -minX;
    for (const auto& entry : series_) {
        if (!entry.visible) continue;
        for (const auto& point : entry.points) {
            if (!finite(point)) continue;
            minX = std::min(minX, point.x);
            maxX = std::max(maxX, point.x);
            minY = std::min(minY, point.y);
            maxY = std::max(maxY, point.y);
        }
    }
    if (!std::isfinite(minX)) { range_ = {}; return; }
    const double xPad = minX == maxX ? 1.0 : (maxX - minX) * 0.03;
    const double yPad = minY == maxY ? 1.0 : (maxY - minY) * 0.08;
    range_ = {minX - xPad, maxX + xPad, minY - yPad, maxY + yPad};
}

bool PlotView::autoRange() const noexcept { return autoRange_; }
PlotRange PlotView::viewRange() const noexcept { return range_; }

Rect PlotView::plotBounds() const noexcept {
    const Rect area = bounds();
    return {area.x + 54.0F, area.y + 18.0F,
        std::max(1.0F, area.width - 72.0F),
        std::max(1.0F, area.height - 52.0F)};
}

std::optional<PlotHover> PlotView::hovered() const { return hover_; }
void PlotView::setOnViewportChanged(ViewportHandler callback) {
    onViewportChanged_ = std::move(callback);
}
void PlotView::setOnHoverChanged(HoverHandler callback) {
    onHoverChanged_ = std::move(callback);
}

void PlotView::label(std::vector<PaintCommand>& commands,
    std::string value, Point origin, Rect visible) const {
    auto layout = textEngine_->createLayout(value, textStyle_, {});
    if (layout) layout->appendPaintCommands(origin, visible, style_.text, commands);
}

void PlotView::onPaint(std::vector<PaintCommand>& commands) const {
    const Rect area = bounds();
    const Rect visible = intersect(area, clip());
    if (!hasArea(visible)) return;
    commands.push_back({area, visible, style_.background, invalidTextureId, 4.0F});
    const Rect plot = plotBounds();
    const Rect plotClip = intersect(plot, visible);
    if (!hasArea(plotClip)) return;
    for (int i = 0; i <= 4; ++i) {
        const float x = plot.x + plot.width * i / 4.0F;
        const float y = plot.y + plot.height * i / 4.0F;
        commands.push_back(linePaintCommand({x, plot.y},
            {x, plot.y + plot.height}, 1.0F, plotClip, style_.grid));
        commands.push_back(linePaintCommand({plot.x, y},
            {plot.x + plot.width, y}, 1.0F, plotClip, style_.grid));
        label(commands, number(range_.xMin +
            (range_.xMax - range_.xMin) * i / 4.0),
            {x - 14.0F, plot.y + plot.height + 5.0F}, visible);
        label(commands, number(range_.yMax -
            (range_.yMax - range_.yMin) * i / 4.0),
            {area.x + 5.0F, y - 7.0F}, visible);
    }
    commands.push_back(linePaintCommand({plot.x, plot.y + plot.height},
        {plot.x + plot.width, plot.y + plot.height}, 1.2F,
        visible, style_.axis));
    commands.push_back(linePaintCommand({plot.x, plot.y},
        {plot.x, plot.y + plot.height}, 1.2F, visible, style_.axis));

    const auto toScreen = [&](PlotPoint p) -> Point {
        return {plot.x + static_cast<float>(
            (p.x - range_.xMin) / (range_.xMax - range_.xMin)) * plot.width,
            plot.y + plot.height - static_cast<float>(
            (p.y - range_.yMin) / (range_.yMax - range_.yMin)) * plot.height};
    };
    for (const auto& entry : series_) {
        if (!entry.visible) continue;
        // Each screen column retains its first/last and vertical extrema.
        std::vector<std::size_t> selected;
        selected.reserve(static_cast<std::size_t>(plot.width) * 4 + 8);
        std::size_t start = 0;
        while (start < entry.points.size()) {
            while (start < entry.points.size() && !finite(entry.points[start])) ++start;
            if (start == entry.points.size()) break;
            std::size_t end = start;
            while (end < entry.points.size() && finite(entry.points[end])) ++end;
            std::size_t cursor = start;
            while (cursor < end) {
                const double screenX = (entry.points[cursor].x - range_.xMin) /
                    (range_.xMax - range_.xMin) * plot.width;
                const int bucket = static_cast<int>(std::clamp(screenX,
                    -1.0, static_cast<double>(plot.width) + 1.0));
                const std::size_t first = cursor;
                std::size_t low = cursor, high = cursor;
                do {
                    if (entry.points[cursor].y < entry.points[low].y) low = cursor;
                    if (entry.points[cursor].y > entry.points[high].y) high = cursor;
                    ++cursor;
                    if (cursor == end) break;
                    const double nextX = (entry.points[cursor].x - range_.xMin) /
                        (range_.xMax - range_.xMin) * plot.width;
                    if (static_cast<int>(std::clamp(nextX, -1.0,
                            static_cast<double>(plot.width) + 1.0)) != bucket) break;
                } while (true);
                std::size_t indices[]{first, low, high, cursor - 1};
                std::sort(std::begin(indices), std::end(indices));
                for (auto index : indices)
                    if (selected.empty() || selected.back() != index)
                        selected.push_back(index);
            }
            for (std::size_t i = 1; i < selected.size(); ++i) {
                const Point a = toScreen(entry.points[selected[i - 1]]);
                const Point b = toScreen(entry.points[selected[i]]);
                if (std::isfinite(a.x) && std::isfinite(a.y) &&
                    std::isfinite(b.x) && std::isfinite(b.y))
                    commands.push_back(linePaintCommand(a, b,
                        style_.lineWidth, plotClip, entry.color));
            }
            selected.clear();
            start = end + 1;
        }
    }
    if (hover_) {
        const Point p = toScreen(hover_->point);
        if (contains(plot, p)) {
            commands.push_back(linePaintCommand({p.x, plot.y},
                {p.x, plot.y + plot.height}, 1.0F, plotClip,
                style_.crosshair));
            commands.push_back(linePaintCommand({plot.x, p.y},
                {plot.x + plot.width, p.y}, 1.0F, plotClip,
                style_.crosshair));
            commands.push_back({{p.x - 3.0F, p.y - 3.0F, 6.0F, 6.0F},
                plotClip, style_.text, invalidTextureId, 3.0F});
        }
    }
}

bool PlotView::acceptsPointerEvents() const noexcept { return true; }

void PlotView::updateHover(Point position) {
    const Rect plot = plotBounds();
    std::optional<PlotHover> next;
    if (contains(plot, position)) {
        const double x = range_.xMin +
            (position.x - plot.x) / plot.width * (range_.xMax - range_.xMin);
        double best = (range_.xMax - range_.xMin) * 12.0 / plot.width;
        for (const auto& entry : series_) {
            if (!entry.visible) continue;
            auto it = std::lower_bound(entry.points.begin(), entry.points.end(), x,
                [](PlotPoint p, double value) { return p.x < value; });
            for (int adjacent = 0; adjacent < 2; ++adjacent) {
                if (it == entry.points.end()) {
                    if (it == entry.points.begin()) break;
                    --it;
                }
                const double distance = std::abs(it->x - x);
                if (finite(*it) && distance < best) {
                    best = distance;
                    next = PlotHover{entry.id, *it,
                        static_cast<std::size_t>(it - entry.points.begin())};
                }
                if (it == entry.points.begin()) break;
                --it;
            }
        }
    }
    if (next.has_value() != hover_.has_value() ||
        (next && (next->seriesId != hover_->seriesId ||
            next->index != hover_->index))) {
        hover_ = std::move(next);
        if (onHoverChanged_) onHoverChanged_(hover_);
    }
}

bool PlotView::onPointerEvent(const WidgetPointerEvent& event) {
    if (event.type == WidgetPointerEventType::Press &&
        event.button == PointerButton::Primary &&
        contains(plotBounds(), event.position)) {
        dragging_ = true;
        lastPointer_ = event.position;
        updateHover(event.position);
        return true;
    }
    if (event.type == WidgetPointerEventType::Move) {
        if (dragging_) {
            pan(event.position.x - lastPointer_.x,
                event.position.y - lastPointer_.y);
            lastPointer_ = event.position;
        }
        updateHover(event.position);
        return dragging_ || hover_.has_value();
    }
    if (event.type == WidgetPointerEventType::Release && dragging_) {
        dragging_ = false;
        updateHover(event.position);
        return true;
    }
    if (event.type == WidgetPointerEventType::Cancel ||
        event.type == WidgetPointerEventType::Leave) {
        if (!dragging_) updateHover({});
        if (event.type == WidgetPointerEventType::Cancel) dragging_ = false;
    }
    return false;
}

void PlotView::notifyViewport() {
    if (onViewportChanged_) onViewportChanged_(range_);
}

void PlotView::pan(float dx, float dy) {
    const Rect plot = plotBounds();
    const double shiftX = dx / plot.width * (range_.xMax - range_.xMin);
    const double shiftY = dy / plot.height * (range_.yMax - range_.yMin);
    setRange({range_.xMin - shiftX, range_.xMax - shiftX,
        range_.yMin + shiftY, range_.yMax + shiftY});
    notifyViewport();
}

void PlotView::zoom(Point center, double factor) {
    const Rect plot = plotBounds();
    const double fx = std::clamp(static_cast<double>(
        (center.x - plot.x) / plot.width), 0.0, 1.0);
    const double fy = std::clamp(static_cast<double>(
        (plot.y + plot.height - center.y) / plot.height), 0.0, 1.0);
    const double pivotX = range_.xMin + fx * (range_.xMax - range_.xMin);
    const double pivotY = range_.yMin + fy * (range_.yMax - range_.yMin);
    PlotRange next{pivotX + (range_.xMin - pivotX) * factor,
        pivotX + (range_.xMax - pivotX) * factor,
        pivotY + (range_.yMin - pivotY) * factor,
        pivotY + (range_.yMax - pivotY) * factor};
    if (valid(next)) { setRange(next); notifyViewport(); }
}

bool PlotView::onScrollEvent(const WidgetScrollEvent& event) {
    if (!contains(plotBounds(), event.position) || event.delta.y == 0.0F)
        return false;
    const double steps = std::clamp(static_cast<double>(event.delta.y) /
        (event.mode == ScrollDeltaMode::Line ? 1.0 : 48.0), -8.0, 8.0);
    zoom(event.position, std::pow(0.88, steps));
    updateHover(event.position);
    return true;
}

bool PlotView::acceptsFocus() const noexcept { return true; }

bool PlotView::onKeyEvent(const WidgetKeyEvent& event) {
    if (event.type != WidgetKeyEventType::Press) return false;
    switch (event.key) {
    case KeyCode::Left: pan(20.0F, 0.0F); return true;
    case KeyCode::Right: pan(-20.0F, 0.0F); return true;
    case KeyCode::Up: pan(0.0F, 20.0F); return true;
    case KeyCode::Down: pan(0.0F, -20.0F); return true;
    case KeyCode::Home: resetAutoRange(); notifyViewport(); return true;
    case KeyCode::PageUp: zoom({plotBounds().x + plotBounds().width * 0.5F,
        plotBounds().y + plotBounds().height * 0.5F}, 0.8); return true;
    case KeyCode::PageDown: zoom({plotBounds().x + plotBounds().width * 0.5F,
        plotBounds().y + plotBounds().height * 0.5F}, 1.25); return true;
    default: return false;
    }
}

} // namespace lotui
