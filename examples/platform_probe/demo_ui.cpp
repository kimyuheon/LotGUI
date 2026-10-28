#include "examples/platform_probe/demo_ui.h"

#include "widgets/box.h"
#include "widgets/button.h"
#include "widgets/checkbox.h"
#include "widgets/disclosure_panel.h"
#include "widgets/document_tab_view.h"
#include "widgets/inline_text_editor.h"
#include "widgets/label.h"
#include "widgets/linear_layout.h"
#include "widgets/menu_bar.h"
#include "widgets/numeric_input.h"
#include "widgets/popup.h"
#include "widgets/ribbon.h"
#include "widgets/text_field.h"

#if __has_include("widgets/radio_group.h") && __has_include("widgets/tree_view.h")
#include "widgets/radio_group.h"
#include "widgets/tree_view.h"
#define LOTUI_DEMO_HAS_TREE_AND_RADIO 1
#endif

#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
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

#ifdef LOTUI_TREE_VIEW_HAS_MIXED_CHECK
void updateDemoTreeChecks(lotui::TreeView& tree, std::string_view id,
    bool checked) {
    const auto& nodes = tree.nodes();
    const auto found = std::find_if(nodes.begin(), nodes.end(),
        [id](const lotui::TreeNode& node) { return node.id == id; });
    if (found == nodes.end()) return;
    const std::size_t index = static_cast<std::size_t>(found - nodes.begin());
    for (std::size_t child = index + 1;
            child < nodes.size() && nodes[child].depth > nodes[index].depth;
            ++child) {
        if (nodes[child].checkable) tree.setChecked(nodes[child].id, checked);
    }
    for (std::size_t parent = nodes.size(); parent-- > 0;) {
        if (!nodes[parent].checkable) continue;
        bool anyChecked = false;
        bool anyUnchecked = false;
        for (std::size_t child = parent + 1;
                child < nodes.size() &&
                    nodes[child].depth > nodes[parent].depth;
                ++child) {
            if (nodes[child].depth != nodes[parent].depth + 1 ||
                !nodes[child].checkable) continue;
            const auto state = tree.checkState(nodes[child].id);
            anyChecked |= state != lotui::TreeCheckState::Unchecked;
            anyUnchecked |= state != lotui::TreeCheckState::Checked;
        }
        if (anyChecked || anyUnchecked) {
            tree.setCheckState(nodes[parent].id,
                anyChecked && anyUnchecked ? lotui::TreeCheckState::Mixed :
                anyChecked ? lotui::TreeCheckState::Checked :
                    lotui::TreeCheckState::Unchecked);
        }
    }
}
#endif

#ifdef LOTUI_TREE_VIEW_HAS_DROP
void moveDemoTreeNode(lotui::TreeView& tree, std::string_view source,
    std::string_view target) {
    auto nodes = tree.nodes();
    const auto sourceIt = std::find_if(nodes.begin(), nodes.end(),
        [source](const auto& node) { return node.id == source; });
    if (sourceIt == nodes.end()) return;
    const std::size_t sourceIndex = static_cast<std::size_t>(
        sourceIt - nodes.begin());
    std::size_t end = sourceIndex + 1;
    while (end < nodes.size() &&
            nodes[end].depth > nodes[sourceIndex].depth) ++end;
    std::vector<lotui::TreeNode> moved(nodes.begin() + sourceIndex,
        nodes.begin() + end);
    nodes.erase(nodes.begin() + sourceIndex, nodes.begin() + end);
    const auto targetIt = std::find_if(nodes.begin(), nodes.end(),
        [target](const auto& node) { return node.id == target; });
    if (targetIt == nodes.end()) return;
    const std::size_t targetIndex = static_cast<std::size_t>(
        targetIt - nodes.begin());
    const std::size_t targetDepth = nodes[targetIndex].depth;
    const auto relativeDepth = static_cast<std::ptrdiff_t>(
        targetDepth + 1) - static_cast<std::ptrdiff_t>(moved.front().depth);
    for (auto& node : moved) {
        node.depth = static_cast<std::size_t>(
            static_cast<std::ptrdiff_t>(node.depth) + relativeDepth);
    }
    std::size_t insertAt = targetIndex + 1;
    while (insertAt < nodes.size() &&
            nodes[insertAt].depth > targetDepth) ++insertAt;
    nodes.insert(nodes.begin() + insertAt, moved.begin(), moved.end());
    tree.setNodes(std::move(nodes));
    tree.setExpanded(target, true);
#ifdef LOTUI_TREE_VIEW_HAS_MIXED_CHECK
    updateDemoTreeChecks(tree, source, tree.isChecked(source));
#endif
    tree.ensureVisible(source);
}
#endif

