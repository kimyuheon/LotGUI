#include "widgets/combo_box.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace lotui {
namespace {

float dimension(float value, float fallback = 0.0F) noexcept {
    return std::isfinite(value) ? std::max(0.0F, value) : fallback;
}

void sanitize(ComboBoxStyle& style) noexcept {
    style.contentPadding.left = dimension(style.contentPadding.left);
    style.contentPadding.top = dimension(style.contentPadding.top);
    style.contentPadding.right = dimension(style.contentPadding.right);
    style.contentPadding.bottom = dimension(style.contentPadding.bottom);
    style.popupPadding.left = dimension(style.popupPadding.left);
    style.popupPadding.top = dimension(style.popupPadding.top);
    style.popupPadding.right = dimension(style.popupPadding.right);
    style.popupPadding.bottom = dimension(style.popupPadding.bottom);
    style.arrowAreaWidth = dimension(style.arrowAreaWidth, 24.0F);
    style.itemHeight = std::max(1.0F, dimension(style.itemHeight, 26.0F));
    style.maximumVisibleItems = std::max<std::size_t>(
        1, style.maximumVisibleItems);
    style.cornerRadius = dimension(style.cornerRadius);
    style.popupCornerRadius = dimension(style.popupCornerRadius);
    style.focusRingWidth = dimension(style.focusRingWidth);
}

Rect inset(Rect bounds, float amount) noexcept {
    const float value = std::min(
        amount, std::min(bounds.width, bounds.height) * 0.5F);
    return {
        bounds.x + value,
        bounds.y + value,
        std::max(0.0F, bounds.width - value * 2.0F),
        std::max(0.0F, bounds.height - value * 2.0F),
    };
}

class ComboPopupList final : public Widget {
public:
    ComboPopupList(
        PopupHost& host,
        std::shared_ptr<const TextEngine> textEngine,
        std::vector<std::string> options,
        std::optional<std::size_t> selectedIndex,
        ComboBox::SelectionChangedHandler onSelectionChanged,
        ComboBoxStyle style,
        TextStyle textStyle)
        : host_(&host),
          textEngine_(std::move(textEngine)),
          options_(std::move(options)),
          highlightedIndex_(selectedIndex),
          onSelectionChanged_(std::move(onSelectionChanged)),
          style_(style),
          textStyle_(std::move(textStyle)),
          layouts_(options_.size()) {
        if (!highlightedIndex_ && !options_.empty()) {
            highlightedIndex_ = 0;
        }
        ensureHighlightedVisible();
    }

    Size measure(const LayoutConstraints& constraints) const override {
        float textWidth = 0.0F;
        for (std::size_t index = 0; index < options_.size(); ++index) {
            textWidth = std::max(
                textWidth,
                ensureLayout(index, unboundedLayoutSize).size().width);
        }
        const std::size_t visible = std::min(
            options_.size(), style_.maximumVisibleItems);
        return constraints.constrain({
            textWidth + style_.popupPadding.horizontal() +
                style_.contentPadding.horizontal(),
            static_cast<float>(visible) * style_.itemHeight +
                style_.popupPadding.vertical(),
        });
    }

protected:
    void onPaint(std::vector<PaintCommand>& commands) const override {
        commands.push_back({
            bounds(), clip(), style_.popupBackground,
            invalidTextureId, style_.popupCornerRadius,
        });
        const std::size_t visible = visibleItemCount();
        for (std::size_t offset = 0; offset < visible; ++offset) {
            const std::size_t index = firstVisibleIndex_ + offset;
            const Rect row = rowBounds(offset);
            if (highlightedIndex_ && *highlightedIndex_ == index) {
                commands.push_back({
                    row, clip(), focused_
                        ? style_.popupSelected
                        : style_.popupHovered,
                    invalidTextureId, 3.0F,
                });
            } else if (hoveredIndex_ && *hoveredIndex_ == index) {
                commands.push_back({
                    row, clip(), style_.popupHovered,
                    invalidTextureId, 3.0F,
                });
            }
            const float maximumWidth = std::max(
                0.0F, row.width - style_.contentPadding.horizontal());
            const TextLayout& layout = ensureLayout(index, maximumWidth);
            layout.appendPaintCommands(
                {
                    row.x + style_.contentPadding.left,
                    row.y + std::max(
                        0.0F, row.height - layout.size().height) * 0.5F,
                },
                intersect(row, clip()),
                style_.text,
                commands);
        }
    }

