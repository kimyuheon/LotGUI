#include "core/widget_tree.h"
#include "widgets/list_control.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "list_control_tests failed: " << message << '\n';
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
        if (size_.width <= 0.0F || size_.height <= 0.0F) {
            return;
        }
        commands.push_back({
            {origin.x, origin.y, size_.width, size_.height},
            clip,
            color,
            91,
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
        ++layoutCount;
        createdText.emplace_back(text);
        float width = static_cast<float>(text.size()) * style.fontSize * 0.5F;
        if (std::isfinite(options.maximumWidth)) {
            width = std::min(width, options.maximumWidth);
        }
        return std::make_unique<TestTextLayout>(
            lotui::Size{width, style.fontSize * 1.2F});
    }

    mutable std::size_t layoutCount{0};
    mutable std::vector<std::string> createdText;
};

std::vector<lotui::ListColumn> makeColumns() {
    return {
        {"visible", "표시", 80.0F, 48.0F, true},
        {"type", "유형", 100.0F, 60.0F, true},
        {"material", "재질", 100.0F, 60.0F, true},
        {"more", "작업", 80.0F, 48.0F, true},
    };
}

lotui::ListRow makeInteractiveRow(std::string name) {
    lotui::ListCell check;
    check.kind = lotui::ListCellKind::CheckBox;
    check.text = std::move(name);

    lotui::ListCell combo;
    combo.kind = lotui::ListCellKind::ComboBox;
    combo.options = {"벽", "문", "창"};
    combo.selectedOption = 1;

    lotui::ListCell lookup;
    lookup.kind = lotui::ListCellKind::Lookup;
    lookup.text = "콘크리트";

    lotui::ListCell button;
    button.kind = lotui::ListCellKind::ActionButton;
    button.text = "상세";

    return {check, combo, lookup, button};
}

void dispatchesCellSpecificActions() {
    auto engine = std::make_shared<TestTextEngine>();
    std::vector<lotui::ListCellEvent> actions;
    std::optional<lotui::ListCellAddress> selection;
    auto list = std::make_unique<lotui::ListControl>(
        engine,
        makeColumns(),
        std::vector<lotui::ListRow>{makeInteractiveRow("객체 A")},
        lotui::Size{400.0F, 150.0F},
        [&selection](std::optional<lotui::ListCellAddress> value) {
            selection = value;
        },
        [&actions](const lotui::ListCellEvent& event) {
            actions.push_back(event);
        });
    lotui::ListControl* observed = list.get();
    lotui::WidgetTree tree(std::move(list));
    tree.layout({0.0F, 0.0F, 400.0F, 150.0F});

    const auto pressAndRelease = [&tree](lotui::Point point) {
        const auto pressed = tree.pointerPressed(
            point, lotui::PointerButton::Primary);
        const auto released = tree.pointerReleased(
            point, lotui::PointerButton::Primary);
        require(pressed.handled && released.handled,
            "list cell pointer input must be reported as handled");
    };

    const auto clickAction = [&pressAndRelease, observed](
        lotui::ListCellAddress address) {
        const lotui::Rect action = observed->cellActionBounds(address);
        pressAndRelease({
            action.x + action.width * 0.5F,
            action.y + action.height * 0.5F,
        });
    };

    clickAction({0, 0});
    require(selection == lotui::ListCellAddress{0, 0} &&
            observed->cell({0, 0})->checked &&
            actions.back().action == lotui::ListCellAction::ToggleCheck,
        "a check cell must select and toggle itself");

    clickAction({0, 1});
    require(actions.back().address == lotui::ListCellAddress{0, 1} &&
            actions.back().action == lotui::ListCellAction::OpenComboBox &&
            lotui::hasArea(actions.back().anchor),
        "a combo cell must request an anchored combo popup");

    clickAction({0, 2});
    require(actions.back().action == lotui::ListCellAction::OpenLookup,
        "a lookup cell must request a lookup surface");

    clickAction({0, 3});
    require(actions.back().action == lotui::ListCellAction::InvokeButton,
        "an action cell must invoke its right-side button action");
}

