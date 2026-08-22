#include "widgets/ribbon.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace lotui {
namespace {

float dimension(float value) noexcept {
    return std::isfinite(value) ? std::max(0.0F, value) : 0.0F;
}

void sanitize(RibbonGroupStyle& style) noexcept {
    style.contentPadding.left = dimension(style.contentPadding.left);
    style.contentPadding.top = dimension(style.contentPadding.top);
    style.contentPadding.right = dimension(style.contentPadding.right);
    style.contentPadding.bottom = dimension(style.contentPadding.bottom);
    style.titleHeight = dimension(style.titleHeight);
    style.separatorWidth = dimension(style.separatorWidth);
    style.cornerRadius = dimension(style.cornerRadius);
}

void sanitize(RibbonStyle& style) noexcept {
    style.contentPadding.left = dimension(style.contentPadding.left);
    style.contentPadding.top = dimension(style.contentPadding.top);
    style.contentPadding.right = dimension(style.contentPadding.right);
    style.contentPadding.bottom = dimension(style.contentPadding.bottom);
    style.tabBarHeight = dimension(style.tabBarHeight);
    style.tabHorizontalPadding = dimension(style.tabHorizontalPadding);
    style.tabSpacing = dimension(style.tabSpacing);
    style.minimumTabWidth = dimension(style.minimumTabWidth);
    style.tabCornerRadius = dimension(style.tabCornerRadius);
    style.focusRingWidth = dimension(style.focusRingWidth);
    style.preferredHeight = dimension(style.preferredHeight);
}

LayoutConstraints contentConstraints(
    const LayoutConstraints& constraints,
    float horizontal,
    float vertical) noexcept {
    const auto reduce = [](float value, float amount) {
        return std::isfinite(value)
            ? std::max(0.0F, value - amount)
            : unboundedLayoutSize;
    };
    return {
        {},
        {
            reduce(constraints.maximum.width, horizontal),
            reduce(constraints.maximum.height, vertical),
        },
    };
}

} // namespace

RibbonGroup::RibbonGroup(
    std::shared_ptr<const TextEngine> textEngine,
    std::string title,
    std::unique_ptr<Widget> content,
    RibbonGroupStyle style,
    TextStyle titleStyle)
    : SingleChildWidget(std::move(content)),
      textEngine_(std::move(textEngine)),
      title_(std::move(title)),
      style_(style),
      titleStyle_(std::move(titleStyle)) {
    if (!textEngine_) {
        throw std::invalid_argument("RibbonGroup requires a TextEngine");
    }
    if (!isValidTextStyle(titleStyle_)) {
        throw std::invalid_argument("RibbonGroup title style is invalid");
    }
    sanitize(style_);
}

Size RibbonGroup::measure(const LayoutConstraints& constraints) const {
    const float titleHeight = std::max(
        style_.titleHeight, titleLayout().size().height);
    Size contentSize{};
    if (child()) {
        contentSize = child()->measure(contentConstraints(
            constraints,
            style_.contentPadding.horizontal(),
            style_.contentPadding.vertical() + titleHeight));
    }
    return constraints.constrain({
        std::max(
            titleLayout().size().width + style_.contentPadding.horizontal(),
            contentSize.width + style_.contentPadding.horizontal()),
        contentSize.height + style_.contentPadding.vertical() + titleHeight,
    });
}

const std::string& RibbonGroup::title() const noexcept {
    return title_;
}

void RibbonGroup::setContent(std::unique_ptr<Widget> content) {
    setChild(std::move(content));
}

Widget* RibbonGroup::content() noexcept {
    return child();
}

const Widget* RibbonGroup::content() const noexcept {
    return child();
}

void RibbonGroup::setStyle(RibbonGroupStyle style) noexcept {
    sanitize(style);
    style_ = style;
}

const RibbonGroupStyle& RibbonGroup::style() const noexcept {
    return style_;
}

void RibbonGroup::onArrange() {
    if (!child()) {
        return;
    }
    const float titleHeight = std::max(
        style_.titleHeight, titleLayout().size().height);
    child()->arrange({
        bounds().x + style_.contentPadding.left,
        bounds().y + style_.contentPadding.top,
        std::max(0.0F,
            bounds().width - style_.contentPadding.horizontal()),
        std::max(0.0F,
            bounds().height - style_.contentPadding.vertical() -
                titleHeight),
    }, clip());
}

