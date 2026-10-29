#include "examples/platform_probe/platform_event_bridge.h"
#include "examples/ribbon_demo/plugin_loader.h"
#include "examples/ribbon_demo/plugin_tab.h"

#include "core/widget_tree.h"
#include "platform/platform_backend.h"
#include "platform/runtime_paths.h"
#include "renderer/vulkan/vulkan_renderer.h"
#include "text/freetype/freetype_text_engine.h"
#include "widgets/button.h"
#include "widgets/dialog.h"
#include "widgets/label.h"
#include "widgets/linear_layout.h"
#include "widgets/list_control.h"
#include "widgets/ribbon.h"
#include "widgets/text_field.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <exception>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace {

constexpr float ribbonHeight = 134.0F;

lotui::ChildLayout fixedHeight(float height) {
    return {0.0F, {0.0F, height},
        {lotui::unboundedLayoutSize, height}};
}

lotui::TextStyle textStyle(float size) {
    lotui::TextStyle style;
    style.fontFamilies = {"Noto Sans KR"};
    style.fontSize = size;
    return style;
}

std::unique_ptr<lotui::Button> commandButton(
    const std::shared_ptr<const lotui::TextEngine>& textEngine,
    std::string title,
    lotui::Button::ClickHandler action,
    float width,
    std::string_view icon = {},
    bool prominent = false) {
    lotui::ButtonStyle style;
    style.normal = {0.0F, 0.0F, 0.0F, 0.0F};
    style.hovered = {0.22F, 0.29F, 0.36F, 0.6F};
    style.pressed = {0.18F, 0.26F, 0.34F, 0.8F};
    style.disabled = {0.0F, 0.0F, 0.0F, 0.0F};
    style.cornerRadius = 2.0F;
    style.contentPadding = lotui::EdgeInsets::all(2.0F);
    auto label = std::make_unique<lotui::Label>(
        textEngine, std::move(title), textStyle(prominent ? 13.0F : 14.0F));
    label->setHorizontalAlignment(prominent
        ? lotui::HorizontalTextAlignment::Center
        : lotui::HorizontalTextAlignment::Start);
    label->setVerticalAlignment(lotui::VerticalTextAlignment::Center);
    std::unique_ptr<lotui::Widget> content = std::move(label);
    if (!icon.empty()) {
        auto iconStyle = textStyle(prominent ? 29.0F : 17.0F);
        iconStyle.fontFamilies = {"Lucide"};
        auto symbol = std::make_unique<lotui::Label>(
            textEngine, std::string(icon), iconStyle);
        symbol->setColor({0.55F, 0.75F, 0.95F, 1.0F});
        symbol->setHorizontalAlignment(
            lotui::HorizontalTextAlignment::Center);
        symbol->setVerticalAlignment(
            lotui::VerticalTextAlignment::Center);
        lotui::LinearLayoutOptions options;
        options.mainAxisSize = prominent
            ? lotui::MainAxisSize::Min
            : lotui::MainAxisSize::Max;
        options.crossAxisAlignment = lotui::CrossAxisAlignment::Center;
        if (prominent) {
            options.spacing = 4.0F;
            auto column = std::make_unique<lotui::Column>();
            column->setOptions(options);
            column->addChild(std::move(symbol),
                {0.0F, {32.0F, 34.0F}, {32.0F, 34.0F}});
            column->addChild(std::move(content));
            content = std::move(column);
        } else {
            options.spacing = 4.0F;
            auto row = std::make_unique<lotui::Row>();
            row->setOptions(options);
            row->addChild(std::move(symbol),
                {0.0F, {18.0F, 19.0F}, {18.0F, 19.0F}});
            row->addChild(std::move(content));
            content = std::move(row);
        }
    }
    return std::make_unique<lotui::Button>(
        std::move(content), lotui::Size{width, prominent ? 76.0F : 24.0F},
        std::move(action), style);
}

lotui::ListRow makeRow(std::string name, std::string type,
    std::string layer, std::string status, bool visible = true) {
    lotui::ListCell check;
    check.kind = lotui::ListCellKind::CheckBox;
    check.checked = visible;
    return {std::move(check),
        {lotui::ListCellKind::Text, std::move(name)},
        {lotui::ListCellKind::Text, std::move(type)},
        {lotui::ListCellKind::Text, std::move(layer)},
        {lotui::ListCellKind::Text, std::move(status)}};
}

struct DemoState {
    std::shared_ptr<const lotui::TextEngine> textEngine;
    lotui::DialogHost* dialogs{nullptr};
    lotui::Ribbon* ribbon{nullptr};
    lotui::Label* status{nullptr};
    lotui::Label* count{nullptr};
    lotui::ListControl* list{nullptr};
    std::vector<lotui::Widget*> pluginControls;
    std::vector<lotui::ListRow> items{
        makeRow("Assembly A", "Assembly", "Architecture", "Ready"),
        makeRow("North elevation", "View", "Drawings", "Review"),
        makeRow("Column grid", "Sketch", "Structure", "Ready"),
        makeRow("Foundation", "Solid", "Structure", "Ready"),
        makeRow("MEP routing", "Assembly", "Services", "Review", false),
        makeRow("Door schedule", "Table", "Architecture", "Ready"),
        makeRow("Site boundary", "Sketch", "Survey", "Ready"),
        makeRow("Material study", "View", "Presentation", "Draft")};
    std::vector<std::size_t> visibleIndices;
    std::vector<std::vector<lotui::ListRow>> undoStack;
    std::vector<std::vector<lotui::ListRow>> redoStack;
    bool visibleOnly{false};
    bool snap{false};
    double size{35.0};
    int nextItem{1};