void supportsSpreadsheetKeyboardNavigation() {
    auto engine = std::make_shared<TestTextEngine>();
    std::vector<lotui::ListCellEvent> actions;
    std::vector<lotui::ListRow> rows;
    for (int index = 0; index < 20; ++index) {
        rows.push_back(makeInteractiveRow("객체 " + std::to_string(index)));
    }
    auto list = std::make_unique<lotui::ListControl>(
        engine,
        makeColumns(),
        std::move(rows),
        lotui::Size{240.0F, 110.0F},
        lotui::ListControl::SelectionChangedHandler{},
        [&actions](const lotui::ListCellEvent& event) {
            actions.push_back(event);
        });
    lotui::ListControl* observed = list.get();
    lotui::WidgetTree tree(std::move(list));
    tree.layout({0.0F, 0.0F, 240.0F, 110.0F});

    tree.keyPressed(lotui::KeyCode::Tab);
    require(observed->isFocused(),
        "Tab must focus a list control");
    tree.keyPressed(lotui::KeyCode::Down);
    tree.keyPressed(lotui::KeyCode::Right);
    require(observed->selectedCell() == lotui::ListCellAddress{0, 1},
        "arrow keys must navigate cells like a spreadsheet");

    tree.keyPressed(lotui::KeyCode::Space);
    require(!actions.empty() &&
            actions.back().action == lotui::ListCellAction::OpenComboBox,
        "Space must run the selected cell's default control action");

    lotui::KeyModifiers control;
    control.control = true;
    tree.keyPressed(lotui::KeyCode::End, control);
    require(observed->selectedCell() == lotui::ListCellAddress{19, 3} &&
            observed->scrollOffset().x > 0.0F &&
            observed->scrollOffset().y > 0.0F,
        "Control+End must select and reveal the final cell");
    tree.keyPressed(lotui::KeyCode::Enter);
    require(actions.back().action == lotui::ListCellAction::InvokeButton,
        "Enter must activate the selected action cell");
}

void virtualizesLargeModelsDuringPaint() {
    auto engine = std::make_shared<TestTextEngine>();
    std::vector<lotui::ListRow> rows;
    rows.reserve(1000);
    for (int index = 0; index < 1000; ++index) {
        lotui::ListCell first;
        first.text = "행 " + std::to_string(index);
        lotui::ListCell second;
        second.text = "값 " + std::to_string(index);
        rows.push_back({std::move(first), std::move(second)});
    }
    std::vector<lotui::ListColumn> columns{
        {"name", "이름", 120.0F, 60.0F, true},
        {"value", "값", 120.0F, 60.0F, true},
    };
    auto list = std::make_unique<lotui::ListControl>(
        engine,
        std::move(columns),
        std::move(rows),
        lotui::Size{240.0F, 100.0F});
    lotui::WidgetTree tree(std::move(list));
    tree.layout({0.0F, 0.0F, 240.0F, 100.0F});

    std::vector<lotui::PaintCommand> commands;
    tree.paint(commands);
    require(engine->layoutCount < 20,
        "painting must shape only visible rows, not the entire model");
    require(engine->layoutCount >= 4,
        "painting must shape headers and visible cell text");
}

void startsTextEditingByDoubleClick() {
    auto engine = std::make_shared<TestTextEngine>();
    lotui::ListCell name;
    name.text = "Editable";
    std::vector<lotui::ListCellEvent> actions;
    auto list = std::make_unique<lotui::ListControl>(
        engine,
        std::vector<lotui::ListColumn>{
            {"name", "Name", 180.0F, 80.0F, true}},
        std::vector<lotui::ListRow>{{name}},
        lotui::Size{220.0F, 120.0F},
        lotui::ListControl::SelectionChangedHandler{},
        [&actions](const lotui::ListCellEvent& event) {
            actions.push_back(event);
        });
    lotui::ListControl* observed = list.get();
    lotui::WidgetTree tree(std::move(list));
    tree.layout({0.0F, 0.0F, 220.0F, 120.0F});
    const lotui::Rect cell = observed->cellBounds({0, 0});
    const lotui::Point center{
        cell.x + cell.width * 0.5F,
        cell.y + cell.height * 0.5F,
    };
    for (int click = 0; click < 2; ++click) {
        tree.pointerPressed(center, lotui::PointerButton::Primary);
        tree.pointerReleased(center, lotui::PointerButton::Primary);
    }
    require(actions.size() == 1 &&
            actions.front().action == lotui::ListCellAction::BeginEdit &&
            actions.front().address == lotui::ListCellAddress{0, 0} &&
            near(actions.front().anchor.x, cell.x) &&
            near(actions.front().anchor.width, cell.width),
        "double-clicking editable text must request a full-cell editor");
}

