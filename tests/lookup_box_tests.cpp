#include "core/widget_tree.h"
#include "widgets/linear_layout.h"
#include "widgets/list_control.h"
#include "widgets/lookup_box.h"
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
        std::cerr << "lookup_box_tests failed: " << message << '\n';
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
            103,
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

std::vector<lotui::LookupItem> suppliers() {
    return {
        {"northwind", "Northwind", "Seattle"},
        {"contoso", "Contoso", "London"},
        {"fabrikam", "Fabrikam", "Berlin"},
        {"busan", "부산 공급사", "부산, 대한민국"},
    };
}

void click(
    lotui::WidgetTree& tree,
    lotui::Point position) {
    tree.pointerPressed(position, lotui::PointerButton::Primary);
    tree.pointerReleased(position, lotui::PointerButton::Primary);
}

void searchesSelectsAndClearsAStandaloneLookup() {
    auto engine = std::make_shared<TestTextEngine>();
    std::vector<std::optional<std::size_t>> changes;
    auto lookup = std::make_unique<lotui::LookupBox>(
        engine,
        suppliers(),
        std::nullopt,
        lotui::Size{180.0F, 32.0F},
        [&changes](std::optional<std::size_t> index) {
            changes.push_back(index);
        });
    lotui::LookupBox* observedLookup = lookup.get();
    auto content = std::make_unique<lotui::Column>();
    content->addChild(
        std::move(lookup),
        {0.0F, {180.0F, 32.0F}, {180.0F, 32.0F}});
    auto host = std::make_unique<lotui::PopupHost>(std::move(content));
    lotui::PopupHost* observedHost = host.get();
    observedLookup->setPopupHost(observedHost);

    lotui::WidgetTree tree(std::move(host));
    tree.layout({0.0F, 0.0F, 420.0F, 320.0F});
    click(tree, {90.0F, 16.0F});
    require(observedLookup->isPopupOpen() && observedHost->hasPopup(),
        "clicking a lookup must open its search popup");

    require(tree.textInputState().enabled,
        "the popup search field must receive immediate text-input focus");
    const auto typed = tree.textInput({
        lotui::TextInputEventType::Commit, "brik"});
    require(typed.handled,
        "typing a lookup query must be handled by the search field");
    tree.keyPressed(lotui::KeyCode::Enter);
    require(!observedHost->hasPopup() &&
            observedLookup->selectedIndex() == 2 &&
            observedLookup->selectedItem() != nullptr &&
            observedLookup->selectedItem()->id == "fabrikam" &&
            changes.size() == 1 && changes.front() == 2,
        "a partial query followed by Enter must commit the matching item");

    click(tree, {90.0F, 16.0F});
    tree.layout({0.0F, 0.0F, 420.0F, 320.0F});
    tree.keyPressed(lotui::KeyCode::Down);
    tree.keyPressed(lotui::KeyCode::Enter);
    require(observedLookup->selectedIndex() == 3 &&
            changes.size() == 2 && changes.back() == 3,
        "arrow keys from the search field must move and commit the result");

    tree.keyPressed(lotui::KeyCode::Delete);
    require(!observedLookup->selectedIndex() && changes.size() == 3 &&
            !changes.back(),
        "Delete on the closed lookup must clear and report the selection");
}

