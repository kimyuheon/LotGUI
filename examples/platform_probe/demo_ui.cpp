#include "examples/platform_probe/demo_ui.h"

#include "widgets/box.h"
#include "widgets/button.h"
#include "widgets/checkbox.h"
#include "widgets/document_tab_view.h"
#include "widgets/label.h"
#include "widgets/linear_layout.h"
#include "widgets/menu_bar.h"
#include "widgets/numeric_input.h"
#include "widgets/ribbon.h"
#include "widgets/text_field.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lotui::example {

lotui::ChildLayout fixedHeight(float height) {
    return {0.0F, {0.0F, height},
        {lotui::unboundedLayoutSize, height}};
}

lotui::TextureImage createDemoMask() {
    constexpr std::uint32_t size = 64;
    lotui::TextureImage image;
    image.width = size;
    image.height = size;
    image.format = lotui::TextureFormat::R8Unorm;
    image.pixels.resize(size * size);
    for (std::uint32_t y = 0; y < size; ++y) {
        for (std::uint32_t x = 0; x < size; ++x) {
            const float dx =
                (static_cast<float>(x) + 0.5F - size * 0.5F) / 27.0F;
            const float dy =
                (static_cast<float>(y) + 0.5F - size * 0.5F) / 27.0F;
            const float coverage = std::clamp(
                (1.0F - std::sqrt(dx * dx + dy * dy)) * 8.0F,
                0.0F,
                1.0F);
            image.pixels[y * size + x] = static_cast<std::uint8_t>(
                coverage * 255.0F);
        }
    }
    return image;
}