    bool acceptsPointerEvents() const noexcept override {
        return true;
    }

    bool onPointerEvent(const WidgetPointerEvent& event) override {
        switch (event.type) {
        case WidgetPointerEventType::Enter:
        case WidgetPointerEventType::Move: {
            const auto next = event.inside
                ? itemAt(event.position)
                : std::nullopt;
            const bool changed = next != hoveredIndex_;
            hoveredIndex_ = next;
            return changed;
        }
        case WidgetPointerEventType::Leave: {
            const bool changed = hoveredIndex_.has_value();
            hoveredIndex_.reset();
            return changed;
        }
        case WidgetPointerEventType::Press:
            if (event.button != PointerButton::Primary) {
                return false;
            }
            pressedIndex_ = itemAt(event.position);
            if (pressedIndex_) {
                highlightedIndex_ = pressedIndex_;
            }
            return true;
        case WidgetPointerEventType::Release: {
            if (event.button != PointerButton::Primary) {
                return false;
            }
            const auto released = event.inside
                ? itemAt(event.position)
                : std::nullopt;
            const bool activate = pressedIndex_ && released == pressedIndex_;
            const auto selected = pressedIndex_;
            pressedIndex_.reset();
            if (activate && selected) {
                accept(*selected);
            }
            return true;
        }
        case WidgetPointerEventType::Cancel:
            pressedIndex_.reset();
            hoveredIndex_.reset();
            return true;
        }
        return false;
    }

    bool acceptsFocus() const noexcept override {
        return true;
    }

    bool onFocusChanged(bool focused) override {
        const bool changed = focused_ != focused;
        focused_ = focused;
        return changed;
    }

    bool onKeyEvent(const WidgetKeyEvent& event) override {
        const bool handled =
            event.key == KeyCode::Up || event.key == KeyCode::Down ||
            event.key == KeyCode::Home || event.key == KeyCode::End ||
            event.key == KeyCode::Enter || event.key == KeyCode::Space;
        if (!handled) {
            return false;
        }
        if (event.type != WidgetKeyEventType::Press) {
            return true;
        }
        if (options_.empty()) {
            return true;
        }
        std::size_t index = highlightedIndex_.value_or(0);
        switch (event.key) {
        case KeyCode::Up:
            index = index == 0 ? options_.size() - 1 : index - 1;
            break;
        case KeyCode::Down:
            index = (index + 1) % options_.size();
            break;
        case KeyCode::Home:
            index = 0;
            break;
        case KeyCode::End:
            index = options_.size() - 1;
            break;
        case KeyCode::Enter:
        case KeyCode::Space:
            accept(index);
            return true;
        default:
            break;
        }
        highlightedIndex_ = index;
        ensureHighlightedVisible();
        return true;
    }

private:
    std::size_t visibleItemCount() const noexcept {
        return std::min(
            options_.size() - std::min(firstVisibleIndex_, options_.size()),
            style_.maximumVisibleItems);
    }

    Rect rowBounds(std::size_t visibleOffset) const noexcept {
        return {
            bounds().x + style_.popupPadding.left,
            bounds().y + style_.popupPadding.top +
                static_cast<float>(visibleOffset) * style_.itemHeight,
            std::max(0.0F,
                bounds().width - style_.popupPadding.horizontal()),
            style_.itemHeight,
        };
    }

    std::optional<std::size_t> itemAt(Point position) const noexcept {
        const Rect content{
            bounds().x + style_.popupPadding.left,
            bounds().y + style_.popupPadding.top,
            std::max(0.0F,
                bounds().width - style_.popupPadding.horizontal()),
            std::max(0.0F,
                bounds().height - style_.popupPadding.vertical()),
        };
        if (!contains(content, position)) {
            return std::nullopt;
        }
        const std::size_t offset = static_cast<std::size_t>(
            (position.y - content.y) / style_.itemHeight);
        const std::size_t index = firstVisibleIndex_ + offset;
        return offset < visibleItemCount() && index < options_.size()
            ? std::optional<std::size_t>{index}
            : std::nullopt;
    }

    void accept(std::size_t index) {
        ComboBox::SelectionChangedHandler callback = onSelectionChanged_;
        if (host_) {
            host_->acceptPopup();
        }
        if (callback) {
            callback(index);
        }
    }