    std::optional<std::size_t> selectedItem() const {
        const auto selection = list->selectedCell();
        if (!selection || selection->row >= visibleIndices.size()) {
            return std::nullopt;
        }
        return visibleIndices[selection->row];
    }

    void refreshList(std::optional<std::size_t> selected = std::nullopt) {
        visibleIndices.clear();
        std::vector<lotui::ListRow> rows;
        for (std::size_t index = 0; index < items.size(); ++index) {
            if (!visibleOnly || items[index][0].checked) {
                visibleIndices.push_back(index);
                rows.push_back(items[index]);
            }
        }
        list->setRows(std::move(rows));
        if (selected) {
            const auto found = std::find(
                visibleIndices.begin(), visibleIndices.end(), *selected);
            if (found != visibleIndices.end()) {
                list->setSelectedCell(lotui::ListCellAddress{
                    static_cast<std::size_t>(
                        found - visibleIndices.begin()), 1});
            } else {
                list->setSelectedCell(std::nullopt);
            }
        } else {
            list->setSelectedCell(std::nullopt);
        }
        count->setText(std::to_string(visibleIndices.size()) + " / " +
            std::to_string(items.size()) + " items");
    }

    void recordEdit() {
        undoStack.push_back(items);
        redoStack.clear();
    }

    void addItem() {
        insertItem("New item", "Object", "Default");
        status->setText("항목 추가");
    }

    void insertItem(std::string name, std::string type,
        std::string layer) {
        recordEdit();
        items.push_back(makeRow(name + " " + std::to_string(nextItem++),
            std::move(type), std::move(layer), "Draft"));
        refreshList(items.size() - 1);
        status->setText("항목 추가");
    }

    void setSelectedVisible(bool visible) {
        const auto index = selectedItem();
        if (!index) {
            status->setText("항목을 선택하세요");
            return;
        }
        if (items[*index][0].checked == visible) {
            status->setText(visible ? "이미 표시 중입니다" : "이미 숨겨져 있습니다");
            return;
        }
        recordEdit();
        items[*index][0].checked = visible;
        refreshList(*index);
        status->setText(visible ? "항목 표시" : "항목 숨김");
    }

    void showAllItems() {
        if (std::all_of(items.begin(), items.end(),
                [](const auto& row) { return row[0].checked; })) {
            status->setText("모든 항목이 표시 중입니다");
            return;
        }
        recordEdit();
        for (auto& row : items) {
            row[0].checked = true;
        }
        refreshList();
        status->setText("모든 항목 표시");
    }

    void duplicateItem() {
        const auto index = selectedItem();
        if (!index) {
            status->setText("복제할 항목을 선택하세요");
            return;
        }
        recordEdit();
        auto copy = items[*index];
        copy[1].text += " copy";
        items.insert(items.begin() + static_cast<std::ptrdiff_t>(*index + 1),
            std::move(copy));
        refreshList(*index + 1);
        status->setText("항목 복제");
    }

    void removeItem() {
        const auto index = selectedItem();
        if (!index) {
            status->setText("삭제할 항목을 선택하세요");
            return;
        }
        recordEdit();
        items.erase(items.begin() + static_cast<std::ptrdiff_t>(*index));
        refreshList();
        status->setText("항목 삭제");
    }

    void undo() {
        if (undoStack.empty()) {
            status->setText("되돌릴 작업이 없습니다");
            return;
        }
        redoStack.push_back(items);
        items = std::move(undoStack.back());
        undoStack.pop_back();
        refreshList();
        status->setText("실행 취소");
    }

    void redo() {
        if (redoStack.empty()) {
            status->setText("다시 실행할 작업이 없습니다");
            return;
        }
        undoStack.push_back(items);
        items = std::move(redoStack.back());
        redoStack.pop_back();
        refreshList();
        status->setText("다시 실행");
    }

    void sortByName(bool ascending) {
        std::stable_sort(items.begin(), items.end(),
            [ascending](const auto& left, const auto& right) {
                return ascending ? left[1].text < right[1].text
                                 : left[1].text > right[1].text;
            });
        list->setSortDescriptor(lotui::ListSortDescriptor{1,
            ascending ? lotui::ListSortDirection::Ascending
                      : lotui::ListSortDirection::Descending});
        refreshList();
        status->setText(ascending ? "이름 오름차순" : "이름 내림차순");
    }

    void setDensity(float height, const char* message) {
        auto style = list->style();
        style.rowHeight = height;
        list->setStyle(style);
        status->setText(message);
    }

