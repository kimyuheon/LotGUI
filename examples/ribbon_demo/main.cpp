#include "examples/platform_probe/platform_event_bridge.h"
#include "examples/ribbon_demo/desktop_palette.h"
#include "examples/ribbon_demo/tga_image.h"
#include "examples/ribbon_demo/plugin_loader.h"
#include "examples/ribbon_demo/plugin_tab.h"

#include "core/widget_tree.h"
#include "platform/platform_backend.h"
#include "platform/runtime_paths.h"
#include "renderer/vulkan/vulkan_renderer.h"
#include "text/freetype/freetype_text_engine.h"
#include "widgets/button.h"
#include "widgets/box.h"
#include "widgets/combo_box.h"
#include "widgets/dialog.h"
#include "widgets/dock_workspace.h"
#include "widgets/label.h"
#include "widgets/linear_layout.h"
#include "widgets/list_control.h"
#include "widgets/menu_bar.h"
#include "widgets/numeric_input.h"
#include "widgets/popup.h"
#include "widgets/ribbon.h"
#include "widgets/single_child_widget.h"
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

using lotui::example::desktopColor;

constexpr float ribbonHeight = 134.0F;

struct RibbonImages {
    lotui::TextureId assembly{lotui::invalidTextureId};
    lotui::TextureId dialog{lotui::invalidTextureId};
};

class RibbonSelectionAccent final : public lotui::SingleChildWidget {
public:
    explicit RibbonSelectionAccent(std::unique_ptr<lotui::Ribbon> ribbon)
        : SingleChildWidget(std::move(ribbon)) {
    }

    lotui::Size measure(const lotui::LayoutConstraints& constraints) const override {
        return child()->measure(constraints);
    }

protected:
    void onArrange() override {
        child()->arrange(bounds(), clip());
    }

    void paintChildren(std::vector<lotui::PaintCommand>& commands) const override {
        SingleChildWidget::paintChildren(commands);
        const auto& ribbon = static_cast<const lotui::Ribbon&>(*child());
        if (ribbon.selectedIndex() >= ribbon.tabCount()) {
            return;
        }
        const auto tab = ribbon.tabHeaderBounds(ribbon.selectedIndex());
        constexpr float inset = 12.0F;
        constexpr float height = 2.0F;
        if (tab.width <= inset * 2.0F || tab.height < height) {
            return;
        }
        commands.push_back({
            {tab.x + inset, tab.y + tab.height - height,
                tab.width - inset * 2.0F, height},
            clip(), desktopColor(0x86abc9), lotui::invalidTextureId, 0.0F,
        });
    }
};

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

lotui::DialogStyle desktopDialogStyle() {
    lotui::DialogStyle style;
    style.background = desktopColor(0x292c31);
    style.titleDivider = desktopColor(0x474b52);
    style.cornerRadius = 8.0F;
    return style;
}

