#include "examples/platform_probe/platform_event_bridge.h"

#include "platform/platform_backend.h"
#include "platform/runtime_paths.h"
#include "renderer/vulkan/vulkan_renderer.h"
#include "text/freetype/freetype_text_engine.h"
#include "widgets/combo_box.h"
#include "widgets/button.h"
#include "widgets/inline_text_editor.h"
#include "widgets/label.h"
#include "widgets/linear_layout.h"
#include "widgets/list_control.h"
#include "widgets/lookup_box.h"
#include "widgets/popup.h"
#include "widgets/text_field.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <exception>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <set>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace {

lotui::ListRow makeRow(
    bool enabled,
    std::string name,
    std::size_t category,
    std::string supplier,
    std::string notes) {
    lotui::ListCell check;
    check.kind = lotui::ListCellKind::CheckBox;
    check.checked = enabled;

    lotui::ListCell nameCell;
    nameCell.text = std::move(name);

    lotui::ListCell categoryCell;
    categoryCell.kind = lotui::ListCellKind::ComboBox;
    categoryCell.options = {"Office", "Lighting", "Furniture"};
    categoryCell.selectedOption = category;

    lotui::ListCell supplierCell;
    supplierCell.kind = lotui::ListCellKind::Lookup;
    supplierCell.text = std::move(supplier);
    supplierCell.options = {
        "Northwind", "Contoso", "Fabrikam", "Adventure Works",
        "한국 공급사"};

    lotui::ListCell actionCell;
    actionCell.kind = lotui::ListCellKind::ActionButton;
    actionCell.text = "Open";

    lotui::ListCell notesCell;
    notesCell.text = std::move(notes);

    return {
        std::move(check),
        std::move(nameCell),
        std::move(categoryCell),
        std::move(supplierCell),
        std::move(notesCell),
        std::move(actionCell),
    };
}

std::string listCellSortKey(const lotui::ListCell& cell) {
    switch (cell.kind) {
    case lotui::ListCellKind::CheckBox:
        return cell.checked ? "1" : "0";
    case lotui::ListCellKind::ComboBox:
        return cell.selectedOption < cell.options.size()
            ? cell.options[cell.selectedOption]
            : std::string{};
    case lotui::ListCellKind::Text:
    case lotui::ListCellKind::Lookup:
    case lotui::ListCellKind::ActionButton:
        return cell.text;
    }
    return {};
}

bool containsText(std::string value, std::string query) {
    auto lower = [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    };
    std::transform(value.begin(), value.end(), value.begin(), lower);
    std::transform(query.begin(), query.end(), query.begin(), lower);
    return value.find(query) != std::string::npos;
}

struct TableState {
    std::vector<lotui::ListRow> rows;
    std::vector<std::size_t> visible;
    std::vector<std::vector<lotui::ListRow>> undo;
    std::vector<std::vector<lotui::ListRow>> redo;
    lotui::ListControl* list{nullptr};
    lotui::Button* undoButton{nullptr};
    lotui::Button* redoButton{nullptr};
    lotui::Button* deleteButton{nullptr};
    lotui::ComboBox* columnPicker{nullptr};
    std::vector<std::optional<std::set<std::string>>> columnFilters;
    std::size_t searchColumn{1};
    std::string query;
    std::size_t nextRowNumber{1};

    void updateButtons() const {
        if (undoButton) undoButton->setEnabled(!undo.empty());
        if (redoButton) redoButton->setEnabled(!redo.empty());
        if (deleteButton) deleteButton->setEnabled(
            list && list->selectedCell().has_value());
    }

    void record() {
        undo.push_back(rows);
        if (undo.size() > 100) undo.erase(undo.begin());
        redo.clear();
        updateButtons();
    }

    bool matches(const lotui::ListRow& row) const {
        return query.empty() || (searchColumn < row.size() &&
            containsText(listCellSortKey(row[searchColumn]), query));
    }

    bool matchesFilters(const lotui::ListRow& row) const {
        for (std::size_t column = 0; column < columnFilters.size(); ++column) {
            if (columnFilters[column] &&
                (column >= row.size() ||
                 columnFilters[column]->count(
                     listCellSortKey(row[column])) == 0)) {
                return false;
            }
        }
        return true;
    }