    void refreshPluginSettings() {
        auto style = list->style();
        style.rowHeight = 22.0F + static_cast<float>(size * 0.12);
        list->setStyle(style);
        list->setFrozenColumnCount(snap ? 2 : 0);
    }

    void showDialog() {
        auto close = commandButton(textEngine, "닫기",
            [this] { dialogs->dismissModal(); }, 76.0F);
        auto dialog = std::make_unique<lotui::Dialog>(
            std::move(close), lotui::Size{260.0F, 142.0F});
        dialog->setTitle(std::make_unique<lotui::Label>(
            textEngine, "LotUI 대화상자", textStyle(14.0F)));
        dialogs->showModal(std::move(dialog));
    }

    void showSelectionDetails() {
        const auto index = selectedItem();
        if (!index) {
            status->setText("정보를 볼 항목을 선택하세요");
            return;
        }
        const auto& row = items[*index];
        auto content = std::make_unique<lotui::Column>();
        lotui::LinearLayoutOptions options;
        options.spacing = 5.0F;
        options.crossAxisAlignment = lotui::CrossAxisAlignment::Stretch;
        content->setOptions(options);
        content->addChild(std::make_unique<lotui::Label>(
            textEngine, "종류: " + row[2].text, textStyle(14.0F)),
            fixedHeight(23.0F));
        content->addChild(std::make_unique<lotui::Label>(
            textEngine, "레이어: " + row[3].text, textStyle(14.0F)),
            fixedHeight(23.0F));
        content->addChild(std::make_unique<lotui::Label>(
            textEngine, "상태: " + row[4].text, textStyle(14.0F)),
            fixedHeight(23.0F));
        content->addChild(commandButton(textEngine, "닫기",
            [this] { dialogs->dismissModal(); }, 70.0F),
            fixedHeight(28.0F));
        auto dialog = std::make_unique<lotui::Dialog>(
            std::move(content), lotui::Size{300.0F, 220.0F});
        dialog->setTitle(std::make_unique<lotui::Label>(
            textEngine, row[1].text, textStyle(15.0F)));
        dialogs->showModal(std::move(dialog));
    }
};

struct RibbonAction {
    std::string title;
    std::string_view icon;
    lotui::Button::ClickHandler action;
    bool prominent{false};
};

std::unique_ptr<lotui::Row> ribbonPanels() {
    auto panels = std::make_unique<lotui::Row>();
    lotui::LinearLayoutOptions options;
    options.crossAxisAlignment = lotui::CrossAxisAlignment::Stretch;
    panels->setOptions(options);
    return panels;
}

std::unique_ptr<lotui::RibbonGroup> commandGroup(
    DemoState& state, std::string title,
    std::vector<std::vector<RibbonAction>> columns) {
    auto controls = std::make_unique<lotui::Row>();
    lotui::LinearLayoutOptions options;
    options.spacing = 4.0F;
    options.mainAxisSize = lotui::MainAxisSize::Min;
    options.crossAxisAlignment = lotui::CrossAxisAlignment::Start;
    controls->setOptions(options);
    for (auto& columnActions : columns) {
        auto column = std::make_unique<lotui::Column>();
        lotui::LinearLayoutOptions columnOptions;
        columnOptions.spacing = 2.0F;
        columnOptions.mainAxisSize = lotui::MainAxisSize::Min;
        columnOptions.crossAxisAlignment = lotui::CrossAxisAlignment::Stretch;
        column->setOptions(columnOptions);
        for (auto& action : columnActions) {
            const float commandWidth = action.prominent ? 68.0F : 88.0F;
            column->addChild(commandButton(state.textEngine,
                std::move(action.title), std::move(action.action),
                commandWidth, action.icon, action.prominent),
                {0.0F, {}, {commandWidth, lotui::unboundedLayoutSize}});
        }
        controls->addChild(std::move(column));
    }
    lotui::RibbonGroupStyle style;
    style.cornerRadius = 0.0F;
    style.background = {0.0F, 0.0F, 0.0F, 0.0F};
    style.titleHeight = 18.0F;
    style.contentPadding = lotui::EdgeInsets::all(2.0F);
    style.titleColor = {0.85F, 0.89F, 0.88F, 1.0F};
    return std::make_unique<lotui::RibbonGroup>(state.textEngine,
        std::move(title), std::move(controls), style, textStyle(13.0F));
}

