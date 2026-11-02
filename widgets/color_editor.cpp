#include "widgets/color_editor.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <utility>

namespace lotui {
namespace {

float dimension(float value) noexcept {
    return std::isfinite(value) ? std::max(0.0F, value) : 0.0F;
}

std::unique_ptr<TextLayout> textLayout(const TextEngine& engine,
    std::string_view text, const TextStyle& style) {
    auto result = engine.createLayout(text, style, {});
    if (!result) throw std::runtime_error("TextEngine returned a null layout");
    return result;
}

void validateColor(Color color) {
    if (!std::isfinite(color.red) || !std::isfinite(color.green) ||
        !std::isfinite(color.blue) || !std::isfinite(color.alpha))
        throw std::invalid_argument("ColorEditor color must be finite");
}

} // namespace

ColorEditor::ColorEditor(std::shared_ptr<const TextEngine> textEngine,
    Color color, ChangedHandler onChanged, Size preferredSize,
    ColorEditorStyle style, TextStyle textStyle)
    : textEngine_(std::move(textEngine)),
      onChanged_(std::move(onChanged)),
      style_(style), textStyle_(std::move(textStyle)) {
    if (!textEngine_) throw std::invalid_argument(
        "ColorEditor requires a TextEngine");
    if (!isValidTextStyle(textStyle_)) throw std::invalid_argument(
        "ColorEditor text style is invalid");
    setPreferredSize(preferredSize);
    setStyle(style_);
    constexpr std::array<std::string_view, 3> names{{"R", "G", "B"}};
    for (std::size_t i = 0; i < labels_.size(); ++i)
        labels_[i] = textLayout(*textEngine_, names[i], textStyle_);
    setColor(color);
}

Size ColorEditor::measure(const LayoutConstraints& constraints) const {
    return constraints.constrain(preferredSize_);
}

void ColorEditor::setColor(Color color) {
    validateColor(color);
    const Color normalized{
        std::clamp(color.red, 0.0F, 1.0F),
        std::clamp(color.green, 0.0F, 1.0F),
        std::clamp(color.blue, 0.0F, 1.0F),
        std::clamp(color.alpha, 0.0F, 1.0F)};
    const Color before = color_;
    color_ = normalized;
    const std::array<float, 3> previous{{
        before.red, before.green, before.blue}};
    for (std::size_t i = 0; i < values_.size(); ++i)
        if (!values_[i] || previous[i] != channelValue(i))
            refreshValueLayout(i);
}

Color ColorEditor::color() const noexcept { return color_; }

void ColorEditor::setOnChanged(ChangedHandler handler) {
    onChanged_ = std::move(handler);
}

void ColorEditor::setOnEditStarted(EditHandler handler) {
    onEditStarted_ = std::move(handler);
}

void ColorEditor::setOnEditFinished(EditHandler handler) {
    onEditFinished_ = std::move(handler);
}

void ColorEditor::setEnabled(bool enabled) {
    if (enabled_ && !enabled && dragging_) finishEdit();
    enabled_ = enabled;
    if (!enabled_) focused_ = false;
}

bool ColorEditor::isEnabled() const noexcept { return enabled_; }

void ColorEditor::setPreferredSize(Size size) noexcept {
    preferredSize_ = {dimension(size.width), dimension(size.height)};
}

Size ColorEditor::preferredSize() const noexcept { return preferredSize_; }

void ColorEditor::setStyle(ColorEditorStyle style) noexcept {
    style.cornerRadius = dimension(style.cornerRadius);
    style_ = style;
}

const ColorEditorStyle& ColorEditor::style() const noexcept { return style_; }

std::size_t ColorEditor::activeChannel() const noexcept {
    return activeChannel_;
}

Rect ColorEditor::channelTrackBounds(std::size_t channel) const noexcept {
    if (channel >= 3) return {};
    const Rect area = bounds();
    return {area.x + 70.0F, area.y + 11.0F + 27.0F * channel + 10.0F,
        std::max(1.0F, area.width - 116.0F), 6.0F};
}

Rect ColorEditor::previewBounds() const noexcept {
    const Rect area = bounds();
    return {area.x + 10.0F, area.y + 12.0F, 34.0F, 76.0F};
}

void ColorEditor::onPaint(std::vector<PaintCommand>& commands) const {
    const Rect area = bounds();
    const Rect visible = intersect(area, clip());
    if (!hasArea(visible)) return;
    commands.push_back({area, visible, style_.background,
        invalidTextureId, style_.cornerRadius});
    const Rect swatch = previewBounds();
    commands.push_back({swatch, visible, color_, invalidTextureId, 4.0F});
    for (std::size_t i = 0; i < 3; ++i) {
        const Rect track = channelTrackBounds(i);
        const float fraction = channelValue(i);
        const Color fill = enabled_ ? style_.channels[i] : style_.disabled;
        commands.push_back({track, visible, style_.track,
            invalidTextureId, track.height * 0.5F});
        if (fraction > 0.0F)
            commands.push_back({{track.x, track.y,
                track.width * fraction, track.height}, visible, fill,
                invalidTextureId, track.height * 0.5F});
        const Rect thumb{track.x + track.width * fraction - 6.0F,
            track.y - 3.0F, 12.0F, 12.0F};
        if (focused_ && activeChannel_ == i) {
            commands.push_back({{thumb.x - 2.0F, thumb.y - 2.0F,
                thumb.width + 4.0F, thumb.height + 4.0F}, visible,
                style_.focusRing, invalidTextureId, 8.0F});
        }
        commands.push_back({thumb, visible,
            enabled_ ? style_.thumb : style_.disabled,
            invalidTextureId, 6.0F});
        const float rowY = area.y + 11.0F + 27.0F * i;
        labels_[i]->appendPaintCommands({area.x + 54.0F, rowY + 4.0F},
            visible, style_.text, commands);
        values_[i]->appendPaintCommands({area.x + area.width - 34.0F,
            rowY + 4.0F}, visible, style_.text, commands);
    }
}

bool ColorEditor::acceptsPointerEvents() const noexcept { return enabled_; }

bool ColorEditor::onPointerEvent(const WidgetPointerEvent& event) {
    if (!enabled_) return false;
    switch (event.type) {
    case WidgetPointerEventType::Press: {
        if (event.button != PointerButton::Primary) return false;
        const std::size_t channel = hitChannel(event.position);
        if (channel >= 3) return false;
        activeChannel_ = channel;
        dragging_ = true;
        if (onEditStarted_) onEditStarted_();
        applyPointer(event.position.x);
        return true;
    }
    case WidgetPointerEventType::Move:
        if (!dragging_) return event.inside;
        applyPointer(event.position.x);
        return true;
    case WidgetPointerEventType::Release:
        if (event.button != PointerButton::Primary || !dragging_)
            return false;
        applyPointer(event.position.x);
        finishEdit();
        return true;
    case WidgetPointerEventType::Cancel:
        if (!dragging_) return false;
        finishEdit();
        return true;
    case WidgetPointerEventType::Enter:
    case WidgetPointerEventType::Leave:
        return false;
    }
    return false;
}

bool ColorEditor::acceptsFocus() const noexcept { return enabled_; }

bool ColorEditor::onFocusChanged(bool focused) {
    const bool changed = focused_ != focused;
    focused_ = focused;
    if (!focused_ && dragging_) finishEdit();
    return changed;
}

bool ColorEditor::onKeyEvent(const WidgetKeyEvent& event) {
    if (!enabled_ || event.type != WidgetKeyEventType::Press) return false;
    if (event.key == KeyCode::Up) {
        activeChannel_ = activeChannel_ == 0 ? 2 : activeChannel_ - 1;
        return true;
    }
    if (event.key == KeyCode::Down) {
        activeChannel_ = (activeChannel_ + 1) % 3;
        return true;
    }
    float next = channelValue(activeChannel_);
    switch (event.key) {
    case KeyCode::Left: next -= 1.0F / 255.0F; break;
    case KeyCode::Right: next += 1.0F / 255.0F; break;
    case KeyCode::PageDown: next -= 10.0F / 255.0F; break;
    case KeyCode::PageUp: next += 10.0F / 255.0F; break;
    case KeyCode::Home: next = 0.0F; break;
    case KeyCode::End: next = 1.0F; break;
    default: return false;
    }
    next = std::clamp(next, 0.0F, 1.0F);
    if (next != channelValue(activeChannel_)) {
        if (onEditStarted_) onEditStarted_();
        applyUserChannel(activeChannel_, next);
        if (onEditFinished_) onEditFinished_();
    }
    return true;
}

void ColorEditor::applyUserChannel(std::size_t channel, float value) {
    const float normalized = std::clamp(value, 0.0F, 1.0F);
    if (normalized == channelValue(channel)) return;
    switch (channel) {
    case 0: color_.red = normalized; break;
    case 1: color_.green = normalized; break;
    case 2: color_.blue = normalized; break;
    default: return;
    }
    refreshValueLayout(channel);
    if (onChanged_) {
        const ChangedHandler callback = onChanged_;
        callback(color_);
    }
}

void ColorEditor::applyPointer(float x) {
    const Rect track = channelTrackBounds(activeChannel_);
    applyUserChannel(activeChannel_, (x - track.x) / track.width);
}

void ColorEditor::refreshValueLayout(std::size_t channel) {
    const int channelByte = static_cast<int>(
        std::round(channelValue(channel) * 255.0F));
    values_[channel] = textLayout(*textEngine_,
        std::to_string(channelByte), textStyle_);
}

void ColorEditor::finishEdit() {
    dragging_ = false;
    if (onEditFinished_) onEditFinished_();
}

float ColorEditor::channelValue(std::size_t channel) const noexcept {
    switch (channel) {
    case 0: return color_.red;
    case 1: return color_.green;
    case 2: return color_.blue;
    default: return 0.0F;
    }
}

std::size_t ColorEditor::hitChannel(Point position) const noexcept {
    for (std::size_t channel = 0; channel < 3; ++channel) {
        const Rect track = channelTrackBounds(channel);
        const Rect hit{track.x - 7.0F, track.y - 9.0F,
            track.width + 14.0F, 24.0F};
        if (contains(intersect(hit, clip()), position)) return channel;
    }
    return 3;
}

} // namespace lotui