void searchesUtf8LabelsWithoutDamagingImeText() {
    auto engine = std::make_shared<TestTextEngine>();
    std::optional<std::size_t> selected;
    auto host = std::make_unique<lotui::PopupHost>(
        std::make_unique<lotui::Column>());
    lotui::PopupHost* observedHost = host.get();
    lotui::LookupBox::showPopupAt(
        *observedHost,
        engine,
        {20.0F, 20.0F, 180.0F, 30.0F},
        suppliers(),
        std::nullopt,
        [&selected](std::optional<std::size_t> index) {
            selected = index;
        });

    lotui::WidgetTree tree(std::move(host));
    tree.layout({0.0F, 0.0F, 420.0F, 320.0F});
    tree.moveFocus();
    require(tree.textInputState().enabled,
        "a directly opened lookup popup must focus its search field");
    tree.textInput({lotui::TextInputEventType::Composition, "부산"});
    require(!selected,
        "IME composition must not prematurely select a lookup result");
    tree.textInput({lotui::TextInputEventType::Commit, "부산"});
    tree.textInput({lotui::TextInputEventType::CompositionEnd});
    tree.keyPressed(lotui::KeyCode::Enter);
    require(selected == 3 && !observedHost->hasPopup(),
        "a committed Korean query must filter and select the exact UTF-8 label");
}

void reusesLookupPopupInsideListControlCells() {
    auto engine = std::make_shared<TestTextEngine>();
    lotui::ListCell lookupCell;
    lookupCell.kind = lotui::ListCellKind::Lookup;
    lookupCell.text = "Northwind";
    lookupCell.options = {
        "Northwind", "Contoso", "Fabrikam", "부산 공급사"};

    auto hostPointer = std::make_shared<lotui::PopupHost*>(nullptr);
    auto listPointer = std::make_shared<lotui::ListControl*>(nullptr);
    auto list = std::make_unique<lotui::ListControl>(
        engine,
        std::vector<lotui::ListColumn>{
            {"supplier", "Supplier", 190.0F, 80.0F, true}},
        std::vector<lotui::ListRow>{{lookupCell}},
        lotui::Size{230.0F, 120.0F},
        lotui::ListControl::SelectionChangedHandler{},
        [engine, hostPointer, listPointer](
            const lotui::ListCellEvent& event) {
            if (event.action != lotui::ListCellAction::OpenLookup ||
                *hostPointer == nullptr || *listPointer == nullptr) {
                return;
            }
            const lotui::ListCell* value =
                (*listPointer)->cell(event.address);
            std::vector<lotui::LookupItem> items;
            for (const std::string& option : value->options) {
                items.push_back({option, option, {}});
            }
            lotui::LookupBox::showPopupAt(
                **hostPointer,
                engine,
                event.anchor,
                std::move(items),
                std::nullopt,
                [listPointer, address = event.address](
                    std::optional<std::size_t> index) {
                    if (!index) {
                        return;
                    }
                    const lotui::ListCell* current =
                        (*listPointer)->cell(address);
                    (*listPointer)->setCellText(
                        address, current->options[*index]);
                });
        });
    *listPointer = list.get();
    auto host = std::make_unique<lotui::PopupHost>(std::move(list));
    *hostPointer = host.get();

    lotui::WidgetTree tree(std::move(host));
    tree.layout({0.0F, 0.0F, 230.0F, 180.0F});
    const lotui::Rect action = (*listPointer)->cellActionBounds({0, 0});
    click(tree, {
        action.x + action.width * 0.5F,
        action.y + action.height * 0.5F,
    });
    require((*hostPointer)->hasPopup() &&
            near((*hostPointer)->anchor().width,
                (*listPointer)->cellBounds({0, 0}).width),
        "a ListControl lookup must anchor the popup to the full cell");

    tree.layout({0.0F, 0.0F, 230.0F, 180.0F});
    tree.textInput({
        lotui::TextInputEventType::Commit, "부산"});
    tree.keyPressed(lotui::KeyCode::Enter);
    require(!(*hostPointer)->hasPopup() &&
            (*listPointer)->cell({0, 0})->text == "부산 공급사",
        "a ListControl lookup selection must update its cell model");
}

} // namespace

int main() {
    searchesSelectsAndClearsAStandaloneLookup();
    searchesUtf8LabelsWithoutDamagingImeText();
    reusesLookupPopupInsideListControlCells();
    std::cout << "lookup_box_tests passed\n";
    return EXIT_SUCCESS;
}