std::unique_ptr<lotui::WidgetTree> createUi(
    DemoState& state,
    lotui::PlatformWindow& window,
    const LotuiDemoPluginV1& pluginDescriptor,
    const LotuiDemoPluginV2* textExtension) {
    auto root = std::make_unique<lotui::Column>();
    root->setDecoration(lotui::BoxDecoration{
        {0.10F, 0.12F, 0.14F, 1.0F}, 0.0F});
    lotui::LinearLayoutOptions rootOptions;
    rootOptions.crossAxisAlignment = lotui::CrossAxisAlignment::Stretch;
    root->setOptions(rootOptions);

    lotui::RibbonStyle ribbonStyle;
    ribbonStyle.preferredHeight = ribbonHeight;
    ribbonStyle.tabBarHeight = 27.0F;
    ribbonStyle.contentPadding = lotui::EdgeInsets::all(3.0F);
    ribbonStyle.tabHorizontalPadding = 11.0F;
    ribbonStyle.minimumTabWidth = 82.0F;
    ribbonStyle.tabCornerRadius = 2.0F;
    ribbonStyle.tabSelected = {0.14F, 0.20F, 0.27F, 1.0F};
    ribbonStyle.text = {0.91F, 0.94F, 0.95F, 1.0F};
    auto ribbon = std::make_unique<lotui::Ribbon>(
        state.textEngine, ribbonStyle, textStyle(13.0F));
    state.ribbon = ribbon.get();

    // Lucide 1.55.0 glyphs are bundled in the small example font subset.
    auto groups = ribbonPanels();
    groups->addChild(commandGroup(state, "편집", {
        {{"취소", u8"\uE2A1", [&state] { state.undo(); }},
         {"다시", u8"\uE2A0", [&state] { state.redo(); }}}}));
    groups->addChild(commandGroup(state, "항목", {
        {{"추가", u8"\uE13D", [&state] { state.addItem(); }, true}},
        {{"복제", u8"\uE09E", [&state] { state.duplicateItem(); }},
         {"삭제", u8"\uE18E", [&state] { state.removeItem(); }}}}));
    groups->addChild(commandGroup(state, "삽입", {
        {{"조립품", u8"\uE2D0", [&state] {
              state.insertItem("Assembly", "Assembly", "Architecture");
          }, true}},
        {{"스케치", u8"\uE4F1", [&state] {
              state.insertItem("Sketch", "Sketch", "Structure");
          }},
         {"뷰", u8"\uE0BA", [&state] {
              state.insertItem("View", "View", "Drawings");
          }},
         {"표", u8"\uE2F9", [&state] {
              state.insertItem("Table", "Table", "Architecture");
          }}}}));
    groups->addChild(commandGroup(state, "창", {
        {{"대화상자", u8"\uE172",
            [&state] { state.showDialog(); }, true}}}));
    ribbon->addTab(std::make_unique<lotui::RibbonTab>(
        "home", "홈", std::move(groups)));

    auto view = ribbonPanels();
    view->addChild(commandGroup(state, "행 간격", {
        {{"촘촘", u8"\uE58A",
            [&state] { state.setDensity(23.0F, "촘촘한 목록"); }},
         {"보통", u8"\uE106",
            [&state] { state.setDensity(28.0F, "보통 목록"); }}}}));
    view->addChild(commandGroup(state, "열", {
        {{"고정", u8"\uE259", [&state] {
            state.list->setFrozenColumnCount(2);
            state.status->setText("이름 열 고정");
        }},
         {"해제", u8"\uE2B6", [&state] {
            state.list->setFrozenColumnCount(0);
            state.status->setText("열 고정 해제");
        }}}}));
    ribbon->addTab(std::make_unique<lotui::RibbonTab>(
        "view", "보기", std::move(view)));

    auto data = ribbonPanels();
    data->addChild(commandGroup(state, "이름 정렬", {
        {{"오름차순", u8"\uE41A",
            [&state] { state.sortByName(true); }},
         {"내림차순", u8"\uE416",
            [&state] { state.sortByName(false); }}}}));
    data->addChild(commandGroup(state, "표시", {
        {{"전체", u8"\uE106", [&state] {
            state.visibleOnly = false;
            state.refreshList();
            state.status->setText("전체 항목");
        }},
         {"표시만", u8"\uE0DC", [&state] {
            state.visibleOnly = true;
            state.refreshList();
            state.status->setText("표시 항목만");
        }}}}));
    data->addChild(commandGroup(state, "검사", {
        {{"정보", u8"\uE0F9", [&state] {
            state.showSelectionDetails();
        }},
         {"숨기기", u8"\uE0BB", [&state] {
            state.setSelectedVisible(false);
        }},
         {"모두 표시", u8"\uE0BA", [&state] {
            state.showAllItems();
        }}}}));
    ribbon->addTab(std::make_unique<lotui::RibbonTab>(
        "data", "데이터", std::move(data)));

    state.pluginControls = lotui::example::addPluginTab(
        *ribbon, state.textEngine,
        pluginDescriptor, textExtension,
        {[&state](std::string_view id, double value) {
            if (id == "run") {
                state.addItem();
                state.status->setText("플러그인 실행");
            } else if (id == "snap") {
                state.snap = value != 0.0;
                state.status->setText(
                    state.snap ? "열 고정 켜짐" : "열 고정 꺼짐");
                state.refreshPluginSettings();
            } else if (id == "size") {
                state.size = value;
                state.status->setText("크기 " + std::to_string(
                    static_cast<int>(value)));
                state.refreshPluginSettings();
            }
        },
        [&state](std::string_view id, std::string_view value) {
            if (id == "name") {
                state.status->setText("플러그인 이름: " + std::string(value));
            }
        },
        [&window](std::string value) {
            return window.writeClipboardText(std::move(value));
        },
        [&window]() { return window.readClipboardText(); }});
    ribbon->selectTab("home");
    root->addChild(std::move(ribbon), fixedHeight(ribbonHeight));

    auto workspace = std::make_unique<lotui::Column>();
    lotui::LinearLayoutOptions workspaceOptions;
    workspaceOptions.spacing = 7.0F;
    workspaceOptions.padding = {13.0F, 11.0F, 13.0F, 9.0F};
    workspaceOptions.crossAxisAlignment = lotui::CrossAxisAlignment::Stretch;
    workspace->setOptions(workspaceOptions);
    auto heading = std::make_unique<lotui::Row>();
    lotui::LinearLayoutOptions headingOptions;
    headingOptions.crossAxisAlignment = lotui::CrossAxisAlignment::Center;
    heading->setOptions(headingOptions);
    heading->addChild(std::make_unique<lotui::Label>(
        state.textEngine, "Objects", textStyle(17.0F)),
        {1.0F, {}, {lotui::unboundedLayoutSize, 26.0F}});
    auto count = std::make_unique<lotui::Label>(
        state.textEngine, "", textStyle(12.0F));
    state.count = count.get();
    heading->addChild(std::move(count));
    workspace->addChild(std::move(heading), fixedHeight(27.0F));

    std::vector<lotui::ListColumn> columns{
        {"visible", "표시", 66.0F, 56.0F, true, false},
        {"name", "이름", 235.0F},
        {"type", "종류", 130.0F},
        {"layer", "레이어", 185.0F},
        {"state", "상태", 125.0F}};
    lotui::ListControlStyle listStyle;
    listStyle.background = {0.13F, 0.15F, 0.17F, 1.0F};
    listStyle.headerBackground = {0.20F, 0.23F, 0.25F, 1.0F};
    listStyle.alternateRow = {0.16F, 0.18F, 0.20F, 1.0F};
    listStyle.selectedCell = {0.10F, 0.22F, 0.30F, 1.0F};
    listStyle.selectedCellFocused = {0.12F, 0.26F, 0.34F, 1.0F};
    listStyle.controlAccent = {0.32F, 0.63F, 0.88F, 1.0F};
    listStyle.rowHeight = 28.0F;
    listStyle.headerHeight = 29.0F;
    listStyle.cornerRadius = 2.0F;
    auto list = std::make_unique<lotui::ListControl>(
        state.textEngine, std::move(columns), std::vector<lotui::ListRow>{},
        lotui::Size{700.0F, 380.0F},
        [&state](std::optional<lotui::ListCellAddress> selected) {
            if (selected && selected->row < state.visibleIndices.size()) {
                state.status->setText(
                    state.items[state.visibleIndices[selected->row]][1].text);
            }
        },
        [&state](const lotui::ListCellEvent& event) {
            if (event.action != lotui::ListCellAction::ToggleCheck ||
                event.address.row >= state.visibleIndices.size()) {
                return;
            }
            const auto index = state.visibleIndices[event.address.row];
            state.recordEdit();
            state.items[index][0].checked =
                state.list->cell(event.address)->checked;
            state.refreshList(index);
            state.status->setText("표시 상태 변경");
        }, listStyle, textStyle(13.0F));
    state.list = list.get();
    list->setSelectionMode(lotui::ListSelectionMode::Row);
    list->setOnSortChanged([&state](auto descriptor) {
        if (descriptor && descriptor->column == 1) {
            state.sortByName(
                descriptor->direction == lotui::ListSortDirection::Ascending);
        }
    });
    workspace->addChild(std::move(list),
        {1.0F, {}, {lotui::unboundedLayoutSize, lotui::unboundedLayoutSize}});
    auto status = std::make_unique<lotui::Label>(
        state.textEngine, "준비", textStyle(12.0F));
    state.status = status.get();
    workspace->addChild(std::move(status), fixedHeight(20.0F));
    root->addChild(std::move(workspace),
        {1.0F, {}, {lotui::unboundedLayoutSize, lotui::unboundedLayoutSize}});
    state.refreshList();

    auto host = std::make_unique<lotui::DialogHost>(std::move(root));
    state.dialogs = host.get();
    return std::make_unique<lotui::WidgetTree>(std::move(host));
}

