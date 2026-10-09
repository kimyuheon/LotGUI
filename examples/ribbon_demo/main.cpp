#include "examples/platform_probe/platform_event_bridge.h"
#include "examples/ribbon_demo/plugin_loader.h"
#include "examples/ribbon_demo/plugin_tab.h"

#include "core/widget_tree.h"
#include "platform/platform_backend.h"
#include "platform/runtime_paths.h"
#include "renderer/vulkan/vulkan_renderer.h"
#include "text/freetype/freetype_text_engine.h"
#include "widgets/box.h"
#include "widgets/button.h"
#include "widgets/dialog.h"
#include "widgets/label.h"
#include "widgets/linear_layout.h"
#include "widgets/ribbon.h"

#include <algorithm>
#include <chrono>
#include <exception>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace {

constexpr float ribbonHeight = 72.0F;

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
    float width = 84.0F) {
    lotui::ButtonStyle style;
    style.normal = {0.24F, 0.37F, 0.47F, 1.0F};
    style.hovered = {0.31F, 0.48F, 0.57F, 1.0F};
    style.cornerRadius = 3.0F;
    style.contentPadding = lotui::EdgeInsets::all(2.0F);
    auto label = std::make_unique<lotui::Label>(
        textEngine, std::move(title), textStyle(14.0F));
    label->setHorizontalAlignment(lotui::HorizontalTextAlignment::Center);
    label->setVerticalAlignment(lotui::VerticalTextAlignment::Center);
    return std::make_unique<lotui::Button>(
        std::move(label), lotui::Size{width, 23.0F},
        std::move(action), style);
}

struct DemoState {
    std::shared_ptr<const lotui::TextEngine> textEngine;
    lotui::DialogHost* dialogs{nullptr};
    lotui::Label* status{nullptr};
    lotui::Box* preview{nullptr};
    std::vector<lotui::Widget*> pluginControls;
    bool snap{false};
    double size{35.0};

