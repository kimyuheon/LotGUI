#include "widgets/radio_group.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace lotui {
namespace {

float dimension(float value) noexcept {
    return std::isfinite(value) ? std::max(0.0F, value) : 0.0F;
}

void sanitize(RadioGroupStyle& style) noexcept {
    style.rowHeight = std::max(1.0F, dimension(style.rowHeight));
    style.indicatorSize = dimension(style.indicatorSize);
    style.padding = dimension(style.padding);
    style.spacing = dimension(style.spacing);
}

} // namespace

RadioGroup::RadioGroup(
    std::shared_ptr<const TextEngine> textEngine,
    std::vector<RadioOption> options,
    std::optional<std::string> selectedId,
    ChangedHandler onChanged,
    RadioGroupStyle style,
    TextStyle textStyle)
    : textEngine_(std::move(textEngine)),
      onChanged_(std::move(onChanged)),
      style_(style),
      textStyle_(std::move(textStyle)) {
    sanitize(style_);
    setOptions(std::move(options));
    if (selectedId) setSelectedId(*selectedId);
}

Size RadioGroup::measure(const LayoutConstraints& constraints) const {
    float width = style_.padding * 2.0F + style_.indicatorSize +
        style_.spacing;
    if (textEngine_) {
        for (const RadioOption& option : options_) {
            const auto layout = textEngine_->createLayout(
                option.label, textStyle_, {});
            width = std::max(width, style_.padding * 2.0F +
                style_.indicatorSize + style_.spacing + layout->size().width);
        }
    }
    return constraints.constrain({width,
        style_.rowHeight * static_cast<float>(options_.size())});
}

void RadioGroup::setOptions(std::vector<RadioOption> options) {
    std::unordered_set<std::string> ids;
    for (const RadioOption& option : options) {
        if (option.id.empty() || !ids.insert(option.id).second) {
            throw std::invalid_argument("RadioGroup option IDs must be unique and nonempty");
        }
    }
    options_ = std::move(options);
    if (selectedId_) {
        const auto selected = indexOf(*selectedId_);
        if (!selected || !options_[*selected].enabled) selectedId_.reset();
    }
    hovered_.reset();
    pressed_.reset();
}

const std::vector<RadioOption>& RadioGroup::options() const noexcept {
    return options_;
}

bool RadioGroup::setSelectedId(std::string_view id) {
    const auto index = indexOf(id);
    return index && select(*index, false);
}

std::optional<std::string> RadioGroup::selectedId() const {
    return selectedId_;
}

void RadioGroup::setOnChanged(ChangedHandler onChanged) {
    onChanged_ = std::move(onChanged);
}

void RadioGroup::setEnabled(bool enabled) noexcept {
    enabled_ = enabled;
    if (!enabled_) {
        hovered_.reset();
        pressed_.reset();
        focused_ = false;
    }
}

bool RadioGroup::isEnabled() const noexcept {
    return enabled_;
}

void RadioGroup::setStyle(RadioGroupStyle style) noexcept {
    sanitize(style);
    style_ = style;
}

Rect RadioGroup::optionBounds(std::string_view id) const noexcept {
    const auto index = indexOf(id);
    if (!index) return {};
    const float y = bounds().y + static_cast<float>(*index) * style_.rowHeight;
    if (y >= bounds().y + bounds().height) return {};
    return {bounds().x, y, bounds().width,
        std::min(style_.rowHeight, bounds().y + bounds().height - y)};
}

void RadioGroup::onPaint(std::vector<PaintCommand>& commands) const {
    commands.push_back({bounds(), clip(), style_.background,
        invalidTextureId, 0.0F});
    for (std::size_t index = 0; index < options_.size(); ++index) {
        const RadioOption& option = options_[index];
        const Rect row = optionBounds(option.id);
        const Rect rowClip = intersect(row, clip());
        if (!hasArea(rowClip)) continue;
        const bool selected = selectedId_ && *selectedId_ == option.id;
        if (selected || (hovered_ == index && option.enabled && enabled_)) {
            commands.push_back({row, rowClip,
                selected ? style_.selected : style_.hovered,
                invalidTextureId, 0.0F});
        }
        const float size = std::min(style_.indicatorSize, row.height);
        const Rect circle{row.x + style_.padding,
            row.y + (row.height - size) * 0.5F, size, size};
        commands.push_back({circle, rowClip, style_.outline,
            invalidTextureId, size * 0.5F});
        if (selected && size > 4.0F) {
            const Rect dot{circle.x + 3.0F, circle.y + 3.0F,
                size - 6.0F, size - 6.0F};
            commands.push_back({dot, rowClip, style_.indicator,
                invalidTextureId, dot.width * 0.5F});
        }
        if (textEngine_) {
            const auto layout = textEngine_->createLayout(
                option.label, textStyle_, {});
            const Point origin{circle.x + size + style_.spacing,
                row.y + std::max(0.0F,
                    (row.height - layout->size().height) * 0.5F)};
            layout->appendPaintCommands(origin, rowClip,
                enabled_ && option.enabled ? style_.text :
                    style_.disabledText, commands);
        }
    }
    if (focused_) {
        commands.push_back({{bounds().x, bounds().y + bounds().height - 1.0F,
            bounds().width, 1.0F}, clip(), style_.focusLine,
            invalidTextureId, 0.0F});
    }
}