void updateLayout(
    lotui::WidgetTree& tree,
    const lotui::WindowMetrics& metrics) {
    const float scale = metrics.dpiScale > 0.0F ? metrics.dpiScale : 1.0F;
    const float width = static_cast<float>(metrics.framebufferWidth) / scale;
    const float height = static_cast<float>(metrics.framebufferHeight) / scale;
    tree.layout({0.0F, 0.0F, width, height});
}

void exercisePlugin(
    lotui::WidgetTree& tree,
    DemoState& state,
    bool expectText) {
    if (state.pluginControls.size() != (expectText ? 4u : 3u)) {
        throw std::runtime_error("unexpected plugin control count");
    }
    const auto* pluginButton = dynamic_cast<const lotui::Button*>(
        state.pluginControls[0]);
    if (!pluginButton || pluginButton->style().normal.alpha != 0.0F) {
        throw std::runtime_error("plugin ribbon button has a default fill");
    }
    state.ribbon->selectTab("plugin");
    const auto click = [&tree](lotui::Point point) {
        tree.pointerMoved(point);
        tree.pointerPressed(point, lotui::PointerButton::Primary);
        tree.pointerReleased(point, lotui::PointerButton::Primary);
    };
    const auto initialCount = state.items.size();
    const auto button = state.pluginControls[0]->bounds();
    click({button.x + button.width * 0.5F,
        button.y + button.height * 0.5F});
    if (state.status->text() != "플러그인 실행" ||
        state.items.size() != initialCount + 1) {
        throw std::runtime_error("plugin button callback was not routed");
    }
    const auto checkbox = state.pluginControls[1]->bounds();
    click({checkbox.x + 8.0F,
        checkbox.y + checkbox.height * 0.5F});
    if (!state.snap || state.list->frozenColumnCount() != 2) {
        throw std::runtime_error("plugin checkbox callback was not routed");
    }
    const auto slider = state.pluginControls[2]->bounds();
    const float y = slider.y + slider.height * 0.5F;
    tree.pointerMoved({slider.x + slider.width * 0.2F, y});
    tree.pointerPressed({slider.x + slider.width * 0.2F, y},
        lotui::PointerButton::Primary);
    tree.pointerMoved({slider.x + slider.width * 0.95F, y});
    tree.pointerReleased({slider.x + slider.width * 0.95F, y},
        lotui::PointerButton::Primary);
    if (state.size < 80.0 || state.list->style().rowHeight < 31.0F) {
        throw std::runtime_error("plugin slider callback was not routed");
    }
    if (!expectText) return;
    auto* field = dynamic_cast<lotui::TextField*>(state.pluginControls[3]);
    if (!field || field->text() != "새 항목") {
        throw std::runtime_error("plugin text field was not created");
    }
    const auto input = field->bounds();
    click({input.x + input.width * 0.5F,
        input.y + input.height * 0.5F});
    if (!tree.textInputState().enabled) {
        throw std::runtime_error("plugin text field did not receive focus");
    }
    tree.keyPressed(lotui::KeyCode::A, {false, true});
    tree.textInput({lotui::TextInputEventType::Commit, u8"한글"});
    tree.keyPressed(lotui::KeyCode::Z, {false, true});
    if (field->text() != "새 항목" ||
        field->selectedText() != "새 항목") {
        throw std::runtime_error("plugin text undo did not restore selection");
    }
    tree.keyPressed(lotui::KeyCode::Z, {true, true});
    if (field->text() != u8"한글") {
        throw std::runtime_error("plugin text redo did not restore input");
    }
    tree.keyPressed(lotui::KeyCode::Enter);
    if (field->text() != u8"한글" ||
        state.status->text() != u8"플러그인 이름: 한글") {
        throw std::runtime_error("plugin text callback was not routed");
    }
}

