#include "widgets/dialog.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <stdexcept>
#include <utility>

namespace lotui {
namespace {

float nonNegative(float value) noexcept {
    return std::isfinite(value) ? std::max(0.0F, value) : 0.0F;
}

EdgeInsets normalized(EdgeInsets value) noexcept {
    value.left = nonNegative(value.left);
    value.top = nonNegative(value.top);
    value.right = nonNegative(value.right);
    value.bottom = nonNegative(value.bottom);
    return value;
}

Size contentMaximum(Size maximum, const EdgeInsets& padding) noexcept {
    return {
        std::max(0.0F, maximum.width - padding.horizontal()),
        std::max(0.0F, maximum.height - padding.vertical()),
    };
}

Size limitedMaximum(Size maximum, Size preferred) noexcept {
    if (preferred.width > 0.0F) {
        maximum.width = std::min(maximum.width, preferred.width);
    }
    if (preferred.height > 0.0F) {
        maximum.height = std::min(maximum.height, preferred.height);
    }
    return maximum;
}

Size measureWithin(
    const Widget* widget,
    Size maximum,
    const EdgeInsets& padding) {
    return widget
        ? widget->measure(LayoutConstraints::loose(
            contentMaximum(maximum, padding)))
        : Size{};
}

} // namespace

Dialog::Dialog(
    std::unique_ptr<Widget> content,
    Size preferredSize,
    DialogStyle style)
    : SingleChildWidget(std::move(content)),
      preferredSize_(preferredSize),
      style_(style) {
    setPreferredSize(preferredSize_);
    setStyle(style_);
}

Size Dialog::measure(const LayoutConstraints& constraints) const {
    const Size dialogMaximum = limitedMaximum(
        constraints.maximum, preferredSize_);
    const Size titleSize = measureWithin(
        title_.get(), dialogMaximum, style_.titlePadding);
    const float titleBlockHeight = title_
        ? style_.titlePadding.vertical() + titleSize.height +
            style_.titleDividerHeight + style_.titleContentSpacing
        : 0.0F;
    Size bodyMaximum = dialogMaximum;
    bodyMaximum.height = std::max(
        0.0F, bodyMaximum.height - titleBlockHeight);
    const Size childSize = measureWithin(
        child(), bodyMaximum, style_.contentPadding);

    Size desired = preferredSize_;
    if (title_) {
        desired.width = std::max(
            desired.width,
            titleSize.width + style_.titlePadding.horizontal());
    }
    if (child()) {
        desired.width = std::max(
            desired.width,
            childSize.width + style_.contentPadding.horizontal());
        desired.height = std::max(
            desired.height,
            titleBlockHeight + childSize.height +
                style_.contentPadding.vertical());
    }
    return constraints.constrain(desired);
}

void Dialog::setContent(std::unique_ptr<Widget> content) {
    setChild(std::move(content));
}

std::unique_ptr<Widget> Dialog::takeContent() noexcept {
    return takeChild();
}

Widget* Dialog::content() noexcept {
    return child();
}

const Widget* Dialog::content() const noexcept {
    return child();
}

void Dialog::setTitle(std::unique_ptr<Widget> title) {
    title_ = std::move(title);
    if (hasArea(bounds())) {
        onArrange();
    }
}

std::unique_ptr<Widget> Dialog::takeTitle() noexcept {
    return std::move(title_);
}

Widget* Dialog::title() noexcept {
    return title_.get();
}

const Widget* Dialog::title() const noexcept {
    return title_.get();
}

void Dialog::setPreferredSize(Size preferredSize) noexcept {
    preferredSize_.width = nonNegative(preferredSize.width);
    preferredSize_.height = nonNegative(preferredSize.height);
}

Size Dialog::preferredSize() const noexcept {
    return preferredSize_;
}

void Dialog::setStyle(DialogStyle style) noexcept {
    style.cornerRadius = nonNegative(style.cornerRadius);
    style.titlePadding = normalized(style.titlePadding);
    style.titleDividerHeight = nonNegative(style.titleDividerHeight);
    style.titleContentSpacing = nonNegative(style.titleContentSpacing);
    style.contentPadding = normalized(style.contentPadding);
    style_ = style;
}

const DialogStyle& Dialog::style() const noexcept {
    return style_;
}

void Dialog::onArrange() {
    float contentTop = bounds().y;
    if (title_) {
        const Size titleSize = measureWithin(
            title_.get(), {bounds().width, bounds().height},
            style_.titlePadding);
        title_->arrange({
            bounds().x + style_.titlePadding.left,
            bounds().y + style_.titlePadding.top,
            titleSize.width,
            titleSize.height,
        }, clip());
        contentTop = bounds().y + style_.titlePadding.top +
            titleSize.height + style_.titlePadding.bottom +
            style_.titleDividerHeight + style_.titleContentSpacing;
    }
    if (!child()) {
        return;
    }
    const EdgeInsets& padding = style_.contentPadding;
    child()->arrange({
        bounds().x + padding.left,
        contentTop + padding.top,
        std::max(0.0F, bounds().width - padding.horizontal()),
        std::max(0.0F,
            bounds().y + bounds().height - contentTop -
                padding.vertical()),
    }, clip());
}

void Dialog::onPaint(std::vector<PaintCommand>& commands) const {
    commands.push_back({
        bounds(), clip(), style_.background, invalidTextureId,
        style_.cornerRadius});
    if (title_ && style_.titleDividerHeight > 0.0F) {
        commands.push_back({
            {
                bounds().x,
                title_->bounds().y + title_->bounds().height +
                    style_.titlePadding.bottom,
                bounds().width,
                style_.titleDividerHeight,
            },
            clip(), style_.titleDivider, invalidTextureId, 0.0F});
    }
}

void Dialog::paintChildren(
    std::vector<PaintCommand>& commands) const {
    if (title_) {
        title_->paint(commands);
    }
    SingleChildWidget::paintChildren(commands);
}

void Dialog::collectChildHitTestEntries(
    std::vector<HitTestEntry>& entries) const {
    if (title_) {
        title_->collectHitTestEntries(entries);
    }
    SingleChildWidget::collectChildHitTestEntries(entries);
}

void Dialog::collectChildFocusTargets(
    std::vector<PointerTargetId>& targets) const {
    SingleChildWidget::collectChildFocusTargets(targets);
    if (title_) {
        title_->collectFocusTargets(targets);
    }
}

Widget* Dialog::findChildByPointerTarget(
    PointerTargetId target) noexcept {
    if (Widget* result =
            SingleChildWidget::findChildByPointerTarget(target)) {
        return result;
    }
    return title_ ? title_->findByPointerTarget(target) : nullptr;
}

bool Dialog::acceptsPointerEvents() const noexcept {
    return true;
}

bool Dialog::onPointerEvent(const WidgetPointerEvent&) {
    return true;
}

DialogHost::DialogHost(
    std::unique_ptr<Widget> content,
    DialogHostStyle style)
    : content_(std::move(content)), style_(style) {
    if (!content_) {
        throw std::invalid_argument("dialog host content must not be null");
    }
    setStyle(style_);
}

Size DialogHost::measure(const LayoutConstraints& constraints) const {
    return content_->measure(constraints);
}

void DialogHost::setContent(std::unique_ptr<Widget> content) {
    if (!content) {
        throw std::invalid_argument("dialog host content must not be null");
    }
    content_ = std::move(content);
    if (hasArea(bounds())) {
        content_->arrange(bounds(), clip());
    }
}

Widget* DialogHost::content() noexcept {
    return content_.get();
}

const Widget* DialogHost::content() const noexcept {
    return content_.get();
}

Widget& DialogHost::showModal(
    std::unique_ptr<Widget> modal,
    DialogClosedHandler onClosed) {
    if (!modal) {
        throw std::invalid_argument("dialog host modal must not be null");
    }
    if (modal_) {
        throw std::logic_error("dialog host already has an active modal");
    }
    modal_ = std::move(modal);
    onModalClosed_ = std::move(onClosed);
    arrangeModal();
    return *modal_;
}

void DialogHost::acceptModal() {
    closeModal(DialogResult::Accepted);
}

void DialogHost::cancelModal() {
    closeModal(DialogResult::Cancelled);
}

void DialogHost::dismissModal() {
    closeModal(DialogResult::Dismissed);
}

std::unique_ptr<Widget> DialogHost::takeModal() noexcept {
    onModalClosed_ = {};
    return std::move(modal_);
}

Widget* DialogHost::modal() noexcept {
    return modal_.get();
}

const Widget* DialogHost::modal() const noexcept {
    return modal_.get();
}

bool DialogHost::hasModal() const noexcept {
    return modal_ != nullptr;
}

ModelessDialogId DialogHost::showModeless(
    std::unique_ptr<Widget> dialog,
    Rect requestedBounds,
    ModelessClosedHandler onClosed) {
    if (!dialog) {
        throw std::invalid_argument("modeless dialog must not be null");
    }
    ModelessDialogId id = nextModelessId_++;
    if (id == invalidModelessDialogId) {
        id = nextModelessId_++;
    }
    requestedBounds.x = std::isfinite(requestedBounds.x)
        ? requestedBounds.x : 0.0F;
    requestedBounds.y = std::isfinite(requestedBounds.y)
        ? requestedBounds.y : 0.0F;
    requestedBounds.width = nonNegative(requestedBounds.width);
    requestedBounds.height = nonNegative(requestedBounds.height);
    modelessDialogs_.push_back({
        id,
        requestedBounds,
        std::move(dialog),
        std::move(onClosed),
    });
    arrangeModeless(modelessDialogs_.back());
    return id;
}

bool DialogHost::closeModeless(ModelessDialogId id) {
    const auto entry = std::find_if(
        modelessDialogs_.begin(), modelessDialogs_.end(),
        [id](const ModelessEntry& candidate) {
            return candidate.id == id;
        });
    if (entry == modelessDialogs_.end()) {
        return false;
    }
    ModelessClosedHandler onClosed = std::move(entry->onClosed);
    dismissedModelessDialogs_.push_back(std::move(entry->dialog));
    modelessDialogs_.erase(entry);
    if (onClosed) {
        onClosed();
    }
    return true;
}

bool DialogHost::bringModelessToFront(ModelessDialogId id) {
    const auto entry = std::find_if(
        modelessDialogs_.begin(), modelessDialogs_.end(),
        [id](const ModelessEntry& candidate) {
            return candidate.id == id;
        });
    if (entry == modelessDialogs_.end()) {
        return false;
    }
    if (std::next(entry) != modelessDialogs_.end()) {
        std::rotate(entry, std::next(entry), modelessDialogs_.end());
    }
    return true;
}

bool DialogHost::setModelessBounds(
    ModelessDialogId id,
    Rect requestedBounds) {
    const auto entry = std::find_if(
        modelessDialogs_.begin(), modelessDialogs_.end(),
        [id](const ModelessEntry& candidate) {
            return candidate.id == id;
        });
    if (entry == modelessDialogs_.end()) {
        return false;
    }
    requestedBounds.x = std::isfinite(requestedBounds.x)
        ? requestedBounds.x : 0.0F;
    requestedBounds.y = std::isfinite(requestedBounds.y)
        ? requestedBounds.y : 0.0F;
    requestedBounds.width = nonNegative(requestedBounds.width);
    requestedBounds.height = nonNegative(requestedBounds.height);
    entry->bounds = requestedBounds;
    arrangeModeless(*entry);
    return true;
}

Widget* DialogHost::modeless(ModelessDialogId id) noexcept {
    const auto entry = std::find_if(
        modelessDialogs_.begin(), modelessDialogs_.end(),
        [id](const ModelessEntry& candidate) {
            return candidate.id == id;
        });
    return entry == modelessDialogs_.end() ? nullptr : entry->dialog.get();
}

const Widget* DialogHost::modeless(ModelessDialogId id) const noexcept {
    return const_cast<DialogHost*>(this)->modeless(id);
}

std::size_t DialogHost::modelessCount() const noexcept {
    return modelessDialogs_.size();
}

void DialogHost::setStyle(DialogHostStyle style) noexcept {
    style.modalMargin = normalized(style.modalMargin);
    style_ = style;
}

const DialogHostStyle& DialogHost::style() const noexcept {
    return style_;
}

void DialogHost::onArrange() {
    content_->arrange(bounds(), clip());
    for (ModelessEntry& entry : modelessDialogs_) {
        arrangeModeless(entry);
    }
    arrangeModal();
}

void DialogHost::onPaint(std::vector<PaintCommand>& commands) const {
    (void)commands;
}

void DialogHost::paintChildren(
    std::vector<PaintCommand>& commands) const {
    dismissedModal_.reset();
    dismissedModelessDialogs_.clear();
    content_->paint(commands);
    for (const ModelessEntry& entry : modelessDialogs_) {
        entry.dialog->paint(commands);
    }
    if (modal_) {
        commands.push_back({
            bounds(), clip(), style_.scrim, invalidTextureId, 0.0F});
        modal_->paint(commands);
    }
}

void DialogHost::collectChildHitTestEntries(
    std::vector<HitTestEntry>& entries) const {
    if (modal_) {
        modal_->collectHitTestEntries(entries);
    } else {
        content_->collectHitTestEntries(entries);
        for (const ModelessEntry& entry : modelessDialogs_) {
            entry.dialog->collectHitTestEntries(entries);
        }
    }
}

void DialogHost::collectChildFocusTargets(
    std::vector<PointerTargetId>& targets) const {
    if (modal_) {
        modal_->collectFocusTargets(targets);
    } else {
        content_->collectFocusTargets(targets);
        for (const ModelessEntry& entry : modelessDialogs_) {
            entry.dialog->collectFocusTargets(targets);
        }
    }
}

Widget* DialogHost::findChildByPointerTarget(
    PointerTargetId target) noexcept {
    if (modal_) {
        if (Widget* result = modal_->findByPointerTarget(target)) {
            return result;
        }
    }
    for (auto entry = modelessDialogs_.rbegin();
         entry != modelessDialogs_.rend(); ++entry) {
        if (Widget* result = entry->dialog->findByPointerTarget(target)) {
            return result;
        }
    }
    if (Widget* result = content_->findByPointerTarget(target)) {
        return result;
    }
    if (dismissedModal_) {
        if (Widget* result =
                dismissedModal_->findByPointerTarget(target)) {
            return result;
        }
    }
    for (const auto& dismissed : dismissedModelessDialogs_) {
        if (Widget* result = dismissed->findByPointerTarget(target)) {
            return result;
        }
    }
    return nullptr;
}

void DialogHost::onPreviewPointerEvent(
    PointerTargetId target,
    const WidgetPointerEvent& event) {
    if (modal_ || event.type != WidgetPointerEventType::Press) {
        return;
    }
    ModelessDialogId selected = invalidModelessDialogId;
    for (auto entry = modelessDialogs_.rbegin();
         entry != modelessDialogs_.rend(); ++entry) {
        if (entry->dialog->findByPointerTarget(target)) {
            selected = entry->id;
            break;
        }
    }
    if (selected != invalidModelessDialogId) {
        bringModelessToFront(selected);
    }
}

bool DialogHost::acceptsPointerEvents() const noexcept {
    return modal_ != nullptr;
}

bool DialogHost::onPointerEvent(const WidgetPointerEvent&) {
    return modal_ != nullptr;
}

bool DialogHost::onPreviewKeyEvent(const WidgetKeyEvent& event) {
    if (!modal_ || !style_.cancelOnEscape ||
        event.type != WidgetKeyEventType::Press ||
        event.key != KeyCode::Escape) {
        return false;
    }
    cancelModal();
    return true;
}

bool DialogHost::onUnhandledKeyEvent(const WidgetKeyEvent& event) {
    if (!modal_ || !style_.acceptOnUnhandledEnter ||
        event.type != WidgetKeyEventType::Press ||
        event.key != KeyCode::Enter || event.repeat ||
        event.modifiers.control || event.modifiers.alt ||
        event.modifiers.meta) {
        return false;
    }
    acceptModal();
    return true;
}

PointerTargetId DialogHost::activeFocusScopeTarget() const noexcept {
    return modal_ ? modal_->pointerTargetId() : content_->pointerTargetId();
}

void DialogHost::arrangeModal() {
    if (!modal_ || !hasArea(bounds())) {
        return;
    }
    const EdgeInsets& margin = style_.modalMargin;
    const Size available{
        std::max(0.0F, bounds().width - margin.horizontal()),
        std::max(0.0F, bounds().height - margin.vertical()),
    };
    const Size modalSize = modal_->measure(
        LayoutConstraints::loose(available));
    const Rect modalBounds{
        bounds().x + (bounds().width - modalSize.width) * 0.5F,
        bounds().y + (bounds().height - modalSize.height) * 0.5F,
        modalSize.width,
        modalSize.height,
    };
    modal_->arrange(modalBounds, clip());
}

void DialogHost::arrangeModeless(ModelessEntry& entry) {
    if (!entry.dialog || !hasArea(bounds())) {
        return;
    }
    Size dialogSize{entry.bounds.width, entry.bounds.height};
    if (dialogSize.width <= 0.0F || dialogSize.height <= 0.0F) {
        const Size measured = entry.dialog->measure(
            LayoutConstraints::loose({bounds().width, bounds().height}));
        if (dialogSize.width <= 0.0F) {
            dialogSize.width = measured.width;
        }
        if (dialogSize.height <= 0.0F) {
            dialogSize.height = measured.height;
        }
    }
    entry.dialog->arrange({
        bounds().x + entry.bounds.x,
        bounds().y + entry.bounds.y,
        dialogSize.width,
        dialogSize.height,
    }, clip());
}

void DialogHost::closeModal(DialogResult result) {
    if (!modal_) {
        return;
    }
    dismissedModal_ = std::move(modal_);
    DialogClosedHandler onClosed = std::move(onModalClosed_);
    onModalClosed_ = {};
    if (onClosed) {
        onClosed(result);
    }
}

} // namespace lotui