bool RadioGroup::acceptsPointerEvents() const noexcept { return enabled_; }

bool RadioGroup::onPointerEvent(const WidgetPointerEvent& event) {
    if (!enabled_) return false;
    const auto row = optionAt(event.position);
    switch (event.type) {
    case WidgetPointerEventType::Enter:
    case WidgetPointerEventType::Move:
        hovered_ = row;
        return true;
    case WidgetPointerEventType::Leave:
        hovered_.reset();
        return true;
    case WidgetPointerEventType::Press:
        if (event.button != PointerButton::Primary || !row ||
            !options_[*row].enabled) return false;
        pressed_ = row;
        return true;
    case WidgetPointerEventType::Release:
        if (event.button != PointerButton::Primary || !pressed_) return false;
        if (row == pressed_) select(*pressed_, true);
        pressed_.reset();
        return true;
    case WidgetPointerEventType::Cancel:
        pressed_.reset();
        hovered_.reset();
        return true;
    }
    return false;
}

bool RadioGroup::acceptsFocus() const noexcept { return enabled_; }

bool RadioGroup::onFocusChanged(bool focused) {
    const bool changed = focused_ != focused;
    focused_ = focused;
    return changed;
}

bool RadioGroup::onKeyEvent(const WidgetKeyEvent& event) {
    if (!enabled_ || event.type != WidgetKeyEventType::Press) return false;
    switch (event.key) {
    case KeyCode::Up:
    case KeyCode::Left:
        moveSelection(-1);
        return true;
    case KeyCode::Down:
    case KeyCode::Right:
        moveSelection(1);
        return true;
    case KeyCode::Home:
    case KeyCode::End: {
        const int direction = event.key == KeyCode::Home ? 1 : -1;
        int index = direction > 0 ? 0 : static_cast<int>(options_.size()) - 1;
        while (index >= 0 && index < static_cast<int>(options_.size())) {
            if (options_[static_cast<std::size_t>(index)].enabled) {
                select(static_cast<std::size_t>(index), true);
                break;
            }
            index += direction;
        }
        return true;
    }
    case KeyCode::Space:
    case KeyCode::Enter:
        if (!selectedId_) moveSelection(1);
        return true;
    default:
        return false;
    }
}

std::optional<std::size_t> RadioGroup::optionAt(Point position) const noexcept {
    if (!contains(bounds(), position)) return std::nullopt;
    const auto index = static_cast<std::size_t>(
        (position.y - bounds().y) / style_.rowHeight);
    return index < options_.size() ? std::optional{index} : std::nullopt;
}

std::optional<std::size_t> RadioGroup::indexOf(std::string_view id) const noexcept {
    for (std::size_t index = 0; index < options_.size(); ++index) {
        if (options_[index].id == id) return index;
    }
    return std::nullopt;
}

bool RadioGroup::select(std::size_t index, bool notify) {
    if (index >= options_.size() || !options_[index].enabled ||
        selectedId_ == options_[index].id) return false;
    selectedId_ = options_[index].id;
    if (notify && onChanged_) {
        const std::string id = *selectedId_;
        ChangedHandler callback = onChanged_;
        callback(id);
    }
    return true;
}

bool RadioGroup::moveSelection(int direction) {
    if (options_.empty()) return false;
    int index = selectedId_ ? static_cast<int>(*indexOf(*selectedId_)) :
        (direction > 0 ? -1 : static_cast<int>(options_.size()));
    index += direction;
    while (index >= 0 && index < static_cast<int>(options_.size())) {
        if (options_[static_cast<std::size_t>(index)].enabled) {
            return select(static_cast<std::size_t>(index), true);
        }
        index += direction;
    }
    return false;
}

} // namespace lotui