    void refreshPreview() {
        const float amount = static_cast<float>(size / 100.0);
        preview->setColor(snap
            ? lotui::Color{0.12F, 0.42F + amount * 0.25F,
                0.49F + amount * 0.25F, 1.0F}
            : lotui::Color{0.22F + amount * 0.25F,
                0.33F + amount * 0.20F, 0.42F, 1.0F});
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
};

std::unique_ptr<lotui::WidgetTree> createUi(
    DemoState& state,
    const LotuiDemoPluginV1& pluginDescriptor) {
    auto root = std::make_unique<lotui::Column>();
    root->setDecoration(lotui::BoxDecoration{
        {0.12F, 0.15F, 0.19F, 1.0F}, 0.0F});
    lotui::LinearLayoutOptions rootOptions;
    rootOptions.crossAxisAlignment = lotui::CrossAxisAlignment::Stretch;
    root->setOptions(rootOptions);

    lotui::RibbonStyle ribbonStyle;
    ribbonStyle.preferredHeight = ribbonHeight;
    ribbonStyle.tabBarHeight = 25.0F;
    ribbonStyle.contentPadding = lotui::EdgeInsets::all(2.0F);
    ribbonStyle.tabHorizontalPadding = 8.0F;
    ribbonStyle.minimumTabWidth = 62.0F;
    ribbonStyle.tabCornerRadius = 3.0F;
    auto ribbon = std::make_unique<lotui::Ribbon>(
        state.textEngine, ribbonStyle, textStyle(14.0F));

    auto commands = std::make_unique<lotui::Row>();
    lotui::LinearLayoutOptions commandOptions;
    commandOptions.spacing = 5.0F;
    commandOptions.mainAxisSize = lotui::MainAxisSize::Min;
    commandOptions.crossAxisAlignment = lotui::CrossAxisAlignment::Center;
    commands->setOptions(commandOptions);
    commands->addChild(commandButton(state.textEngine, "새 문서",
        [&state] { state.status->setText("새 문서"); }));
    commands->addChild(commandButton(state.textEngine, "열기",
        [&state] { state.status->setText("열기"); }));
    commands->addChild(commandButton(state.textEngine, "대화상자",
        [&state] { state.showDialog(); }, 96.0F));

    lotui::RibbonGroupStyle groupStyle;
    groupStyle.cornerRadius = 0.0F;
    groupStyle.titleHeight = 14.0F;
    groupStyle.contentPadding = lotui::EdgeInsets::all(2.0F);
    groupStyle.titleColor = {0.87F, 0.91F, 0.96F, 1.0F};
    auto groups = std::make_unique<lotui::Row>();
    groups->addChild(std::make_unique<lotui::RibbonGroup>(
        state.textEngine, "파일", std::move(commands),
        groupStyle, textStyle(12.0F)));
    ribbon->addTab(std::make_unique<lotui::RibbonTab>(
        "home", "홈", std::move(groups)));

    state.pluginControls = lotui::example::addPluginTab(
        *ribbon, state.textEngine,
        pluginDescriptor, {[&state](std::string_view id, double value) {
            if (id == "run") {
                state.status->setText("플러그인 실행");
            } else if (id == "snap") {
                state.snap = value != 0.0;
                state.status->setText(
                    state.snap ? "스냅 켜짐" : "스냅 꺼짐");
                state.refreshPreview();
            } else if (id == "size") {
                state.size = value;
                state.status->setText("크기 " + std::to_string(
                    static_cast<int>(value)));
                state.refreshPreview();
            }
        }});
    ribbon->selectTab(pluginDescriptor.tabId);
    root->addChild(std::move(ribbon), fixedHeight(ribbonHeight));

    auto workspace = std::make_unique<lotui::Column>();
    lotui::LinearLayoutOptions workspaceOptions;
    workspaceOptions.spacing = 12.0F;
    workspaceOptions.padding = {20.0F, 20.0F, 20.0F, 20.0F};
    workspaceOptions.crossAxisAlignment = lotui::CrossAxisAlignment::Stretch;
    workspace->setOptions(workspaceOptions);
    workspace->addChild(std::make_unique<lotui::Label>(
        state.textEngine, "LotUI Workspace", textStyle(18.0F)),
        fixedHeight(28.0F));
    auto status = std::make_unique<lotui::Label>(
        state.textEngine, "준비", textStyle(14.0F));
    state.status = status.get();
    workspace->addChild(std::move(status), fixedHeight(22.0F));
    auto preview = std::make_unique<lotui::Box>(
        lotui::Size{0.0F, 80.0F},
        lotui::Color{0.22F, 0.33F, 0.42F, 1.0F});
    state.preview = preview.get();
    workspace->addChild(std::move(preview), fixedHeight(80.0F));
    root->addChild(std::move(workspace),
        {1.0F, {}, {lotui::unboundedLayoutSize, lotui::unboundedLayoutSize}});

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

void exercisePlugin(lotui::WidgetTree& tree, DemoState& state) {
    if (state.pluginControls.size() != 3) {
        throw std::runtime_error("expected three plugin controls");
    }
    const auto click = [&tree](lotui::Point point) {
        tree.pointerMoved(point);
        tree.pointerPressed(point, lotui::PointerButton::Primary);
        tree.pointerReleased(point, lotui::PointerButton::Primary);
    };
    const auto button = state.pluginControls[0]->bounds();
    click({button.x + button.width * 0.5F,
        button.y + button.height * 0.5F});
    if (state.status->text() != "플러그인 실행") {
        throw std::runtime_error("plugin button callback was not routed");
    }
    const auto checkbox = state.pluginControls[1]->bounds();
    click({checkbox.x + 8.0F,
        checkbox.y + checkbox.height * 0.5F});
    if (!state.snap) {
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
    if (state.size < 80.0) {
        throw std::runtime_error("plugin slider callback was not routed");
    }
}

} // namespace

int main(int argc, char** argv) {
    try {
        const std::string mode = argc == 2 ? argv[1] : "";
        if (argc > 2 || (argc == 2 && mode != "--smoke" &&
                mode != "--input-test")) {
            std::cerr << "usage: ribbon_demo [--smoke|--input-test]\n";
            return 2;
        }
        const bool smoke = !mode.empty();
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
                {"Noto Sans KR", fontFile}});
        auto tree = createUi(state, plugin.descriptor());
        updateLayout(*tree, window->metrics());
        if (mode == "--input-test") {
            exercisePlugin(*tree, state);
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
                renderer.drawFrame(commands);
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
