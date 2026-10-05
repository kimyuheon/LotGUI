#include "widgets/menu_bar.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace lotui {
namespace {

constexpr std::size_t noIndex = static_cast<std::size_t>(-1);

float dimension(float value, float fallback) noexcept {
    return std::isfinite(value) && value > 0.0F ? value : fallback;
}

float rowHeight(const MenuEntry& entry, const MenuBarStyle& style) noexcept {
    return entry.kind == MenuEntryKind::Separator
        ? style.separatorHeight : style.rowHeight;
}

bool selectable(const MenuEntry& entry) noexcept {
    return entry.enabled && entry.kind != MenuEntryKind::Separator;
}

std::size_t firstSelectable(const std::vector<MenuEntry>& entries) noexcept {
    for (std::size_t i = 0; i < entries.size(); ++i) {
        if (selectable(entries[i])) return i;
    }
    return noIndex;
}

void validateEntries(const std::vector<MenuEntry>& entries,
    std::unordered_set<std::string>& ids) {
    for (const MenuEntry& entry : entries) {
        if (entry.kind == MenuEntryKind::Separator) continue;
        if (entry.label.empty()) {
            throw std::invalid_argument("menu entry label must not be empty");
        }
        if (entry.kind == MenuEntryKind::Submenu) {
            if (entry.children.empty()) {
                throw std::invalid_argument("submenu must contain entries");
            }
            validateEntries(entry.children, ids);
        } else {
            if (entry.id.empty() || !entry.children.empty() ||
                !ids.insert(entry.id).second) {
                throw std::invalid_argument("menu command ids must be unique");
            }
        }
    }
}

std::size_t depthOf(const std::vector<MenuEntry>& entries) noexcept {
    std::size_t depth = 1;
    for (const MenuEntry& entry : entries) {
        if (entry.kind == MenuEntryKind::Submenu) {
            depth = std::max(depth, 1 + depthOf(entry.children));
        }
    }
    return depth;
}

float maximumPanelHeight(const std::vector<MenuEntry>& entries,
    const MenuBarStyle& style) noexcept {
    float height = style.popupPadding * 2.0F;
    for (const MenuEntry& entry : entries) {
        height += rowHeight(entry, style);
    }
    for (const MenuEntry& entry : entries) {
        if (entry.kind == MenuEntryKind::Submenu) {
            height = std::max(height, maximumPanelHeight(entry.children, style));
        }
    }
    return height;
}

class MenuPopup final : public Widget {
public:
    MenuPopup(PopupHost& host,
        std::shared_ptr<const TextEngine> textEngine,
        std::vector<MenuEntry> entries,
        MenuBar::ActionHandler onAction,
        MenuBarStyle style,
        TextStyle textStyle)
        : host_(host), textEngine_(std::move(textEngine)),
          entries_(std::move(entries)), onAction_(std::move(onAction)),
          style_(style), textStyle_(std::move(textStyle)),
          maximumDepth_(depthOf(entries_)),
          firstVisible_(maximumDepth_, 0) {
        const std::size_t first = firstSelectable(entries_);
        if (first != noIndex) path_.push_back(first);
    }

