#include "widgets/lookup_box.h"

#include "widgets/linear_layout.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iterator>
#include <stdexcept>
#include <utility>

namespace lotui {
namespace {

float dimension(float value, float fallback = 0.0F) noexcept {
    return std::isfinite(value) ? std::max(0.0F, value) : fallback;
}

void sanitize(LookupBoxStyle& style) noexcept {
    style.contentPadding.left = dimension(style.contentPadding.left);
    style.contentPadding.top = dimension(style.contentPadding.top);
    style.contentPadding.right = dimension(style.contentPadding.right);
    style.contentPadding.bottom = dimension(style.contentPadding.bottom);
    style.popupPadding.left = dimension(style.popupPadding.left);
    style.popupPadding.top = dimension(style.popupPadding.top);
    style.popupPadding.right = dimension(style.popupPadding.right);
    style.popupPadding.bottom = dimension(style.popupPadding.bottom);
    style.actionAreaWidth = dimension(style.actionAreaWidth, 26.0F);
    style.searchFieldHeight = std::max(
        1.0F, dimension(style.searchFieldHeight, 32.0F));
    style.itemHeight = std::max(1.0F, dimension(style.itemHeight, 34.0F));
    style.popupSpacing = dimension(style.popupSpacing);
    style.popupPreferredWidth = std::max(
        1.0F, dimension(style.popupPreferredWidth, 280.0F));
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

char folded(unsigned char value) noexcept {
    return value < 0x80U
        ? static_cast<char>(std::tolower(value))
        : static_cast<char>(value);
}

std::string foldAscii(std::string_view text) {
    std::string result;
    result.reserve(text.size());
    for (unsigned char value : text) {
        result.push_back(folded(value));
    }
    return result;
}

bool matches(const LookupItem& item, const std::string& foldedQuery) {
    if (foldedQuery.empty()) {
        return true;
    }
    return foldAscii(item.label).find(foldedQuery) != std::string::npos ||
        foldAscii(item.secondaryText).find(foldedQuery) != std::string::npos ||
        foldAscii(item.id).find(foldedQuery) != std::string::npos;
}

class LookupResultList final : public Widget {
public:
    LookupResultList(
        std::shared_ptr<const TextEngine> textEngine,
        std::vector<LookupItem> items,
        std::optional<std::size_t> selectedIndex,
        LookupBox::SelectionChangedHandler onSelected,
        LookupBoxStyle style,
        TextStyle textStyle)
        : textEngine_(std::move(textEngine)),
          items_(std::move(items)),
          selectedIndex_(selectedIndex),
          onSelected_(std::move(onSelected)),
          style_(style),
          textStyle_(std::move(textStyle)),
          layouts_(items_.size()) {
        setFilter({});
    }

    Size measure(const LayoutConstraints& constraints) const override {
        const std::size_t count = std::max<std::size_t>(
            1, std::min(filteredIndices_.size(), style_.maximumVisibleItems));
        return constraints.constrain({
            style_.popupPreferredWidth,
            static_cast<float>(count) * style_.itemHeight,
        });
    }

    void setFilter(std::string query) {
        const std::string foldedQuery = foldAscii(query);
        filteredIndices_.clear();
        for (std::size_t index = 0; index < items_.size(); ++index) {
            if (matches(items_[index], foldedQuery)) {
                filteredIndices_.push_back(index);
            }
        }
        highlightedResult_.reset();
        if (!filteredIndices_.empty()) {
            const auto selected = selectedIndex_
                ? std::find(
                    filteredIndices_.begin(),
                    filteredIndices_.end(),
                    *selectedIndex_)
                : filteredIndices_.end();
            highlightedResult_ = selected != filteredIndices_.end()
                ? static_cast<std::size_t>(
                    std::distance(filteredIndices_.begin(), selected))
                : 0;
        }
        firstVisibleResult_ = 0;
        ensureHighlightedVisible();
        hoveredResult_.reset();
        pressedResult_.reset();
    }

    std::size_t filteredCount() const noexcept {
        return filteredIndices_.size();
    }

    bool acceptHighlighted() {
        if (!highlightedResult_ ||
            *highlightedResult_ >= filteredIndices_.size()) {
            return false;
        }
        accept(*highlightedResult_);
        return true;
    }

    bool handleNavigationKey(const WidgetKeyEvent& event) {
        return onKeyEvent(event);
    }

protected:
    void onPaint(std::vector<PaintCommand>& commands) const override {
        const std::size_t count = visibleCount();
        for (std::size_t offset = 0; offset < count; ++offset) {
            const std::size_t resultIndex = firstVisibleResult_ + offset;
            const std::size_t itemIndex = filteredIndices_[resultIndex];
            const Rect row = rowBounds(offset);
            if (highlightedResult_ && *highlightedResult_ == resultIndex) {
                commands.push_back({
                    row,
                    clip(),
                    focused_ ? style_.resultSelected : style_.resultHovered,
                    invalidTextureId,
                    3.0F,
                });
            } else if (hoveredResult_ && *hoveredResult_ == resultIndex) {
                commands.push_back({
                    row, clip(), style_.resultHovered,
                    invalidTextureId, 3.0F,
                });
            }
            const LookupItem& item = items_[itemIndex];
            const float textWidth = std::max(
                0.0F, row.width - style_.contentPadding.horizontal());
            const TextLayout& label = ensureLayout(
                itemIndex, false, textWidth);
            const TextLayout* secondary = item.secondaryText.empty()
                ? nullptr
                : &ensureLayout(itemIndex, true, textWidth);
            const float totalHeight = label.size().height +
                (secondary ? secondary->size().height : 0.0F);
            float y = row.y + std::max(
                0.0F, row.height - totalHeight) * 0.5F;
            label.appendPaintCommands(
                {row.x + style_.contentPadding.left, y},
                intersect(row, clip()),
                style_.text,
                commands);
            if (secondary) {
                y += label.size().height;
                secondary->appendPaintCommands(
                    {row.x + style_.contentPadding.left, y},
                    intersect(row, clip()),
                    style_.secondaryText,
                    commands);
            }
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
                ? resultAt(event.position)
                : std::nullopt;
            const bool changed = next != hoveredResult_;
            hoveredResult_ = next;
            return changed;
        }
        case WidgetPointerEventType::Leave: {
            const bool changed = hoveredResult_.has_value();
            hoveredResult_.reset();
            return changed;
        }
        case WidgetPointerEventType::Press:
            if (event.button != PointerButton::Primary) {
                return false;
            }
            pressedResult_ = resultAt(event.position);
            if (pressedResult_) {
                highlightedResult_ = pressedResult_;
            }
            return true;
        case WidgetPointerEventType::Release: {
            if (event.button != PointerButton::Primary) {
                return false;
            }
            const auto released = event.inside
                ? resultAt(event.position)
                : std::nullopt;
            const bool activate = pressedResult_ && released == pressedResult_;
            const auto selected = pressedResult_;
            pressedResult_.reset();
            if (activate && selected) {
                accept(*selected);
            }
            return true;
        }
        case WidgetPointerEventType::Cancel:
            pressedResult_.reset();
            hoveredResult_.reset();
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
        if (filteredIndices_.empty()) {
            return true;
        }
        std::size_t result = highlightedResult_.value_or(0);
        switch (event.key) {
        case KeyCode::Up:
            result = result == 0 ? filteredIndices_.size() - 1 : result - 1;
            break;
        case KeyCode::Down:
            result = (result + 1) % filteredIndices_.size();
            break;
        case KeyCode::Home:
            result = 0;
            break;
        case KeyCode::End:
            result = filteredIndices_.size() - 1;
            break;
        case KeyCode::Enter:
        case KeyCode::Space:
            accept(result);
            return true;
        default:
            break;
        }
        highlightedResult_ = result;
        ensureHighlightedVisible();
        return true;
    }

private:
    struct ItemLayouts {
        std::unique_ptr<TextLayout> label;
        std::unique_ptr<TextLayout> secondary;
        float labelWidth{-1.0F};
        float secondaryWidth{-1.0F};
    };

    std::size_t visibleCount() const noexcept {
        std::size_t capacity = style_.maximumVisibleItems;
        if (bounds().height > 0.0F) {
            capacity = std::max<std::size_t>(
                1,
                static_cast<std::size_t>(
                    std::floor(bounds().height / style_.itemHeight)));
            capacity = std::min(capacity, style_.maximumVisibleItems);
        }
        return std::min(
            filteredIndices_.size() -
                std::min(firstVisibleResult_, filteredIndices_.size()),
            capacity);
    }

    Rect rowBounds(std::size_t visibleOffset) const noexcept {
        return {
            bounds().x,
            bounds().y + static_cast<float>(visibleOffset) * style_.itemHeight,
            bounds().width,
            style_.itemHeight,
        };
    }

    std::optional<std::size_t> resultAt(Point position) const noexcept {
        if (!contains(bounds(), position)) {
            return std::nullopt;
        }
        const std::size_t offset = static_cast<std::size_t>(
            (position.y - bounds().y) / style_.itemHeight);
        const std::size_t result = firstVisibleResult_ + offset;
        return offset < visibleCount() && result < filteredIndices_.size()
            ? std::optional<std::size_t>{result}
            : std::nullopt;
    }

    void accept(std::size_t resultIndex) {
        if (resultIndex >= filteredIndices_.size() || !onSelected_) {
            return;
        }
        LookupBox::SelectionChangedHandler callback = onSelected_;
        callback(filteredIndices_[resultIndex]);
    }

    void ensureHighlightedVisible() noexcept {
        if (!highlightedResult_) {
            firstVisibleResult_ = 0;
            return;
        }
        const std::size_t capacity = bounds().height > 0.0F
            ? std::max<std::size_t>(
                1,
                std::min(
                    style_.maximumVisibleItems,
                    static_cast<std::size_t>(std::floor(
                        bounds().height / style_.itemHeight))))
            : style_.maximumVisibleItems;
        if (*highlightedResult_ < firstVisibleResult_) {
            firstVisibleResult_ = *highlightedResult_;
        } else if (*highlightedResult_ >=
            firstVisibleResult_ + capacity) {
            firstVisibleResult_ = *highlightedResult_ -
                capacity + 1;
        }
    }

    const TextLayout& ensureLayout(
        std::size_t itemIndex,
        bool secondary,
        float maximumWidth) const {
        ItemLayouts& cache = layouts_[itemIndex];
        std::unique_ptr<TextLayout>& layout = secondary
            ? cache.secondary : cache.label;
        float& width = secondary ? cache.secondaryWidth : cache.labelWidth;
        if (!layout || width != maximumWidth) {
            TextLayoutOptions options;
            options.maximumWidth = maximumWidth;
            options.maximumLines = 1;
            TextStyle style = textStyle_;
            if (secondary) {
                style.fontSize = std::max(8.0F, style.fontSize - 2.0F);
            }
            layout = textEngine_->createLayout(
                secondary
                    ? items_[itemIndex].secondaryText
                    : items_[itemIndex].label,
                style,
                options);
            width = maximumWidth;
            if (!layout) {
                throw std::runtime_error(
                    "LookupBox TextEngine returned a null layout");
            }
        }
        return *layout;
    }

    std::shared_ptr<const TextEngine> textEngine_;
    std::vector<LookupItem> items_;
    std::optional<std::size_t> selectedIndex_;
    std::vector<std::size_t> filteredIndices_;
    std::optional<std::size_t> highlightedResult_;
    std::optional<std::size_t> hoveredResult_;
    std::optional<std::size_t> pressedResult_;
    LookupBox::SelectionChangedHandler onSelected_{};
    LookupBoxStyle style_{};
    TextStyle textStyle_{};
    std::size_t firstVisibleResult_{0};
    bool focused_{false};
    mutable std::vector<ItemLayouts> layouts_;
};

class LookupPopup final : public LinearLayout {
public:
    explicit LookupPopup(LookupResultList& results)
        : LinearLayout(false), results_(&results) {
    }

protected:
    bool onUnhandledKeyEvent(const WidgetKeyEvent& event) override {
        return results_ && results_->handleNavigationKey(event);
    }

private:
    LookupResultList* results_{nullptr};
};

} // namespace

struct LookupBox::CallbackState {
    LookupBox* owner{nullptr};
};

LookupBox::LookupBox(
    std::shared_ptr<const TextEngine> textEngine,
    std::vector<LookupItem> items,
    std::optional<std::size_t> selectedIndex,
    Size preferredSize,
    SelectionChangedHandler onSelectionChanged,
    LookupBoxStyle style,
    TextStyle textStyle)
    : textEngine_(std::move(textEngine)),
      items_(std::move(items)),
      preferredSize_(preferredSize),
      onSelectionChanged_(std::move(onSelectionChanged)),
      style_(style),
      textStyle_(std::move(textStyle)),
      callbackState_(std::make_shared<CallbackState>()) {
    if (!textEngine_) {
        throw std::invalid_argument("LookupBox requires a TextEngine");
    }
    if (!isValidTextStyle(textStyle_)) {
        throw std::invalid_argument("LookupBox text style is invalid");
    }
    sanitize(style_);
    callbackState_->owner = this;
    setSelectedIndex(selectedIndex);
}

Size LookupBox::measure(const LayoutConstraints& constraints) const {
    return constraints.constrain({
        dimension(preferredSize_.width),
        dimension(preferredSize_.height),
    });
}

void LookupBox::setPopupHost(PopupHost* host) noexcept {
    popupHost_ = host;
    popupId_ = invalidPopupId;
}

PopupHost* LookupBox::popupHost() const noexcept {
    return popupHost_;
}

bool LookupBox::openPopup() {
    if (!enabled_ || !popupHost_ || items_.empty()) {
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
        items_,
        selectedIndex_,
        [state](std::optional<std::size_t> index) {
            if (index) {
                if (const auto locked = state.lock()) {
                    locked->owner->select(*index, true);
                }
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

bool LookupBox::closePopup() {
    if (!isPopupOpen()) {
        popupId_ = invalidPopupId;
        return false;
    }
    const PopupId id = popupId_;
    popupId_ = invalidPopupId;
    return popupHost_->dismissPopup(id);
}

bool LookupBox::isPopupOpen() const noexcept {
    return popupHost_ && popupId_ != invalidPopupId &&
        popupHost_->popupId() == popupId_;
}

void LookupBox::setItems(std::vector<LookupItem> items) {
    items_ = std::move(items);
    if (selectedIndex_ && *selectedIndex_ >= items_.size()) {
        selectedIndex_.reset();
    }
    closePopup();
    invalidateLayout();
}

const std::vector<LookupItem>& LookupBox::items() const noexcept {
    return items_;
}

void LookupBox::setSelectedIndex(
    std::optional<std::size_t> index,
    bool notify) {
    if (index && *index >= items_.size()) {
        throw std::out_of_range("LookupBox selection is out of range");
    }
    if (selectedIndex_ == index) {
        return;
    }
    selectedIndex_ = index;
    invalidateLayout();
    if (notify && onSelectionChanged_) {
        SelectionChangedHandler callback = onSelectionChanged_;
        callback(selectedIndex_);
    }
}

std::optional<std::size_t> LookupBox::selectedIndex() const noexcept {
    return selectedIndex_;
}

const LookupItem* LookupBox::selectedItem() const noexcept {
    return selectedIndex_ ? &items_[*selectedIndex_] : nullptr;
}

void LookupBox::clearSelection(bool notify) {
    setSelectedIndex(std::nullopt, notify);
}

void LookupBox::setEnabled(bool enabled) noexcept {
    enabled_ = enabled;
    if (!enabled_) {
        hovered_ = false;
        pressed_ = false;
        focused_ = false;
        closePopup();
    }
}

bool LookupBox::isEnabled() const noexcept {
    return enabled_;
}

bool LookupBox::isFocused() const noexcept {
    return focused_;
}

void LookupBox::setPreferredSize(Size preferredSize) noexcept {
    preferredSize_ = preferredSize;
}

void LookupBox::setOnSelectionChanged(SelectionChangedHandler handler) {
    onSelectionChanged_ = std::move(handler);
}

void LookupBox::setStyle(LookupBoxStyle style) noexcept {
    sanitize(style);
    style_ = style;
}

const LookupBoxStyle& LookupBox::style() const noexcept {
    return style_;
}

PopupId LookupBox::showPopupAt(
    PopupHost& host,
    std::shared_ptr<const TextEngine> textEngine,
    Rect anchor,
    std::vector<LookupItem> items,
    std::optional<std::size_t> selectedIndex,
    SelectionChangedHandler onSelectionChanged,
    LookupBoxStyle style,
    TextStyle textStyle,
    PopupClosedHandler onClosed) {
    if (!textEngine) {
        throw std::invalid_argument("LookupBox popup requires a TextEngine");
    }
    if (selectedIndex && *selectedIndex >= items.size()) {
        throw std::out_of_range("LookupBox popup selection is out of range");
    }
    if (!isValidTextStyle(textStyle)) {
        throw std::invalid_argument("LookupBox popup text style is invalid");
    }
    sanitize(style);

    auto resultList = std::make_unique<LookupResultList>(
        textEngine,
        std::move(items),
        selectedIndex,
        [&host, callback = std::move(onSelectionChanged)](
            std::optional<std::size_t> index) mutable {
            host.acceptPopup();
            if (callback) {
                callback(index);
            }
        },
        style,
        textStyle);
    LookupResultList* results = resultList.get();

    TextFieldStyle searchStyle;
    searchStyle.normal = style.normal;
    searchStyle.hovered = style.hovered;
    searchStyle.text = style.text;
    searchStyle.focusRing = style.focusRing;
    searchStyle.contentPadding = {8.0F, 4.0F, 8.0F, 4.0F};
    searchStyle.cornerRadius = style.cornerRadius;
    searchStyle.focusRingWidth = 1.0F;
    auto search = std::make_unique<TextField>(
        textEngine,
        std::string{},
        Size{style.popupPreferredWidth, style.searchFieldHeight},
        [results](const std::string& query) {
            results->setFilter(query);
        },
        [results](const std::string&) {
            results->acceptHighlighted();
        },
        searchStyle,
        textStyle);

    auto popup = std::make_unique<LookupPopup>(*results);
    popup->setDecoration(BoxDecoration{
        style.popupBackground, style.popupCornerRadius});
    LinearLayoutOptions layoutOptions;
    layoutOptions.spacing = style.popupSpacing;
    layoutOptions.padding = style.popupPadding;
    layoutOptions.mainAxisSize = MainAxisSize::Min;
    layoutOptions.crossAxisAlignment = CrossAxisAlignment::Stretch;
    popup->setOptions(layoutOptions);
    popup->addChild(
        std::move(search),
        {0.0F,
            {0.0F, style.searchFieldHeight},
            {unboundedLayoutSize, style.searchFieldHeight}});
    popup->addChild(
        std::move(resultList),
        {0.0F,
            {0.0F, style.itemHeight},
            {unboundedLayoutSize,
                style.itemHeight *
                    static_cast<float>(style.maximumVisibleItems)}});

    PopupOptions popupOptions;
    popupOptions.matchAnchorWidth = true;
    return host.showPopup(
        std::move(popup), anchor, popupOptions, std::move(onClosed));
}

void LookupBox::onPaint(std::vector<PaintCommand>& commands) const {
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

    const float actionWidth = std::min(style_.actionAreaWidth, content.width);
    const float dot = std::max(1.0F, content.height * 0.07F);
    for (int index = 0; index < 3; ++index) {
        commands.push_back({
            {
                content.x + content.width - actionWidth *
                    (0.72F - 0.22F * static_cast<float>(index)),
                content.y + content.height * 0.5F - dot * 0.5F,
                dot,
                dot,
            },
            clip(), enabled_ ? style_.action : style_.disabledText,
            invalidTextureId, dot * 0.5F,
        });
    }

    const float textWidth = std::max(
        0.0F, content.width - style_.contentPadding.horizontal() -
            actionWidth);
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

bool LookupBox::acceptsPointerEvents() const noexcept {
    return enabled_;
}

bool LookupBox::onPointerEvent(const WidgetPointerEvent& event) {
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

bool LookupBox::acceptsFocus() const noexcept {
    return enabled_;
}

bool LookupBox::onFocusChanged(bool focused) {
    const bool changed = focused_ != focused;
    focused_ = focused;
    return changed;
}

bool LookupBox::onKeyEvent(const WidgetKeyEvent& event) {
    const bool openKey = event.key == KeyCode::Enter ||
        event.key == KeyCode::Space;
    const bool clearKey = event.key == KeyCode::Delete ||
        event.key == KeyCode::Backspace;
    if (!enabled_ || (!openKey && !clearKey)) {
        return false;
    }
    if (event.type != WidgetKeyEventType::Press) {
        return true;
    }
    if (clearKey) {
        clearSelection(true);
    } else {
        openPopup();
    }
    return true;
}

void LookupBox::select(std::size_t index, bool notify) {
    setSelectedIndex(index, notify);
}

void LookupBox::invalidateLayout() const noexcept {
    layout_.reset();
    layoutWidth_ = -1.0F;
}

const TextLayout* LookupBox::ensureLayout(float maximumWidth) const {
    const LookupItem* item = selectedItem();
    if (!item) {
        return nullptr;
    }
    const float width = std::max(0.0F, maximumWidth);
    if (!layout_ || layoutWidth_ != width) {
        TextLayoutOptions options;
        options.maximumWidth = width;
        options.maximumLines = 1;
        layout_ = textEngine_->createLayout(item->label, textStyle_, options);
        layoutWidth_ = width;
        if (!layout_) {
            throw std::runtime_error(
                "LookupBox TextEngine returned a null layout");
        }
    }
    return layout_.get();
}

Color LookupBox::currentColor() const noexcept {
    if (!enabled_) {
        return style_.disabled;
    }
    if (pressed_) {
        return style_.pressed;
    }
    return hovered_ || isPopupOpen() ? style_.hovered : style_.normal;
}

} // namespace lotui