std::unique_ptr<lotui::Button> commandButton(
    const std::shared_ptr<const lotui::TextEngine>& textEngine,
    std::string title,
    lotui::Button::ClickHandler action,
    float width,
    std::string_view icon = {},
    bool prominent = false,
    lotui::TextureId image = lotui::invalidTextureId) {
    lotui::ButtonStyle style;
    style.normal = {0.0F, 0.0F, 0.0F, 0.0F};
    style.hovered = desktopColor(0x3b4149, 0.85F);
    style.pressed = desktopColor(0x465363);
    style.disabled = {0.0F, 0.0F, 0.0F, 0.0F};
    style.cornerRadius = 2.0F;
    style.contentPadding = lotui::EdgeInsets::all(2.0F);
    auto label = std::make_unique<lotui::Label>(
        textEngine, std::move(title), textStyle(14.0F));
    label->setHorizontalAlignment(prominent
        ? lotui::HorizontalTextAlignment::Center
        : lotui::HorizontalTextAlignment::Start);
    label->setVerticalAlignment(lotui::VerticalTextAlignment::Center);
    std::unique_ptr<lotui::Widget> content = std::move(label);
    if (image != lotui::invalidTextureId || !icon.empty()) {
        std::unique_ptr<lotui::Widget> symbol;
        if (image != lotui::invalidTextureId) {
            auto bitmap = std::make_unique<lotui::Box>(
                lotui::Size{28.0F, 28.0F},
                lotui::Color{1.0F, 1.0F, 1.0F, 1.0F});
            bitmap->setTexture(image);
            symbol = std::move(bitmap);
        } else {
            auto iconStyle = textStyle(prominent ? 24.0F : 18.0F);
            iconStyle.fontFamilies = {"Lucide"};
            auto glyph = std::make_unique<lotui::Label>(
                textEngine, std::string(icon), iconStyle);
            glyph->setColor(desktopColor(
                prominent ? 0xd2e8fa : 0xaed0ec));
            glyph->setHorizontalAlignment(
                lotui::HorizontalTextAlignment::Center);
            glyph->setVerticalAlignment(
                lotui::VerticalTextAlignment::Center);
            symbol = std::move(glyph);
        }
        lotui::LinearLayoutOptions options;
        options.mainAxisSize = prominent
            ? lotui::MainAxisSize::Min
            : lotui::MainAxisSize::Max;
        options.crossAxisAlignment = lotui::CrossAxisAlignment::Center;
        if (prominent) {
            options.spacing = 5.0F;
            auto iconTile = std::make_unique<lotui::Column>();
            lotui::LinearLayoutOptions tileOptions;
            tileOptions.mainAxisAlignment = lotui::MainAxisAlignment::Center;
            tileOptions.crossAxisAlignment =
                lotui::CrossAxisAlignment::Center;
            iconTile->setOptions(tileOptions);
            iconTile->setDecoration(lotui::BoxDecoration{
                desktopColor(0x344558), 6.0F});
            iconTile->addChild(std::move(symbol),
                {0.0F, {28.0F, 28.0F}, {28.0F, 28.0F}});
            auto column = std::make_unique<lotui::Column>();
            column->setOptions(options);
            column->addChild(std::move(iconTile),
                {0.0F, {36.0F, 36.0F}, {36.0F, 36.0F}});
            column->addChild(std::move(content));
            content = std::move(column);
        } else {
            options.spacing = 5.0F;
            auto row = std::make_unique<lotui::Row>();
            row->setOptions(options);
            row->addChild(std::move(symbol),
                {0.0F, {20.0F, 20.0F}, {20.0F, 20.0F}});
            row->addChild(std::move(content));
            content = std::move(row);
        }
    }
    return std::make_unique<lotui::Button>(
        std::move(content), lotui::Size{width, prominent ? 76.0F : 24.0F},
        std::move(action), style);
}

std::unique_ptr<lotui::Button> quickIconButton(
    const std::shared_ptr<const lotui::TextEngine>& textEngine,
    std::string_view icon, lotui::Button::ClickHandler action) {
    auto glyphStyle = textStyle(20.0F);
    glyphStyle.fontFamilies = {"Lucide"};
    auto glyph = std::make_unique<lotui::Label>(
        textEngine, std::string(icon), glyphStyle);
    glyph->setColor(desktopColor(0xc1d8e8));
    glyph->setHorizontalAlignment(lotui::HorizontalTextAlignment::Center);
    glyph->setVerticalAlignment(lotui::VerticalTextAlignment::Center);
    lotui::ButtonStyle style;
    style.normal = {0.0F, 0.0F, 0.0F, 0.0F};
    style.hovered = desktopColor(0x3b4652);
    style.pressed = desktopColor(0x50677a);
    style.cornerRadius = 4.0F;
    style.contentPadding = lotui::EdgeInsets::all(2.0F);
    return std::make_unique<lotui::Button>(
        std::move(glyph), lotui::Size{38.0F, 30.0F},
        std::move(action), style);
}

