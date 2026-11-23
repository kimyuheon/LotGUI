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

enum class TimelineInterpolation { Smooth, Linear, Step };

struct TimelineKey {
    std::string id;
    double time{0.0};
    TimelineInterpolation interpolation{TimelineInterpolation::Smooth};
};

struct TimelineTrack {
    std::string id;
    std::string label;
    std::vector<TimelineKey> keys;
    Color color{colorFromSrgb(0x7797AC)};
};

struct TimelineKeyRef {
    std::string trackId;
    std::string keyId;
    bool operator==(const TimelineKeyRef& other) const noexcept {
        return trackId == other.trackId && keyId == other.keyId;
    }
};

struct TimelineViewStyle {
    Color background{colorFromSrgb(0x181E26)};
    Color header{colorFromSrgb(0x242C35)};
    Color alternateRow{colorFromSrgb(0x202832)};
    Color grid{colorFromSrgb(0x394550)};
    Color text{colorFromSrgb(0xE5EAF0)};
    Color playhead{colorFromSrgb(0xB4875E)};
    Color selected{colorFromSrgb(0x7795AD)};
    Color namePanel{colorFromSrgb(0x202832)};
    Color minorGrid{colorFromSrgb(0x303B46)};
    Color selectedRow{colorFromSrgb(0x2A3B4B)};
};

class TimelineView final : public Widget {
public:
    using TimeHandler = std::function<void(double)>;
    using PlayHandler = std::function<void(bool)>;
    using KeyHandler = std::function<void(std::optional<TimelineKeyRef>)>;
    using KeyMovedHandler = std::function<void(const TimelineKeyRef&, double)>;
    using SelectionHandler = std::function<void(const std::vector<TimelineKeyRef>&)>;
    using KeysMovedHandler = std::function<void(const std::vector<TimelineKeyRef>&, double)>;
    using KeysDeletedHandler = std::function<void(const std::vector<TimelineKeyRef>&)>;
    using InterpolationHandler = std::function<void(const std::vector<TimelineKeyRef>&,
        TimelineInterpolation)>;

    explicit TimelineView(std::shared_ptr<const TextEngine> textEngine,
        Size preferredSize = {800.0F, 320.0F},
        TimelineViewStyle style = {}, TextStyle textStyle = {});
    Size measure(const LayoutConstraints& constraints) const override;
    void setTracks(std::vector<TimelineTrack> tracks);
    const std::vector<TimelineTrack>& tracks() const noexcept;
    void setDuration(double seconds);
    double duration() const noexcept;
    void setTime(double seconds) noexcept;
    double time() const noexcept;
    void setPlaying(bool playing) noexcept;
    bool playing() const noexcept;
    void setView(double startSeconds, double visibleSeconds);
    double viewStart() const noexcept;
    double viewDuration() const noexcept;
    std::optional<TimelineKeyRef> selectedKey() const;
    const std::vector<TimelineKeyRef>& selectedKeys() const noexcept;
    Rect keyBounds(std::string_view trackId, std::string_view keyId) const noexcept;
    Rect rulerBounds() const noexcept;
    void setOnTimeChanged(TimeHandler callback);
    void setOnPlayChanged(PlayHandler callback);
    void setOnKeySelected(KeyHandler callback);
    void setOnKeyMoved(KeyMovedHandler callback);
    void setOnSelectionChanged(SelectionHandler callback);
    void setOnKeysMoved(KeysMovedHandler callback);
    void setOnKeysDeleted(KeysDeletedHandler callback);
    void setOnInterpolationChanged(InterpolationHandler callback);

protected:
    void onPaint(std::vector<PaintCommand>& commands) const override;
    bool acceptsPointerEvents() const noexcept override;
    bool onPointerEvent(const WidgetPointerEvent& event) override;
    bool onScrollEvent(const WidgetScrollEvent& event) override;
    bool acceptsFocus() const noexcept override;
    bool onKeyEvent(const WidgetKeyEvent& event) override;

private:
    enum class Drag { None, Scrub, Key, Marquee };
    float nameWidth() const noexcept;
    double timeAt(float x) const noexcept;
    float xAt(double seconds) const noexcept;
    void seek(double seconds);
    void select(std::optional<TimelineKeyRef> key);
    bool containsSelection(const TimelineKeyRef& key) const noexcept;
    void toggleSelection(TimelineKeyRef key);
    void selectRange(const TimelineKeyRef& key, bool additive);
    void updateMarquee(Point position);
    Rect marqueeBounds() const noexcept;
    void notifySelection();
    void deleteSelection();
    void cycleInterpolation();
    void label(std::vector<PaintCommand>& commands, std::string_view text,
        Point origin, Rect clip, Color color) const;

    std::shared_ptr<const TextEngine> textEngine_;
    Size preferredSize_{};
    TimelineViewStyle style_{};
    TextStyle textStyle_{};
    std::vector<TimelineTrack> tracks_;
    double duration_{10.0};
    double time_{0.0};
    double viewStart_{0.0};
    double viewDuration_{10.0};
    bool playing_{false};
    std::optional<TimelineKeyRef> selected_;
    std::optional<TimelineKeyRef> anchor_;
    std::vector<TimelineKeyRef> selection_;
    std::vector<TimelineKeyRef> marqueeBaseSelection_;
    std::vector<TimelineKeyRef> marqueeOriginalSelection_;
    std::optional<TimelineKeyRef> marqueeOriginalSelected_;
    std::optional<TimelineKeyRef> marqueeOriginalAnchor_;
    Drag drag_{Drag::None};
    Point marqueeStart_{};
    Point marqueeEnd_{};
    bool marqueeActive_{false};
    std::vector<double> dragStartTimes_;
    float dragStartX_{0.0F};
    PlayHandler onPlayChanged_;
    TimeHandler onTimeChanged_;
    KeyHandler onKeySelected_;
    KeyMovedHandler onKeyMoved_;
    SelectionHandler onSelectionChanged_;
    KeysMovedHandler onKeysMoved_;
    KeysDeletedHandler onKeysDeleted_;
    InterpolationHandler onInterpolationChanged_;
};

} // namespace lotui
