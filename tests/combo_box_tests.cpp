#include "core/widget_tree.h"
#include "widgets/box.h"
#include "widgets/button.h"
#include "widgets/combo_box.h"
#include "widgets/linear_layout.h"
#include "widgets/list_control.h"
#include "widgets/popup.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "combo_box_tests failed: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

bool near(float left, float right) {
    return std::abs(left - right) < 0.01F;
}

class TestTextLayout final : public lotui::TextLayout {
public:
    explicit TestTextLayout(lotui::Size size)
        : size_(size) {
    }

    lotui::Size size() const noexcept override {
        return size_;
    }

    float baseline() const noexcept override {
        return size_.height * 0.8F;
    }

    void appendPaintCommands(
        lotui::Point origin,
        lotui::Rect clip,
        lotui::Color color,
        std::vector<lotui::PaintCommand>& commands) const override {
        commands.push_back({
            {origin.x, origin.y, size_.width, size_.height},
            clip,
            color,
            101,
            0.0F,
        });
    }

private:
    lotui::Size size_{};
};

class TestTextEngine final : public lotui::TextEngine {
public:
    std::unique_ptr<lotui::TextLayout> createLayout(
        std::string_view text,
        const lotui::TextStyle& style,
        const lotui::TextLayoutOptions& options) const override {
        float width = static_cast<float>(text.size()) * style.fontSize * 0.5F;
        if (std::isfinite(options.maximumWidth)) {
            width = std::min(width, options.maximumWidth);
        }
        return std::make_unique<TestTextLayout>(
            lotui::Size{width, style.fontSize * 1.2F});
    }
};

void positionsAndDismissesPopupSafely() {
    int backgroundClicks = 0;
    auto background = std::make_unique<lotui::Button>(
        lotui::Size{300.0F, 200.0F},
        [&backgroundClicks]() { ++backgroundClicks; });
    auto host = std::make_unique<lotui::PopupHost>(std::move(background));
    lotui::PopupHost* observedHost = host.get();
    lotui::PopupCloseReason reason = lotui::PopupCloseReason::Accepted;
    observedHost->showPopup(
        std::make_unique<lotui::Box>(lotui::Size{100.0F, 80.0F}),
        {120.0F, 175.0F, 80.0F, 20.0F},
        {},
        [&reason](lotui::PopupCloseReason value) { reason = value; });

    lotui::WidgetTree tree(std::move(host));
    tree.layout({0.0F, 0.0F, 300.0F, 200.0F});
    const lotui::Rect popupBounds = observedHost->popupBounds();
    require(near(popupBounds.x, 120.0F) &&
            near(popupBounds.y, 92.0F) &&
            near(popupBounds.width, 100.0F) &&
            near(popupBounds.height, 80.0F),
        "an automatic popup must flip above an anchor near the bottom edge");

    const auto press = tree.pointerPressed(
        {20.0F, 20.0F}, lotui::PointerButton::Primary);
    tree.pointerReleased(
        {20.0F, 20.0F}, lotui::PointerButton::Primary);
    require(press.handled && !observedHost->hasPopup() &&
            reason == lotui::PopupCloseReason::Dismissed &&
            backgroundClicks == 0,
        "an outside press must dismiss and consume without click-through");

    observedHost->showPopup(
        std::make_unique<lotui::Box>(lotui::Size{100.0F, 60.0F}),
        {20.0F, 20.0F, 100.0F, 24.0F},
        {},
        [&reason](lotui::PopupCloseReason value) { reason = value; });
    const auto escape = tree.keyPressed(lotui::KeyCode::Escape);
    require(escape.handled && !observedHost->hasPopup() &&
            reason == lotui::PopupCloseReason::Escape,
        "Escape must dismiss the active popup with a distinct reason");
}