std::unique_ptr<lotui::Button> dialogButton(
    const std::shared_ptr<const lotui::TextEngine>& textEngine,
    lotui::Button::ClickHandler action) {
    lotui::ButtonStyle style;
    style.normal = desktopColor(0x3b526b);
    style.hovered = desktopColor(0x496883);
    style.pressed = desktopColor(0x30455b);
    style.focusRing = desktopColor(0x9bc9f1);
    style.cornerRadius = 4.0F;
    style.contentPadding = lotui::EdgeInsets::all(4.0F);
    auto label = std::make_unique<lotui::Label>(
        textEngine, "닫기", textStyle(14.0F));
    return std::make_unique<lotui::Button>(
        std::move(label), lotui::Size{78.0F, 30.0F},
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
    lotui::PopupHost* popups{nullptr};
    lotui::MenuBar* menuBar{nullptr};
    lotui::Ribbon* ribbon{nullptr};
    lotui::DockWorkspace* dock{nullptr};
    lotui::Button* insertMenuButton{nullptr};
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

    void showInsertMenu() {
        if (!popups || !insertMenuButton) {
            throw std::runtime_error("insert menu is unavailable");
        }
        auto menu = std::make_unique<lotui::Column>();
        lotui::LinearLayoutOptions options;
        options.spacing = 2.0F;
        options.padding = lotui::EdgeInsets::all(5.0F);
        options.mainAxisSize = lotui::MainAxisSize::Min;
        options.crossAxisAlignment = lotui::CrossAxisAlignment::Stretch;
        menu->setOptions(options);
        menu->setDecoration(lotui::BoxDecoration{
            desktopColor(0x30343a), 3.0F});
        const auto addOption = [this, &menu](std::string title,
                                   std::string_view icon, std::string name,
                                   std::string type, std::string layer) {
            auto button = commandButton(textEngine, std::move(title),
                [this, name = std::move(name), type = std::move(type),
                    layer = std::move(layer)] {
                    insertItem(name, type, layer);
                    popups->acceptPopup();
                }, 144.0F, icon);
            menu->addChild(std::move(button),
                {0.0F, {144.0F, 26.0F}, {144.0F, 26.0F}});
        };
        addOption("표", u8"\uE2F9", "Table", "Table", "Architecture");
        addOption("솔리드", u8"\uE2D0", "Solid", "Solid", "Structure");
        addOption("새 뷰", u8"\uE0BA", "View", "View", "Drawings");
        lotui::PopupOptions popupOptions;
        popupOptions.placement = lotui::PopupPlacement::BelowStart;
        popupOptions.matchAnchorWidth = false;
        popups->showPopup(std::move(menu), insertMenuButton->bounds(),
            popupOptions);
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
        auto content = std::make_unique<lotui::Row>();
        lotui::LinearLayoutOptions options;
        options.crossAxisAlignment = lotui::CrossAxisAlignment::Center;
        content->setOptions(options);
        content->addChild(std::make_unique<lotui::Label>(
            textEngine, "LotUI 데스크톱 대화상자", textStyle(14.0F)),
            {1.0F, {}, {lotui::unboundedLayoutSize, 30.0F}});
        content->addChild(dialogButton(textEngine,
            [this] { dialogs->dismissModal(); }),
            {0.0F, {78.0F, 30.0F}, {78.0F, 30.0F}});
        auto dialog = std::make_unique<lotui::Dialog>(
            std::move(content), lotui::Size{380.0F, 162.0F},
            desktopDialogStyle());
        dialog->setTitle(std::make_unique<lotui::Label>(
            textEngine, "LotUI 대화상자", textStyle(14.0F)));
        dialogs->showModal(std::move(dialog));
    }

    lotui::ModelessDialogId showResizableDialog() {
        auto content = std::make_unique<lotui::Column>();
        lotui::LinearLayoutOptions options;
        options.spacing = 8.0F;
        options.crossAxisAlignment = lotui::CrossAxisAlignment::Stretch;
        content->setOptions(options);
        content->addChild(std::make_unique<lotui::Label>(
            textEngine, "선택 항목의 설정을 확인하세요", textStyle(14.0F)),
            fixedHeight(26.0F));
        content->addChild(std::make_unique<lotui::Label>(
            textEngine, "크기를 바꾸면 내용도 다시 배치됩니다", textStyle(13.0F)),
            fixedHeight(26.0F));
        auto actions = std::make_unique<lotui::Row>();
        lotui::LinearLayoutOptions actionOptions;
        actionOptions.mainAxisAlignment = lotui::MainAxisAlignment::End;
        actions->setOptions(actionOptions);
        auto id = std::make_shared<lotui::ModelessDialogId>(
            lotui::invalidModelessDialogId);
        actions->addChild(dialogButton(textEngine,
            [this, id] { dialogs->closeModeless(*id); }),
            {0.0F, {78.0F, 30.0F}, {78.0F, 30.0F}});
        content->addChild(std::move(actions), fixedHeight(30.0F));
        auto dialog = std::make_unique<lotui::Dialog>(
            std::move(content), lotui::Size{300.0F, 220.0F},
            desktopDialogStyle());
        dialog->setTitle(std::make_unique<lotui::Label>(
            textEngine, "크기 조절 대화상자", textStyle(14.0F)));
        *id = dialogs->showModeless(std::move(dialog),
            {160.0F, 80.0F, 300.0F, 220.0F});
        return *id;
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
        auto actions = std::make_unique<lotui::Row>();
        lotui::LinearLayoutOptions actionOptions;
        actionOptions.mainAxisAlignment = lotui::MainAxisAlignment::End;
        actionOptions.crossAxisAlignment = lotui::CrossAxisAlignment::Center;
        actions->setOptions(actionOptions);
        actions->addChild(dialogButton(textEngine,
            [this] { dialogs->dismissModal(); }),
            {0.0F, {78.0F, 30.0F}, {78.0F, 30.0F}});
        content->addChild(std::move(actions), fixedHeight(30.0F));
        auto dialog = std::make_unique<lotui::Dialog>(
            std::move(content), lotui::Size{300.0F, 220.0F},
            desktopDialogStyle());
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
    lotui::TextureId image{lotui::invalidTextureId};
    lotui::Button** outputButton{nullptr};
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
            auto button = commandButton(state.textEngine,
                std::move(action.title), std::move(action.action),
                commandWidth, action.icon, action.prominent, action.image);
            if (action.outputButton) {
                *action.outputButton = button.get();
            }
            column->addChild(std::move(button),
                {0.0F, {}, {commandWidth, lotui::unboundedLayoutSize}});
        }
        controls->addChild(std::move(column));
    }
    lotui::RibbonGroupStyle style;
    style.cornerRadius = 0.0F;
    style.background = {0.0F, 0.0F, 0.0F, 0.0F};
    style.titleHeight = 18.0F;
    style.contentPadding = lotui::EdgeInsets::all(2.0F);
    style.titleColor = desktopColor(0xb9c0c9);
    return std::make_unique<lotui::RibbonGroup>(state.textEngine,
        std::move(title), std::move(controls), style, textStyle(14.0F));
}

