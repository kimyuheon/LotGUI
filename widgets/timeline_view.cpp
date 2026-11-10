#include "widgets/timeline_view.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace lotui {
namespace {
constexpr float kNames = 146.0F;
constexpr float kRuler = 37.0F;
constexpr float kRow = 36.0F;
constexpr float kKeySize = 12.0F;
constexpr float kRulerInset = 9.0F;

bool valid(double value) noexcept { return std::isfinite(value) && value >= 0.0; }

std::string tickText(double seconds) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(seconds < 10.0 ? 1 : 0)
        << seconds << "s";
    return out.str();
}
} // namespace

TimelineView::TimelineView(std::shared_ptr<const TextEngine> textEngine,
    Size preferredSize, TimelineViewStyle style, TextStyle textStyle)
    : textEngine_(std::move(textEngine)), preferredSize_(preferredSize),
      style_(style), textStyle_(std::move(textStyle)) {
    if (!textEngine_ || !isValidTextStyle(textStyle_) ||
        !std::isfinite(preferredSize_.width) ||
        !std::isfinite(preferredSize_.height) ||
        preferredSize_.width <= kNames + 80.0F ||
        preferredSize_.height <= kRuler + kRow)
        throw std::invalid_argument("invalid TimelineView configuration");
}

Size TimelineView::measure(const LayoutConstraints& constraints) const {
    return constraints.constrain(preferredSize_);
}

void TimelineView::setTracks(std::vector<TimelineTrack> tracks) {
    std::unordered_set<std::string> trackIds;
    for (const auto& track : tracks) {
        if (track.id.empty() || !trackIds.insert(track.id).second)
            throw std::invalid_argument("invalid TimelineView track id");
        std::unordered_set<std::string> keyIds;
        for (const auto& key : track.keys)
            if (key.id.empty() || !valid(key.time) ||
                !keyIds.insert(key.id).second)
                throw std::invalid_argument("invalid TimelineView key");
    }
    tracks_ = std::move(tracks);
    drag_ = Drag::None;
    if (selected_ && !hasArea(keyBounds(selected_->trackId,
            selected_->keyId))) select(std::nullopt);
}

const std::vector<TimelineTrack>& TimelineView::tracks() const noexcept {
    return tracks_;
}

void TimelineView::setDuration(double seconds) {
    if (!std::isfinite(seconds) || seconds <= 0.0)
        throw std::invalid_argument("invalid TimelineView duration");
    duration_ = seconds;
    time_ = std::clamp(time_, 0.0, duration_);
    viewDuration_ = std::clamp(viewDuration_,
        std::min(0.1, duration_), duration_);
    viewStart_ = std::clamp(viewStart_, 0.0, duration_ - viewDuration_);
}

double TimelineView::duration() const noexcept { return duration_; }
void TimelineView::setTime(double seconds) noexcept {
    if (std::isfinite(seconds)) time_ = std::clamp(seconds, 0.0, duration_);
}
double TimelineView::time() const noexcept { return time_; }
void TimelineView::setPlaying(bool playing) noexcept { playing_ = playing; }
bool TimelineView::playing() const noexcept { return playing_; }

void TimelineView::setView(double startSeconds, double visibleSeconds) {
    if (!valid(startSeconds) || !std::isfinite(visibleSeconds) ||
        visibleSeconds <= 0.0)
        throw std::invalid_argument("invalid TimelineView range");
    viewDuration_ = std::clamp(visibleSeconds,
        std::min(0.1, duration_), duration_);
    viewStart_ = std::clamp(startSeconds, 0.0, duration_ - viewDuration_);
}

double TimelineView::viewStart() const noexcept { return viewStart_; }
double TimelineView::viewDuration() const noexcept { return viewDuration_; }
std::optional<TimelineKeyRef> TimelineView::selectedKey() const { return selected_; }

Rect TimelineView::rulerBounds() const noexcept {
    const Rect area = bounds();
    return {area.x + kNames, area.y,
        std::max(0.0F, area.width - kNames),
        std::min(kRuler, area.height)};
}

float TimelineView::xAt(double seconds) const noexcept {
    const Rect ruler = rulerBounds();
    return ruler.x + kRulerInset + static_cast<float>(
        (seconds - viewStart_) / viewDuration_ *
        std::max(1.0F, ruler.width - 2.0F * kRulerInset));
}

double TimelineView::timeAt(float x) const noexcept {
    const Rect ruler = rulerBounds();
    if (ruler.width <= 0.0F) return viewStart_;
    return viewStart_ + (x - ruler.x - kRulerInset) /
        std::max(1.0F, ruler.width - 2.0F * kRulerInset) * viewDuration_;
}