    void ensureHighlightedVisible() noexcept {
        if (!highlightedIndex_) {
            firstVisibleIndex_ = 0;
            return;
        }
        if (*highlightedIndex_ < firstVisibleIndex_) {
            firstVisibleIndex_ = *highlightedIndex_;
        } else if (*highlightedIndex_ >=
            firstVisibleIndex_ + style_.maximumVisibleItems) {
            firstVisibleIndex_ = *highlightedIndex_ -
                style_.maximumVisibleItems + 1;
        }
    }

    const TextLayout& ensureLayout(
        std::size_t index,
        float maximumWidth) const {
        const float width = std::isfinite(maximumWidth)
            ? std::max(0.0F, maximumWidth)
            : unboundedLayoutSize;
        if (!layouts_[index] || layoutWidths_[index] != width) {
            TextLayoutOptions options;
            options.maximumWidth = width;
            options.maximumLines = 1;
            layouts_[index] = textEngine_->createLayout(
                options_[index], textStyle_, options);
            layoutWidths_[index] = width;
            if (!layouts_[index]) {
                throw std::runtime_error(
                    "ComboBox TextEngine returned a null layout");
            }
        }
        return *layouts_[index];
    }

    PopupHost* host_{nullptr};
    std::shared_ptr<const TextEngine> textEngine_;
    std::vector<std::string> options_;
    std::optional<std::size_t> highlightedIndex_;
    std::optional<std::size_t> hoveredIndex_;
    std::optional<std::size_t> pressedIndex_;
    ComboBox::SelectionChangedHandler onSelectionChanged_{};
    ComboBoxStyle style_{};
    TextStyle textStyle_{};
    std::size_t firstVisibleIndex_{0};
    bool focused_{false};
    mutable std::vector<std::unique_ptr<TextLayout>> layouts_;
    mutable std::vector<float> layoutWidths_ =
        std::vector<float>(options_.size(), -1.0F);
};

} // namespace

struct ComboBox::CallbackState {
    ComboBox* owner{nullptr};
};

ComboBox::ComboBox(
    std::shared_ptr<const TextEngine> textEngine,
    std::vector<std::string> options,
    std::optional<std::size_t> selectedIndex,
    Size preferredSize,
    SelectionChangedHandler onSelectionChanged,
    ComboBoxStyle style,
    TextStyle textStyle)
    : textEngine_(std::move(textEngine)),
      options_(std::move(options)),
      preferredSize_(preferredSize),
      onSelectionChanged_(std::move(onSelectionChanged)),
      style_(style),
      textStyle_(std::move(textStyle)),
      callbackState_(std::make_shared<CallbackState>()) {
    if (!textEngine_) {
        throw std::invalid_argument("ComboBox requires a TextEngine");
    }
    if (!isValidTextStyle(textStyle_)) {
        throw std::invalid_argument("ComboBox text style is invalid");
    }
    sanitize(style_);
    callbackState_->owner = this;
    setSelectedIndex(selectedIndex);
}

Size ComboBox::measure(const LayoutConstraints& constraints) const {
    return constraints.constrain({
        dimension(preferredSize_.width),
        dimension(preferredSize_.height),
    });
}

void ComboBox::setPopupHost(PopupHost* host) noexcept {
    popupHost_ = host;
    popupId_ = invalidPopupId;
}

PopupHost* ComboBox::popupHost() const noexcept {
    return popupHost_;
}

bool ComboBox::openPopup() {
    if (!enabled_ || !popupHost_ || options_.empty()) {
        return false;
    }
    if (isPopupOpen()) {
        return closePopup();
    }
    std::weak_ptr<CallbackState> state = callbackState_;
    popupId_ = showPopupAt(
        *popupHost_,
        textEngine_,
        bounds(),
        options_,
        selectedIndex_,
        [state](std::size_t index) {
            if (const auto locked = state.lock()) {
                locked->owner->select(index, true);
            }
        },
        style_,
        textStyle_,
        [state](PopupCloseReason) {
            if (const auto locked = state.lock()) {
                locked->owner->popupId_ = invalidPopupId;
            }
        });
    return popupId_ != invalidPopupId;
}