std::unique_ptr<lotui::WidgetTree> createUi(
    DemoState& state,
    lotui::PlatformWindow& window,
    const LotuiDemoPluginV1& pluginDescriptor,
    const LotuiDemoPluginV2* textExtension,
    const LotuiDemoPluginV3* fieldsExtension,
    RibbonImages images) {
    auto root = std::make_unique<lotui::Column>();
    root->setDecoration(lotui::BoxDecoration{
        desktopColor(0x1b1d21), 0.0F});
    lotui::LinearLayoutOptions rootOptions;
    rootOptions.crossAxisAlignment = lotui::CrossAxisAlignment::Stretch;
    root->setOptions(rootOptions);

    const auto menuCommand = [](std::string id, std::string label,
            std::string shortcut = {}) {
        return lotui::MenuEntry{lotui::MenuEntryKind::Command,
            std::move(id), std::move(label), std::move(shortcut),
            true, false, {}};
    };
    lotui::MenuBarStyle menuStyle;
    menuStyle.background = desktopColor(0x202328);
    menuStyle.popupBackground = desktopColor(0x292e34);
    menuStyle.hovered = desktopColor(0x36414b);
    menuStyle.selected = desktopColor(0x40505d);
    menuStyle.text = desktopColor(0xe8edf1);
    menuStyle.muted = desktopColor(0x9ba9b5);
    menuStyle.separator = desktopColor(0x47515a);
    menuStyle.barHeight = 26.0F;
    menuStyle.popupWidth = 188.0F;
    auto menuBar = std::make_unique<lotui::MenuBar>(state.textEngine,
        std::vector<lotui::Menu>{
            {"edit", "편집", {menuCommand("undo", "실행 취소", "Ctrl+Z"),
                menuCommand("redo", "다시 실행", "Ctrl+Y"),
                {lotui::MenuEntryKind::Separator, {}, {}, {}, true, false, {}},
                menuCommand("duplicate", "복제"),
                menuCommand("delete", "삭제")}},
            {"insert", "삽입", {menuCommand("add", "항목 추가"),
                menuCommand("table", "표 삽입"),
                menuCommand("sketch", "스케치 삽입"),
                menuCommand("view", "뷰 삽입")}},
            {"display", "보기", {menuCommand("home-tab", "홈 탭"),
                menuCommand("view-tab", "보기 탭"),
                menuCommand("data-tab", "데이터 탭"),
                menuCommand("plugin-tab", "플러그인 탭")}},
            {"help", "도움말", {menuCommand("details", "선택 정보"),
                menuCommand("dialog", "대화상자")}}},
        [&state](std::string_view id) {
            if (id == "undo") state.undo();
            else if (id == "redo") state.redo();
            else if (id == "duplicate") state.duplicateItem();
            else if (id == "delete") state.removeItem();
            else if (id == "add") state.addItem();
            else if (id == "table")
                state.insertItem("Table", "Table", "Architecture");
            else if (id == "sketch")
                state.insertItem("Sketch", "Sketch", "Structure");
            else if (id == "view")
                state.insertItem("View", "View", "Drawings");
            else if (id == "home-tab") state.ribbon->selectTab("home");
            else if (id == "view-tab") state.ribbon->selectTab("view");
            else if (id == "data-tab") state.ribbon->selectTab("data");
            else if (id == "plugin-tab") state.ribbon->selectTab("plugin");
            else if (id == "details") state.showSelectionDetails();
            else if (id == "dialog") state.showDialog();
        }, menuStyle, textStyle(13.0F));
    state.menuBar = menuBar.get();
    root->addChild(std::move(menuBar), fixedHeight(menuStyle.barHeight));

    lotui::RibbonStyle ribbonStyle;
    ribbonStyle.preferredHeight = ribbonHeight;
    ribbonStyle.tabBarHeight = 27.0F;
    ribbonStyle.contentPadding = lotui::EdgeInsets::all(3.0F);
    ribbonStyle.tabHorizontalPadding = 11.0F;
    ribbonStyle.minimumTabWidth = 82.0F;
    ribbonStyle.tabCornerRadius = 2.0F;
    ribbonStyle.background = desktopColor(0x282b30);
    ribbonStyle.tabBar = desktopColor(0x202328);
    ribbonStyle.tabNormal = desktopColor(0x202328);
    ribbonStyle.tabHovered = desktopColor(0x30353b);
    ribbonStyle.tabPressed = desktopColor(0x30353b);
    ribbonStyle.tabSelected = ribbonStyle.background;
    ribbonStyle.focusRing = desktopColor(0x414a55);
    ribbonStyle.focusRingWidth = 1.0F;
    ribbonStyle.text = desktopColor(0xd5d9df);
    ribbonStyle.selectedText = desktopColor(0xf6f7f9);
    auto ribbon = std::make_unique<lotui::Ribbon>(
        state.textEngine, ribbonStyle, textStyle(14.0F));
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
        {{"조립품", {}, [&state] {
              state.insertItem("Assembly", "Assembly", "Architecture");
          }, true, images.assembly}},
        {{"스케치", u8"\uE4F1", [&state] {
              state.insertItem("Sketch", "Sketch", "Structure");
          }},
         {"뷰", u8"\uE0BA", [&state] {
              state.insertItem("View", "View", "Drawings");
          }},
         {"더보기...", u8"\uE2F9", [&state] {
              state.showInsertMenu();
          }, false, lotui::invalidTextureId, &state.insertMenuButton}}}));
    groups->addChild(commandGroup(state, "창", {
        {{"대화상자", {},
            [&state] { state.showDialog(); }, true, images.dialog}}}));
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
        pluginDescriptor, textExtension, fieldsExtension,
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
            } else if (id == "mode") {
                state.visibleOnly = value == 1.0;
                state.refreshList();
                state.status->setText("표시 모드 변경");
            } else if (id == "count") {
                auto style = state.list->style();
                style.rowHeight = static_cast<float>(value);
                state.list->setStyle(style);
                state.status->setText("행 높이 변경");
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
    root->addChild(std::make_unique<RibbonSelectionAccent>(
        std::move(ribbon)), fixedHeight(ribbonHeight));

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
        state.textEngine, "", textStyle(14.0F));
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
    listStyle.background = desktopColor(0x202328);
    listStyle.headerBackground = desktopColor(0x30343b);
    listStyle.alternateRow = desktopColor(0x25292f);
    listStyle.hoveredCell = desktopColor(0x35404c);
    listStyle.selectedCell = desktopColor(0x2e4964);
    listStyle.selectedCellFocused = desktopColor(0x365e83);
    listStyle.gridLine = desktopColor(0x343a43);
    listStyle.text = desktopColor(0xf2f4f7);
    listStyle.secondaryText = desktopColor(0xc4cbd3);
    listStyle.controlBackground = desktopColor(0x313841);
    listStyle.controlAccent = desktopColor(0x85b9eb);
    listStyle.headerHovered = desktopColor(0x3a424c);
    listStyle.headerPressed = desktopColor(0x46525e);
    listStyle.sortIndicator = desktopColor(0xa8c9eb);
    listStyle.scrollBarTrack = desktopColor(0x25292f);
    listStyle.scrollBarThumb = desktopColor(0x505964);
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
        }, listStyle, textStyle(14.0F));
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
        state.textEngine, "준비", textStyle(14.0F));
    state.status = status.get();
    workspace->addChild(std::move(status), fixedHeight(20.0F));
    auto toolbar = std::make_unique<lotui::DockToolbar>();
    toolbar->addCommand(quickIconButton(state.textEngine, u8"\uE2A1",
        [&state] { state.undo(); }));
    toolbar->addCommand(quickIconButton(state.textEngine, u8"\uE2A0",
        [&state] { state.redo(); }));
    toolbar->addCommand(quickIconButton(state.textEngine, u8"\uE13D",
        [&state] { state.addItem(); }));
    toolbar->addCommand(quickIconButton(state.textEngine, u8"\uE18E",
        [&state] { state.removeItem(); }));
    lotui::DockWorkspaceStyle dockStyle;
    dockStyle.horizontalThickness = 36.0F;
    dockStyle.verticalThickness = 44.0F;
    dockStyle.toolbarBackground = desktopColor(0x292e34);
    dockStyle.gripColor = desktopColor(0x9baab7);
    dockStyle.dockPreview = desktopColor(0x779fc0, 0.35F);
    auto dock = std::make_unique<lotui::DockWorkspace>(
        std::move(workspace), std::move(toolbar),
        lotui::DockSide::Top, dockStyle);
    state.dock = dock.get();
    root->addChild(std::move(dock),
        {1.0F, {}, {lotui::unboundedLayoutSize, lotui::unboundedLayoutSize}});
    state.refreshList();

    auto popup = std::make_unique<lotui::PopupHost>(std::move(root));
    state.popups = popup.get();
    state.menuBar->setPopupHost(popup.get());
    for (auto* control : state.pluginControls) {
        if (auto* combo = dynamic_cast<lotui::ComboBox*>(control)) {
            combo->setPopupHost(popup.get());
        }
    }
    auto host = std::make_unique<lotui::DialogHost>(std::move(popup));
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
    if (state.pluginControls.size() != (expectText ? 6u : 3u)) {
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
    auto* combo = dynamic_cast<lotui::ComboBox*>(state.pluginControls[4]);
    auto* number = dynamic_cast<lotui::NumericInput*>(state.pluginControls[5]);
    if (!combo || !number || combo->options().size() != 3 ||
        number->value() != 28.0) {
        throw std::runtime_error("plugin fields were not created");
    }
    const auto choice = combo->bounds();
    click({choice.x + choice.width * 0.5F,
        choice.y + choice.height * 0.5F});
    const auto menu = state.popups->popupBounds();
    click({menu.x + menu.width * 0.5F,
        menu.y + combo->style().popupPadding.top +
            combo->style().itemHeight * 1.5F});
    if (combo->selectedIndex() != 1 || !state.visibleOnly ||
        state.status->text() != "표시 모드 변경") {
        throw std::runtime_error("plugin choice callback was not routed: selected=" +
            std::to_string(combo->selectedIndex().value_or(99)) +
            " open=" + std::to_string(combo->isPopupOpen()) +
            " visible=" + std::to_string(state.visibleOnly) +
            " x=" + std::to_string(choice.x));
    }
    const auto numeric = number->bounds();
    click({numeric.x + 8.0F, numeric.y + numeric.height * 0.5F});
    tree.keyPressed(lotui::KeyCode::Up);
    if (number->value() != 30.0 ||
        state.list->style().rowHeight != 30.0F) {
        throw std::runtime_error("plugin number callback was not routed");
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
    std::vector<lotui::PaintCommand> selectedCommands;
    tree.paint(selectedCommands);
    const auto indicatorColor = desktopColor(0x86abc9);
    const auto indicator = std::find_if(selectedCommands.begin(),
        selectedCommands.end(), [&indicatorColor](const auto& command) {
            return command.color.red == indicatorColor.red &&
                command.color.green == indicatorColor.green &&
                command.color.blue == indicatorColor.blue &&
                command.bounds.height == 2.0F;
        });
    if (indicator == selectedCommands.end() ||
        std::abs(indicator->bounds.x - viewTab.x - 12.0F) > 0.5F) {
        throw std::runtime_error("ribbon selection accent did not follow tab");
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
    state.ribbon->selectTab("home");
    const auto moreBounds = state.insertMenuButton->bounds();
    click({moreBounds.x + moreBounds.width * 0.5F,
        moreBounds.y + moreBounds.height * 0.5F});
    const auto* menu = dynamic_cast<const lotui::Column*>(
        state.popups->popup());
    if (!menu || menu->childCount() != 3) {
        throw std::runtime_error("insert ribbon menu did not open");
    }
    const auto tableBounds = menu->childAt(0).bounds();
    const auto countBeforeMenu = state.items.size();
    click({tableBounds.x + tableBounds.width * 0.5F,
        tableBounds.y + tableBounds.height * 0.5F});
    if (state.popups->hasPopup() ||
        state.items.size() != countBeforeMenu + 1 ||
        state.items.back()[2].text != "Table") {
        throw std::runtime_error("insert ribbon menu action failed");
    }
}

void exerciseDocking(lotui::WidgetTree& tree, DemoState& state) {
    if (!state.dock || state.dock->toolbar().commandCount() != 4)
        throw std::runtime_error("quick toolbar is missing");
    for (const auto side : {lotui::DockSide::Bottom,
            lotui::DockSide::Left, lotui::DockSide::Right,
            lotui::DockSide::Top}) {
        const auto area = state.dock->bounds();
        const lotui::Point drop = side == lotui::DockSide::Bottom
            ? lotui::Point{area.x + area.width * 0.5F,
                area.y + area.height - 4.0F}
            : side == lotui::DockSide::Left
            ? lotui::Point{area.x + 4.0F,
                area.y + area.height * 0.5F}
            : side == lotui::DockSide::Right
            ? lotui::Point{area.x + area.width - 4.0F,
                area.y + area.height * 0.5F}
            : lotui::Point{area.x + area.width * 0.5F, area.y + 4.0F};
        const auto grip = state.dock->toolbar().gripBounds();
        const lotui::Point start{grip.x + grip.width * 0.5F,
            grip.y + grip.height * 0.5F};
        tree.pointerPressed(start, lotui::PointerButton::Primary);
        tree.pointerMoved(drop);
        if (state.dock->previewSide() != side)
            throw std::runtime_error("toolbar edge preview failed");
        tree.pointerReleased(drop, lotui::PointerButton::Primary);
        if (state.dock->side() != side ||
                state.dock->toolbar().vertical() !=
                    (side == lotui::DockSide::Left ||
                     side == lotui::DockSide::Right))
            throw std::runtime_error("toolbar did not dock on requested side");
    }
    const auto initialCount = state.items.size();
    const auto click = [&tree](lotui::Rect rect) {
        const lotui::Point center{rect.x + rect.width * 0.5F,
            rect.y + rect.height * 0.5F};
        tree.pointerPressed(center, lotui::PointerButton::Primary);
        tree.pointerReleased(center, lotui::PointerButton::Primary);
    };
    click(state.dock->toolbar().commandAt(2).bounds());
    if (state.items.size() != initialCount + 1)
        throw std::runtime_error("docked add command failed");
    click(state.dock->toolbar().commandAt(0).bounds());
    if (state.items.size() != initialCount)
        throw std::runtime_error("docked undo command failed");
}

void exerciseMenuBar(lotui::WidgetTree& tree, DemoState& state) {
    if (!state.menuBar || !state.popups)
        throw std::runtime_error("menu bar is missing");
    const auto click = [&tree](lotui::Rect area) {
        const lotui::Point center{area.x + area.width * 0.5F,
            area.y + area.height * 0.5F};
        tree.pointerPressed(center, lotui::PointerButton::Primary);
        tree.pointerReleased(center, lotui::PointerButton::Primary);
    };
    const auto initialCount = state.items.size();
    click(state.menuBar->menuBounds(1));
    if (!state.menuBar->isOpen() || !state.popups->hasPopup())
        throw std::runtime_error("insert menu did not open");
    auto popup = state.popups->popupBounds();
    click({popup.x + 12.0F, popup.y + 4.0F, 70.0F, 25.0F});
    if (state.items.size() != initialCount + 1 || state.popups->hasPopup())
        throw std::runtime_error("insert menu action did not run");
    click(state.menuBar->menuBounds(0));
    popup = state.popups->popupBounds();
    click({popup.x + 12.0F, popup.y + 4.0F, 70.0F, 25.0F});
    if (state.items.size() != initialCount || state.popups->hasPopup())
        throw std::runtime_error("edit menu undo did not run");
}

} // namespace

int main(int argc, char** argv) {
    try {
        const std::string mode = argc == 2 ? argv[1] : "";
        const bool capturePlugin = argc == 3 &&
            std::string_view(argv[1]) == "--capture-plugin";
        const bool captureClickedPlugin = argc == 3 &&
            std::string_view(argv[1]) == "--capture-clicked-plugin";
        const bool captureHome = argc == 3 &&
            std::string_view(argv[1]) == "--capture-home";
        const bool captureMenu = argc == 3 &&
            std::string_view(argv[1]) == "--capture-menu";
        const bool captureDockLeft = argc == 3 &&
            std::string_view(argv[1]) == "--capture-dock-left";
        const bool captureDockBottom = argc == 3 &&
            std::string_view(argv[1]) == "--capture-dock-bottom";
        const bool captureDockRight = argc == 3 &&
            std::string_view(argv[1]) == "--capture-dock-right";
        const bool captureInsertMenu = argc == 3 &&
            std::string_view(argv[1]) == "--capture-insert-menu";
        const bool captureDialog = argc == 3 &&
            std::string_view(argv[1]) == "--capture-dialog";
        const bool captureModelessSmall = argc == 3 &&
            std::string_view(argv[1]) == "--capture-modeless-small";
        const bool captureModelessLarge = argc == 3 &&
            std::string_view(argv[1]) == "--capture-modeless-large";
        const bool captureFrame = capturePlugin || captureClickedPlugin ||
            captureHome || captureMenu || captureDockLeft || captureDockBottom ||
            captureDockRight || captureInsertMenu || captureDialog ||
            captureModelessSmall || captureModelessLarge;
        if ((argc > 2 && !captureFrame) ||
            (argc == 2 && mode != "--smoke" &&
                mode != "--input-test" && mode != "--input-test-v1")) {
            std::cerr << "usage: ribbon_demo "
                "[--smoke|--input-test|--input-test-v1|"
                "--capture-plugin output.ppm|"
                "--capture-clicked-plugin output.ppm|"
                "--capture-home output.ppm|"
                "--capture-menu output.ppm|"
                "--capture-dock-left output.ppm|"
                "--capture-dock-bottom output.ppm|"
                "--capture-dock-right output.ppm|"
                "--capture-insert-menu output.ppm|"
                "--capture-dialog output.ppm|"
                "--capture-modeless-small output.ppm|"
                "--capture-modeless-large output.ppm]\n";
            return 2;
        }
        const bool smoke = !mode.empty() || captureFrame;
        auto platform = lotui::createPlatformBackend();
        auto window = platform->createWindow(
            {"LotUI Ribbon Demo", 960, 540, true});
        window->show();
        lotui::VulkanRenderer renderer(*window);
        const auto iconRoot = lotui::executableDirectory() /
            "resources" / "icons";
        auto assemblyImage = renderer.createTexture(
            lotui::example::loadRibbonTga(iconRoot / "assembly.tga"));
        auto dialogImage = renderer.createTexture(
            lotui::example::loadRibbonTga(iconRoot / "dialog.tga"));
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
            mode == "--input-test-v1" ? nullptr : plugin.textExtension(),
            mode == "--input-test-v1" ? nullptr : plugin.fieldsExtension(),
            {assemblyImage.id(), dialogImage.id()});
        updateLayout(*tree, window->metrics());
        if (mode == "--input-test") {
            std::vector<lotui::PaintCommand> visible;
            tree->paint(visible);
            const auto usesTexture = [&visible](lotui::TextureId id) {
                return std::any_of(visible.begin(), visible.end(),
                    [id](const auto& command) {
                        return command.texture == id;
                    });
            };
            if (!usesTexture(assemblyImage.id()) ||
                !usesTexture(dialogImage.id())) {
                throw std::runtime_error(
                    "ribbon image assets were not painted");
            }
        }
        if (capturePlugin) state.ribbon->selectTab("plugin");
        if (captureMenu) {
            const auto area = state.menuBar->menuBounds(1);
            const lotui::Point center{area.x + area.width * 0.5F,
                area.y + area.height * 0.5F};
            tree->pointerPressed(center, lotui::PointerButton::Primary);
            tree->pointerReleased(center, lotui::PointerButton::Primary);
            if (!state.menuBar->isOpen())
                throw std::runtime_error("capture menu did not open");
        }
        if (captureDockLeft) state.dock->setSide(lotui::DockSide::Left);
        if (captureDockBottom) state.dock->setSide(lotui::DockSide::Bottom);
        if (captureDockRight) state.dock->setSide(lotui::DockSide::Right);
        if (captureClickedPlugin) {
            const auto header = state.ribbon->tabHeaderBounds(3);
            const lotui::Point center{
                header.x + header.width * 0.5F,
                header.y + header.height * 0.5F};
            tree->pointerMoved(center);
            tree->pointerPressed(center, lotui::PointerButton::Primary);
            tree->pointerReleased(center, lotui::PointerButton::Primary);
            tree->pointerMoved({header.x, state.ribbon->bounds().height + 8.0F});
            if (state.ribbon->selectedId() != "plugin") {
                throw std::runtime_error("plugin tab click failed");
            }
        }
        if (captureDialog) state.showDialog();
        if (captureModelessSmall || captureModelessLarge) {
            const auto id = state.showResizableDialog();
            if (captureModelessLarge) {
                const auto initial = state.dialogs->modeless(id)->bounds();
                const lotui::Point grip{
                    initial.x + initial.width - 8.0F,
                    initial.y + initial.height - 8.0F};
                tree->pointerPressed(grip, lotui::PointerButton::Primary);
                tree->pointerMoved({grip.x + 100.0F, grip.y + 70.0F});
                tree->pointerReleased({grip.x + 100.0F, grip.y + 70.0F},
                    lotui::PointerButton::Primary);
                const auto resized = state.dialogs->modeless(id)->bounds();
                if (resized.width <= initial.width ||
                        resized.height <= initial.height)
                    throw std::runtime_error("modeless resize failed");
            }
        }
        if (captureInsertMenu) state.showInsertMenu();
        if (mode == "--input-test" || mode == "--input-test-v1") {
            exerciseWorkspace(*tree, state);
            exercisePlugin(*tree, state, mode == "--input-test");
            exerciseDocking(*tree, state);
            exerciseMenuBar(*tree, state);
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
                if (captureFrame && frames == 7) {
                    const auto captured = renderer.drawFrameCaptured(commands);
                    std::ofstream output(argv[2], std::ios::binary);
                    if (!output || captured.rgba.empty()) {
                        throw std::runtime_error("ribbon capture failed");
                    }
                    output << "P6\n" << captured.width << ' ' <<
                        captured.height << "\n255\n";
                    for (std::size_t pixel = 0;
                            pixel < captured.rgba.size(); pixel += 4) {
                        output.write(reinterpret_cast<const char*>(
                            captured.rgba.data() + pixel), 3);
                    }
                    if (!output) {
                        throw std::runtime_error("ribbon capture write failed");
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