    Size measure(const LayoutConstraints& constraints) const override {
        return constraints.constrain({
            style_.popupWidth * static_cast<float>(maximumDepth_),
            maximumPanelHeight(entries_, style_)});
    }

protected:
    void onPaint(std::vector<PaintCommand>& commands) const override {
        const std::vector<MenuEntry>* entries = &entries_;
        for (std::size_t depth = 0; depth < maximumDepth_; ++depth) {
            if (depth > 0) {
                if (depth - 1 >= path_.size() ||
                    path_[depth - 1] >= entries->size()) break;
                const MenuEntry& parent = (*entries)[path_[depth - 1]];
                if (parent.kind != MenuEntryKind::Submenu) break;
                entries = &parent.children;
            }
            const Rect panel = panelBounds(depth, *entries);
            if (!hasArea(panel)) continue;
            const Rect panelClip = intersect(panel, clip());
            commands.push_back({panel, panelClip,
                style_.popupBackground, invalidTextureId, 3.0F});
            for (std::size_t i = firstVisible_[depth];
                 i < entries->size(); ++i) {
                const Rect row = rowBounds(depth, *entries, i);
                if (row.y >= panel.y + panel.height) break;
                const Rect rowClip = intersect(panelClip, row);
                if (!hasArea(rowClip)) continue;
                const MenuEntry& entry = (*entries)[i];
                if (entry.kind == MenuEntryKind::Separator) {
                    commands.push_back({
                        {row.x + style_.horizontalPadding,
                         row.y + row.height * 0.5F,
                         std::max(0.0F, row.width -
                             style_.horizontalPadding * 2.0F), 1.0F},
                        rowClip, style_.separator, invalidTextureId, 0.0F});
                    continue;
                }
                if (depth < path_.size() && path_[depth] == i) {
                    commands.push_back({row, rowClip,
                        focused_ ? style_.selected : style_.hovered,
                        invalidTextureId, 2.0F});
                }
                const Color color = entry.enabled ? style_.text : style_.muted;
                float textX = row.x + style_.horizontalPadding;
                if (entry.kind == MenuEntryKind::Toggle && entry.checked) {
                    paintText("*", {textX, row.y}, rowClip,
                        style_.text, commands);
                }
                textX += 16.0F;
                const float trailing = entry.kind == MenuEntryKind::Submenu
                    ? 14.0F : textWidth(entry.shortcut) + 8.0F;
                const Rect labelClip = intersect(rowClip,
                    {textX, row.y,
                     std::max(0.0F, row.x + row.width -
                         style_.horizontalPadding - trailing - textX),
                     row.height});
                paintText(entry.label, {textX, row.y}, labelClip,
                    color, commands);
                if (entry.kind == MenuEntryKind::Submenu) {
                    paintText(">", {row.x + row.width -
                        style_.horizontalPadding - textWidth(">"), row.y},
                        rowClip, color, commands);
                } else if (!entry.shortcut.empty()) {
                    paintText(entry.shortcut,
                        {row.x + row.width - style_.horizontalPadding -
                            textWidth(entry.shortcut), row.y},
                        rowClip, style_.muted, commands);
                }
            }
        }
    }

    bool acceptsPointerEvents() const noexcept override { return true; }

    bool onPointerEvent(const WidgetPointerEvent& event) override {
        switch (event.type) {
        case WidgetPointerEventType::Enter:
        case WidgetPointerEventType::Move:
            return highlightAt(event.position);
        case WidgetPointerEventType::Leave:
            return false;
        case WidgetPointerEventType::Press:
            if (event.button != PointerButton::Primary) return false;
            highlightAt(event.position);
            pressed_ = itemAt(event.position);
            return true;
        case WidgetPointerEventType::Release: {
            if (event.button != PointerButton::Primary) return false;
            const auto released = itemAt(event.position);
            const bool matched = pressed_ && released == pressed_;
            const auto selected = pressed_;
            pressed_.reset();
            if (matched && selected) activate(selected->first,
                selected->second);
            return true;
        }
        case WidgetPointerEventType::Cancel:
            pressed_.reset();
            return true;
        }
        return false;
    }

    bool acceptsFocus() const noexcept override { return true; }
    bool onFocusChanged(bool focused) override {
        focused_ = focused;
        return true;
    }

    bool onKeyEvent(const WidgetKeyEvent& event) override {
        const bool navigation = event.key == KeyCode::Up ||
            event.key == KeyCode::Down || event.key == KeyCode::Home ||
            event.key == KeyCode::End || event.key == KeyCode::Left ||
            event.key == KeyCode::Right || event.key == KeyCode::Enter ||
            event.key == KeyCode::Space;
        if (!navigation) return false;
        if (event.type != WidgetKeyEventType::Press || path_.empty()) {
            return true;
        }
        const std::size_t depth = path_.size() - 1;
        const auto* entries = entriesAt(depth);
        if (!entries || path_[depth] >= entries->size()) return true;
        if (event.key == KeyCode::Left) {
            if (depth > 0) path_.pop_back();
            return true;
        }
        if (event.key == KeyCode::Right || event.key == KeyCode::Enter ||
            event.key == KeyCode::Space) {
            activate(depth, path_[depth]);
            return true;
        }
        std::size_t next = path_[depth];
        for (std::size_t count = 0; count < entries->size(); ++count) {
            if (event.key == KeyCode::Home) next = count;
            else if (event.key == KeyCode::End)
                next = entries->size() - 1 - count;
            else if (event.key == KeyCode::Down)
                next = (next + 1) % entries->size();
            else next = (next + entries->size() - 1) % entries->size();
            if (selectable((*entries)[next])) {
                path_[depth] = next;
                ensureVisible(depth, *entries, next);
                break;
            }
        }
        return true;
    }