bool ComboBox::closePopup() {
    if (!isPopupOpen()) {
        popupId_ = invalidPopupId;
        return false;
    }
    const PopupId id = popupId_;
    popupId_ = invalidPopupId;
    return popupHost_->dismissPopup(id);
}

bool ComboBox::isPopupOpen() const noexcept {
    return popupHost_ && popupId_ != invalidPopupId &&
        popupHost_->popupId() == popupId_;
}

void ComboBox::setOptions(std::vector<std::string> options) {
    options_ = std::move(options);
    if (selectedIndex_ && *selectedIndex_ >= options_.size()) {
        selectedIndex_.reset();
    }
    closePopup();
    invalidateLayout();
}

const std::vector<std::string>& ComboBox::options() const noexcept {
    return options_;
}

void ComboBox::setSelectedIndex(
    std::optional<std::size_t> index,
    bool notify) {
    if (index && *index >= options_.size()) {
        throw std::out_of_range("ComboBox selection is out of range");
    }
    if (selectedIndex_ == index) {
        return;
    }
    selectedIndex_ = index;
    invalidateLayout();
    if (notify && index && onSelectionChanged_) {
        SelectionChangedHandler callback = onSelectionChanged_;
        callback(*index);
    }
}

std::optional<std::size_t> ComboBox::selectedIndex() const noexcept {
    return selectedIndex_;
}

const std::string* ComboBox::selectedText() const noexcept {
    return selectedIndex_ ? &options_[*selectedIndex_] : nullptr;
}

void ComboBox::setEnabled(bool enabled) noexcept {
    enabled_ = enabled;
    if (!enabled_) {
        hovered_ = false;
        pressed_ = false;
        focused_ = false;
        closePopup();
    }
}

bool ComboBox::isEnabled() const noexcept {
    return enabled_;
}

bool ComboBox::isFocused() const noexcept {
    return focused_;
}

void ComboBox::setPreferredSize(Size preferredSize) noexcept {
    preferredSize_ = preferredSize;
}

void ComboBox::setOnSelectionChanged(SelectionChangedHandler handler) {
    onSelectionChanged_ = std::move(handler);
}

void ComboBox::setStyle(ComboBoxStyle style) noexcept {
    sanitize(style);
    style_ = style;
}

const ComboBoxStyle& ComboBox::style() const noexcept {
    return style_;
}

PopupId ComboBox::showPopupAt(
    PopupHost& host,
    std::shared_ptr<const TextEngine> textEngine,
    Rect anchor,
    std::vector<std::string> options,
    std::optional<std::size_t> selectedIndex,
    SelectionChangedHandler onSelectionChanged,
    ComboBoxStyle style,
    TextStyle textStyle,
    PopupClosedHandler onClosed) {
    if (!textEngine) {
        throw std::invalid_argument("ComboBox popup requires a TextEngine");
    }
    if (selectedIndex && *selectedIndex >= options.size()) {
        throw std::out_of_range("ComboBox popup selection is out of range");
    }
    if (!isValidTextStyle(textStyle)) {
        throw std::invalid_argument("ComboBox popup text style is invalid");
    }
    sanitize(style);
    auto popup = std::make_unique<ComboPopupList>(
        host,
        std::move(textEngine),
        std::move(options),
        selectedIndex,
        std::move(onSelectionChanged),
        style,
        std::move(textStyle));
    PopupOptions popupOptions;
    popupOptions.matchAnchorWidth = true;
    return host.showPopup(
        std::move(popup), anchor, popupOptions, std::move(onClosed));
}