void opensAndSelectsAStandaloneComboBox() {
    auto engine = std::make_shared<TestTextEngine>();
    std::vector<std::size_t> changes;
    auto combo = std::make_unique<lotui::ComboBox>(
        engine,
        std::vector<std::string>{"Small", "Medium", "Large"},
        0,
        lotui::Size{160.0F, 32.0F},
        [&changes](std::size_t index) { changes.push_back(index); });
    lotui::ComboBox* observedCombo = combo.get();
    auto content = std::make_unique<lotui::Column>();
    content->addChild(
        std::move(combo),
        {0.0F, {160.0F, 32.0F}, {160.0F, 32.0F}});
    auto host = std::make_unique<lotui::PopupHost>(std::move(content));
    lotui::PopupHost* observedHost = host.get();
    observedCombo->setPopupHost(observedHost);

    lotui::WidgetTree tree(std::move(host));
    tree.layout({0.0F, 0.0F, 400.0F, 260.0F});
    tree.pointerPressed({80.0F, 16.0F}, lotui::PointerButton::Primary);
    tree.pointerReleased({80.0F, 16.0F}, lotui::PointerButton::Primary);
    require(observedCombo->isPopupOpen() && observedHost->hasPopup() &&
            observedHost->popupBounds().y > observedCombo->bounds().y,
        "a combo click must open an anchored popup below the field");

    tree.keyPressed(lotui::KeyCode::Down);
    tree.keyPressed(lotui::KeyCode::Enter);
    require(!observedHost->hasPopup() &&
            observedCombo->selectedIndex() == 1 &&
            changes.size() == 1 && changes.front() == 1 &&
            *observedCombo->selectedText() == "Medium",
        "popup keyboard navigation must commit exactly one selection");

    tree.keyPressed(lotui::KeyCode::Down);
    require(observedCombo->selectedIndex() == 2 &&
            changes.size() == 2 && changes.back() == 2,
        "closed combo arrow navigation must change the selection");
}

void reusesComboPopupInsideListControlCells() {
    auto engine = std::make_shared<TestTextEngine>();
    lotui::ListCell comboCell;
    comboCell.kind = lotui::ListCellKind::ComboBox;
    comboCell.options = {"Draft", "Review", "Approved"};

    auto hostPointer = std::make_shared<lotui::PopupHost*>(nullptr);
    auto listPointer = std::make_shared<lotui::ListControl*>(nullptr);
    auto list = std::make_unique<lotui::ListControl>(
        engine,
        std::vector<lotui::ListColumn>{
            {"state", "State", 180.0F, 80.0F, true}},
        std::vector<lotui::ListRow>{{comboCell}},
        lotui::Size{220.0F, 120.0F},
        lotui::ListControl::SelectionChangedHandler{},
        [engine, hostPointer, listPointer](
            const lotui::ListCellEvent& event) {
            if (event.action != lotui::ListCellAction::OpenComboBox ||
                *hostPointer == nullptr || *listPointer == nullptr) {
                return;
            }
            const lotui::ListCell* value = (*listPointer)->cell(event.address);
            lotui::ComboBox::showPopupAt(
                **hostPointer,
                engine,
                event.anchor,
                value->options,
                value->selectedOption,
                [listPointer, address = event.address](std::size_t index) {
                    (*listPointer)->setComboSelection(address, index);
                });
        });
    *listPointer = list.get();
    auto host = std::make_unique<lotui::PopupHost>(std::move(list));
    *hostPointer = host.get();

    lotui::WidgetTree tree(std::move(host));
    tree.layout({0.0F, 0.0F, 220.0F, 140.0F});
    const lotui::Rect action = (*listPointer)->cellActionBounds({0, 0});
    const lotui::Point center{
        action.x + action.width * 0.5F,
        action.y + action.height * 0.5F,
    };
    tree.pointerPressed(center, lotui::PointerButton::Primary);
    tree.pointerReleased(center, lotui::PointerButton::Primary);
    require((*hostPointer)->hasPopup() &&
            near((*hostPointer)->popupBounds().width,
                (*listPointer)->cellBounds({0, 0}).width),
        "a ListControl combo cell must open the shared full-cell popup");

    tree.keyPressed(lotui::KeyCode::Down);
    tree.keyPressed(lotui::KeyCode::Enter);
    require(!(*hostPointer)->hasPopup() &&
            (*listPointer)->cell({0, 0})->selectedOption == 1,
        "a ListControl combo popup selection must update the cell model");
}

} // namespace

int main() {
    positionsAndDismissesPopupSafely();
    opensAndSelectsAStandaloneComboBox();
    reusesComboPopupInsideListControlCells();
    std::cout << "combo_box_tests passed\n";
    return EXIT_SUCCESS;
}