    void refresh(std::optional<std::size_t> selectedModelRow = std::nullopt,
                 std::size_t selectedColumn = 1) {
        if (!list) return;
        if (!selectedModelRow) {
            const auto selected = list->selectedCell();
            if (selected && selected->row < visible.size()) {
                selectedModelRow = visible[selected->row];
                selectedColumn = selected->column;
            }
        }
        visible.clear();
        std::vector<lotui::ListRow> shown;
        for (std::size_t index = 0; index < rows.size(); ++index) {
            if (matchesFilters(rows[index])) {
                visible.push_back(index);
                shown.push_back(rows[index]);
            }
        }
        list->setRows(std::move(shown));
        const auto found = selectedModelRow
            ? std::find(visible.begin(), visible.end(), *selectedModelRow)
            : visible.end();
        if (found != visible.end()) {
            list->setSelectedCell(lotui::ListCellAddress{
                static_cast<std::size_t>(found - visible.begin()),
                selectedColumn});
        } else {
            list->setSelectedCell(std::nullopt);
        }
        updateButtons();
    }

    void syncEdits() {
        if (!list || list->rows().size() != visible.size()) return;
        bool changed = false;
        for (std::size_t index = 0; index < visible.size(); ++index) {
            const auto& displayed = list->rows()[index];
            const auto& original = rows[visible[index]];
            for (std::size_t column = 0;
                 column < displayed.size() && column < original.size(); ++column) {
                if (displayed[column].text != original[column].text ||
                    displayed[column].checked != original[column].checked ||
                    displayed[column].selectedOption != original[column].selectedOption) {
                    if (!changed) record();
                    changed = true;
                    rows[visible[index]][column] = displayed[column];
                }
            }
        }
        if (changed) refresh();
        updateButtons();
    }

    void stepBack(bool backwards) {
        auto& source = backwards ? undo : redo;
        auto& destination = backwards ? redo : undo;
        if (source.empty()) return;
        destination.push_back(rows);
        rows = std::move(source.back());
        source.pop_back();
        refresh();
    }

    void addRow() {
        record();
        const std::size_t index = rows.size();
        rows.push_back(makeRow(true,
            "New item " + std::to_string(nextRowNumber++), 0, "", ""));
        std::fill(columnFilters.begin(), columnFilters.end(), std::nullopt);
        for (std::size_t column = 0; column < columnFilters.size(); ++column) {
            if (list) list->setHeaderFilterActive(column, false);
        }
        refresh(index);
    }

    void eraseRow(std::size_t index) {
        if (index >= rows.size()) return;
        record();
        rows.erase(rows.begin() + static_cast<std::ptrdiff_t>(index));
        refresh(rows.empty() ? std::nullopt
            : std::optional<std::size_t>{std::min(index, rows.size() - 1)});
    }

    void deleteRow() {
        const auto selected = list->selectedCell();
        if (selected && selected->row < visible.size()) {
            eraseRow(visible[selected->row]);
        }
    }

    void findNext() {
        if (query.empty() || !list || visible.empty()) return;
        const auto selected = list->selectedCell();
        const std::size_t start = selected
            ? (selected->row + 1) % visible.size() : 0;
        for (std::size_t offset = 0; offset < visible.size(); ++offset) {
            const std::size_t index = (start + offset) % visible.size();
            if (matches(rows[visible[index]])) {
                list->setSelectedCell(
                    lotui::ListCellAddress{index, searchColumn});
                updateButtons();
                return;
            }
        }
    }
};

struct FilterPopupState {
    lotui::ListControl* list{nullptr};
    std::vector<std::string> values;
    std::vector<std::string> shown;
    std::set<std::string> checked;
    std::string query;

    void refresh() {
        shown.clear();
        std::vector<lotui::ListRow> rows;
        for (const std::string& value : values) {
            if (!containsText(value, query)) continue;
            shown.push_back(value);
            lotui::ListCell check;
            check.kind = lotui::ListCellKind::CheckBox;
            check.checked = checked.count(value) != 0;
            lotui::ListCell label;
            label.text = value.empty() ? "(Empty)" : value;
            rows.push_back({std::move(check), std::move(label)});
        }
        if (list) list->setRows(std::move(rows));
    }

    void setAll(bool selected) {
        checked.clear();
        if (selected) checked.insert(values.begin(), values.end());
        refresh();
    }
};

