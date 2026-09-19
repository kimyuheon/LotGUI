#include "examples/platform_probe/platform_event_bridge.h"

#include "platform/platform_backend.h"
#include "platform/runtime_paths.h"
#include "renderer/vulkan/vulkan_renderer.h"
#include "text/freetype/freetype_text_engine.h"
#include "widgets/combo_box.h"
#include "widgets/inline_text_editor.h"
#include "widgets/label.h"
#include "widgets/linear_layout.h"
#include "widgets/list_control.h"
#include "widgets/lookup_box.h"
#include "widgets/popup.h"

#include <algorithm>
#include <chrono>
#include <exception>
#include <iostream>
#include <memory>
#include <string>
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

std::unique_ptr<lotui::WidgetTree> createUi(
    const std::shared_ptr<const lotui::TextEngine>& textEngine) {
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
        textEngine, "LotUI ListControl — compact example", headingStyle);
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
        {"action", "", 68.0F, 52.0F, true, false},
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
    list->setOnSortChanged(
        [listPointer](
            std::optional<lotui::ListSortDescriptor> descriptor) {
            if (*listPointer == nullptr || !descriptor) {
                return;
            }
            lotui::ListControl* control = *listPointer;
            std::vector<lotui::ListRow> sorted = control->rows();
            std::stable_sort(
                sorted.begin(),
                sorted.end(),
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
            control->setRows(std::move(sorted));
        });
    list->setOnColumnResized(
        [](std::size_t column, float width) {
            std::cout << "column-resized: column=" << column
                      << " width=" << width << '\n';
        });
    root->addChild(
        std::move(list),
        {1.0F, {0.0F, 120.0F},
            {lotui::unboundedLayoutSize, lotui::unboundedLayoutSize}});
    auto popupHost = std::make_unique<lotui::PopupHost>(std::move(root));
    *hostPointer = popupHost.get();
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

} // namespace

int main() {
    try {
        auto platform = lotui::createPlatformBackend();
        auto window = platform->createWindow(
            {"LotUI ListControl", 640, 300, true});
        window->show();

        lotui::VulkanRenderer renderer(*window);
        const auto fontFile = lotui::executableDirectory() /
            "resources" / "fonts" / "NotoSansKR-Regular.ttf";
        auto textEngine = std::make_shared<lotui::FreetypeTextEngine>(
            renderer,
            std::vector<lotui::FontSource>{
                {"Noto Sans KR", fontFile}});
        auto tree = createUi(textEngine);
        updateLayout(*tree, window->metrics());

        std::vector<lotui::PaintCommand> commands;
        bool running = true;
        while (running) {
            bool receivedEvent = false;
            lotui::PlatformEvent event{};
            while (window->pollEvent(event)) {
                receivedEvent = true;
                running = lotui::example::dispatchPlatformEvent(
                    event, *tree, *window, updateLayout) && running;
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