    bool onScrollEvent(const WidgetScrollEvent& event) override {
        const std::size_t depth = panelAt(event.position);
        const auto* entries = entriesAt(depth);
        if (!entries || entries->empty() ||
            !std::isfinite(event.delta.y) || event.delta.y == 0.0F)
            return false;
        const std::size_t old = firstVisible_[depth];
        const std::size_t steps = std::max<std::size_t>(1,
            static_cast<std::size_t>(std::ceil(std::abs(event.delta.y) /
                (event.mode == ScrollDeltaMode::Line
                    ? 1.0F : style_.rowHeight))));
        if (event.delta.y > 0.0F) {
            firstVisible_[depth] = std::min(entries->size() - 1,
                firstVisible_[depth] + steps);
        } else {
            firstVisible_[depth] = steps >= firstVisible_[depth]
                ? 0 : firstVisible_[depth] - steps;
        }
        return old != firstVisible_[depth];
    }

private:
    using Location = std::pair<std::size_t, std::size_t>;

    const TextLayout& layout(std::string_view text) const {
        auto found = layouts_.find(std::string(text));
        if (found == layouts_.end()) {
            auto created = textEngine_->createLayout(text, textStyle_, {});
            if (!created) throw std::runtime_error("TextEngine returned null layout");
            found = layouts_.emplace(std::string(text),
                std::move(created)).first;
        }
        return *found->second;
    }

    float textWidth(std::string_view text) const {
        return text.empty() ? 0.0F : layout(text).size().width;
    }

    void paintText(std::string_view text, Point origin, Rect clip,
        Color color, std::vector<PaintCommand>& commands) const {
        if (text.empty() || !hasArea(clip)) return;
        const TextLayout& textLayout = layout(text);
        origin.y += std::max(0.0F,
            (style_.rowHeight - textLayout.size().height) * 0.5F);
        textLayout.appendPaintCommands(origin, clip, color, commands);
    }

    Rect panelBounds(std::size_t depth,
        const std::vector<MenuEntry>& entries) const noexcept {
        const float width = bounds().width /
            static_cast<float>(maximumDepth_);
        float height = style_.popupPadding * 2.0F;
        for (const MenuEntry& entry : entries) {
            height += rowHeight(entry, style_);
        }
        return {bounds().x + width * static_cast<float>(depth),
            bounds().y, width, std::min(bounds().height, height)};
    }

    Rect rowBounds(std::size_t depth,
        const std::vector<MenuEntry>& entries,
        std::size_t index) const noexcept {
        const Rect panel = panelBounds(depth, entries);
        float y = panel.y + style_.popupPadding;
        for (std::size_t i = firstVisible_[depth]; i < index; ++i) {
            y += rowHeight(entries[i], style_);
        }
        return {panel.x + style_.popupPadding, y,
            std::max(0.0F, panel.width - style_.popupPadding * 2.0F),
            rowHeight(entries[index], style_)};
    }

    const std::vector<MenuEntry>* entriesAt(std::size_t depth) const noexcept {
        const auto* entries = &entries_;
        for (std::size_t i = 0; i < depth; ++i) {
            if (i >= path_.size() || path_[i] >= entries->size())
                return nullptr;
            const MenuEntry& parent = (*entries)[path_[i]];
            if (parent.kind != MenuEntryKind::Submenu) return nullptr;
            entries = &parent.children;
        }
        return entries;
    }

    std::size_t panelAt(Point point) const noexcept {
        for (std::size_t depth = 0; depth < maximumDepth_; ++depth) {
            const auto* entries = entriesAt(depth);
            if (!entries || (depth > 0 && depth >= path_.size())) break;
            if (contains(panelBounds(depth, *entries), point)) return depth;
        }
        return noIndex;
    }

    std::optional<Location> itemAt(Point point) const noexcept {
        const std::size_t depth = panelAt(point);
        const auto* entries = entriesAt(depth);
        if (!entries) return std::nullopt;
        for (std::size_t i = firstVisible_[depth]; i < entries->size(); ++i) {
            const Rect row = rowBounds(depth, *entries, i);
            if (row.y >= bounds().y + bounds().height) break;
            if (contains(row, point) && selectable((*entries)[i])) {
                return Location{depth, i};
            }
        }
        return std::nullopt;
    }

    bool highlightAt(Point point) {
        const auto item = itemAt(point);
        if (!item) return false;
        const auto [depth, index] = *item;
        const bool changed = depth >= path_.size() ||
            path_[depth] != index || path_.size() > depth + 1;
        path_.resize(depth + 1);
        path_[depth] = index;
        const auto* entries = entriesAt(depth);
        if ((*entries)[index].kind == MenuEntryKind::Submenu) {
            const std::size_t first = firstSelectable((*entries)[index].children);
            if (first != noIndex) path_.push_back(first);
        }
        return changed;
    }