std::unique_ptr<lotui::Button> toolbarButton(
    const std::shared_ptr<const lotui::TextEngine>& textEngine,
    const lotui::TextStyle& textStyle,
    std::string title, float width,
    lotui::Button::ClickHandler onClick) {
    lotui::ButtonStyle style;
    style.normal = {0.17F, 0.22F, 0.29F, 1.0F};
    style.hovered = {0.23F, 0.31F, 0.42F, 1.0F};
    style.pressed = {0.13F, 0.40F, 0.66F, 1.0F};
    style.cornerRadius = 4.0F;
    style.contentPadding = {4.0F, 2.0F, 4.0F, 2.0F};
    auto label = std::make_unique<lotui::Label>(
        textEngine, std::move(title), textStyle);
    label->setVerticalAlignment(lotui::VerticalTextAlignment::Center);
    return std::make_unique<lotui::Button>(
        std::move(label), lotui::Size{width, 28.0F},
        std::move(onClick), style);
}

std::unique_ptr<lotui::WidgetTree> createUi(
    const std::shared_ptr<const lotui::TextEngine>& textEngine,
    lotui::PlatformWindow& window,
    std::shared_ptr<TableState>& state) {
    auto root = std::make_unique<lotui::Column>();
    root->setDecoration(lotui::BoxDecoration{
        {0.08F, 0.10F, 0.14F, 1.0F}, 8.0F});
    lotui::LinearLayoutOptions options;
    options.spacing = 8.0F;
    options.padding = {12.0F, 10.0F, 12.0F, 12.0F};
    options.crossAxisAlignment = lotui::CrossAxisAlignment::Stretch;
    root->setOptions(options);

    lotui::TextStyle headingStyle;
    headingStyle.fontFamilies = {"Noto Sans KR"};
    headingStyle.fontSize = 15.0F;
    headingStyle.weight = lotui::FontWeight::Medium;
    auto heading = std::make_unique<lotui::Label>(
        textEngine,
        "LotUI ListControl — drag or Shift+click to select a range",
        headingStyle);
    heading->setVerticalAlignment(lotui::VerticalTextAlignment::Center);
    root->addChild(
        std::move(heading),
        {0.0F, {0.0F, 24.0F}, {lotui::unboundedLayoutSize, 24.0F}});

    std::vector<lotui::ListColumn> columns{
        {"enabled", "Use", 54.0F, 44.0F, true},
        {"name", "Name", 148.0F, 80.0F, true},
        {"category", "Category", 112.0F, 72.0F, true},
        {"supplier", "Supplier", 142.0F, 80.0F, true},
        {"notes", "Notes", 180.0F, 100.0F, true},
        {"action", "", 68.0F, 52.0F, true, false, false},
    };
    std::vector<lotui::ListRow> rows;
    rows.push_back(makeRow(
        true, "Desk lamp", 1, "Northwind", "Warm white sample"));
    rows.push_back(makeRow(
        true, "Office chair", 2, "Contoso", "Ergonomic model"));
    rows.push_back(makeRow(
        false, "Notebook", 0, "Fabrikam", "Recycled paper"));
    rows.push_back(makeRow(
        true, "Standing desk", 2, "Adventure Works", "Electric frame"));
    rows.push_back(makeRow(
        true, "Pen set", 0, "Northwind", "Black and blue ink"));
    rows.push_back(makeRow(
        false, "Floor lamp", 1, "Contoso", "Showroom sample"));
    rows.push_back(makeRow(
        true, "Monitor arm", 2, "Fabrikam", "Dual display"));
    rows.push_back(makeRow(
        true, "Cable tray", 0, "Adventure Works", "Under-desk mount"));
    rows.push_back(makeRow(
        false, "Task light", 1, "Northwind", "USB-C powered"));
    rows.push_back(makeRow(
        true, "Foot rest", 2, "Contoso", "Height adjustable"));
    rows.push_back(makeRow(
        true, "Desk mat", 0, "Fabrikam", "Large format"));
    rows.push_back(makeRow(
        false, "Pendant lamp", 1, "Adventure Works", "Awaiting review"));

    lotui::ListControlStyle listStyle;
    listStyle.headerHeight = 25.0F;
    listStyle.rowHeight = 24.0F;
    listStyle.controlSize = 15.0F;
    listStyle.cellPadding = {6.0F, 2.0F, 6.0F, 2.0F};
    listStyle.cornerRadius = 5.0F;
    listStyle.focusRingWidth = 1.5F;

    lotui::TextStyle listTextStyle;
    listTextStyle.fontFamilies = {"Noto Sans KR"};
    listTextStyle.fontSize = 12.0F;

    auto hostPointer = std::make_shared<lotui::PopupHost*>(nullptr);
    auto listPointer = std::make_shared<lotui::ListControl*>(nullptr);
    state = std::make_shared<TableState>();
    state->rows = rows;
    state->columnFilters.resize(columns.size());
    auto list = std::make_unique<lotui::ListControl>(
        textEngine,
        std::move(columns),
        std::move(rows),
        lotui::Size{560.0F, 190.0F},
        [](std::optional<lotui::ListCellAddress> selected) {
            if (selected) {
                std::cout << "selected: row=" << selected->row
                          << " column=" << selected->column << '\n';
            }
        },
        [textEngine, hostPointer, listPointer, listStyle, listTextStyle](
            const lotui::ListCellEvent& event) {
            lotui::ListControl* control = *listPointer;
            lotui::PopupHost* host = *hostPointer;
            if (control == nullptr || host == nullptr) {
                return;
            }
            const lotui::ListCell* value = control->cell(event.address);
            if (value == nullptr) {
                return;
            }
            switch (event.action) {
            case lotui::ListCellAction::OpenComboBox:
                if (!value->options.empty()) {
                    lotui::ComboBoxStyle comboStyle;
                    comboStyle.itemHeight = listStyle.rowHeight;
                    comboStyle.maximumVisibleItems = 6;
                    comboStyle.popupCornerRadius = listStyle.cornerRadius;
                    lotui::ComboBox::showPopupAt(
                        *host,
                        textEngine,
                        event.anchor,
                        value->options,
                        value->selectedOption,
                        [listPointer, address = event.address](
                            std::size_t index) {
                            (*listPointer)->setComboSelection(address, index);
                        },
                        comboStyle,
                        listTextStyle);
                }
                break;
            case lotui::ListCellAction::OpenLookup:
                if (!value->options.empty()) {
                    std::vector<lotui::LookupItem> items;
                    items.reserve(value->options.size());
                    for (std::size_t index = 0;
                         index < value->options.size(); ++index) {
                        const std::string& option = value->options[index];
                        items.push_back({
                            "supplier-" + std::to_string(index + 1),
                            option,
                            index == value->options.size() - 1
                                ? "Seoul, Korea" : "Approved supplier",
                        });
                    }
                    std::optional<std::size_t> selected;
                    const auto found = std::find(
                        value->options.begin(), value->options.end(),
                        value->text);
                    if (found != value->options.end()) {
                        selected = static_cast<std::size_t>(
                            std::distance(value->options.begin(), found));
                    }
                    lotui::LookupBoxStyle lookupStyle;
                    lookupStyle.itemHeight = 32.0F;
                    lookupStyle.maximumVisibleItems = 5;
                    lookupStyle.popupPreferredWidth = 250.0F;
                    lookupStyle.popupCornerRadius = listStyle.cornerRadius;
                    lotui::LookupBox::showPopupAt(
                        *host,
                        textEngine,
                        event.anchor,
                        std::move(items),
                        selected,
                        [listPointer, address = event.address](
                            std::optional<std::size_t> index) {
                            if (!index || *listPointer == nullptr) {
                                return;
                            }
                            const lotui::ListCell* current =
                                (*listPointer)->cell(address);
                            if (current != nullptr &&
                                *index < current->options.size()) {
                                (*listPointer)->setCellText(
                                    address, current->options[*index]);
                            }
                        },
                        lookupStyle,
                        listTextStyle);
                }
                break;
            case lotui::ListCellAction::InvokeButton:
                control->setCellText(event.address, "Done");
                break;
            case lotui::ListCellAction::BeginEdit: {
                lotui::TextFieldStyle editorStyle;
                editorStyle.normal = listStyle.selectedCellFocused;
                editorStyle.hovered = listStyle.selectedCellFocused;
                editorStyle.text = listStyle.text;
                editorStyle.focusRing = listStyle.focusRing;
                editorStyle.contentPadding = listStyle.cellPadding;
                editorStyle.cornerRadius = 0.0F;
                editorStyle.focusRingWidth = 1.5F;
                lotui::InlineTextEditHandlers handlers;
                handlers.committed =
                    [listPointer, address = event.address](std::string text) {
                        if (*listPointer != nullptr) {
                            (*listPointer)->setCellText(
                                address, std::move(text));
                        }
                    };
                handlers.validate = [](std::string_view text)
                    -> std::optional<std::string> {
                    return text.empty()
                        ? std::optional<std::string>{
                            "Name must not be empty"}
                        : std::nullopt;
                };
                handlers.validationFailed = [](std::string message) {
                    std::cout << "validation: " << message << '\n';
                };
                lotui::InlineTextEditor::showAt(
                    *host,
                    textEngine,
                    event.anchor,
                    value->text,
                    std::move(handlers),
                    {},
                    editorStyle,
                    listTextStyle);
                break;
            }
            case lotui::ListCellAction::ToggleCheck:
            case lotui::ListCellAction::Activate:
                break;
            }
            std::cout << "cell-action: row=" << event.address.row
                      << " column=" << event.address.column
                      << " action=" << static_cast<int>(event.action) << '\n';
        },
        listStyle,
        listTextStyle);
    *listPointer = list.get();
    list->setClipboardHandlers(
        [&window](std::string text) {
            window.writeClipboardText(text);
        },
        [&window]() { return window.readClipboardText(); });
    list->setFrozenColumnCount(1);
    list->setOnSortChanged(
        [state](
            std::optional<lotui::ListSortDescriptor> descriptor) {
            if (!descriptor) {
                return;
            }
            state->record();
            std::stable_sort(
                state->rows.begin(),
                state->rows.end(),
                [descriptor](
                    const lotui::ListRow& left,
                    const lotui::ListRow& right) {
                    const std::string leftKey =
                        descriptor->column < left.size()
                        ? listCellSortKey(left[descriptor->column])
                        : std::string{};
                    const std::string rightKey =
                        descriptor->column < right.size()
                        ? listCellSortKey(right[descriptor->column])
                        : std::string{};
                    return descriptor->direction ==
                            lotui::ListSortDirection::Ascending
                        ? leftKey < rightKey
                        : rightKey < leftKey;
                });
            state->refresh();
        });
    list->setOnColumnResized(
        [](std::size_t column, float width) {
            std::cout << "column-resized: column=" << column
                      << " width=" << width << '\n';
        });
    list->setOnColumnReordered(
        [state](std::size_t from, std::size_t to) {
            for (auto& row : state->rows) {
                auto cell = std::move(row[from]);
                row.erase(row.begin() + static_cast<std::ptrdiff_t>(from));
                row.insert(row.begin() + static_cast<std::ptrdiff_t>(to),
                    std::move(cell));
            }
            auto filter = std::move(state->columnFilters[from]);
            state->columnFilters.erase(state->columnFilters.begin() +
                static_cast<std::ptrdiff_t>(from));
            state->columnFilters.insert(state->columnFilters.begin() +
                static_cast<std::ptrdiff_t>(to), std::move(filter));
            if (state->searchColumn == from) state->searchColumn = to;
            else if (from < state->searchColumn &&
                     to >= state->searchColumn) --state->searchColumn;
            else if (from > state->searchColumn &&
                     to <= state->searchColumn) ++state->searchColumn;
            state->undo.clear();
            state->redo.clear();
            std::vector<std::string> titles;
            for (const auto& column : state->list->columns()) {
                titles.push_back(column.title);
            }
            state->columnPicker->setOptions(std::move(titles));
            state->columnPicker->setSelectedIndex(state->searchColumn);
            state->refresh();
            std::cout << "column-reordered: from=" << from
                      << " to=" << to << '\n';
        });
    list->setOnHeaderFilterRequested(
        [state, textEngine, hostPointer, listTextStyle, &window](
            std::size_t column, lotui::Rect anchor) {
            if (*hostPointer == nullptr || column >= state->columnFilters.size()) {
                return;
            }
            std::set<std::string> values;
            for (const auto& row : state->rows) {
                if (column < row.size()) values.insert(listCellSortKey(row[column]));
            }
            auto popupState = std::make_shared<FilterPopupState>();
            popupState->values.assign(values.begin(), values.end());
            popupState->checked = state->columnFilters[column]
                ? *state->columnFilters[column] : values;

            auto panel = std::make_unique<lotui::Column>();
            panel->setDecoration(lotui::BoxDecoration{
                {0.12F, 0.15F, 0.20F, 1.0F}, 4.0F});
            lotui::LinearLayoutOptions panelOptions;
            panelOptions.spacing = 5.0F;
            panelOptions.padding = {6.0F, 6.0F, 6.0F, 6.0F};
            panelOptions.mainAxisSize = lotui::MainAxisSize::Min;
            panelOptions.crossAxisAlignment = lotui::CrossAxisAlignment::Stretch;
            panel->setOptions(panelOptions);

            auto searchRow = std::make_unique<lotui::Row>();
            lotui::LinearLayoutOptions searchOptions;
            searchOptions.spacing = 5.0F;
            searchOptions.crossAxisAlignment = lotui::CrossAxisAlignment::Center;
            searchRow->setOptions(searchOptions);
            auto searchLabel = std::make_unique<lotui::Label>(
                textEngine, "Search", listTextStyle);
            searchLabel->setVerticalAlignment(
                lotui::VerticalTextAlignment::Center);
            searchRow->addChild(std::move(searchLabel),
                {0.0F, {50.0F, 28.0F}, {50.0F, 28.0F}});
            lotui::TextFieldStyle searchStyle;
            searchStyle.normal = {0.07F, 0.09F, 0.13F, 1.0F};
            searchStyle.hovered = {0.10F, 0.13F, 0.18F, 1.0F};
            searchStyle.contentPadding = {6.0F, 2.0F, 6.0F, 2.0F};
            searchStyle.cornerRadius = 4.0F;
            auto search = std::make_unique<lotui::TextField>(
                textEngine, "", lotui::Size{170.0F, 28.0F},
                [popupState](const std::string& query) {
                    popupState->query = query;
                    popupState->refresh();
                }, lotui::TextField::SubmittedHandler{},
                searchStyle, listTextStyle);
            search->setClipboardHandlers(
                [&window](std::string text) {
                    return window.writeClipboardText(text);
                },
                [&window]() { return window.readClipboardText(); });
            searchRow->addChild(std::move(search),
                {1.0F, {170.0F, 28.0F},
                    {lotui::unboundedLayoutSize, 28.0F}});
            panel->addChild(std::move(searchRow),
                {0.0F, {230.0F, 28.0F}, {230.0F, 28.0F}});

            lotui::ListControlStyle valueStyle;
            valueStyle.headerHeight = 1.0F;
            valueStyle.rowHeight = 24.0F;
            valueStyle.controlSize = 15.0F;
            valueStyle.cellPadding = {5.0F, 2.0F, 5.0F, 2.0F};
            valueStyle.cornerRadius = 3.0F;
            valueStyle.focusRingWidth = 0.0F;
            const float listHeight = std::max(28.0F,
                5.0F + 24.0F * static_cast<float>(
                    std::min<std::size_t>(8, popupState->values.size())));
            auto valuesList = std::make_unique<lotui::ListControl>(
                textEngine,
                std::vector<lotui::ListColumn>{
                    {"checked", "", 26.0F, 26.0F, false, false, false},
                    {"value", "", 204.0F, 100.0F, false, false, false},
                },
                std::vector<lotui::ListRow>{},
                lotui::Size{230.0F, listHeight},
                lotui::ListControl::SelectionChangedHandler{},
                [popupState](const lotui::ListCellEvent& event) {
                    if (event.address.row >= popupState->shown.size()) return;
                    if (event.action != lotui::ListCellAction::ToggleCheck &&
                        event.action != lotui::ListCellAction::Activate) return;
                    const std::string& value =
                        popupState->shown[event.address.row];
                    const bool checked =
                        event.action == lotui::ListCellAction::ToggleCheck
                        ? popupState->list->cell({event.address.row, 0})->checked
                        : popupState->checked.count(value) == 0;
                    if (checked) popupState->checked.insert(value);
                    else popupState->checked.erase(value);
                    popupState->list->setChecked({event.address.row, 0}, checked);
                }, valueStyle, listTextStyle);
            popupState->list = valuesList.get();
            popupState->refresh();
            panel->addChild(std::move(valuesList),
                {0.0F, {230.0F, listHeight}, {230.0F, listHeight}});

            auto actions = std::make_unique<lotui::Row>();
            lotui::LinearLayoutOptions actionOptions;
            actionOptions.spacing = 5.0F;
            actionOptions.crossAxisAlignment = lotui::CrossAxisAlignment::Center;
            actions->setOptions(actionOptions);
            actions->addChild(toolbarButton(textEngine, listTextStyle,
                "All", 45.0F,
                [popupState]() { popupState->setAll(true); }));
            actions->addChild(toolbarButton(textEngine, listTextStyle,
                "None", 50.0F,
                [popupState]() { popupState->setAll(false); }));
            actions->addChild(toolbarButton(textEngine, listTextStyle,
                "Apply", 60.0F,
                [state, popupState, hostPointer, column]() {
                    state->columnFilters[column] =
                        popupState->checked.size() == popupState->values.size()
                        ? std::nullopt
                        : std::optional<std::set<std::string>>{
                            popupState->checked};
                    state->list->setHeaderFilterActive(column,
                        state->columnFilters[column].has_value());
                    state->refresh();
                    (*hostPointer)->acceptPopup();
                }));
            actions->addChild(toolbarButton(textEngine, listTextStyle,
                "Cancel", 60.0F,
                [hostPointer]() { (*hostPointer)->dismissPopup(); }));
            panel->addChild(std::move(actions),
                {0.0F, {230.0F, 28.0F}, {230.0F, 28.0F}});
            const lotui::Rect header = state->list->headerCellBounds(column);
            anchor.x = header.x;
            anchor.width = header.width;
            lotui::PopupOptions options;
            options.matchAnchorWidth = false;
            (*hostPointer)->showPopup(
                std::move(panel), anchor, options);
        });
    list->setOnSelectionRangeChanged(
        [](std::optional<lotui::ListCellRange> range) {
            if (range) {
                std::cout << "selected-range: "
                          << range->first.row << ',' << range->first.column
                          << " -> "
                          << range->last.row << ',' << range->last.column
                          << '\n';
            }
        });
    state->list = list.get();
    auto toolbar = std::make_unique<lotui::Row>();
    lotui::LinearLayoutOptions toolbarOptions;
    toolbarOptions.spacing = 5.0F;
    toolbarOptions.crossAxisAlignment = lotui::CrossAxisAlignment::Center;
    toolbar->setOptions(toolbarOptions);
    auto undoButton = toolbarButton(textEngine, listTextStyle, "Undo", 51.0F,
        [state]() { state->stepBack(true); });
    state->undoButton = undoButton.get();
    toolbar->addChild(std::move(undoButton));
    auto redoButton = toolbarButton(textEngine, listTextStyle, "Redo", 51.0F,
        [state]() { state->stepBack(false); });
    state->redoButton = redoButton.get();
    toolbar->addChild(std::move(redoButton));
    toolbar->addChild(toolbarButton(textEngine, listTextStyle,
        "+ Row", 62.0F, [state]() { state->addRow(); }));
    auto deleteButton = toolbarButton(textEngine, listTextStyle,
        "- Row", 62.0F, [state]() { state->deleteRow(); });
    state->deleteButton = deleteButton.get();
    toolbar->addChild(std::move(deleteButton));
    lotui::ComboBoxStyle columnStyle;
    columnStyle.itemHeight = 26.0F;
    columnStyle.cornerRadius = 4.0F;
    columnStyle.contentPadding = {6.0F, 2.0F, 4.0F, 2.0F};
    auto columnPicker = std::make_unique<lotui::ComboBox>(
        textEngine,
        std::vector<std::string>{"Use", "Name", "Category", "Supplier", "Notes"},
        1, lotui::Size{116.0F, 28.0F},
        [state](std::size_t index) {
            state->searchColumn = index;
        }, columnStyle, listTextStyle);
    state->columnPicker = columnPicker.get();
    toolbar->addChild(std::move(columnPicker));
    lotui::TextFieldStyle searchStyle;
    searchStyle.contentPadding = {7.0F, 2.0F, 7.0F, 2.0F};
    searchStyle.cornerRadius = 4.0F;
    auto searchField = std::make_unique<lotui::TextField>(
        textEngine, "", lotui::Size{180.0F, 28.0F},
        [state](const std::string& value) {
            state->query = value;
        },
        [state](const std::string&) { state->findNext(); },
        searchStyle, listTextStyle);
    searchField->setClipboardHandlers(
        [&window](std::string text) { return window.writeClipboardText(text); },
        [&window]() { return window.readClipboardText(); });
    toolbar->addChild(std::move(searchField));
    toolbar->addChild(toolbarButton(textEngine, listTextStyle,
        "Find", 46.0F, [state]() { state->findNext(); }));
    root->addChild(std::move(toolbar),
        {0.0F, {0.0F, 28.0F}, {lotui::unboundedLayoutSize, 28.0F}});
    state->updateButtons();
    root->addChild(
        std::move(list),
        {1.0F, {0.0F, 120.0F},
            {lotui::unboundedLayoutSize, lotui::unboundedLayoutSize}});
    auto popupHost = std::make_unique<lotui::PopupHost>(std::move(root));
    *hostPointer = popupHost.get();
    state->columnPicker->setPopupHost(popupHost.get());
    state->refresh();
    return std::make_unique<lotui::WidgetTree>(std::move(popupHost));
}