Rect TimelineView::keyBounds(std::string_view trackId,
    std::string_view keyId) const noexcept {
    for (std::size_t i = 0; i < tracks_.size(); ++i) {
        if (tracks_[i].id != trackId) continue;
        for (const auto& key : tracks_[i].keys)
            if (key.id == keyId) {
                const float x = xAt(key.time);
                const float y = bounds().y + kRuler +
                    (static_cast<float>(i) + 0.5F) * kRow;
                return {x - kKeySize * 0.5F, y - kKeySize * 0.5F,
                    kKeySize, kKeySize};
            }
    }
    return {};
}

void TimelineView::setOnTimeChanged(TimeHandler callback) {
    onTimeChanged_ = std::move(callback);
}
void TimelineView::setOnPlayChanged(PlayHandler callback) {
    onPlayChanged_ = std::move(callback);
}
void TimelineView::setOnKeySelected(KeyHandler callback) {
    onKeySelected_ = std::move(callback);
}
void TimelineView::setOnKeyMoved(KeyMovedHandler callback) {
    onKeyMoved_ = std::move(callback);
}

void TimelineView::seek(double seconds) {
    const double next = std::clamp(seconds, 0.0, duration_);
    if (std::abs(next - time_) < 1e-8) return;
    time_ = next;
    if (onTimeChanged_) onTimeChanged_(time_);
}

void TimelineView::select(std::optional<TimelineKeyRef> key) {
    if (selected_ == key) return;
    selected_ = std::move(key);
    if (onKeySelected_) onKeySelected_(selected_);
}

void TimelineView::label(std::vector<PaintCommand>& commands,
    std::string_view text, Point origin, Rect visible, Color color) const {
    auto layout = textEngine_->createLayout(text, textStyle_, {});
    if (layout) layout->appendPaintCommands(origin, visible, color, commands);
}

void TimelineView::onPaint(std::vector<PaintCommand>& commands) const {
    const Rect area = bounds();
    const Rect visible = intersect(area, clip());
    if (!hasArea(visible)) return;
    commands.push_back({area, visible, style_.background,
        invalidTextureId, 3.0F});
    commands.push_back({{area.x, area.y, area.width, kRuler}, visible,
        style_.header, invalidTextureId, 2.0F});
    const Rect ruler = rulerBounds();
    const Rect timelineClip = intersect(visible, {ruler.x, area.y,
        ruler.width, area.height});
    label(commands, playing_ ? "❚❚  일시정지" : "▶  재생",
        {area.x + 13.0F, area.y + 9.0F}, visible, style_.text);
    commands.push_back(linePaintCommand({ruler.x, area.y},
        {ruler.x, area.y + area.height}, 1.0F, visible, style_.grid));

    const double rawStep = viewDuration_ /
        std::max(1.0F, ruler.width / 90.0F);
    const double magnitude = std::pow(10.0, std::floor(std::log10(rawStep)));
    const double step = rawStep / magnitude <= 2.0 ? 2.0 * magnitude :
        rawStep / magnitude <= 5.0 ? 5.0 * magnitude : 10.0 * magnitude;
    const double first = std::ceil(viewStart_ / step) * step;
    for (double t = first; t <= viewStart_ + viewDuration_ + step * 0.001;
            t += step) {
        const float x = xAt(t);
        commands.push_back(linePaintCommand({x, area.y + 24.0F},
            {x, area.y + area.height}, 1.0F, timelineClip, style_.grid));
        label(commands, tickText(t), {x + 4.0F, area.y + 5.0F},
            timelineClip, style_.text);
    }
    for (std::size_t i = 0; i < tracks_.size(); ++i) {
        const float y = area.y + kRuler + static_cast<float>(i) * kRow;
        if (y >= area.y + area.height) break;
        const Rect row{area.x, y, area.width, kRow};
        if (i % 2 != 0)
            commands.push_back({row, intersect(visible, row),
                style_.alternateRow, invalidTextureId, 0.0F});
        commands.push_back(linePaintCommand({area.x, y + kRow},
            {area.x + area.width, y + kRow}, 1.0F, visible, style_.grid));
        label(commands, tracks_[i].label,
            {area.x + 12.0F, y + 9.0F},
            intersect(visible, {area.x, y, kNames, kRow}), style_.text);
        for (const auto& key : tracks_[i].keys) {
            const Rect marker = keyBounds(tracks_[i].id, key.id);
            if (!hasArea(intersect(marker, timelineClip))) continue;
            const bool chosen = selected_ &&
                selected_->trackId == tracks_[i].id &&
                selected_->keyId == key.id;
            commands.push_back({marker, timelineClip,
                chosen ? style_.selected : tracks_[i].color,
                invalidTextureId, 3.0F});
        }
    }
    const float playX = xAt(time_);
    commands.push_back(linePaintCommand({playX, area.y + 24.0F},
        {playX, area.y + area.height}, 2.0F,
        timelineClip, style_.playhead));
    commands.push_back({{playX - 5.0F, area.y + 24.0F, 10.0F, 9.0F},
        timelineClip, style_.playhead, invalidTextureId, 2.0F});
}