void RibbonGroup::onPaint(
    std::vector<PaintCommand>& commands) const {
    commands.push_back({
        bounds(), clip(), style_.background, invalidTextureId,
        style_.cornerRadius});
    if (style_.separatorWidth > 0.0F) {
        commands.push_back({
            {
                bounds().x + bounds().width - style_.separatorWidth,
                bounds().y,
                style_.separatorWidth,
                bounds().height,
            },
            clip(), style_.separator, invalidTextureId, 0.0F,
        });
    }
    const TextLayout& layout = titleLayout();
    layout.appendPaintCommands({
        bounds().x + std::max(
            0.0F, (bounds().width - layout.size().width) * 0.5F),
        bounds().y + bounds().height - std::max(
            style_.titleHeight, layout.size().height) +
            std::max(0.0F,
                (style_.titleHeight - layout.size().height) * 0.5F),
    }, clip(), style_.titleColor, commands);
}

const TextLayout& RibbonGroup::titleLayout() const {
    if (!titleLayout_) {
        titleLayout_ = textEngine_->createLayout(
            title_, titleStyle_, {});
        if (!titleLayout_) {
            throw std::runtime_error("TextEngine returned a null layout");
        }
    }
    return *titleLayout_;
}

RibbonTab::RibbonTab(
    std::string id,
    std::string title,
    std::unique_ptr<Widget> content)
    : SingleChildWidget(std::move(content)),
      id_(std::move(id)),
      title_(std::move(title)) {
    if (id_.empty()) {
        throw std::invalid_argument("RibbonTab id must not be empty");
    }
    if (title_.empty()) {
        throw std::invalid_argument("RibbonTab title must not be empty");
    }
}

Size RibbonTab::measure(const LayoutConstraints& constraints) const {
    return child()
        ? child()->measure(constraints)
        : constraints.constrain({});
}

const std::string& RibbonTab::id() const noexcept {
    return id_;
}

const std::string& RibbonTab::title() const noexcept {
    return title_;
}

void RibbonTab::setContent(std::unique_ptr<Widget> content) {
    setChild(std::move(content));
}

Widget* RibbonTab::content() noexcept {
    return child();
}

const Widget* RibbonTab::content() const noexcept {
    return child();
}

void RibbonTab::onArrange() {
    if (child()) {
        child()->arrange(bounds(), clip());
    }
}

Ribbon::Ribbon(
    std::shared_ptr<const TextEngine> textEngine,
    RibbonStyle style,
    TextStyle tabTextStyle,
    TabChangedHandler onTabChanged)
    : textEngine_(std::move(textEngine)),
      style_(style),
      tabTextStyle_(std::move(tabTextStyle)),
      onTabChanged_(std::move(onTabChanged)) {
    if (!textEngine_) {
        throw std::invalid_argument("Ribbon requires a TextEngine");
    }
    if (!isValidTextStyle(tabTextStyle_)) {
        throw std::invalid_argument("Ribbon tab text style is invalid");
    }
    sanitize(style_);
}

Size Ribbon::measure(const LayoutConstraints& constraints) const {
    float headersWidth = style_.contentPadding.horizontal();
    for (std::size_t index = 0; index < tabs_.size(); ++index) {
        headersWidth += tabWidth(index);
        if (index > 0) {
            headersWidth += style_.tabSpacing;
        }
    }

    Size contentSize{};
    if (child()) {
        contentSize = child()->measure(contentConstraints(
            constraints,
            style_.contentPadding.horizontal(),
            style_.tabBarHeight + style_.contentPadding.vertical()));
    }
    return constraints.constrain({
        std::max(headersWidth,
            contentSize.width + style_.contentPadding.horizontal()),
        std::max(
            style_.preferredHeight,
            style_.tabBarHeight + style_.contentPadding.vertical() +
                contentSize.height),
    });
}

RibbonTab& Ribbon::addTab(std::unique_ptr<RibbonTab> tab) {
    std::unique_ptr<Widget> widget = std::move(tab);
    return addTab(std::move(widget));
}

RibbonTab& Ribbon::addTab(std::unique_ptr<Widget> widget) {
    auto* tab = dynamic_cast<RibbonTab*>(widget.get());
    if (!tab) {
        throw std::invalid_argument("Ribbon child must be a RibbonTab");
    }
    for (std::size_t index = 0; index < tabs_.size(); ++index) {
        if (tabAt(index).id() == tab->id()) {
            throw std::invalid_argument(
                "Ribbon tab id must be unique: " + tab->id());
        }
    }
    RibbonTab& result = *tab;
    tabs_.push_back({std::move(widget), {}, {}});
    if (selectedIndex_ == noTab) {
        selectedIndex_ = 0;
        setChild(std::move(tabs_.front().widget));
    }
    return result;
}

std::size_t Ribbon::tabCount() const noexcept {
    return tabs_.size();
}

RibbonTab& Ribbon::tabAt(std::size_t index) {
    RibbonTab* tab = tabPointer(index);
    if (!tab) {
        throw std::out_of_range("Ribbon tab index is out of range");
    }
    return *tab;
}

const RibbonTab& Ribbon::tabAt(std::size_t index) const {
    const RibbonTab* tab = tabPointer(index);
    if (!tab) {
        throw std::out_of_range("Ribbon tab index is out of range");
    }
    return *tab;
}

