#include "widgets/document_tab_view.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace lotui {
namespace {

float positive(float value) noexcept {
    return std::isfinite(value) ? std::max(0.0F, value) : 0.0F;
}

void paintRect(std::vector<PaintCommand>& commands, Rect bounds,
    Rect clip, Color color, float radius = 0.0F) {
    if (hasArea(intersect(bounds, clip))) {
        commands.push_back({bounds, clip, color, invalidTextureId, radius});
    }
}

} // namespace

DocumentTabView::DocumentTabView(
    std::shared_ptr<const TextEngine> textEngine,
    DocumentTabStyle style,
    TextStyle textStyle)
    : textEngine_(std::move(textEngine)),
      style_(style), textStyle_(std::move(textStyle)) {
    if (!textEngine_ || !isValidTextStyle(textStyle_)) {
        throw std::invalid_argument("DocumentTabView requires valid text settings");
    }
    style_.height = positive(style_.height);
    style_.minimumTabWidth = positive(style_.minimumTabWidth);
    style_.maximumTabWidth = std::max(style_.minimumTabWidth,
        positive(style_.maximumTabWidth));
    style_.horizontalPadding = positive(style_.horizontalPadding);
    style_.controlWidth = positive(style_.controlWidth);
    style_.spacing = positive(style_.spacing);
}

Size DocumentTabView::measure(const LayoutConstraints& constraints) const {
    float width = style_.controlWidth;
    for (std::size_t i = 0; i < tabs_.size(); ++i) {
        width += widthFor(i) + style_.spacing;
    }
    return constraints.constrain({width, style_.height});
}

void DocumentTabView::setTabs(std::vector<DocumentTab> tabs) {
    for (std::size_t i = 0; i < tabs.size(); ++i) {
        if (tabs[i].id.empty() || tabs[i].title.empty()) {
            throw std::invalid_argument("document tab id and title must not be empty");
        }
        for (std::size_t j = 0; j < i; ++j) {
            if (tabs[j].id == tabs[i].id) {
                throw std::invalid_argument("document tab ids must be unique");
            }
        }
    }
    tabs_ = std::move(tabs);
    slots_.clear();
    slots_.resize(tabs_.size());
    pressed_ = {};
    hovered_ = {};
    if (std::none_of(tabs_.begin(), tabs_.end(),
            [this](const DocumentTab& tab) { return tab.id == selectedId_; })) {
        selectedId_ = tabs_.empty() ? std::string{} : tabs_.front().id;
    }
    firstVisible_ = tabs_.empty() ? 0 :
        std::min(firstVisible_, tabs_.size() - 1);
    ensureSelectedVisible();
    onArrange();
}

const std::vector<DocumentTab>& DocumentTabView::tabs() const noexcept {
    return tabs_;
}

bool DocumentTabView::setSelectedId(std::string_view id) {
    const auto found = std::find_if(tabs_.begin(), tabs_.end(),
        [id](const DocumentTab& tab) { return tab.id == id; });
    if (found == tabs_.end() || selectedId_ == id) {
        return false;
    }
    selectedId_ = found->id;
    ensureSelectedVisible();
    onArrange();
    return true;
}

std::string_view DocumentTabView::selectedId() const noexcept {
    return selectedId_;
}

void DocumentTabView::setOnActivate(TabHandler handler) {
    onActivate_ = std::move(handler);
}

void DocumentTabView::setOnClose(TabHandler handler) {
    onClose_ = std::move(handler);
}

void DocumentTabView::setOnAdd(AddHandler handler) {
    onAdd_ = std::move(handler);
}

Rect DocumentTabView::tabBounds(std::size_t index) const {
    return slots_.at(index).tab;
}

Rect DocumentTabView::closeBounds(std::size_t index) const {
    return slots_.at(index).close;
}

Rect DocumentTabView::addBounds() const noexcept { return add_; }
Rect DocumentTabView::previousBounds() const noexcept { return previous_; }
Rect DocumentTabView::nextBounds() const noexcept { return next_; }