bool TimelineView::acceptsPointerEvents() const noexcept { return true; }

bool TimelineView::onPointerEvent(const WidgetPointerEvent& event) {
    if (event.type == WidgetPointerEventType::Press &&
        event.button == PointerButton::Primary &&
        contains(bounds(), event.position)) {
        if (event.position.y < bounds().y + kRuler &&
            event.position.x < bounds().x + kNames) {
            playing_ = !playing_;
            if (onPlayChanged_) onPlayChanged_(playing_);
            return true;
        }
        if (event.position.x < bounds().x + kNames) return true;
        for (std::size_t i = tracks_.size(); i > 0; --i)
            for (auto& key : tracks_[i - 1].keys)
                if (contains(keyBounds(tracks_[i - 1].id, key.id),
                        event.position)) {
                    select(TimelineKeyRef{tracks_[i - 1].id, key.id});
                    drag_ = Drag::Key;
                    dragStartTime_ = key.time;
                    dragStartX_ = event.position.x;
                    return true;
                }
        select(std::nullopt);
        drag_ = Drag::Scrub;
        seek(timeAt(event.position.x));
        return true;
    }
    if (event.type == WidgetPointerEventType::Move) {
        if (drag_ == Drag::Scrub) {
            seek(timeAt(event.position.x));
            return true;
        }
        if (drag_ == Drag::Key && selected_) {
            const double delta = (event.position.x - dragStartX_) /
                std::max(1.0F, rulerBounds().width -
                    2.0F * kRulerInset) * viewDuration_;
            for (auto& track : tracks_)
                if (track.id == selected_->trackId)
                    for (auto& key : track.keys)
                        if (key.id == selected_->keyId) {
                            key.time = std::clamp(dragStartTime_ + delta,
                                0.0, duration_);
                            return true;
                        }
            drag_ = Drag::None;
        }
    }
    if (event.type == WidgetPointerEventType::Release ||
        event.type == WidgetPointerEventType::Cancel) {
        const Drag ending = drag_;
        drag_ = Drag::None;
        if (ending == Drag::Key && selected_) {
            for (auto& track : tracks_)
                if (track.id == selected_->trackId)
                    for (auto& key : track.keys)
                        if (key.id == selected_->keyId) {
                            if (event.type == WidgetPointerEventType::Cancel)
                                key.time = dragStartTime_;
                            else if (std::abs(key.time - dragStartTime_) > 1e-8 &&
                                    onKeyMoved_)
                                onKeyMoved_(*selected_, key.time);
                            return true;
                        }
        }
        return ending != Drag::None;
    }
    return false;
}

bool TimelineView::onScrollEvent(const WidgetScrollEvent& event) {
    if (!contains(bounds(), event.position) || event.delta.y == 0.0F)
        return false;
    const double factor = std::pow(1.12,
        -std::clamp(static_cast<double>(event.delta.y) /
            (event.mode == ScrollDeltaMode::Line ? 1.0 : 48.0), -8.0, 8.0));
    const double anchor = timeAt(event.position.x);
    const double next = std::clamp(viewDuration_ * factor,
        std::min(0.1, duration_), duration_);
    const double ratio = (event.position.x - rulerBounds().x -
        kRulerInset) /
        std::max(1.0F, rulerBounds().width - 2.0F * kRulerInset);
    setView(std::max(0.0, anchor - ratio * next), next);
    return true;
}

bool TimelineView::acceptsFocus() const noexcept { return true; }
bool TimelineView::onKeyEvent(const WidgetKeyEvent& event) {
    if (event.type != WidgetKeyEventType::Press) return false;
    if (event.key == KeyCode::Space) {
        playing_ = !playing_;
        if (onPlayChanged_) onPlayChanged_(playing_);
        return true;
    }
    if (event.key == KeyCode::Home) { seek(0.0); return true; }
    if (event.key == KeyCode::Left || event.key == KeyCode::Right) {
        seek(time_ + (event.key == KeyCode::Right ? 1.0 : -1.0) /
            std::max(1.0, std::round(24.0 / viewDuration_)));
        return true;
    }
    return false;
}

} // namespace lotui