void updateLayout(
    lotui::WidgetTree& tree,
    const lotui::WindowMetrics& metrics) {
    const float scale = metrics.dpiScale > 0.0F ? metrics.dpiScale : 1.0F;
    const float width =
        static_cast<float>(metrics.framebufferWidth) / scale;
    const float height =
        static_cast<float>(metrics.framebufferHeight) / scale;
    tree.layout(
        {10.0F, 10.0F, std::max(0.0F, width - 20.0F),
            std::max(0.0F, height - 20.0F)},
        {0.0F, 0.0F, width, height});
}

void testTableState() {
    TableState state;
    state.rows.push_back(makeRow(true, "Desk lamp", 1, "Northwind", "Sample"));
    state.rows.push_back(makeRow(false, "Office chair", 2, "Contoso", ""));
    state.query = "LAMP";
    if (!state.matches(state.rows[0]) || state.matches(state.rows[1])) {
        throw std::runtime_error("column search failed");
    }
    state.searchColumn = 3;
    state.query = "conto";
    if (state.matches(state.rows[0]) || !state.matches(state.rows[1])) {
        throw std::runtime_error("column filter failed");
    }
    state.columnFilters.resize(6);
    state.columnFilters[2] = std::set<std::string>{"Furniture"};
    if (state.matchesFilters(state.rows[0]) ||
        !state.matchesFilters(state.rows[1])) {
        throw std::runtime_error("header filter failed");
    }
    state.columnFilters[2] = std::set<std::string>{"Furniture", "Lighting"};
    if (!state.matchesFilters(state.rows[0]) ||
        !state.matchesFilters(state.rows[1])) {
        throw std::runtime_error("multi-value filter failed");
    }
    FilterPopupState popup;
    popup.values = {"Office", "Furniture", "Lighting"};
    popup.setAll(true);
    popup.query = "furn";
    popup.refresh();
    if (popup.shown != std::vector<std::string>{"Furniture"}) {
        throw std::runtime_error("filter value search failed");
    }
    popup.setAll(false);
    if (!popup.checked.empty()) {
        throw std::runtime_error("clear filter checkboxes failed");
    }
    state.addRow();
    if (state.rows.size() != 3) throw std::runtime_error("add row failed");
    state.stepBack(true);
    if (state.rows.size() != 2) throw std::runtime_error("undo failed");
    state.stepBack(false);
    if (state.rows.size() != 3) throw std::runtime_error("redo failed");
    state.eraseRow(1);
    if (state.rows.size() != 2 ||
        state.rows[1][1].text != "New item 1") {
        throw std::runtime_error("delete row failed");
    }
    state.stepBack(true);
    if (state.rows.size() != 3 ||
        state.rows[1][1].text != "Office chair") {
        throw std::runtime_error("delete undo failed");
    }
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc > 1 && std::string_view(argv[1]) == "--self-test") {
            testTableState();
            return 0;
        }
        auto platform = lotui::createPlatformBackend();
        auto window = platform->createWindow(
            {"LotUI ListControl", 820, 430, true});
        window->show();

        lotui::VulkanRenderer renderer(*window);
        const auto fontFile = lotui::executableDirectory() /
            "resources" / "fonts" / "NotoSansKR-Regular.ttf";
        auto textEngine = std::make_shared<lotui::FreetypeTextEngine>(
            renderer,
            std::vector<lotui::FontSource>{
                {"Noto Sans KR", fontFile}});
        std::shared_ptr<TableState> state;
        auto tree = createUi(textEngine, *window, state);
        updateLayout(*tree, window->metrics());
        if (argc > 1 && std::string_view(argv[1]) == "--preview-filter") {
            const lotui::Rect filter = state->list->headerFilterBounds(2);
            const lotui::Point point{
                filter.x + filter.width * 0.5F,
                filter.y + filter.height * 0.5F,
            };
            tree->pointerPressed(point, lotui::PointerButton::Primary);
            tree->pointerReleased(point, lotui::PointerButton::Primary);
        }

        std::vector<lotui::PaintCommand> commands;
        bool running = true;
        while (running) {
            bool receivedEvent = false;
            lotui::PlatformEvent event{};
            while (window->pollEvent(event)) {
                receivedEvent = true;
                running = lotui::example::dispatchPlatformEvent(
                    event, *tree, *window, updateLayout) && running;
                state->syncEdits();
            }
            if (running) {
                commands.clear();
                tree->paint(commands);
                renderer.drawFrame(commands);
            }
            if (!receivedEvent) {
                std::this_thread::sleep_for(std::chrono::milliseconds(8));
            }
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "list_control_demo failed: " << error.what() << '\n';
        return 1;
    }
}