void scrollsWithWheelTracksAndDraggableThumbs() {
    auto engine = std::make_shared<TestTextEngine>();
    std::vector<lotui::ListRow> rows;
    for (int row = 0; row < 40; ++row) {
        lotui::ListCell first;
        first.text = "Row " + std::to_string(row);
        lotui::ListCell second;
        second.text = "Value " + std::to_string(row);
        lotui::ListCell third;
        third.text = "More " + std::to_string(row);
        rows.push_back({
            std::move(first), std::move(second), std::move(third)});
    }
    auto list = std::make_unique<lotui::ListControl>(
        engine,
        std::vector<lotui::ListColumn>{
            {"first", "First", 120.0F, 60.0F, true},
            {"second", "Second", 120.0F, 60.0F, true},
            {"third", "Third", 120.0F, 60.0F, true},
        },
        std::move(rows),
        lotui::Size{240.0F, 120.0F});
    lotui::ListControl* observed = list.get();
    lotui::WidgetTree tree(std::move(list));
    tree.layout({0.0F, 0.0F, 240.0F, 120.0F});

    require(observed->hasVerticalScrollBar() &&
            observed->hasHorizontalScrollBar() &&
            lotui::hasArea(observed->verticalScrollThumbBounds()) &&
            lotui::hasArea(observed->horizontalScrollThumbBounds()),
        "overflowing rows and columns must expose both scroll bars");

    const auto wheel = tree.scroll(
        {100.0F, 70.0F},
        {1.0F, 2.0F},
        lotui::ScrollDeltaMode::Line);
    require(wheel.handled && wheel.needsRepaint &&
            near(observed->scrollOffset().x, 28.0F) &&
            near(observed->scrollOffset().y, 56.0F),
        "line wheel deltas must scroll by ListControl row-height units");

    observed->setScrollOffset({0.0F, 0.0F});
    const lotui::Rect verticalThumb =
        observed->verticalScrollThumbBounds();
    const lotui::Point verticalStart{
        verticalThumb.x + verticalThumb.width * 0.5F,
        verticalThumb.y + verticalThumb.height * 0.5F,
    };
    tree.pointerPressed(verticalStart, lotui::PointerButton::Primary);
    tree.pointerMoved({verticalStart.x, verticalStart.y + 20.0F});
    tree.pointerReleased(
        {verticalStart.x, verticalStart.y + 20.0F},
        lotui::PointerButton::Primary);
    require(observed->scrollOffset().y > 0.0F,
        "dragging the vertical thumb must change the vertical offset");

    observed->setScrollOffset({0.0F, 0.0F});
    const lotui::Rect horizontalThumb =
        observed->horizontalScrollThumbBounds();
    const lotui::Point horizontalStart{
        horizontalThumb.x + horizontalThumb.width * 0.5F,
        horizontalThumb.y + horizontalThumb.height * 0.5F,
    };
    tree.pointerPressed(horizontalStart, lotui::PointerButton::Primary);
    tree.pointerMoved({horizontalStart.x + 20.0F, horizontalStart.y});
    tree.pointerReleased(
        {horizontalStart.x + 20.0F, horizontalStart.y},
        lotui::PointerButton::Primary);
    require(observed->scrollOffset().x > 0.0F,
        "dragging the horizontal thumb must change the horizontal offset");

    observed->setScrollOffset({0.0F, 0.0F});
    const lotui::Rect verticalTrack = observed->verticalScrollBarBounds();
    tree.pointerPressed(
        {
            verticalTrack.x + verticalTrack.width * 0.5F,
            verticalTrack.y + verticalTrack.height - 1.0F,
        },
        lotui::PointerButton::Primary);
    tree.pointerReleased(
        {
            verticalTrack.x + verticalTrack.width * 0.5F,
            verticalTrack.y + verticalTrack.height - 1.0F,
        },
        lotui::PointerButton::Primary);
    require(observed->scrollOffset().y > 0.0F,
        "clicking below the vertical thumb must page the viewport down");
}