std::unique_ptr<lotui::WidgetTree> createDemoUi(
    lotui::TextureId demoMaskTexture,
    const std::shared_ptr<const lotui::TextEngine>& textEngine,
    lotui::PlatformWindow& window) {
    auto root = std::make_unique<lotui::Row>();
    lotui::LinearLayoutOptions rootOptions;
    rootOptions.spacing = 12.0F;
    rootOptions.padding = {12.0F, 0.0F, 12.0F, 12.0F};
    rootOptions.crossAxisAlignment = lotui::CrossAxisAlignment::Stretch;
    root->setOptions(rootOptions);

    auto leftPanel = std::make_unique<lotui::Column>();
    leftPanel->setDecoration(lotui::BoxDecoration{
        {0.11F, 0.15F, 0.22F, 1.0F}, 4.0F});
    lotui::LinearLayoutOptions leftOptions;
    leftOptions.padding = {14.0F, 14.0F, 14.0F, 14.0F};
    leftOptions.spacing = 14.0F;
    leftOptions.crossAxisAlignment = lotui::CrossAxisAlignment::Stretch;
    leftPanel->setOptions(leftOptions);
    if (textEngine) {
        auto heading = std::make_unique<lotui::Row>();
        heading->setDecoration(lotui::BoxDecoration{
            {0.18F, 0.23F, 0.34F, 1.0F}, 4.0F});
        lotui::LinearLayoutOptions headingOptions;
        headingOptions.padding = {10.0F, 4.0F, 10.0F, 4.0F};
        headingOptions.crossAxisAlignment =
            lotui::CrossAxisAlignment::Center;
        heading->setOptions(headingOptions);

        lotui::TextStyle headingStyle;
        headingStyle.fontFamilies = {"Noto Sans KR"};
        headingStyle.fontSize = 15.0F;
        headingStyle.weight = lotui::FontWeight::Medium;
        auto headingLabel = std::make_unique<lotui::Label>(
            textEngine, "LotUI 공용 C++ GUI", headingStyle);
        headingLabel->setVerticalAlignment(
            lotui::VerticalTextAlignment::Center);
        heading->addChild(
            std::move(headingLabel),
            {1.0F, {0.0F, 24.0F},
                {lotui::unboundedLayoutSize, 32.0F}});
        leftPanel->addChild(
            std::move(heading),
            fixedHeight(32.0F));
    } else {
        leftPanel->addChild(
            std::make_unique<lotui::Box>(
                lotui::Size{0.0F, 32.0F},
                lotui::Color{0.18F, 0.23F, 0.34F, 1.0F}, 4.0F),
            fixedHeight(32.0F));
    }

    auto status = std::make_unique<lotui::Box>(
        lotui::Size{0.0F, 6.0F},
        lotui::Color{0.28F, 0.72F, 0.63F, 1.0F}, 2.0F);
    lotui::Box* statusIndicator = status.get();

    auto buttonRow = std::make_unique<lotui::Row>();
    lotui::ButtonStyle formButtonStyle;
    formButtonStyle.cornerRadius = 4.0F;
    formButtonStyle.contentPadding = {8.0F, 4.0F, 8.0F, 4.0F};
    lotui::LinearLayoutOptions buttonOptions;
    buttonOptions.spacing = 8.0F;
    buttonOptions.crossAxisAlignment = lotui::CrossAxisAlignment::Start;
    buttonRow->setOptions(buttonOptions);
    std::unique_ptr<lotui::Widget> primaryContent;
    if (textEngine) {
        lotui::TextStyle buttonTextStyle;
        buttonTextStyle.fontFamilies = {"Noto Sans KR"};
        buttonTextStyle.fontSize = 14.0F;
        auto label = std::make_unique<lotui::Label>(
            textEngine, "확인", buttonTextStyle);
        label->setHorizontalAlignment(lotui::HorizontalTextAlignment::Center);
        label->setVerticalAlignment(lotui::VerticalTextAlignment::Center);
        primaryContent = std::move(label);
    }
    buttonRow->addChild(
        primaryContent
            ? std::make_unique<lotui::Button>(
                std::move(primaryContent),
                lotui::Size{96.0F, 34.0F},
                [statusIndicator]() {
                    statusIndicator->setColor(
                        {0.98F, 0.80F, 0.24F, 1.0F});
                    std::cout << "button-clicked: primary\n";
                }, formButtonStyle)
            : std::make_unique<lotui::Button>(
                lotui::Size{96.0F, 34.0F},
            [statusIndicator]() {
                statusIndicator->setColor(
                    {0.98F, 0.80F, 0.24F, 1.0F});
                std::cout << "button-clicked: primary\n";
            }, formButtonStyle),
        {0.0F, {96.0F, 34.0F}, {96.0F, 34.0F}});

    lotui::ButtonStyle secondaryStyle;
    secondaryStyle.normal = {0.22F, 0.28F, 0.39F, 1.0F};
    secondaryStyle.hovered = {0.30F, 0.37F, 0.50F, 1.0F};
    secondaryStyle.pressed = {0.15F, 0.20F, 0.30F, 1.0F};
    secondaryStyle.cornerRadius = 4.0F;
    secondaryStyle.contentPadding = {8.0F, 4.0F, 8.0F, 4.0F};
    std::unique_ptr<lotui::Widget> secondaryContent;
    if (textEngine) {
        lotui::TextStyle buttonTextStyle;
        buttonTextStyle.fontFamilies = {"Noto Sans KR"};
        buttonTextStyle.fontSize = 14.0F;
        auto label = std::make_unique<lotui::Label>(
            textEngine, "취소", buttonTextStyle);
        label->setHorizontalAlignment(lotui::HorizontalTextAlignment::Center);
        label->setVerticalAlignment(lotui::VerticalTextAlignment::Center);
        secondaryContent = std::move(label);
    }
    buttonRow->addChild(
        secondaryContent
            ? std::make_unique<lotui::Button>(
                std::move(secondaryContent),
                lotui::Size{96.0F, 34.0F},
                [statusIndicator]() {
                    statusIndicator->setColor(
                        {0.72F, 0.42F, 0.94F, 1.0F});
                    std::cout << "button-clicked: secondary\n";
                },
                secondaryStyle)
            : std::make_unique<lotui::Button>(
                lotui::Size{96.0F, 34.0F},
            [statusIndicator]() {
                statusIndicator->setColor(
                    {0.72F, 0.42F, 0.94F, 1.0F});
                std::cout << "button-clicked: secondary\n";
            },
            secondaryStyle),
        {0.0F, {96.0F, 34.0F}, {96.0F, 34.0F}});
    leftPanel->addChild(
        std::move(buttonRow),
        fixedHeight(34.0F));
    leftPanel->addChild(std::move(status), fixedHeight(6.0F));

    auto rightPanel = std::make_unique<lotui::Column>();
    rightPanel->setDecoration(lotui::BoxDecoration{
        {0.11F, 0.15F, 0.22F, 1.0F}, 4.0F});
    lotui::LinearLayoutOptions rightOptions;
    rightOptions.padding = {12.0F, 14.0F, 12.0F, 14.0F};
    rightOptions.crossAxisAlignment = lotui::CrossAxisAlignment::Stretch;
    rightPanel->setOptions(rightOptions);

    auto rightContent = std::make_unique<lotui::Column>();
    lotui::LinearLayoutOptions contentOptions;
    contentOptions.spacing = 12.0F;
    contentOptions.crossAxisAlignment = lotui::CrossAxisAlignment::Stretch;
    rightContent->setOptions(contentOptions);
    auto texturedBox = std::make_unique<lotui::Box>(
        lotui::Size{0.0F, 40.0F},
        lotui::Color{0.62F, 0.38F, 0.92F, 1.0F}, 4.0F);
    texturedBox->setTexture(demoMaskTexture);
    rightContent->addChild(std::move(texturedBox), fixedHeight(40.0F));
    if (textEngine) {
        auto form = std::make_unique<lotui::Column>();
        lotui::LinearLayoutOptions formOptions;
        formOptions.spacing = 8.0F;
        formOptions.crossAxisAlignment = lotui::CrossAxisAlignment::Stretch;
        form->setOptions(formOptions);

        lotui::TextStyle controlTextStyle;
        controlTextStyle.fontFamilies = {"Noto Sans KR"};
        controlTextStyle.fontSize = 14.0F;
        lotui::TextFieldStyle fieldStyle;
        fieldStyle.normal = {0.07F, 0.09F, 0.13F, 1.0F};
        fieldStyle.hovered = {0.11F, 0.14F, 0.19F, 1.0F};
        fieldStyle.contentPadding = {8.0F, 4.0F, 8.0F, 4.0F};
        fieldStyle.cornerRadius = 4.0F;
        auto textField = std::make_unique<lotui::TextField>(
                textEngine,
                "한글 입력",
                lotui::Size{0.0F, 34.0F},
                [](const std::string& text) {
                    std::cout << "text-field-changed: " << text << '\n';
                },
                [](const std::string& text) {
                    std::cout << "text-field-submitted: " << text << '\n';
                },
                fieldStyle,
                controlTextStyle);
        textField->setClipboardHandlers(
            [&window](std::string text) {
                return window.writeClipboardText(text);
            },
            [&window]() { return window.readClipboardText(); });
        form->addChild(std::move(textField), fixedHeight(34.0F));

        auto checkboxLabel = std::make_unique<lotui::Label>(
            textEngine, "격자에 맞춤", controlTextStyle);
        checkboxLabel->setVerticalAlignment(
            lotui::VerticalTextAlignment::Center);
        lotui::CheckboxStyle checkboxStyle;
        checkboxStyle.boxSize = 18.0F;
        checkboxStyle.spacing = 7.0F;
        checkboxStyle.cornerRadius = 3.0F;
        form->addChild(
            std::make_unique<lotui::Checkbox>(
                std::move(checkboxLabel),
                true,
                [](bool checked) {
                    std::cout << "checkbox-changed: "
                              << std::boolalpha << checked << '\n';
                }, checkboxStyle),
            fixedHeight(28.0F));

        lotui::NumericInputOptions numberOptions;
        numberOptions.value = 10.0;
        numberOptions.minimum = 0.5;
        numberOptions.maximum = 100.0;
        numberOptions.step = 0.5;
        numberOptions.decimalPlaces = 1;
        numberOptions.preferredSize = {0.0F, 34.0F};
        lotui::NumericInputStyle numberStyle;
        numberStyle.normal = fieldStyle.normal;
        numberStyle.hovered = fieldStyle.hovered;
        numberStyle.contentPadding = {8.0F, 4.0F, 4.0F, 4.0F};
        numberStyle.stepButtonWidth = 24.0F;
        numberStyle.cornerRadius = 4.0F;
        form->addChild(
            std::make_unique<lotui::NumericInput>(
                textEngine,
                numberOptions,
                [](double value) {
                    std::cout << "numeric-input-changed: "
                              << value << '\n';
                },
                numberStyle,
                controlTextStyle),
            fixedHeight(34.0F));
        rightContent->addChild(
            std::move(form),
            {0.0F, {0.0F, 112.0F},
                {lotui::unboundedLayoutSize, 112.0F}});
    } else {
        rightContent->addChild(
            std::make_unique<lotui::Box>(
                lotui::Size{0.0F, 40.0F},
                lotui::Color{0.96F, 0.55F, 0.24F, 1.0F}, 4.0F),
            fixedHeight(40.0F));
    }
    rightPanel->addChild(
        std::move(rightContent),
        {1.0F, {0.0F, 120.0F},
            {lotui::unboundedLayoutSize, lotui::unboundedLayoutSize}});

    root->addChild(
        std::move(leftPanel),
        {1.7F, {260.0F, 180.0F},
            {lotui::unboundedLayoutSize, lotui::unboundedLayoutSize}});
    root->addChild(
        std::move(rightPanel),
        {1.0F, {180.0F, 180.0F},
            {lotui::unboundedLayoutSize, lotui::unboundedLayoutSize}});
    if (!textEngine) {
        return std::make_unique<lotui::WidgetTree>(std::move(root));
    }

    lotui::TextStyle ribbonTextStyle;
    ribbonTextStyle.fontFamilies = {"Noto Sans KR"};
    ribbonTextStyle.fontSize = 13.0F;
    lotui::RibbonStyle ribbonStyle;
    ribbonStyle.tabBarHeight = 29.0F;
    ribbonStyle.tabHorizontalPadding = 12.0F;
    ribbonStyle.minimumTabWidth = 64.0F;
    ribbonStyle.preferredHeight = 100.0F;
    ribbonStyle.contentPadding = {6.0F, 4.0F, 6.0F, 4.0F};
    ribbonStyle.tabCornerRadius = 4.0F;
    lotui::RibbonGroupStyle groupStyle;
    groupStyle.titleHeight = 18.0F;
    groupStyle.contentPadding = {6.0F, 4.0F, 6.0F, 3.0F};
    groupStyle.cornerRadius = 3.0F;
    lotui::ButtonStyle ribbonButtonStyle;
    ribbonButtonStyle.cornerRadius = 4.0F;
    ribbonButtonStyle.contentPadding = {6.0F, 3.0F, 6.0F, 3.0F};
    auto ribbon = std::make_unique<lotui::Ribbon>(
        textEngine,
        ribbonStyle,
        ribbonTextStyle,
        [](std::size_t, std::string_view id) {
            std::cout << "ribbon-tab-changed: " << id << '\n';
        });

    const auto commandButton = [&](
        std::string text,
        std::function<void()> callback) -> std::unique_ptr<lotui::Button> {
        auto label = std::make_unique<lotui::Label>(
            textEngine, std::move(text), ribbonTextStyle);
        label->setHorizontalAlignment(
            lotui::HorizontalTextAlignment::Center);
        label->setVerticalAlignment(
            lotui::VerticalTextAlignment::Center);
        return std::make_unique<lotui::Button>(
            std::move(label),
            lotui::Size{92.0F, 34.0F},
            std::move(callback), ribbonButtonStyle);
    };

    auto homeGroups = std::make_unique<lotui::Row>();
    lotui::LinearLayoutOptions groupRowOptions;
    groupRowOptions.spacing = 4.0F;
    groupRowOptions.crossAxisAlignment =
        lotui::CrossAxisAlignment::Stretch;
    homeGroups->setOptions(groupRowOptions);
    auto fileCommands = std::make_unique<lotui::Row>();
    lotui::LinearLayoutOptions commandOptions;
    commandOptions.spacing = 4.0F;
    commandOptions.crossAxisAlignment =
        lotui::CrossAxisAlignment::Center;
    fileCommands->setOptions(commandOptions);
    fileCommands->addChild(commandButton(
        "새로 만들기",
        []() { std::cout << "ribbon-command: new\n"; }),
        {0.0F, {92.0F, 34.0F}, {92.0F, 34.0F}});
    fileCommands->addChild(commandButton(
        "저장",
        []() { std::cout << "ribbon-command: save\n"; }),
        {0.0F, {92.0F, 34.0F}, {92.0F, 34.0F}});
    homeGroups->addChild(
        std::make_unique<lotui::RibbonGroup>(
            textEngine, "파일", std::move(fileCommands),
            groupStyle, ribbonTextStyle),
        {0.0F, {202.0F, 58.0F}, {202.0F, 58.0F}});

    auto editCommands = std::make_unique<lotui::Row>();
    editCommands->setOptions(commandOptions);
    editCommands->addChild(commandButton(
        "실행 취소",
        []() { std::cout << "ribbon-command: undo\n"; }),
        {0.0F, {92.0F, 34.0F}, {92.0F, 34.0F}});
    editCommands->addChild(commandButton(
        "다시 실행",
        []() { std::cout << "ribbon-command: redo\n"; }),
        {0.0F, {92.0F, 34.0F}, {92.0F, 34.0F}});
    homeGroups->addChild(
        std::make_unique<lotui::RibbonGroup>(
            textEngine, "편집", std::move(editCommands),
            groupStyle, ribbonTextStyle),
        {0.0F, {202.0F, 58.0F}, {202.0F, 58.0F}});
    ribbon->addTab(std::make_unique<lotui::RibbonTab>(
        "home", "홈", std::move(homeGroups)));

    auto viewGroups = std::make_unique<lotui::Row>();
    viewGroups->setOptions(groupRowOptions);
    auto displayCommands = std::make_unique<lotui::Row>();
    displayCommands->setOptions(commandOptions);
    auto gridLabel = std::make_unique<lotui::Label>(
        textEngine, "격자 표시", ribbonTextStyle);
    gridLabel->setVerticalAlignment(
        lotui::VerticalTextAlignment::Center);
    lotui::CheckboxStyle ribbonCheckboxStyle;
    ribbonCheckboxStyle.boxSize = 18.0F;
    ribbonCheckboxStyle.spacing = 7.0F;
    ribbonCheckboxStyle.cornerRadius = 3.0F;
    displayCommands->addChild(
        std::make_unique<lotui::Checkbox>(
            std::move(gridLabel), true,
            [](bool checked) {
                std::cout << "ribbon-grid: " << std::boolalpha
                          << checked << '\n';
            }, ribbonCheckboxStyle),
        {0.0F, {130.0F, 30.0F}, {130.0F, 30.0F}});
    viewGroups->addChild(
        std::make_unique<lotui::RibbonGroup>(
            textEngine, "표시", std::move(displayCommands),
            groupStyle, ribbonTextStyle),
        {0.0F, {145.0F, 58.0F}, {145.0F, 58.0F}});
    ribbon->addTab(std::make_unique<lotui::RibbonTab>(
        "view", "보기", std::move(viewGroups)));

    auto shell = std::make_unique<lotui::Column>();
    lotui::LinearLayoutOptions shellOptions;
    shellOptions.spacing = 4.0F;
    shellOptions.crossAxisAlignment = lotui::CrossAxisAlignment::Stretch;
    shell->setOptions(shellOptions);
    lotui::MenuBarStyle menuStyle;
    menuStyle.barHeight = 28.0F;
    menuStyle.popupWidth = 210.0F;
    auto menuBar = std::make_unique<lotui::MenuBar>(
        textEngine,
        std::vector<lotui::Menu>{
            {"file", "파일", {
                {lotui::MenuEntryKind::Command, "new", "새 문서", "Ctrl+N"},
                {lotui::MenuEntryKind::Command, "open", "열기...", "Ctrl+O"},
                {lotui::MenuEntryKind::Separator},
                {lotui::MenuEntryKind::Submenu, {}, "최근 문서", {}, true,
                    false, {
                        {lotui::MenuEntryKind::Command, "sample-a", "샘플 A"},
                        {lotui::MenuEntryKind::Command, "sample-b", "샘플 B"},
                    }},
                {lotui::MenuEntryKind::Command, "export", "내보내기...",
                    {}, false},
            }},
            {"edit", "편집", {
                {lotui::MenuEntryKind::Command, "undo", "실행 취소", "Ctrl+Z"},
                {lotui::MenuEntryKind::Command, "redo", "다시 실행", "Ctrl+Y"},
            }},
            {"view", "보기", {
                {lotui::MenuEntryKind::Toggle, "grid", "격자 표시", {},
                    true, true},
            }},
        },
        lotui::MenuBar::ActionHandler{}, menuStyle, ribbonTextStyle);
    auto* menuBarPointer = menuBar.get();
    menuBar->setOnAction([menuBarPointer](std::string_view id) {
        std::cout << "menu-action: " << id << '\n';
        if (id == "grid") {
            auto menus = menuBarPointer->menus();
            menus[2].entries[0].checked = !menus[2].entries[0].checked;
            menuBarPointer->setMenus(std::move(menus));
        }
    });
    shell->addChild(std::move(menuBar), fixedHeight(28.0F));
    shell->addChild(std::move(ribbon), fixedHeight(100.0F));
    lotui::DocumentTabStyle documentTabStyle;
    documentTabStyle.height = 30.0F;
    documentTabStyle.controlWidth = 26.0F;
    documentTabStyle.minimumTabWidth = 72.0F;
    documentTabStyle.maximumTabWidth = 156.0F;
    documentTabStyle.horizontalPadding = 9.0F;
    auto documentTabs = std::make_unique<lotui::DocumentTabView>(
        textEngine, documentTabStyle, ribbonTextStyle);
    auto* documentTabsPointer = documentTabs.get();
    documentTabs->setTabs({
        {"drawing-1", "Drawing 1", false, true},
        {"drawing-2", "Drawing 2", true, true},
        {"reference", "Reference", false, false},
    });
    documentTabs->setOnActivate([](std::string_view id) {
        std::cout << "document-activated: " << id << '\n';
    });
    documentTabs->setOnClose([documentTabsPointer](std::string_view id) {
        const std::string closedId(id);
        auto tabs = documentTabsPointer->tabs();
        tabs.erase(std::remove_if(tabs.begin(), tabs.end(),
            [&closedId](const lotui::DocumentTab& tab) {
                return tab.id == closedId;
            }),
            tabs.end());
        documentTabsPointer->setTabs(std::move(tabs));
        std::cout << "document-closed: " << closedId << '\n';
    });
    auto nextDocumentId = std::make_shared<unsigned>(3);
    documentTabs->setOnAdd([documentTabsPointer, nextDocumentId] {
        const std::string id = "drawing-" +
            std::to_string((*nextDocumentId)++);
        auto tabs = documentTabsPointer->tabs();
        tabs.push_back({id, "Drawing " + std::to_string(*nextDocumentId - 1),
            false, true});
        documentTabsPointer->setTabs(std::move(tabs));
        documentTabsPointer->setSelectedId(id);
        std::cout << "document-added: " << id << '\n';
    });
    shell->addChild(std::move(documentTabs), fixedHeight(30.0F));
    shell->addChild(
        std::move(root),
        {1.0F, {0.0F, 220.0F},
            {lotui::unboundedLayoutSize, lotui::unboundedLayoutSize}});
    auto popupHost = std::make_unique<lotui::PopupHost>(std::move(shell));
    menuBarPointer->setPopupHost(popupHost.get());
    return std::make_unique<lotui::WidgetTree>(std::move(popupHost));
}

void updateDemoLayout(
    lotui::WidgetTree& tree,
    const lotui::WindowMetrics& metrics) {
    const float scale = metrics.dpiScale > 0.0F ? metrics.dpiScale : 1.0F;
    const float width =
        static_cast<float>(metrics.framebufferWidth) / scale;
    const float height =
        static_cast<float>(metrics.framebufferHeight) / scale;
    tree.layout(
        {32.0F, 20.0F, std::max(0.0F, width - 64.0F),
            std::min(570.0F, std::max(0.0F, height - 40.0F))},
        {0.0F, 0.0F, width, height});
}

} // namespace lotui::example