void exerciseWorkspace(lotui::WidgetTree& tree, DemoState& state) {
    const auto click = [&tree](lotui::Point point) {
        tree.pointerMoved(point);
        tree.pointerPressed(point, lotui::PointerButton::Primary);
        tree.pointerReleased(point, lotui::PointerButton::Primary);
    };
    if (state.ribbon->tabCount() != 4) {
        throw std::runtime_error("expected four ribbon tabs");
    }
    float previousRight = state.ribbon->bounds().x;
    for (std::size_t index = 0; index < state.ribbon->tabCount(); ++index) {
        const auto header = state.ribbon->tabHeaderBounds(index);
        if (header.width < 82.0F || header.x < previousRight ||
            header.x + header.width >
                state.ribbon->bounds().x + state.ribbon->bounds().width) {
            throw std::runtime_error("ribbon tab headers overlap or overflow");
        }
        previousRight = header.x + header.width;
    }
    const auto ribbonBounds = state.ribbon->bounds();
    const auto& ribbonStyle = state.ribbon->style();
    const float panelTop = ribbonBounds.y + ribbonStyle.tabBarHeight +
        ribbonStyle.contentPadding.top;
    const float panelBottom = ribbonBounds.y + ribbonBounds.height -
        ribbonStyle.contentPadding.bottom;
    for (std::size_t tabIndex = 0;
            tabIndex < state.ribbon->tabCount(); ++tabIndex) {
        state.ribbon->selectTab(tabIndex);
        const auto* panels = dynamic_cast<const lotui::Row*>(
            state.ribbon->tabAt(tabIndex).content());
        if (!panels || panels->childCount() == 0) {
            throw std::runtime_error("ribbon panels are missing");
        }
        for (std::size_t panelIndex = 0;
                panelIndex < panels->childCount(); ++panelIndex) {
            const auto* panel = dynamic_cast<const lotui::RibbonGroup*>(
                &panels->childAt(panelIndex));
            if (!panel || std::abs(panel->bounds().y - panelTop) > 0.5F ||
                std::abs(panel->bounds().y + panel->bounds().height -
                    panelBottom) > 0.5F) {
                throw std::runtime_error(
                    "ribbon panel titles are not aligned");
            }
        }
    }
    state.ribbon->selectTab("data");
    const auto* dataPanels = dynamic_cast<const lotui::Row*>(
        state.ribbon->tabAt(2).content());
    const auto* displayPanel = dynamic_cast<const lotui::RibbonGroup*>(
        &dataPanels->childAt(1));
    const auto* displayColumns = dynamic_cast<const lotui::Row*>(
        displayPanel->content());
    const auto* displayCommands = dynamic_cast<const lotui::Column*>(
        &displayColumns->childAt(0));
    const auto commandLabel = [](const lotui::Widget& widget) {
        const auto* button = dynamic_cast<const lotui::Button*>(&widget);
        const auto* content = button
            ? dynamic_cast<const lotui::Row*>(button->content()) : nullptr;
        return content && content->childCount() == 2
            ? dynamic_cast<const lotui::Label*>(&content->childAt(1))
            : nullptr;
    };
    const auto* firstLabel = commandLabel(displayCommands->childAt(0));
    const auto* secondLabel = commandLabel(displayCommands->childAt(1));
    if (!firstLabel || !secondLabel ||
        firstLabel->horizontalAlignment() !=
            lotui::HorizontalTextAlignment::Start ||
        std::abs(firstLabel->bounds().x -
            secondLabel->bounds().x) > 0.5F) {
        throw std::runtime_error("ribbon command labels are not left-aligned");
    }
    const auto viewTab = state.ribbon->tabHeaderBounds(1);
    click({viewTab.x + viewTab.width * 0.5F,
        viewTab.y + viewTab.height * 0.5F});
    if (state.ribbon->selectedId() != "view") {
        throw std::runtime_error("view ribbon tab did not activate");
    }
    state.ribbon->selectTab("home");
    const auto* homeGroups = dynamic_cast<const lotui::Row*>(
        state.ribbon->tabAt(0).content());
    if (!homeGroups || homeGroups->childCount() != 4) {
        throw std::runtime_error("home ribbon groups are missing");
    }
    bool hasThreeRows = false;
    float previousGroupRight = homeGroups->bounds().x;
    for (std::size_t groupIndex = 0;
            groupIndex < homeGroups->childCount(); ++groupIndex) {
        const auto* group = dynamic_cast<const lotui::RibbonGroup*>(
            &homeGroups->childAt(groupIndex));
        if (!group || group->bounds().width > 200.0F ||
            std::abs(group->bounds().x - previousGroupRight) > 0.5F) {
            throw std::runtime_error("ribbon group width or spacing is invalid");
        }
        previousGroupRight = group->bounds().x + group->bounds().width;
        const auto* columns = group
            ? dynamic_cast<const lotui::Row*>(group->content()) : nullptr;
        if (!columns) {
            throw std::runtime_error("ribbon command columns are missing");
        }
        for (std::size_t columnIndex = 0;
                columnIndex < columns->childCount(); ++columnIndex) {
            const auto* column = dynamic_cast<const lotui::Column*>(
                &columns->childAt(columnIndex));
            if (!column || column->childCount() == 0) {
                throw std::runtime_error("ribbon command rows are missing");
            }
            hasThreeRows = hasThreeRows || column->childCount() == 3;
            float previousBottom = group->bounds().y;
            for (std::size_t buttonIndex = 0;
                    buttonIndex < column->childCount(); ++buttonIndex) {
                const auto bounds = column->childAt(buttonIndex).bounds();
                if (bounds.y < previousBottom - 0.5F ||
                    bounds.y + bounds.height >
                        group->bounds().y + group->bounds().height + 0.5F) {
                    throw std::runtime_error("ribbon command rows overlap");
                }
                previousBottom = bounds.y + bounds.height;
            }
        }
    }
    if (!hasThreeRows) {
        throw std::runtime_error("expected a three-row ribbon group");
    }
    const auto* insertGroup = dynamic_cast<const lotui::RibbonGroup*>(
        &homeGroups->childAt(2));
    const auto* insertColumns = dynamic_cast<const lotui::Row*>(
        insertGroup->content());
    const auto* firstColumn = dynamic_cast<const lotui::Column*>(
        &insertColumns->childAt(0));
    const auto* firstButton = dynamic_cast<const lotui::Button*>(
        &firstColumn->childAt(0));
    if (!firstButton || firstButton->style().normal.alpha != 0.0F) {
        throw std::runtime_error("ribbon button has a default fill");
    }
    const auto* primaryContent = dynamic_cast<const lotui::Column*>(
        firstButton->content());
    const auto* secondaryColumn = dynamic_cast<const lotui::Column*>(
        &insertColumns->childAt(1));
    if (!primaryContent || primaryContent->childCount() != 2 ||
        !secondaryColumn || secondaryColumn->childCount() != 3 ||
        firstButton->bounds().height < 70.0F ||
        primaryContent->childAt(0).bounds().y >=
            primaryContent->childAt(1).bounds().y ||
        secondaryColumn->childAt(0).bounds().height > 25.0F ||
        secondaryColumn->childAt(2).bounds().y +
            secondaryColumn->childAt(2).bounds().height >
            firstButton->bounds().y + firstButton->bounds().height + 0.5F) {
        throw std::runtime_error("ribbon primary command layout is invalid");
    }
    const auto initialCount = state.items.size();
    const auto insertButton = firstColumn->childAt(0).bounds();
    click({insertButton.x + insertButton.width * 0.5F,
        insertButton.y + insertButton.height * 0.5F});
    if (state.items.size() != initialCount + 1 ||
        state.items.back()[2].text != "Assembly" ||
        !state.selectedItem()) {
        throw std::runtime_error("insert ribbon action did not update list");
    }
    state.showSelectionDetails();
    if (!state.dialogs->hasModal()) {
        throw std::runtime_error("inspect ribbon did not show details");
    }
    state.dialogs->dismissModal();
    state.setSelectedVisible(false);
    if (state.items.back()[0].checked) {
        throw std::runtime_error("inspect ribbon did not hide selection");
    }
    state.undo();
    state.showAllItems();
    if (!std::all_of(state.items.begin(), state.items.end(),
            [](const auto& row) { return row[0].checked; })) {
        throw std::runtime_error("inspect ribbon did not show all items");
    }
    state.undo();
    state.undo();
    if (state.items.size() != initialCount) {
        throw std::runtime_error("insert ribbon undo did not restore list");
    }
    state.addItem();
    if (state.items.size() != initialCount + 1 ||
        !state.selectedItem()) {
        throw std::runtime_error("ribbon add did not update list");
    }
    state.duplicateItem();
    if (state.items.size() != initialCount + 2) {
        throw std::runtime_error("ribbon duplicate did not update list");
    }
    state.removeItem();
    state.undo();
    state.redo();
    if (state.items.size() != initialCount + 1) {
        throw std::runtime_error("ribbon undo/redo did not restore list");
    }
    state.sortByName(true);
    if (!std::is_sorted(state.items.begin(), state.items.end(),
            [](const auto& left, const auto& right) {
                return left[1].text < right[1].text;
            })) {
        throw std::runtime_error("ribbon sort did not order list");
    }
    state.visibleOnly = true;
    state.refreshList();
    if (state.visibleIndices.size() != initialCount) {
        throw std::runtime_error("ribbon filter did not update list");
    }
    state.visibleOnly = false;
    state.refreshList();
}

} // namespace

