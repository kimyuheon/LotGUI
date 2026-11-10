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

struct TimelineKey {
    std::string id;
    double time{0.0};
};

struct TimelineTrack {
    std::string id;
    std::string label;
    std::vector<TimelineKey> keys;
    Color color{0.42F, 0.76F, 0.96F, 1.0F};
};

struct TimelineKeyRef {
    std::string trackId;
    std::string keyId;
    bool operator==(const TimelineKeyRef& other) const noexcept {
        return trackId == other.trackId && keyId == other.keyId;
    }
};

struct TimelineViewStyle {
    Color background{0.025F, 0.032F, 0.045F, 1.0F};
    Color header{0.09F, 0.12F, 0.16F, 1.0F};
    Color alternateRow{0.045F, 0.06F, 0.08F, 1.0F};
    Color grid{0.16F, 0.21F, 0.27F, 1.0F};
    Color text{0.88F, 0.91F, 0.95F, 1.0F};
    Color playhead{0.98F, 0.64F, 0.32F, 1.0F};
    Color selected{1.0F, 0.9F, 0.57F, 1.0F};
};

class TimelineView final : public Widget {
public:
    using TimeHandler = std::function<void(double)>;
    using PlayHandler = std::function<void(bool)>;
    using KeyHandler = std::function<void(std::optional<TimelineKeyRef>)>;
    using KeyMovedHandler = std::function<void(const TimelineKeyRef&, double)>;

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
    Rect keyBounds(std::string_view trackId, std::string_view keyId) const noexcept;
    Rect rulerBounds() const noexcept;
    void setOnTimeChanged(TimeHandler callback);
    void setOnPlayChanged(PlayHandler callback);
    void setOnKeySelected(KeyHandler callback);
    void setOnKeyMoved(KeyMovedHandler callback);

protected:
    void onPaint(std::vector<PaintCommand>& commands) const override;
    bool acceptsPointerEvents() const noexcept override;
    bool onPointerEvent(const WidgetPointerEvent& event) override;
    bool onScrollEvent(const WidgetScrollEvent& event) override;
    bool acceptsFocus() const noexcept override;
    bool onKeyEvent(const WidgetKeyEvent& event) override;

private:
    enum class Drag { None, Scrub, Key };
    double timeAt(float x) const noexcept;
    float xAt(double seconds) const noexcept;
    void seek(double seconds);
    void select(std::optional<TimelineKeyRef> key);
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
    Drag drag_{Drag::None};
    double dragStartTime_{0.0};
    float dragStartX_{0.0F};
    PlayHandler onPlayChanged_;
    TimeHandler onTimeChanged_;
    KeyHandler onKeySelected_;
    KeyMovedHandler onKeyMoved_;
};

} // namespace lotui