void resizesColumnsAndRequestsSortingFromHeaders() {
    auto engine = std::make_shared<TestTextEngine>();
    std::vector<std::pair<std::size_t, float>> resized;
    std::vector<lotui::ListSortDescriptor> sorts;
    auto list = std::make_unique<lotui::ListControl>(
        engine,
        std::vector<lotui::ListColumn>{
            {"name", "Name", 100.0F, 60.0F, true, true},
            {"locked", "Locked", 100.0F, 80.0F, false, false},
        },
        std::vector<lotui::ListRow>{{
            lotui::ListCell{lotui::ListCellKind::Text, "Beta"},
            lotui::ListCell{lotui::ListCellKind::Text, "Read only"},
        }},
        lotui::Size{240.0F, 120.0F});
    lotui::ListControl* observed = list.get();
    observed->setOnColumnResized(
        [&resized](std::size_t column, float width) {
            resized.emplace_back(column, width);
        });
    observed->setOnSortChanged(
        [&sorts](std::optional<lotui::ListSortDescriptor> descriptor) {
            if (descriptor) {
                sorts.push_back(*descriptor);
            }
        });
    lotui::WidgetTree tree(std::move(list));
    tree.layout({0.0F, 0.0F, 240.0F, 120.0F});

    const lotui::Rect firstHandle =
        observed->columnResizeHandleBounds(0);
    const lotui::Point resizeStart{
        firstHandle.x + firstHandle.width * 0.5F,
        firstHandle.y + firstHandle.height * 0.5F,
    };
    tree.pointerPressed(resizeStart, lotui::PointerButton::Primary);
    tree.pointerMoved({resizeStart.x + 35.0F, resizeStart.y});
    tree.pointerReleased(
        {resizeStart.x + 35.0F, resizeStart.y},
        lotui::PointerButton::Primary);
    require(near(observed->columnWidth(0), 135.0F) &&
            !resized.empty() && resized.back().first == 0 &&
            near(resized.back().second, 135.0F),
        "dragging a resizable header divider must update and report its width");

    const lotui::Rect resizedHandle =
        observed->columnResizeHandleBounds(0);
    const lotui::Point clampStart{
        resizedHandle.x + resizedHandle.width * 0.5F,
        resizedHandle.y + resizedHandle.height * 0.5F,
    };
    tree.pointerPressed(clampStart, lotui::PointerButton::Primary);
    tree.pointerMoved({clampStart.x - 500.0F, clampStart.y});
    tree.pointerReleased(
        {clampStart.x - 500.0F, clampStart.y},
        lotui::PointerButton::Primary);
    require(near(observed->columnWidth(0), 60.0F),
        "column resizing must clamp to the declared minimum width");

    const auto clickHeader = [&tree, observed](std::size_t column) {
        const lotui::Rect header = observed->headerCellBounds(column);
        const lotui::Point point{
            header.x + std::min(20.0F, header.width * 0.25F),
            header.y + header.height * 0.5F,
        };
        tree.pointerPressed(point, lotui::PointerButton::Primary);
        tree.pointerReleased(point, lotui::PointerButton::Primary);
    };

    clickHeader(0);
    require(observed->sortDescriptor() == lotui::ListSortDescriptor{
                0, lotui::ListSortDirection::Ascending} &&
            sorts.size() == 1,
        "the first sortable-header click must request ascending order");
    clickHeader(0);
    require(observed->sortDescriptor() == lotui::ListSortDescriptor{
                0, lotui::ListSortDirection::Descending} &&
            sorts.size() == 2,
        "the second sortable-header click must request descending order");
    clickHeader(1);
    require(sorts.size() == 2 &&
            observed->sortDescriptor()->column == 0,
        "a non-sortable header must not change the sort descriptor");
}