int main(int argc, char** argv) {
    try {
        const std::string mode = argc == 2 ? argv[1] : "";
        const bool capturePlugin = argc == 3 &&
            std::string_view(argv[1]) == "--capture-plugin";
        if ((argc > 2 && !capturePlugin) ||
            (argc == 2 && mode != "--smoke" &&
                mode != "--input-test" && mode != "--input-test-v1")) {
            std::cerr << "usage: ribbon_demo "
                "[--smoke|--input-test|--input-test-v1|"
                "--capture-plugin output.ppm]\n";
            return 2;
        }
        const bool smoke = !mode.empty() || capturePlugin;
        auto platform = lotui::createPlatformBackend();
        auto window = platform->createWindow(
            {"LotUI Ribbon Demo", 960, 540, true});
        window->show();
        lotui::VulkanRenderer renderer(*window);
        const auto fontFile = lotui::executableDirectory() /
            "resources" / "fonts" / "NotoSansKR-Regular.ttf";
        lotui::example::PluginLibrary plugin(
            lotui::executableDirectory() / LOTUI_DEMO_PLUGIN_NAME);
        DemoState state;
        state.textEngine = std::make_shared<lotui::FreetypeTextEngine>(
            renderer, std::vector<lotui::FontSource>{
                {"Noto Sans KR", fontFile},
                {"Lucide", lotui::executableDirectory() /
                    "resources" / "fonts" / "lucide-subset.ttf"}});
        auto tree = createUi(state, *window,
            plugin.descriptor(),
            mode == "--input-test-v1" ? nullptr : plugin.textExtension());
        updateLayout(*tree, window->metrics());
        if (capturePlugin) state.ribbon->selectTab("plugin");
        if (mode == "--input-test" || mode == "--input-test-v1") {
            exerciseWorkspace(*tree, state);
            exercisePlugin(*tree, state, mode == "--input-test");
        }

        std::vector<lotui::PaintCommand> commands;
        bool running = true;
        int frames = 0;
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
                if (capturePlugin && frames == 7) {
                    const auto captured = renderer.drawFrameCaptured(commands);
                    std::ofstream output(argv[2], std::ios::binary);
                    if (!output || captured.rgba.empty()) {
                        throw std::runtime_error("plugin capture failed");
                    }
                    output << "P6\n" << captured.width << ' ' <<
                        captured.height << "\n255\n";
                    for (std::size_t pixel = 0;
                            pixel < captured.rgba.size(); pixel += 4) {
                        output.write(reinterpret_cast<const char*>(
                            captured.rgba.data() + pixel), 3);
                    }
                    if (!output) {
                        throw std::runtime_error("plugin capture write failed");
                    }
                } else {
                    renderer.drawFrame(commands);
                }
                window->setTextInputState(tree->textInputState());
                ++frames;
            }
            if (smoke && frames >= 8) {
                running = false;
            }
            if (!receivedEvent) {
                std::this_thread::sleep_for(std::chrono::milliseconds(8));
            }
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "ribbon_demo failed: " << error.what() << '\n';
        return 1;
    }
}