std::size_t Ribbon::selectedIndex() const noexcept {
    return selectedIndex_;
}

std::string_view Ribbon::selectedId() const noexcept {
    const RibbonTab* selected = tabPointer(selectedIndex_);
    return selected ? std::string_view(selected->id()) : std::string_view{};
}

bool Ribbon::selectTab(std::size_t index) {
    if (index >= tabs_.size() || index == selectedIndex_) {
        return false;
    }
    if (selectedIndex_ != noTab) {
        tabs_[selectedIndex_].widget = takeChild();
    }
    selectedIndex_ = index;
    setChild(std::move(tabs_[selectedIndex_].widget));
    arrangeActiveContent();

    if (onTabChanged_) {
        TabChangedHandler callback = onTabChanged_;
        const std::string id(tabAt(index).id());
        callback(index, id);
    }
    return true;
}

bool Ribbon::selectTab(std::string_view id) {
    for (std::size_t index = 0; index < tabs_.size(); ++index) {
        if (tabAt(index).id() == id) {
            return selectTab(index);
        }
    }
    return false;
}

Rect Ribbon::tabHeaderBounds(std::size_t index) const {
    return tabs_.at(index).headerBounds;
}

void Ribbon::setEnabled(bool enabled) noexcept {
    enabled_ = enabled;
    if (!enabled_) {
        hoveredIndex_ = noTab;
        pressedIndex_ = noTab;
        focused_ = false;
    }
}

bool Ribbon::isEnabled() const noexcept {
    return enabled_;
}

bool Ribbon::isFocused() const noexcept {
    return focused_;
}

void Ribbon::setStyle(RibbonStyle style) noexcept {
    sanitize(style);
    style_ = style;
    invalidateTitleLayouts();
}

const RibbonStyle& Ribbon::style() const noexcept {
    return style_;
}

void Ribbon::setOnTabChanged(TabChangedHandler onTabChanged) {
    onTabChanged_ = std::move(onTabChanged);
}

void Ribbon::onArrange() {
    float x = bounds().x + style_.contentPadding.left;
    for (std::size_t index = 0; index < tabs_.size(); ++index) {
        if (index > 0) {
            x += style_.tabSpacing;
        }
        const float width = tabWidth(index);
        tabs_[index].headerBounds = {
            x, bounds().y, width,
            std::min(style_.tabBarHeight, bounds().height),
        };
        x += width;
    }
    contentBounds_ = {
        bounds().x + style_.contentPadding.left,
        bounds().y + style_.tabBarHeight + style_.contentPadding.top,
        std::max(0.0F,
            bounds().width - style_.contentPadding.horizontal()),
        std::max(0.0F,
            bounds().height - style_.tabBarHeight -
                style_.contentPadding.vertical()),
    };
    arrangeActiveContent();
}

void Ribbon::onPaint(std::vector<PaintCommand>& commands) const {
    commands.push_back({
        bounds(), clip(), style_.background, invalidTextureId, 0.0F});
    commands.push_back({
        {
            bounds().x,
            bounds().y,
            bounds().width,
            std::min(style_.tabBarHeight, bounds().height),
        },
        clip(), style_.tabBar, invalidTextureId, 0.0F,
    });

    for (std::size_t index = 0; index < tabs_.size(); ++index) {
        Rect header = tabs_[index].headerBounds;
        const bool selected = index == selectedIndex_;
        const bool ring = focused_ && selected && style_.focusRingWidth > 0.0F;
        if (ring) {
            commands.push_back({
                header, clip(), style_.focusRing, invalidTextureId,
                style_.tabCornerRadius});
            const float width = std::min(
                style_.focusRingWidth,
                std::min(header.width, header.height) * 0.5F);
            header = {
                header.x + width,
                header.y + width,
                std::max(0.0F, header.width - width * 2.0F),
                std::max(0.0F, header.height - width * 2.0F),
            };
        }
        const Color tabColor = selected
            ? style_.tabSelected
            : index == pressedIndex_
                ? style_.tabPressed
                : index == hoveredIndex_
                    ? style_.tabHovered
                    : style_.tabNormal;
        commands.push_back({
            header, clip(), tabColor, invalidTextureId,
            std::max(0.0F,
                style_.tabCornerRadius -
                    (ring ? style_.focusRingWidth : 0.0F)),
        });

        const TextLayout& layout = titleLayout(index);
        layout.appendPaintCommands({
            tabs_[index].headerBounds.x + std::max(
                0.0F,
                (tabs_[index].headerBounds.width - layout.size().width) *
                    0.5F),
            tabs_[index].headerBounds.y + std::max(
                0.0F,
                (tabs_[index].headerBounds.height - layout.size().height) *
                    0.5F),
        }, clip(), selected ? style_.selectedText : style_.text, commands);
    }
}