void reordersAndFreezesColumns() {
    auto engine = std::make_shared<TestTextEngine>();
    auto textCell = [](std::string text) {
        lotui::ListCell cell;
        cell.text = std::move(text);
        return cell;
    };
    auto list = std::make_unique<lotui::ListControl>(
        engine,
        std::vector<lotui::ListColumn>{
            {"a", "A", 80.0F, 40.0F, true, true, true},
            {"b", "B", 80.0F, 40.0F, true, true, true},
            {"c", "C", 80.0F, 40.0F, true, true, true},
        },
        std::vector<lotui::ListRow>{{
            textCell("A0"), textCell("B0"), textCell("C0")}},
        lotui::Size{170.0F, 110.0F});
    lotui::ListControl* observed = list.get();
    std::vector<std::pair<std::size_t, std::size_t>> reordered;
    observed->setOnColumnReordered(
        [&reordered](std::size_t from, std::size_t to) {
            reordered.emplace_back(from, to);
        });
    observed->setSelectedCell(lotui::ListCellAddress{0, 0});
    observed->setSortDescriptor(lotui::ListSortDescriptor{
        0, lotui::ListSortDirection::Ascending});
    lotui::WidgetTree tree(std::move(list));
    tree.layout({0.0F, 0.0F, 170.0F, 110.0F});

    const lotui::Rect firstHeader = observed->headerCellBounds(0);
    const lotui::Rect lastHeader = observed->headerCellBounds(2);
    const lotui::Point dragStart{
        firstHeader.x + firstHeader.width * 0.35F,
        firstHeader.y + firstHeader.height * 0.5F,
    };
    const lotui::Point dragEnd{
        lastHeader.x + lastHeader.width * 0.75F,
        lastHeader.y + lastHeader.height * 0.5F,
    };
    tree.pointerPressed(dragStart, lotui::PointerButton::Primary);
    tree.pointerMoved(dragEnd);
    tree.pointerReleased(dragEnd, lotui::PointerButton::Primary);

    require(reordered.size() == 1 && reordered.front() ==
            std::pair<std::size_t, std::size_t>{0, 2},
        "dragging a reorderable header must report its final column index");
    require(observed->columns()[0].id == "b" &&
            observed->columns()[1].id == "c" &&
            observed->columns()[2].id == "a" &&
            observed->cell({0, 0})->text == "B0" &&
            observed->cell({0, 1})->text == "C0" &&
            observed->cell({0, 2})->text == "A0",
        "column reordering must move column metadata and every row cell together");
    require(observed->selectedCell() == lotui::ListCellAddress{0, 2} &&
            observed->sortDescriptor()->column == 2,
        "column reordering must remap selection and sort state");

    observed->setFrozenColumnCount(1);
    observed->setScrollOffset({0.0F, 0.0F});
    const float frozenX = observed->cellBounds({0, 0}).x;
    const float scrollingX = observed->cellBounds({0, 2}).x;
    observed->setScrollOffset({60.0F, 0.0F});
    require(near(observed->cellBounds({0, 0}).x, frozenX) &&
            near(observed->cellBounds({0, 2}).x, scrollingX - 60.0F),
        "frozen columns must remain fixed while later columns scroll");
    require(observed->horizontalScrollBarBounds().x >=
            observed->cellBounds({0, 0}).x + observed->columnWidth(0),
        "the horizontal scroll bar must begin after the frozen region");

    observed->ensureCellVisible({0, 0});
    require(near(observed->scrollOffset().x, 60.0F),
        "revealing a frozen cell must not change horizontal scroll");
    const lotui::Rect frozenCell = observed->cellBounds({0, 0});
    tree.pointerPressed(
        {
            frozenCell.x + frozenCell.width * 0.5F,
            frozenCell.y + frozenCell.height * 0.5F,
        },
        lotui::PointerButton::Primary);
    require(observed->selectedCell() == lotui::ListCellAddress{0, 0},
        "hit testing in the fixed region must resolve the frozen cell");
    tree.cancelPointer();
}

} // namespace

int main() {
    dispatchesCellSpecificActions();
    supportsSpreadsheetKeyboardNavigation();
    virtualizesLargeModelsDuringPaint();
    startsTextEditingByDoubleClick();
    scrollsWithWheelTracksAndDraggableThumbs();
    resizesColumnsAndRequestsSortingFromHeaders();
    reordersAndFreezesColumns();
    std::cout << "list_control_tests passed\n";
    return EXIT_SUCCESS;
}
