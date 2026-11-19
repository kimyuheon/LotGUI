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

TimelineKey* findKey(std::vector<TimelineTrack>& tracks,
    const TimelineKeyRef& ref) noexcept {
    for (auto& track : tracks)
        if (track.id == ref.trackId)
            for (auto& key : track.keys)
                if (key.id == ref.keyId) return &key;
    return nullptr;
}

std::string tickText(double seconds) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(seconds < 10.0 ? 1 : 0)
        << seconds << "s";
    return out.str();
}

void drawKey(std::vector<PaintCommand>& commands, Rect marker, Rect clip,
    Color color, TimelineInterpolation interpolation, bool selected,
    Color halo) {
    if (selected) {
        Color tint = halo;
        tint.alpha = 0.22F;
        commands.push_back({{marker.x - 4.0F, marker.y - 4.0F,
            marker.width + 8.0F, marker.height + 8.0F}, clip,
            tint, invalidTextureId, 6.0F});
    }
    if (interpolation == TimelineInterpolation::Smooth) {
        const Point top{marker.x + marker.width * 0.5F, marker.y};
        const Point right{marker.x + marker.width, marker.y + marker.height * 0.5F};
        const Point bottom{top.x, marker.y + marker.height};
        const Point left{marker.x, right.y};
        for (const auto& edge : {std::pair{top, right},
                std::pair{right, bottom}, std::pair{bottom, left},
                std::pair{left, top}})
            commands.push_back(linePaintCommand(edge.first, edge.second,
                selected ? 2.3F : 1.8F, clip, color));
        commands.push_back({{top.x - 2.0F, right.y - 2.0F, 4.0F, 4.0F},
            clip, color, invalidTextureId, 1.0F});
    } else {
        const float inset = interpolation == TimelineInterpolation::Step
            ? 1.0F : 0.0F;
        commands.push_back({{marker.x + inset, marker.y + inset,
            marker.width - 2.0F * inset,
            marker.height - 2.0F * inset}, clip, color,
            invalidTextureId,
            interpolation == TimelineInterpolation::Linear ? 3.0F : 1.0F});
    }
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
    const auto previous = selection_;
    selection_.erase(std::remove_if(selection_.begin(), selection_.end(),
        [this](const TimelineKeyRef& ref) {
            return findKey(tracks_, ref) == nullptr;
        }), selection_.end());
    selected_ = selection_.empty() ? std::nullopt :
        std::optional<TimelineKeyRef>(selection_.back());
    if (selection_ != previous) notifySelection();
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
const std::vector<TimelineKeyRef>& TimelineView::selectedKeys() const noexcept {
    return selection_;
}

float TimelineView::nameWidth() const noexcept {
    const float width = std::max(0.0F, bounds().width);
    return std::min({kNames, std::max(72.0F, width * 0.27F),
        width * 0.45F});
}

Rect TimelineView::rulerBounds() const noexcept {
    const Rect area = bounds();
    const float names = nameWidth();
    return {area.x + names, area.y,
        std::max(0.0F, area.width - names),
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
void TimelineView::setOnSelectionChanged(SelectionHandler callback) {
    onSelectionChanged_ = std::move(callback);
}
void TimelineView::setOnKeysMoved(KeysMovedHandler callback) {
    onKeysMoved_ = std::move(callback);
}
void TimelineView::setOnKeysDeleted(KeysDeletedHandler callback) {
    onKeysDeleted_ = std::move(callback);
}
void TimelineView::setOnInterpolationChanged(InterpolationHandler callback) {
    onInterpolationChanged_ = std::move(callback);
}

void TimelineView::seek(double seconds) {
    const double next = std::clamp(seconds, 0.0, duration_);
    if (std::abs(next - time_) < 1e-8) return;
    time_ = next;
    if (onTimeChanged_) onTimeChanged_(time_);
}

void TimelineView::select(std::optional<TimelineKeyRef> key) {
    const std::vector<TimelineKeyRef> next = key ?
        std::vector<TimelineKeyRef>{*key} : std::vector<TimelineKeyRef>{};
    if (selection_ == next) return;
    selection_ = next;
    selected_ = std::move(key);
    notifySelection();
}

bool TimelineView::containsSelection(const TimelineKeyRef& key) const noexcept {
    return std::find(selection_.begin(), selection_.end(), key) !=
        selection_.end();
}

void TimelineView::toggleSelection(TimelineKeyRef key) {
    const auto found = std::find(selection_.begin(), selection_.end(), key);
    if (found == selection_.end()) selection_.push_back(std::move(key));
    else selection_.erase(found);
    selected_ = selection_.empty() ? std::nullopt :
        std::optional<TimelineKeyRef>(selection_.back());
    notifySelection();
}

void TimelineView::notifySelection() {
    if (onKeySelected_) onKeySelected_(selected_);
    if (onSelectionChanged_) onSelectionChanged_(selection_);
}

void TimelineView::deleteSelection() {
    if (selection_.empty()) return;
    const auto removing = selection_;
    for (auto& track : tracks_)
        track.keys.erase(std::remove_if(track.keys.begin(), track.keys.end(),
            [&](const TimelineKey& key) {
                return containsSelection({track.id, key.id});
            }), track.keys.end());
    select(std::nullopt);
    if (onKeysDeleted_) onKeysDeleted_(removing);
}

void TimelineView::cycleInterpolation() {
    if (selection_.empty()) return;
    const auto* current = findKey(tracks_, selection_.front());
    if (!current) return;
    const TimelineInterpolation next = current->interpolation ==
        TimelineInterpolation::Smooth ? TimelineInterpolation::Linear :
        current->interpolation == TimelineInterpolation::Linear ?
            TimelineInterpolation::Step : TimelineInterpolation::Smooth;
    const auto changing = selection_;
    for (const auto& ref : changing)
        if (auto* key = findKey(tracks_, ref)) key->interpolation = next;
    if (onInterpolationChanged_)
        onInterpolationChanged_(changing, next);
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
    commands.push_back({{area.x, area.y, area.width,
        std::min(kRuler, area.height)}, visible,
        style_.header, invalidTextureId, 2.0F});
    const Rect ruler = rulerBounds();
    const float names = nameWidth();
    const Rect timelineClip = intersect(visible, {ruler.x, area.y,
        ruler.width, area.height});
    commands.push_back({{area.x, area.y, names, ruler.height}, visible,
        style_.namePanel, invalidTextureId, 2.0F});
    for (std::size_t i = 0; i < tracks_.size(); ++i) {
        const float y = area.y + kRuler + static_cast<float>(i) * kRow;
        if (y >= area.y + area.height) break;
        const Rect row{area.x, y, area.width, kRow};
        const bool active = std::any_of(selection_.begin(), selection_.end(),
            [&](const TimelineKeyRef& key) {
                return key.trackId == tracks_[i].id;
            });
        if (active || i % 2 != 0)
            commands.push_back({row, intersect(visible, row),
                active ? style_.selectedRow : style_.alternateRow,
                invalidTextureId, 0.0F});
        commands.push_back({{area.x, y, names, kRow},
            intersect(visible, row), style_.namePanel,
            invalidTextureId, 0.0F});
    }
    const float buttonWidth = names / 3.0F;
    const Rect headerClip = intersect(visible,
        {area.x, area.y, names, std::min(kRuler, area.height)});
    const auto headerButton = [&](int index, std::string_view caption,
            Color color) {
        auto layout = textEngine_->createLayout(caption, textStyle_, {});
        if (!layout) return;
        const Rect segment{area.x + index * buttonWidth, area.y,
            buttonWidth, kRuler};
        layout->appendPaintCommands({segment.x + std::max(0.0F,
                (segment.width - layout->size().width) * 0.5F),
                segment.y + std::max(0.0F,
                (segment.height - layout->size().height) * 0.5F)},
            intersect(headerClip, segment), color, commands);
    };
    headerButton(0, playing_ ? "❚❚" : "▶", style_.text);
    headerButton(1, names < 105.0F ? "◇" : "보간",
        selection_.empty() ? style_.grid : style_.text);
    headerButton(2, names < 105.0F ? "×" : "삭제",
        selection_.empty() ? style_.grid : style_.text);
    for (int index = 1; index < 3; ++index)
        commands.push_back(linePaintCommand(
            {area.x + index * buttonWidth, area.y + 4.0F},
            {area.x + index * buttonWidth,
                area.y + std::min(kRuler, area.height) - 4.0F},
            1.0F, headerClip, style_.grid));
    commands.push_back(linePaintCommand({area.x, area.y + kRuler},
        {area.x + area.width, area.y + kRuler},
        1.0F, visible, style_.grid));
    commands.push_back(linePaintCommand({ruler.x, area.y},
        {ruler.x, area.y + area.height}, 1.0F, visible, style_.grid));

    const double rawStep = viewDuration_ /
        std::max(1.0F, ruler.width / 90.0F);
    const double magnitude = std::pow(10.0, std::floor(std::log10(rawStep)));
    const double step = rawStep / magnitude <= 2.0 ? 2.0 * magnitude :
        rawStep / magnitude <= 5.0 ? 5.0 * magnitude : 10.0 * magnitude;
    const double minor = step / 5.0;
    const double firstMinor = std::ceil(viewStart_ / minor) * minor;
    for (double t = firstMinor; t <= viewStart_ + viewDuration_;
            t += minor) {
        if (std::abs(t / step - std::round(t / step)) < 0.001)
            continue;
        const float x = xAt(t);
        if (x < ruler.x || x > ruler.x + ruler.width) continue;
        commands.push_back(linePaintCommand({x, area.y + kRuler - 9.0F},
            {x, area.y + kRuler - 3.0F}, 1.0F,
            timelineClip, style_.minorGrid));
    }
    const double first = std::ceil(viewStart_ / step) * step;
    for (double t = first; t <= viewStart_ + viewDuration_ + step * 0.001;
            t += step) {
        const float x = xAt(t);
        if (x < ruler.x || x > ruler.x + ruler.width) continue;
        commands.push_back(linePaintCommand({x, area.y + kRuler - 10.0F},
            {x, area.y + area.height}, 1.0F, timelineClip, style_.grid));
        auto tick = textEngine_->createLayout(tickText(t), textStyle_, {});
        if (tick && x + 4.0F + tick->size().width <=
                ruler.x + ruler.width - 2.0F)
            tick->appendPaintCommands({x + 4.0F, area.y + 5.0F},
                timelineClip, style_.text, commands);
    }
    for (std::size_t i = 0; i < tracks_.size(); ++i) {
        const float y = area.y + kRuler + static_cast<float>(i) * kRow;
        if (y >= area.y + area.height) break;
        commands.push_back(linePaintCommand({area.x, y + kRow},
            {area.x + area.width, y + kRow}, 1.0F,
            visible, style_.minorGrid));
        commands.push_back({{area.x + 8.0F, y + 11.0F, 2.0F, 14.0F},
            intersect(visible, {area.x, y, names, kRow}),
            tracks_[i].color, invalidTextureId, 1.0F});
        label(commands, tracks_[i].label,
            {area.x + 16.0F, y + 9.0F},
            intersect(visible, {area.x, y, names, kRow}), style_.text);
        for (const auto& key : tracks_[i].keys) {
            const Rect marker = keyBounds(tracks_[i].id, key.id);
            if (!hasArea(intersect(marker, timelineClip))) continue;
            const bool chosen = containsSelection({tracks_[i].id, key.id});
            drawKey(commands, marker, timelineClip,
                chosen ? style_.selected : tracks_[i].color,
                key.interpolation, chosen, style_.selected);
        }
    }
    const float playX = xAt(time_);
    commands.push_back(linePaintCommand({playX, area.y + kRuler - 11.0F},
        {playX, area.y + area.height}, 1.5F,
        timelineClip, style_.playhead));
    commands.push_back({{playX - 4.5F, area.y + kRuler - 13.0F,
        9.0F, 9.0F}, timelineClip, style_.playhead,
        invalidTextureId, 4.0F});
}

bool TimelineView::acceptsPointerEvents() const noexcept { return true; }

bool TimelineView::onPointerEvent(const WidgetPointerEvent& event) {
    if (event.type == WidgetPointerEventType::Press &&
        event.button == PointerButton::Primary &&
        contains(bounds(), event.position)) {
        if (event.position.y < bounds().y + kRuler &&
            event.position.x < bounds().x + nameWidth()) {
            const float local = event.position.x - bounds().x;
            const float buttonWidth = nameWidth() / 3.0F;
            if (local < buttonWidth) {
                playing_ = !playing_;
                if (onPlayChanged_) onPlayChanged_(playing_);
            } else if (local < buttonWidth * 2.0F) cycleInterpolation();
            else deleteSelection();
            return true;
        }
        if (event.position.x < bounds().x + nameWidth()) return true;
        for (std::size_t i = tracks_.size(); i > 0; --i)
            for (auto& key : tracks_[i - 1].keys)
                if (contains(keyBounds(tracks_[i - 1].id, key.id),
                        event.position)) {
                    const TimelineKeyRef ref{tracks_[i - 1].id, key.id};
                    if (event.modifiers.control || event.modifiers.meta) {
                        toggleSelection(ref);
                        return true;
                    }
                    if (!containsSelection(ref)) select(ref);
                    drag_ = Drag::Key;
                    dragStartTimes_.clear();
                    for (const auto& chosen : selection_)
                        dragStartTimes_.push_back(
                            findKey(tracks_, chosen)->time);
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
        if (drag_ == Drag::Key && !selection_.empty()) {
            const double delta = (event.position.x - dragStartX_) /
                std::max(1.0F, rulerBounds().width -
                    2.0F * kRulerInset) * viewDuration_;
            const auto limits = std::minmax_element(
                dragStartTimes_.begin(), dragStartTimes_.end());
            const double bounded = std::clamp(delta, -*limits.first,
                std::max(-*limits.first,
                    duration_ - *limits.second));
            for (std::size_t i = 0; i < selection_.size(); ++i)
                if (auto* key = findKey(tracks_, selection_[i]))
                    key->time = dragStartTimes_[i] + bounded;
            return true;
        }
    }
    if (event.type == WidgetPointerEventType::Release ||
        event.type == WidgetPointerEventType::Cancel) {
        const Drag ending = drag_;
        drag_ = Drag::None;
        if (ending == Drag::Key && !selection_.empty()) {
            const auto chosen = selection_;
            const auto origins = dragStartTimes_;
            dragStartTimes_.clear();
            if (event.type == WidgetPointerEventType::Cancel) {
                for (std::size_t i = 0; i < chosen.size(); ++i)
                    if (auto* key = findKey(tracks_, chosen[i]))
                        key->time = origins[i];
            } else if (auto* first = findKey(tracks_, chosen.front())) {
                const double delta = first->time - origins.front();
                if (std::abs(delta) > 1e-8) {
                    if (onKeysMoved_) onKeysMoved_(chosen, delta);
                    else if (onKeyMoved_)
                        for (const auto& ref : chosen)
                            if (auto* key = findKey(tracks_, ref))
                                onKeyMoved_(ref, key->time);
                }
            }
            return true;
        }
        return ending != Drag::None;
    }
    return false;
}

bool TimelineView::onScrollEvent(const WidgetScrollEvent& event) {
    if (!contains(bounds(), event.position) ||
        event.position.x < rulerBounds().x || event.delta.y == 0.0F)
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
    if (event.key == KeyCode::Delete || event.key == KeyCode::Backspace) {
        if (selection_.empty()) return false;
        deleteSelection();
        return true;
    }
    if (event.key == KeyCode::F2) {
        if (selection_.empty()) return false;
        cycleInterpolation();
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