std::unique_ptr<lotui::WidgetTree> createDemoUi(
    lotui::TextureId demoMaskTexture,
    const std::shared_ptr<const lotui::TextEngine>& textEngine,
    lotui::PlatformWindow& window,
    lotui::TreeView** sceneTreeOut) {
    if (sceneTreeOut) *sceneTreeOut = nullptr;
    auto popupHostSlot = std::make_shared<lotui::PopupHost*>(nullptr);
#ifdef LOTUI_DEMO_HAS_TREE_AND_RADIO
    lotui::TreeView* sceneTreePointer = nullptr;
#endif
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
#ifdef LOTUI_DEMO_HAS_TREE_AND_RADIO
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
    if (textEngine) {
        lotui::TextStyle treeTextStyle;
        treeTextStyle.fontFamilies = {"Noto Sans KR"};
        treeTextStyle.fontSize = 15.0F;
        auto treeHeading = std::make_unique<lotui::Label>(
            textEngine, "장면 트리", treeTextStyle);
        leftPanel->addChild(std::move(treeHeading), fixedHeight(20.0F));

        lotui::TreeViewStyle treeStyle;
        treeStyle.rowHeight = 24.0F;
        auto sceneTree = std::make_unique<lotui::TreeView>(
            textEngine,
            std::vector<lotui::TreeNode>{
                {"scene", "장면", 0, true, true, true, true},
                {"assembly", "조립품", 1, true, true, true, true},
                {"frame", "프레임", 2, false, true, true, true},
                {"shaft", "축", 2, false, true, true, true},
                {"fasteners", "체결 부품", 1, false, true, true, true},
                {"bolt-1", "볼트 01", 2, false, true, true, true},
                {"bolt-2", "볼트 02", 2, false, true, true, true},
                {"camera", "카메라", 1, false, true, true, false},
            },
            lotui::Size{240.0F, 180.0F}, treeStyle, treeTextStyle);
        sceneTreePointer = sceneTree.get();
        if (sceneTreeOut) *sceneTreeOut = sceneTreePointer;
#ifdef LOTUI_TREE_VIEW_HAS_MIXED_CHECK
        sceneTree->setCheckState("scene", lotui::TreeCheckState::Mixed);
#endif
        sceneTree->setOnSelectionChanged([](auto id) {
            std::cout << "tree-selection: " <<
                (id ? *id : std::string_view{"none"}) << '\n';
        });
        sceneTree->setOnExpansionChanged([](std::string_view id, bool open) {
            std::cout << "tree-expanded: " << id << ' ' <<
                std::boolalpha << open << '\n';
        });
        sceneTree->setOnCheckedChanged(
            [sceneTreePointer](std::string_view id, bool checked) {
            if (!sceneTreePointer) return;
#ifdef LOTUI_TREE_VIEW_HAS_MIXED_CHECK
            updateDemoTreeChecks(*sceneTreePointer, id, checked);
#endif
            std::cout << "tree-checked: " << id << ' ' <<
                std::boolalpha << checked << '\n';
        });
#ifdef LOTUI_TREE_VIEW_HAS_DROP
        sceneTree->setCanDrop([](std::string_view source,
                std::string_view target) {
            const bool leaf = source == "frame" || source == "shaft" ||
                source == "bolt-1" || source == "bolt-2" ||
                source == "camera";
            return leaf && (target == "scene" || target == "assembly" ||
                target == "fasteners");
        });
        sceneTree->setOnDropped([sceneTreePointer](std::string_view source,
                std::string_view target) {
            moveDemoTreeNode(*sceneTreePointer, source, target);
            std::cout << "tree-dropped: " << source << " -> " <<
                target << '\n';
        });
#endif
#ifdef LOTUI_TREE_VIEW_HAS_RENAME
        sceneTree->setOnRenameRequested(
            [popupHostSlot, sceneTreePointer, textEngine, treeTextStyle](
                    std::string_view id, lotui::Rect anchor) {
                if (!*popupHostSlot) return;
                const std::string stableId(id);
                const auto& nodes = sceneTreePointer->nodes();
                const auto node = std::find_if(nodes.begin(), nodes.end(),
                    [&stableId](const lotui::TreeNode& item) {
                        return item.id == stableId;
                    });
                if (node == nodes.end()) return;
                lotui::InlineTextEditHandlers handlers;
                handlers.validate = [](std::string_view text)
                    -> std::optional<std::string> {
                    return text.find_first_not_of(" \t\r\n") ==
                        std::string_view::npos ?
                        std::optional<std::string>{"Name is required"} :
                        std::nullopt;
                };
                handlers.committed = [sceneTreePointer, stableId](
                        std::string label) {
                    sceneTreePointer->setLabel(stableId, std::move(label));
                    std::cout << "tree-renamed: " << stableId << '\n';
                };
                lotui::TextFieldStyle editorStyle;
                editorStyle.contentPadding = {4.0F, 2.0F, 4.0F, 2.0F};
                editorStyle.cornerRadius = 2.0F;
                editorStyle.focusRingWidth = 1.0F;
                lotui::InlineTextEditor::showAt(**popupHostSlot,
                    textEngine, anchor, node->label, std::move(handlers),
                    {}, editorStyle, treeTextStyle);
            });
#endif
        leftPanel->addChild(std::move(sceneTree),
            {1.0F, {0.0F, 130.0F},
                {lotui::unboundedLayoutSize, lotui::unboundedLayoutSize}});
    }
#endif

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
        formOptions.mainAxisSize = lotui::MainAxisSize::Min;
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
#ifdef LOTUI_DEMO_HAS_TREE_AND_RADIO
        lotui::RadioGroupStyle radioStyle;
        radioStyle.rowHeight = 23.0F;
        radioStyle.indicatorSize = 13.0F;
        form->addChild(
            std::make_unique<lotui::RadioGroup>(
                textEngine,
                std::vector<lotui::RadioOption>{
                    {"union", "합집합"},
                    {"subtract", "차집합"},
                    {"intersect", "교집합"},
                },
                std::optional<std::string>{"union"},
                [](std::string_view id) {
                    std::cout << "boolean-operation: " << id << '\n';
                }, radioStyle, controlTextStyle),
            fixedHeight(69.0F));
#endif
        lotui::DisclosurePanelStyle sectionStyle;
        sectionStyle.normal = {0.18F, 0.23F, 0.28F, 1.0F};
        sectionStyle.hovered = {0.23F, 0.29F, 0.34F, 1.0F};
        sectionStyle.pressed = {0.13F, 0.18F, 0.22F, 1.0F};
        lotui::TextStyle sectionTextStyle = controlTextStyle;
        sectionTextStyle.weight = lotui::FontWeight::Medium;
        rightContent->addChild(
            std::make_unique<lotui::DisclosurePanel>(
                textEngine, "객체 속성", std::move(form), true,
                [](bool expanded) {
                    std::cout << "property-section: " << std::boolalpha
                              << expanded << '\n';
                }, sectionStyle, sectionTextStyle),
            {0.0F, {0.0F, 28.0F},
                {lotui::unboundedLayoutSize,
                    lotui::unboundedLayoutSize}});
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
    ribbonTextStyle.fontSize = 14.0F;
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
#if defined(LOTUI_DEMO_HAS_TREE_AND_RADIO) && \
    defined(LOTUI_TREE_VIEW_HAS_RENAME)
    editCommands->addChild(commandButton(
        "이름 변경",
        [sceneTreePointer]() {
            if (sceneTreePointer) {
                const auto selected = sceneTreePointer->selectedId();
                if (selected) sceneTreePointer->requestRename(*selected);
            }
        }),
        {0.0F, {92.0F, 34.0F}, {92.0F, 34.0F}});
#endif
    homeGroups->addChild(
        std::make_unique<lotui::RibbonGroup>(
            textEngine, "편집", std::move(editCommands),
            groupStyle, ribbonTextStyle),
#if defined(LOTUI_DEMO_HAS_TREE_AND_RADIO) && \
    defined(LOTUI_TREE_VIEW_HAS_RENAME)
        {0.0F, {298.0F, 58.0F}, {298.0F, 58.0F}});
#else
        {0.0F, {202.0F, 58.0F}, {202.0F, 58.0F}});
#endif
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
    *popupHostSlot = popupHost.get();
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