bool Ribbon::acceptsPointerEvents() const noexcept {
    return enabled_ && !tabs_.empty();
}

bool Ribbon::onPointerEvent(const WidgetPointerEvent& event) {
    if (!enabled_) {
        return false;
    }
    switch (event.type) {
    case WidgetPointerEventType::Enter:
    case WidgetPointerEventType::Move: {
        const std::size_t hovered = tabAtPoint(event.position);
        const bool changed = hoveredIndex_ != hovered;
        hoveredIndex_ = hovered;
        return changed;
    }
    case WidgetPointerEventType::Leave: {
        const bool changed = hoveredIndex_ != noTab;
        hoveredIndex_ = noTab;
        return changed;
    }
    case WidgetPointerEventType::Press:
        if (event.button != PointerButton::Primary) {
            return false;
        }
        pressedIndex_ = tabAtPoint(event.position);
        if (pressedIndex_ != noTab) {
            selectTab(pressedIndex_);
        }
        return true;
    case WidgetPointerEventType::Release:
        if (event.button != PointerButton::Primary) {
            return false;
        }
        pressedIndex_ = noTab;
        return true;
    case WidgetPointerEventType::Cancel:
        pressedIndex_ = noTab;
        return true;
    }
    return false;
}

bool Ribbon::acceptsFocus() const noexcept {
    return enabled_ && !tabs_.empty();
}

bool Ribbon::onFocusChanged(bool focused) {
    const bool changed = focused_ != focused;
    focused_ = focused;
    if (!focused_) {
        pressedIndex_ = noTab;
    }
    return changed;
}

bool Ribbon::onKeyEvent(const WidgetKeyEvent& event) {
    if (!enabled_ || tabs_.empty()) {
        return false;
    }
    const bool navigationKey = event.key == KeyCode::Left ||
        event.key == KeyCode::Right || event.key == KeyCode::Home ||
        event.key == KeyCode::End;
    if (!navigationKey) {
        return false;
    }
    if (event.type != WidgetKeyEventType::Press) {
        return true;
    }
    if (event.key == KeyCode::Home) {
        selectTab(0);
    } else if (event.key == KeyCode::End) {
        selectTab(tabs_.size() - 1);
    } else if (event.key == KeyCode::Left) {
        selectTab(selectedIndex_ == 0
            ? tabs_.size() - 1
            : selectedIndex_ - 1);
    } else {
        selectTab((selectedIndex_ + 1) % tabs_.size());
    }
    return true;
}

void Ribbon::collectChildHitTestEntries(
    std::vector<HitTestEntry>& entries) const {
    if (enabled_) {
        SingleChildWidget::collectChildHitTestEntries(entries);
    }
}

void Ribbon::collectChildFocusTargets(
    std::vector<PointerTargetId>& targets) const {
    if (enabled_) {
        SingleChildWidget::collectChildFocusTargets(targets);
    }
}

RibbonTab* Ribbon::tabPointer(std::size_t index) noexcept {
    if (index >= tabs_.size()) {
        return nullptr;
    }
    Widget* widget = index == selectedIndex_
        ? child()
        : tabs_[index].widget.get();
    return dynamic_cast<RibbonTab*>(widget);
}

const RibbonTab* Ribbon::tabPointer(std::size_t index) const noexcept {
    if (index >= tabs_.size()) {
        return nullptr;
    }
    const Widget* widget = index == selectedIndex_
        ? child()
        : tabs_[index].widget.get();
    return dynamic_cast<const RibbonTab*>(widget);
}

const TextLayout& Ribbon::titleLayout(std::size_t index) const {
    const TabSlot& slot = tabs_.at(index);
    if (!slot.titleLayout) {
        slot.titleLayout = textEngine_->createLayout(
            tabAt(index).title(), tabTextStyle_, {});
        if (!slot.titleLayout) {
            throw std::runtime_error("TextEngine returned a null layout");
        }
    }
    return *slot.titleLayout;
}

float Ribbon::tabWidth(std::size_t index) const {
    return std::max(
        style_.minimumTabWidth,
        titleLayout(index).size().width +
            style_.tabHorizontalPadding * 2.0F);
}

std::size_t Ribbon::tabAtPoint(Point point) const noexcept {
    for (std::size_t index = 0; index < tabs_.size(); ++index) {
        if (contains(tabs_[index].headerBounds, point)) {
            return index;
        }
    }
    return noTab;
}

void Ribbon::arrangeActiveContent() {
    if (child()) {
        child()->arrange(contentBounds_, clip());
    }
}

void Ribbon::invalidateTitleLayouts() noexcept {
    for (TabSlot& slot : tabs_) {
        slot.titleLayout.reset();
    }
}

} // namespace lotui