void ComboBox::onPaint(std::vector<PaintCommand>& commands) const {
    const float ringWidth = focused_
        ? std::min(style_.focusRingWidth,
            std::min(bounds().width, bounds().height) * 0.5F)
        : 0.0F;
    if (ringWidth > 0.0F) {
        commands.push_back({
            bounds(), clip(), style_.focusRing,
            invalidTextureId, style_.cornerRadius,
        });
    }
    const Rect content = inset(bounds(), ringWidth);
    commands.push_back({
        content, clip(), currentColor(), invalidTextureId,
        std::max(0.0F, style_.cornerRadius - ringWidth),
    });

    const float arrowWidth = std::min(style_.arrowAreaWidth, content.width);
    const float lineHeight = std::max(1.0F, content.height * 0.06F);
    commands.push_back({
        {
            content.x + content.width - arrowWidth * 0.72F,
            content.y + content.height * 0.44F,
            arrowWidth * 0.44F,
            lineHeight,
        },
        clip(), enabled_ ? style_.arrow : style_.disabledText,
        invalidTextureId, lineHeight * 0.5F,
    });
    commands.push_back({
        {
            content.x + content.width - arrowWidth * 0.61F,
            content.y + content.height * 0.58F,
            arrowWidth * 0.22F,
            lineHeight,
        },
        clip(), enabled_ ? style_.arrow : style_.disabledText,
        invalidTextureId, lineHeight * 0.5F,
    });

    const float textWidth = std::max(
        0.0F, content.width - style_.contentPadding.horizontal() -
            arrowWidth);
    if (const TextLayout* text = ensureLayout(textWidth)) {
        text->appendPaintCommands(
            {
                content.x + style_.contentPadding.left,
                content.y + std::max(
                    0.0F, content.height - text->size().height) * 0.5F,
            },
            intersect(content, clip()),
            enabled_ ? style_.text : style_.disabledText,
            commands);
    }
}

bool ComboBox::acceptsPointerEvents() const noexcept {
    return enabled_;
}

bool ComboBox::onPointerEvent(const WidgetPointerEvent& event) {
    if (!enabled_) {
        return false;
    }
    switch (event.type) {
    case WidgetPointerEventType::Enter:
        hovered_ = true;
        return true;
    case WidgetPointerEventType::Leave:
        hovered_ = false;
        return true;
    case WidgetPointerEventType::Move: {
        const bool changed = hovered_ != event.inside;
        hovered_ = event.inside;
        return changed;
    }
    case WidgetPointerEventType::Press:
        if (event.button != PointerButton::Primary) {
            return false;
        }
        pressed_ = true;
        return true;
    case WidgetPointerEventType::Release:
        if (event.button != PointerButton::Primary || !pressed_) {
            return false;
        }
        pressed_ = false;
        if (event.inside) {
            openPopup();
        }
        return true;
    case WidgetPointerEventType::Cancel:
        pressed_ = false;
        hovered_ = false;
        return true;
    }
    return false;
}

bool ComboBox::acceptsFocus() const noexcept {
    return enabled_;
}

bool ComboBox::onFocusChanged(bool focused) {
    const bool changed = focused_ != focused;
    focused_ = focused;
    return changed;
}

bool ComboBox::onKeyEvent(const WidgetKeyEvent& event) {
    const bool handled =
        event.key == KeyCode::Up || event.key == KeyCode::Down ||
        event.key == KeyCode::Enter || event.key == KeyCode::Space;
    if (!enabled_ || !handled) {
        return false;
    }
    if (event.type != WidgetKeyEventType::Press) {
        return true;
    }
    if (event.key == KeyCode::Enter || event.key == KeyCode::Space) {
        openPopup();
        return true;
    }
    if (options_.empty()) {
        return true;
    }
    std::size_t index = selectedIndex_.value_or(0);
    if (event.key == KeyCode::Up) {
        index = index == 0 ? options_.size() - 1 : index - 1;
    } else {
        index = (index + 1) % options_.size();
    }
    select(index, true);
    return true;
}

void ComboBox::select(std::size_t index, bool notify) {
    setSelectedIndex(index, notify);
}

void ComboBox::invalidateLayout() const noexcept {
    layout_.reset();
    layoutWidth_ = -1.0F;
}

const TextLayout* ComboBox::ensureLayout(float maximumWidth) const {
    if (!selectedIndex_) {
        return nullptr;
    }
    const float width = std::max(0.0F, maximumWidth);
    if (!layout_ || layoutWidth_ != width) {
        TextLayoutOptions options;
        options.maximumWidth = width;
        options.maximumLines = 1;
        layout_ = textEngine_->createLayout(
            options_[*selectedIndex_], textStyle_, options);
        layoutWidth_ = width;
        if (!layout_) {
            throw std::runtime_error(
                "ComboBox TextEngine returned a null layout");
        }
    }
    return layout_.get();
}

Color ComboBox::currentColor() const noexcept {
    if (!enabled_) {
        return style_.disabled;
    }
    if (pressed_) {
        return style_.pressed;
    }
    return hovered_ || isPopupOpen() ? style_.hovered : style_.normal;
}

} // namespace lotui