void DocumentTabView::onArrange() {
    for (Slot& slot : slots_) {
        slot.tab = {};
        slot.close = {};
    }
    add_ = previous_ = next_ = {};
    visibleEnd_ = firstVisible_;
    if (!hasArea(bounds())) {
        return;
    }

    const float height = std::min(style_.height, bounds().height);
    const float control = std::min(style_.controlWidth, bounds().width);
    const bool overflow = [&] {
        float total = control;
        for (std::size_t i = 0; i < tabs_.size(); ++i) {
            total += widthFor(i) + style_.spacing;
        }
        return total > bounds().width;
    }();
    if (!overflow) firstVisible_ = 0;
    float x = bounds().x;
    if (overflow) {
        previous_ = {x, bounds().y, control, height};
        x += control;
        next_ = {x, bounds().y, control, height};
        x += control;
    }
    const float right = std::max(x, bounds().x + bounds().width - control);
    add_ = {right, bounds().y,
        std::max(0.0F, bounds().x + bounds().width - right), height};
    for (std::size_t i = firstVisible_; i < tabs_.size(); ++i) {
        const float width = widthFor(i);
        if (x + width > right && i > firstVisible_) {
            break;
        }
        const float shown = std::max(0.0F, std::min(width, right - x));
        slots_[i].tab = {x, bounds().y, shown, height};
        if (tabs_[i].closable && shown >= 44.0F) {
            slots_[i].close = {x + shown - 26.0F, bounds().y + 4.0F,
                22.0F, std::max(0.0F, height - 8.0F)};
        }
        x += width + style_.spacing;
        visibleEnd_ = i + 1;
    }
}

void DocumentTabView::onPaint(std::vector<PaintCommand>& commands) const {
    paintRect(commands, bounds(), clip(), style_.background);
    const auto paintControl = [&](Rect rect, const char* label, Target target) {
        if (!hasArea(rect)) return;
        const Color color = hovered_ == target ? style_.hovered : style_.normal;
        paintRect(commands, rect, clip(), color, 3.0F);
        auto layout = textEngine_->createLayout(label, textStyle_, {});
        if (layout) {
            layout->appendPaintCommands({
                rect.x + (rect.width - layout->size().width) * 0.5F,
                rect.y + (rect.height - layout->size().height) * 0.5F},
                intersect(rect, clip()), style_.text, commands);
        }
    };
    paintControl(previous_, "<", {Part::Previous, 0});
    paintControl(next_, ">", {Part::Next, 0});
    for (std::size_t i = firstVisible_; i < visibleEnd_; ++i) {
        const Rect tab = slots_[i].tab;
        if (!hasArea(tab)) continue;
        const bool selected = tabs_[i].id == selectedId_;
        const Color color = selected ? style_.selected :
            (hovered_ == Target{Part::Tab, i} ? style_.hovered : style_.normal);
        paintRect(commands, tab, clip(), color, 3.0F);
        if (selected) {
            paintRect(commands, {tab.x, tab.y, tab.width, 2.0F},
                clip(), style_.accent);
            if (focused_) {
                paintRect(commands, {tab.x, tab.y + tab.height - 2.0F,
                    tab.width, 2.0F}, clip(), style_.accent);
            }
        }
        const Rect textClip = intersect(clip(), {
            tab.x + style_.horizontalPadding, tab.y,
            std::max(0.0F, tab.width - style_.horizontalPadding -
                (hasArea(slots_[i].close) ? 28.0F : style_.horizontalPadding) -
                (tabs_[i].dirty ? 12.0F : 0.0F)), tab.height});
        const TextLayout& title = titleLayout(i);
        title.appendPaintCommands({tab.x + style_.horizontalPadding,
            tab.y + (tab.height - title.size().height) * 0.5F},
            textClip, style_.text, commands);
        if (tabs_[i].dirty) {
            const float dotX = hasArea(slots_[i].close)
                ? slots_[i].close.x - 9.0F : tab.x + tab.width - 12.0F;
            paintRect(commands, {dotX, tab.y + tab.height * 0.5F - 2.5F,
                5.0F, 5.0F}, intersect(tab, clip()), style_.accent, 2.5F);
        }
        if (hasArea(slots_[i].close)) {
            paintControl(slots_[i].close, "x", {Part::Close, i});
        }
    }
    paintControl(add_, "+", {Part::Add, 0});
}

bool DocumentTabView::acceptsPointerEvents() const noexcept { return true; }

