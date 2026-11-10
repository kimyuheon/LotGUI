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
    commands.push_back({{area.x, area.y, area.width, kRuler}, visible,
        style_.header, invalidTextureId, 2.0F});
    const Rect ruler = rulerBounds();
    const Rect timelineClip = intersect(visible, {ruler.x, area.y,
        ruler.width, area.height});
    label(commands, playing_ ? "❚❚" : "▶",
        {area.x + 14.0F, area.y + 9.0F}, visible, style_.text);
    label(commands, "보간", {area.x + 48.0F, area.y + 9.0F},
        visible, selection_.empty() ? style_.grid : style_.text);
    label(commands, "삭제", {area.x + 103.0F, area.y + 9.0F},
        visible, selection_.empty() ? style_.grid : style_.text);
    commands.push_back(linePaintCommand({area.x + 42.0F, area.y + 4.0F},
        {area.x + 42.0F, area.y + kRuler - 4.0F},
        1.0F, visible, style_.grid));
    commands.push_back(linePaintCommand({area.x + 98.0F, area.y + 4.0F},
        {area.x + 98.0F, area.y + kRuler - 4.0F},
        1.0F, visible, style_.grid));
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
            const bool chosen = containsSelection({tracks_[i].id, key.id});
            commands.push_back({marker, timelineClip,
                chosen ? style_.selected : tracks_[i].color,
                invalidTextureId,
                key.interpolation == TimelineInterpolation::Smooth ? 5.0F :
                key.interpolation == TimelineInterpolation::Linear ? 2.0F : 0.0F});
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
            const float local = event.position.x - bounds().x;
            if (local < 42.0F) {
                playing_ = !playing_;
                if (onPlayChanged_) onPlayChanged_(playing_);
            } else if (local < 98.0F) cycleInterpolation();
            else deleteSelection();
            return true;
        }
        if (event.position.x < bounds().x + kNames) return true;
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