    void ensureVisible(std::size_t depth,
        const std::vector<MenuEntry>& entries, std::size_t index) {
        if (index < firstVisible_[depth]) firstVisible_[depth] = index;
        const Rect panel = panelBounds(depth, entries);
        while (index > firstVisible_[depth] &&
            rowBounds(depth, entries, index).y +
                rowHeight(entries[index], style_) >
                panel.y + panel.height - style_.popupPadding) {
            ++firstVisible_[depth];
        }
    }

    void activate(std::size_t depth, std::size_t index) {
        const auto* entries = entriesAt(depth);
        if (!entries || index >= entries->size() ||
            !selectable((*entries)[index])) return;
        const MenuEntry& entry = (*entries)[index];
        if (entry.kind == MenuEntryKind::Submenu) {
            path_.resize(depth + 1);
            path_[depth] = index;
            const std::size_t first = firstSelectable(entry.children);
            if (first != noIndex) path_.push_back(first);
            return;
        }
        const std::string id = entry.id;
        host_.acceptPopup();
        if (onAction_) onAction_(id);
    }

    PopupHost& host_;
    std::shared_ptr<const TextEngine> textEngine_;
    std::vector<MenuEntry> entries_;
    MenuBar::ActionHandler onAction_{};
    MenuBarStyle style_{};
    TextStyle textStyle_{};
    std::size_t maximumDepth_{1};
    std::vector<std::size_t> firstVisible_;
    std::vector<std::size_t> path_;
    std::optional<Location> pressed_;
    mutable std::unordered_map<std::string,
        std::unique_ptr<TextLayout>> layouts_;
    bool focused_{false};
};

} // namespace

MenuBar::MenuBar(std::shared_ptr<const TextEngine> textEngine,
    std::vector<Menu> menus, ActionHandler onAction,
    MenuBarStyle style, TextStyle textStyle)
    : textEngine_(std::move(textEngine)),
      onAction_(std::move(onAction)), style_(style),
      textStyle_(std::move(textStyle)) {
    if (!textEngine_ || !isValidTextStyle(textStyle_)) {
        throw std::invalid_argument("MenuBar requires valid text settings");
    }
    style_.barHeight = dimension(style_.barHeight, 28.0F);
    style_.rowHeight = dimension(style_.rowHeight, 27.0F);
    style_.separatorHeight = dimension(style_.separatorHeight, 9.0F);
    style_.popupWidth = dimension(style_.popupWidth, 230.0F);
    style_.horizontalPadding = dimension(style_.horizontalPadding, 11.0F);
    style_.popupPadding = dimension(style_.popupPadding, 4.0F);
    setMenus(std::move(menus));
}

Size MenuBar::measure(const LayoutConstraints& constraints) const {
    float width = 0.0F;
    for (std::size_t i = 0; i < menus_.size(); ++i) {
        width += labelLayout(i).size().width +
            style_.horizontalPadding * 2.0F;
    }
    return constraints.constrain({width, style_.barHeight});
}

void MenuBar::setPopupHost(PopupHost* host) noexcept {
    host_ = host;
}

void MenuBar::setMenus(std::vector<Menu> menus) {
    std::unordered_set<std::string> ids;
    for (const Menu& menu : menus) {
        if (menu.id.empty() || menu.label.empty() ||
            !ids.insert(menu.id).second) {
            throw std::invalid_argument("menu ids and labels must be unique");
        }
        validateEntries(menu.entries, ids);
    }
    if (isOpen()) host_->dismissPopup(popupId_);
    menus_ = std::move(menus);
    menuBounds_.resize(menus_.size());
    labels_.clear();
    labels_.resize(menus_.size());
    openIndex_ = hoveredIndex_ = noIndex;
    onArrange();
}

const std::vector<Menu>& MenuBar::menus() const noexcept { return menus_; }

void MenuBar::setOnAction(ActionHandler onAction) {
    onAction_ = std::move(onAction);
}

bool MenuBar::isOpen() const noexcept {
    return host_ && popupId_ != invalidPopupId &&
        host_->popupId() == popupId_;
}

Rect MenuBar::menuBounds(std::size_t index) const {
    return menuBounds_.at(index);
}

void MenuBar::onArrange() {
    float x = bounds().x;
    for (std::size_t i = 0; i < menus_.size(); ++i) {
        const float width = labelLayout(i).size().width +
            style_.horizontalPadding * 2.0F;
        menuBounds_[i] = {x, bounds().y, width,
            std::min(style_.barHeight, bounds().height)};
        x += width;
    }
}