bool DocumentTabView::onPointerEvent(const WidgetPointerEvent& event) {
    switch (event.type) {
    case WidgetPointerEventType::Enter:
    case WidgetPointerEventType::Move:
        hovered_ = hit(event.position);
        return true;
    case WidgetPointerEventType::Leave:
        hovered_ = {};
        return true;
    case WidgetPointerEventType::Press:
        if (event.button != PointerButton::Primary) return false;
        pressed_ = hit(event.position);
        return pressed_.part != Part::None;
    case WidgetPointerEventType::Release: {
        if (event.button != PointerButton::Primary) return false;
        const Target target = hit(event.position);
        const bool clicked = pressed_.part != Part::None && pressed_ == target;
        pressed_ = {};
        if (!clicked) return true;
        switch (target.part) {
        case Part::Tab: activate(target.index); break;
        case Part::Close:
            if (onClose_) {
                const std::string id = tabs_[target.index].id;
                onClose_(id);
            }
            break;
        case Part::Add:
            if (onAdd_) onAdd_();
            break;
        case Part::Previous:
            if (firstVisible_ > 0) { --firstVisible_; onArrange(); }
            break;
        case Part::Next:
            if (visibleEnd_ < tabs_.size()) { ++firstVisible_; onArrange(); }
            break;
        case Part::None: break;
        }
        return true;
    }
    case WidgetPointerEventType::Cancel:
        pressed_ = {};
        return true;
    }
    return false;
}

bool DocumentTabView::acceptsFocus() const noexcept {
    return !tabs_.empty();
}

bool DocumentTabView::onFocusChanged(bool focused) {
    focused_ = focused;
    return true;
}

bool DocumentTabView::onKeyEvent(const WidgetKeyEvent& event) {
    if (tabs_.empty() || (event.key != KeyCode::Left &&
            event.key != KeyCode::Right && event.key != KeyCode::Home &&
            event.key != KeyCode::End)) return false;
    if (event.type != WidgetKeyEventType::Press) return true;
    auto found = std::find_if(tabs_.begin(), tabs_.end(),
        [this](const DocumentTab& tab) { return tab.id == selectedId_; });
    const std::size_t selected = found == tabs_.end() ? 0 :
        static_cast<std::size_t>(found - tabs_.begin());
    const std::size_t next = event.key == KeyCode::Home ? 0 :
        event.key == KeyCode::End ? tabs_.size() - 1 :
        event.key == KeyCode::Left ?
            (selected == 0 ? tabs_.size() - 1 : selected - 1) :
            (selected + 1) % tabs_.size();
    activate(next);
    return true;
}

const TextLayout& DocumentTabView::titleLayout(std::size_t index) const {
    const Slot& slot = slots_.at(index);
    if (!slot.title) {
        slot.title = textEngine_->createLayout(tabs_[index].title, textStyle_, {});
        if (!slot.title) throw std::runtime_error("TextEngine returned null layout");
    }
    return *slot.title;
}

float DocumentTabView::widthFor(std::size_t index) const {
    const float extras = style_.horizontalPadding * 2.0F +
        (tabs_[index].closable ? 26.0F : 0.0F) +
        (tabs_[index].dirty ? 12.0F : 0.0F);
    return std::clamp(titleLayout(index).size().width + extras,
        style_.minimumTabWidth, style_.maximumTabWidth);
}

DocumentTabView::Target DocumentTabView::hit(Point point) const noexcept {
    if (contains(previous_, point)) return {Part::Previous, 0};
    if (contains(next_, point)) return {Part::Next, 0};
    if (contains(add_, point)) return {Part::Add, 0};
    for (std::size_t i = firstVisible_; i < visibleEnd_; ++i) {
        if (contains(slots_[i].close, point)) return {Part::Close, i};
        if (contains(slots_[i].tab, point)) return {Part::Tab, i};
    }
    return {};
}

void DocumentTabView::activate(std::size_t index) {
    if (index >= tabs_.size() || tabs_[index].id == selectedId_) return;
    selectedId_ = tabs_[index].id;
    ensureSelectedVisible();
    onArrange();
    if (onActivate_) {
        const std::string id = selectedId_;
        onActivate_(id);
    }
}

void DocumentTabView::ensureSelectedVisible() {
    const auto found = std::find_if(tabs_.begin(), tabs_.end(),
        [this](const DocumentTab& tab) { return tab.id == selectedId_; });
    if (found == tabs_.end()) return;
    const auto index = static_cast<std::size_t>(found - tabs_.begin());
    if (index < firstVisible_) firstVisible_ = index;
    onArrange();
    while (index >= visibleEnd_ && firstVisible_ < index) {
        ++firstVisible_;
        onArrange();
    }
}

} // namespace lotui