void MenuBar::onPaint(std::vector<PaintCommand>& commands) const {
    commands.push_back({bounds(), clip(), style_.background,
        invalidTextureId, 0.0F});
    for (std::size_t i = 0; i < menus_.size(); ++i) {
        const Rect item = menuBounds_[i];
        if (!hasArea(intersect(item, clip()))) continue;
        if (i == openIndex_ && isOpen()) {
            commands.push_back({item, clip(), style_.selected,
                invalidTextureId, 2.0F});
        } else if (i == hoveredIndex_ || (focused_ && i == openIndex_)) {
            commands.push_back({item, clip(), style_.hovered,
                invalidTextureId, 2.0F});
        }
        const TextLayout& label = labelLayout(i);
        label.appendPaintCommands({
            item.x + style_.horizontalPadding,
            item.y + (item.height - label.size().height) * 0.5F},
            intersect(item, clip()), style_.text, commands);
    }
}

bool MenuBar::acceptsPointerEvents() const noexcept {
    return !menus_.empty();
}

bool MenuBar::onPointerEvent(const WidgetPointerEvent& event) {
    switch (event.type) {
    case WidgetPointerEventType::Enter:
    case WidgetPointerEventType::Move: {
        const std::size_t next = menuAt(event.position);
        const bool changed = hoveredIndex_ != next;
        hoveredIndex_ = next;
        if (isOpen() && next != noIndex && next != openIndex_) open(next);
        return changed;
    }
    case WidgetPointerEventType::Leave:
        hoveredIndex_ = noIndex;
        return true;
    case WidgetPointerEventType::Press: {
        if (event.button != PointerButton::Primary) return false;
        const std::size_t next = menuAt(event.position);
        if (next == noIndex) return false;
        if (isOpen() && next == openIndex_) host_->dismissPopup(popupId_);
        else open(next);
        return true;
    }
    case WidgetPointerEventType::Release:
    case WidgetPointerEventType::Cancel:
        return true;
    }
    return false;
}

bool MenuBar::acceptsFocus() const noexcept { return !menus_.empty(); }
bool MenuBar::onFocusChanged(bool focused) {
    focused_ = focused;
    return true;
}

bool MenuBar::onKeyEvent(const WidgetKeyEvent& event) {
    if (menus_.empty()) return false;
    if (event.key != KeyCode::Left && event.key != KeyCode::Right &&
        event.key != KeyCode::Home && event.key != KeyCode::End &&
        event.key != KeyCode::Down && event.key != KeyCode::Enter &&
        event.key != KeyCode::Space) return false;
    if (event.type != WidgetKeyEventType::Press) return true;
    if (event.key == KeyCode::Down || event.key == KeyCode::Enter ||
        event.key == KeyCode::Space) {
        open(openIndex_ == noIndex ? 0 : openIndex_);
        return true;
    }
    const std::size_t current = openIndex_ == noIndex ? 0 : openIndex_;
    const std::size_t next = event.key == KeyCode::Home ? 0 :
        event.key == KeyCode::End ? menus_.size() - 1 :
        event.key == KeyCode::Left ?
            (current == 0 ? menus_.size() - 1 : current - 1) :
            (current + 1) % menus_.size();
    openIndex_ = next;
    if (isOpen()) open(next);
    return true;
}

const TextLayout& MenuBar::labelLayout(std::size_t index) const {
    auto& cached = labels_.at(index);
    if (!cached) {
        cached = textEngine_->createLayout(menus_[index].label,
            textStyle_, {});
        if (!cached) throw std::runtime_error("TextEngine returned null layout");
    }
    return *cached;
}

std::size_t MenuBar::menuAt(Point point) const noexcept {
    for (std::size_t i = 0; i < menuBounds_.size(); ++i) {
        if (contains(menuBounds_[i], point)) return i;
    }
    return noIndex;
}

void MenuBar::open(std::size_t index) {
    if (!host_ || index >= menus_.size()) return;
    PopupOptions options;
    options.placement = PopupPlacement::BelowStart;
    options.gap = 1.0F;
    options.matchAnchorWidth = false;
    options.allowContentPointerEvents = true;
    options.contentPointerRegion = bounds();
    auto popup = std::make_unique<MenuPopup>(*host_, textEngine_,
        menus_[index].entries, onAction_, style_, textStyle_);
    const PopupId id = host_->showPopup(std::move(popup),
        menuBounds_[index], options,
        [this](PopupCloseReason) {
            popupId_ = invalidPopupId;
            openIndex_ = noIndex;
        });
    popupId_ = id;
    openIndex_ = index;
}

} // namespace lotui
