#include "api/VulkanCAD_API.h"

#include "core/widget_tree.h"
#include "platform/runtime_paths.h"
#include "renderer/vulkan/vulkan_embedded_renderer.h"
#include "text/freetype/freetype_text_engine.h"
#include "widgets/box.h"
#include "widgets/button.h"
#include "widgets/checkbox.h"
#include "widgets/dialog.h"
#include "widgets/label.h"
#include "widgets/linear_layout.h"
#include "widgets/ribbon.h"
#include "widgets/slider.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <functional>
#include <iostream>
#include <iterator>
#include <memory>
#include <map>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace {

constexpr float kRibbonHeight = 72.0F;

template<typename Handle>
Handle fromBits(std::uint64_t bits) noexcept {
    if constexpr (std::is_pointer_v<Handle>) {
        return reinterpret_cast<Handle>(static_cast<std::uintptr_t>(bits));
    } else {
        return static_cast<Handle>(bits);
    }
}

struct Demo {
    lotui::VulkanEmbeddedRenderer renderer;
    std::shared_ptr<lotui::FreetypeTextEngine> text;
    std::unique_ptr<lotui::WidgetTree> tree;
    lotui::DialogHost* host{nullptr};
    lotui::Column* root{nullptr};
    lotui::Ribbon* ribbon{nullptr};
    lotui::ModelessDialogId modelessId{lotui::invalidModelessDialogId};
    lotui::Button* openButton{nullptr};
    lotui::Slider* slider{nullptr};
    lotui::Button* pluginButton{nullptr};
    unsigned int pluginItemId{0};
    std::vector<unsigned int> pluginIds;
    std::map<unsigned int, lotui::Checkbox*> pluginCheckboxes;
    std::map<unsigned int, lotui::Slider*> pluginSliders;
    bool primaryDown{false};
    std::uint32_t lastInputFlags{0};

    explicit Demo(unsigned int itemId) : pluginItemId(itemId) {
        const auto font = lotui::executableDirectory() /
            "fonts/NotoSansKR-Regular.ttf";
        if (!std::filesystem::exists(font)) {
            throw std::runtime_error("LotUI font is missing: " + font.string());
        }
        text = std::make_shared<lotui::FreetypeTextEngine>(renderer,
            std::vector<lotui::FontSource>{{"Noto Sans KR", font}});
        auto content = buildContent();
        auto dialogHost = std::make_unique<lotui::DialogHost>(
            std::move(content));
        host = dialogHost.get();
        tree = std::make_unique<lotui::WidgetTree>(std::move(dialogHost));
        pluginIds = registeredRibbonIds();

        auto modelessContent = std::make_unique<lotui::Label>(
            text, "LotUI modeless", labelStyle());
        auto modeless = std::make_unique<lotui::Dialog>(
            std::move(modelessContent), lotui::Size{196.0F, 140.0F});
        modeless->setTitle(std::make_unique<lotui::Label>(
            text, "LotUI dialog", labelStyle()));
        modelessId = host->showModeless(std::move(modeless),
            {600.0F, 280.0F, 196.0F, 140.0F});
    }

    static std::vector<unsigned int> registeredRibbonIds() {
        std::vector<unsigned int> ids;
        for (unsigned int i = 0; i < CAD_GetUiItemCount(); ++i) {
            const int kind = CAD_GetUiItemKind(i);
            if (kind == 3 || kind == 5 || kind == 6) {
                ids.push_back(CAD_GetUiItemId(i));
            }
        }
        return ids;
    }

    void refreshPluginUi() {
        if (primaryDown) return;
        auto ids = registeredRibbonIds();
        if (ids == pluginIds) {
            for (const auto& [id, checkbox] : pluginCheckboxes) {
                double value = 0.0;
                if (CAD_GetUiControlValue(id, &value, nullptr, nullptr)) {
                    checkbox->setChecked(value != 0.0);
                }
            }
            for (const auto& [id, slider] : pluginSliders) {
                double value = 0.0;
                if (!slider->isDragging() &&
                    CAD_GetUiControlValue(id, &value, nullptr, nullptr)) {
                    slider->setValue(value);
                }
            }
            return;
        }
        const std::string selected(ribbon->selectedId());
        host->setContent(buildContent());
        ribbon->selectTab(selected);
        pluginIds = std::move(ids);
    }

    static lotui::TextStyle labelStyle() {
        lotui::TextStyle style;
        style.fontFamilies = {"Noto Sans KR"};
        style.fontSize = 14.0F;
        return style;
    }

    std::unique_ptr<lotui::Button> textButton(
        std::string title, std::function<void()> action,
        float width = 76.0F) {
        lotui::ButtonStyle style;
        style.normal = {0.19F, 0.25F, 0.31F, 1.0F};
        style.hovered = {0.26F, 0.39F, 0.45F, 1.0F};
        style.pressed = {0.11F, 0.55F, 0.47F, 1.0F};
        style.cornerRadius = 3.0F;
        style.contentPadding = lotui::EdgeInsets::all(2.0F);
        auto label = std::make_unique<lotui::Label>(
            text, std::move(title), labelStyle());
        label->setHorizontalAlignment(lotui::HorizontalTextAlignment::Center);
        label->setVerticalAlignment(lotui::VerticalTextAlignment::Center);
        return std::make_unique<lotui::Button>(
            std::move(label), lotui::Size{width, 23.0F},
            std::move(action), style);
    }

    std::unique_ptr<lotui::Row> groupButtons(
        const std::vector<std::pair<std::string, std::string>>& actions) {
        auto row = std::make_unique<lotui::Row>();
        lotui::LinearLayoutOptions layout;
        layout.spacing = 4.0F;
        layout.mainAxisSize = lotui::MainAxisSize::Min;
        row->setOptions(layout);
        for (const auto& [title, command] : actions) {
            row->addChild(textButton(title, [command] {
                CAD_ExecuteCommand(command.c_str());
            }));
        }
        return row;
    }

    std::unique_ptr<lotui::Widget> buildContent() {
        auto root = std::make_unique<lotui::Column>();
        this->root = root.get();
        lotui::LinearLayoutOptions rootLayout;
        rootLayout.padding.top = 27.0F;
        rootLayout.crossAxisAlignment = lotui::CrossAxisAlignment::Stretch;
        root->setOptions(rootLayout);

        lotui::RibbonStyle ribbonStyle;
        ribbonStyle.preferredHeight = kRibbonHeight;
        ribbonStyle.tabBarHeight = 25.0F;
        ribbonStyle.contentPadding = lotui::EdgeInsets::all(2.0F);
        ribbonStyle.tabHorizontalPadding = 8.0F;
        ribbonStyle.minimumTabWidth = 62.0F;
        ribbonStyle.tabCornerRadius = 3.0F;
        auto ribbonWidget = std::make_unique<lotui::Ribbon>(
            text, ribbonStyle, labelStyle());
        ribbon = ribbonWidget.get();
        auto addGroup = [this](lotui::Row& row, std::string title,
            std::unique_ptr<lotui::Widget> controls) {
            lotui::RibbonGroupStyle style;
            style.cornerRadius = 0.0F;
            style.titleHeight = 14.0F;
            style.contentPadding = lotui::EdgeInsets::all(2.0F);
            style.titleColor = {0.87F, 0.91F, 0.96F, 1.0F};
            auto titleStyle = labelStyle();
            titleStyle.fontSize = 12.0F;
            row.addChild(std::make_unique<lotui::RibbonGroup>(text,
                std::move(title), std::move(controls), style,
                std::move(titleStyle)));
        };
        auto addTab = [&](std::string id, std::string title,
            std::string group, std::unique_ptr<lotui::Widget> controls) {
            auto row = std::make_unique<lotui::Row>();
            addGroup(*row, std::move(group), std::move(controls));
            ribbonWidget->addTab(std::make_unique<lotui::RibbonTab>(
                std::move(id), std::move(title), std::move(row)));
        };
        addTab("home", "홈", "기본", groupButtons({
            {"선", "line"}, {"원", "circle"}, {"상자", "box"}}));
        addTab("2d", "2D", "스케치", groupButtons({
            {"선", "line"}, {"원", "circle"}, {"폴리선", "polyline"}}));
        addTab("3d", "3D", "모델", groupButtons({
            {"상자", "box"}, {"구", "sphere"}}));

        auto controls = std::make_unique<lotui::Row>();
        lotui::LinearLayoutOptions controlLayout;
        controlLayout.spacing = 5.0F;
        controlLayout.mainAxisSize = lotui::MainAxisSize::Min;
        controls->setOptions(controlLayout);
        auto open = textButton("대화상자", [this] { showModal(); }, 94.0F);
        openButton = open.get();
        controls->addChild(std::move(open));
        lotui::SliderOptions sliderOptions;
        sliderOptions.value = 35.0;
        sliderOptions.preferredSize = {124.0F, 18.0F};
        auto sliderWidget = std::make_unique<lotui::Slider>(sliderOptions);
        slider = sliderWidget.get();
        controls->addChild(std::move(sliderWidget));
        addTab("controls", "컨트롤", "입력", std::move(controls));

        struct PluginControl {
            unsigned int id{0};
            int kind{0};
            std::string title;
            std::string command;
            double minimum{0.0};
            double maximum{1.0};
            double value{0.0};
        };
        struct PluginGroup {
            std::string title;
            std::vector<PluginControl> items;
        };
        struct PluginTab {
            std::string title;
            std::vector<PluginGroup> groups;
        };
        std::vector<PluginTab> pluginTabs;
        for (unsigned int i = 0; i < CAD_GetUiItemCount(); ++i) {
            const int kind = CAD_GetUiItemKind(i);
            if (kind != 3 && kind != 5 && kind != 6) continue;
            const auto read = [i](auto getter) {
                const int count = getter(i, nullptr, 0);
                if (count <= 0) return std::string{};
                std::string value(static_cast<std::size_t>(count) + 1, '\0');
                getter(i, value.data(), count + 1);
                value.resize(std::strlen(value.c_str()));
                return value;
            };
            const std::string path = read(CAD_GetUiItemPath);
            const auto slash = path.find('/');
            const std::string tab = slash == std::string::npos
                ? path : path.substr(0, slash);
            const std::string group = slash == std::string::npos
                ? "도구" : path.substr(slash + 1);
            if (tab.empty()) continue;
            PluginControl item;
            item.id = CAD_GetUiItemId(i);
            item.kind = kind;
            item.title = read(CAD_GetUiItemTitle);
            item.command = read(CAD_GetUiItemCommand);
            if (kind != 3 && !CAD_GetUiControlValue(item.id,
                    &item.value, &item.minimum, &item.maximum)) {
                continue;
            }
            auto tabIt = std::find_if(pluginTabs.begin(), pluginTabs.end(),
                [&tab](const PluginTab& candidate) {
                    return candidate.title == tab;
                });
            if (tabIt == pluginTabs.end()) {
                pluginTabs.push_back({tab, {}});
                tabIt = std::prev(pluginTabs.end());
            }
            auto groupIt = std::find_if(
                tabIt->groups.begin(), tabIt->groups.end(),
                [&group](const PluginGroup& candidate) {
                    return candidate.title == group;
                });
            if (groupIt == tabIt->groups.end()) {
                tabIt->groups.push_back({group, {}});
                groupIt = std::prev(tabIt->groups.end());
            }
            groupIt->items.push_back(std::move(item));
        }
        pluginButton = nullptr;
        pluginCheckboxes.clear();
        pluginSliders.clear();
        for (const auto& tab : pluginTabs) {
            auto row = std::make_unique<lotui::Row>();
            for (const auto& group : tab.groups) {
                auto buttons = std::make_unique<lotui::Row>();
                lotui::LinearLayoutOptions options;
                options.spacing = 4.0F;
                options.mainAxisSize = lotui::MainAxisSize::Min;
                buttons->setOptions(options);
                for (const auto& item : group.items) {
                    if (item.kind == 5) {
                        lotui::CheckboxStyle style;
                        style.boxSize = 14.0F;
                        style.spacing = 3.0F;
                        auto checkbox = std::make_unique<lotui::Checkbox>(
                            std::make_unique<lotui::Label>(
                                text, item.title, labelStyle()),
                            item.value != 0.0,
                            [id = item.id, command = item.command](bool value) {
                                if (CAD_SetUiControlValue(id, value ? 1.0 : 0.0)) {
                                    CAD_ExecuteCommand(command.c_str());
                                }
                            }, style);
                        pluginCheckboxes[item.id] = checkbox.get();
                        buttons->addChild(std::move(checkbox));
                    } else if (item.kind == 6) {
                        auto control = std::make_unique<lotui::Row>();
                        lotui::LinearLayoutOptions options;
                        options.mainAxisSize = lotui::MainAxisSize::Min;
                        options.spacing = 3.0F;
                        control->setOptions(options);
                        control->addChild(std::make_unique<lotui::Label>(
                            text, item.title, labelStyle()));
                        lotui::SliderOptions sliderOptions;
                        sliderOptions.minimum = item.minimum;
                        sliderOptions.maximum = item.maximum;
                        sliderOptions.value = item.value;
                        sliderOptions.preferredSize = {100.0F, 18.0F};
                        auto sliderWidget = std::make_unique<lotui::Slider>(
                            sliderOptions,
                            [id = item.id, command = item.command](double value) {
                                if (CAD_SetUiControlValue(id, value)) {
                                    CAD_ExecuteCommand(command.c_str());
                                }
                            });
                        pluginSliders[item.id] = sliderWidget.get();
                        control->addChild(std::move(sliderWidget));
                        buttons->addChild(std::move(control));
                    } else {
                        auto button = textButton(item.title,
                            [command = item.command] {
                                CAD_ExecuteCommand(command.c_str());
                            });
                        if (item.id == pluginItemId) pluginButton = button.get();
                        buttons->addChild(std::move(button));
                    }
                }
                addGroup(*row, group.title, std::move(buttons));
            }
            ribbonWidget->addTab(std::make_unique<lotui::RibbonTab>(
                "plugin:" + tab.title, tab.title, std::move(row)));
        }
        root->addChild(std::move(ribbonWidget));
        return root;
    }

    void layout(int width, int height) {
        auto options = root->options();
        const float top = CAD_GetOverlayRibbonTop();
        if (top > 0.0F) options.padding.top = top;
        root->setOptions(options);
        host->setModelessBounds(modelessId, {
            std::max(12.0F, static_cast<float>(width) - 212.0F),
            std::max(150.0F, static_cast<float>(height) * 0.4F),
            196.0F, 140.0F});
        tree->layout({0.0F, 0.0F,
            static_cast<float>(width), static_cast<float>(height)});
    }

    void showModal() {
        if (host->hasModal()) {
            return;
        }
        lotui::ButtonStyle closeStyle;
        closeStyle.normal = {0.81F, 0.34F, 0.29F, 1.0F};
        closeStyle.hovered = {0.94F, 0.42F, 0.33F, 1.0F};
        closeStyle.cornerRadius = 5.0F;
        auto close = std::make_unique<lotui::Button>(
            std::make_unique<lotui::Label>(text, "닫기", labelStyle()),
            lotui::Size{90.0F, 30.0F},
            [this] { host->dismissModal(); }, closeStyle);
        auto dialog = std::make_unique<lotui::Dialog>(
            std::move(close), lotui::Size{250.0F, 130.0F});
        dialog->setTitle(std::make_unique<lotui::Label>(
            text, "LotUI modal", labelStyle()));
        host->showModal(std::move(dialog));
    }

    std::uint32_t input(const CAD_OverlayPointerInfo& source) {
        if (source.windowWidth <= 0 || source.windowHeight <= 0) {
            return 0;
        }
        refreshPluginUi();
        layout(source.windowWidth, source.windowHeight);

        const lotui::Point pointer{
            static_cast<float>(source.x),
            static_cast<float>(source.y)};
        const auto move = tree->pointerMoved(pointer);
        const bool down = (source.buttons & 1U) != 0;
        bool handled = move.handled;
        if (down && !primaryDown) {
            handled |= tree->pointerPressed(
                pointer, lotui::PointerButton::Primary).handled;
        } else if (!down && primaryDown) {
            handled |= tree->pointerReleased(
                pointer, lotui::PointerButton::Primary).handled;
        }
        primaryDown = down;

        lastInputFlags = host->hasModal()
            ? CAD_OVERLAY_CAPTURE_POINTER | CAD_OVERLAY_CAPTURE_KEYBOARD
            : (handled ? CAD_OVERLAY_CAPTURE_POINTER : 0);
        return lastInputFlags;
    }

    std::uint32_t render(const CAD_OverlayFrameInfo& source) {
        if (source.framebufferWidth == 0 || source.framebufferHeight == 0 ||
            source.windowWidth <= 0 || source.windowHeight <= 0) {
            return 0;
        }
        refreshPluginUi();
        layout(source.windowWidth, source.windowHeight);

        std::vector<lotui::PaintCommand> commands;
        tree->paint(commands);
        lotui::VulkanEmbeddedFrame frame;
        frame.physicalDevice = fromBits<VkPhysicalDevice>(
            source.physicalDevice);
        frame.device = fromBits<VkDevice>(source.device);
        frame.graphicsQueue = fromBits<VkQueue>(source.graphicsQueue);
        frame.graphicsQueueFamily = source.graphicsQueueFamily;
        frame.renderPass = fromBits<VkRenderPass>(source.renderPass);
        frame.commandBuffer = fromBits<VkCommandBuffer>(source.commandBuffer);
        frame.frameIndex = source.frameIndex;
        frame.frameCount = source.frameCount;
        frame.framebufferWidth = source.framebufferWidth;
        frame.framebufferHeight = source.framebufferHeight;
        frame.dpiScale = source.dpiScale;
        if (source.structSize >= offsetof(CAD_OverlayFrameInfo, sampleCount) +
                sizeof(source.sampleCount)) {
            frame.samples = static_cast<VkSampleCountFlagBits>(
                source.sampleCount);
        }
        renderer.draw(frame, commands);

        return lastInputFlags;
    }
};

std::uint32_t input(const CAD_OverlayPointerInfo* pointer, void* user) {
    if (!pointer || !user ||
        pointer->structSize < sizeof(CAD_OverlayPointerInfo)) {
        return 0;
    }
    return static_cast<Demo*>(user)->input(*pointer);
}

std::uint32_t overlay(const CAD_OverlayFrameInfo* frame, void* user) {
    if (!frame || !user ||
        frame->structSize < offsetof(CAD_OverlayFrameInfo, modifiers) +
            sizeof(frame->modifiers)) {
        return 0;
    }
    return static_cast<Demo*>(user)->render(*frame);
}

} // namespace

int main(int argc, char** argv) {
    try {
        {
            if (!CAD_CreateEngine()) {
                throw std::runtime_error("CAD_CreateEngine failed");
            }
            CAD_SetOverlayRibbonHeight(kRibbonHeight);
            int pluginClicks = 0;
            struct ControlProbe {
                unsigned int id{0};
                int calls{0};
                double observed{-1.0};
            };
            ControlProbe toggleProbe;
            ControlProbe sliderProbe;
            constexpr unsigned int pluginOwner = 0x4C5549;
            if (!CAD_RegisterCommand("lotui_smoke_plugin", "LotUI plugin",
                    [](void* user) { ++*static_cast<int*>(user); },
                    &pluginClicks, pluginOwner)) {
                throw std::runtime_error("plugin command registration failed");
            }
            if (!CAD_RegisterCommand("lotui_smoke_toggle", "LotUI toggle",
                    [](void* user) {
                        auto* probe = static_cast<ControlProbe*>(user);
                        ++probe->calls;
                        CAD_GetUiControlValue(probe->id,
                            &probe->observed, nullptr, nullptr);
                    }, &toggleProbe, pluginOwner) ||
                !CAD_RegisterCommand("lotui_smoke_slider", "LotUI slider",
                    [](void* user) {
                        auto* probe = static_cast<ControlProbe*>(user);
                        ++probe->calls;
                        CAD_GetUiControlValue(probe->id,
                            &probe->observed, nullptr, nullptr);
                    }, &sliderProbe, pluginOwner)) {
                throw std::runtime_error("plugin control commands failed");
            }
            const unsigned int pluginItemId = CAD_AddUiItem(3,
                "플러그인/도구", "실행", "lotui_smoke_plugin", nullptr,
                pluginOwner);
            const unsigned int toggleId = CAD_AddUiControl(5,
                "플러그인/도구", "스냅", "lotui_smoke_toggle",
                0.0, 1.0, 0.0, pluginOwner);
            const unsigned int sliderId = CAD_AddUiControl(6,
                "플러그인/도구", "크기", "lotui_smoke_slider",
                0.0, 10.0, 2.0, pluginOwner);
            if (!pluginItemId || !toggleId || !sliderId) {
                throw std::runtime_error("plugin ribbon controls failed");
            }
            toggleProbe.id = toggleId;
            sliderProbe.id = sliderId;
            Demo demo(pluginItemId);
            CAD_SetOverlayRenderCallback(&overlay, &demo);
            CAD_SetOverlayInputCallback(&input, &demo);
            const bool capture = argc == 3 &&
                (std::string(argv[1]) == "--capture" ||
                 std::string(argv[1]) == "--capture-plugin");
            const bool inputTest = argc == 3 &&
                std::string(argv[1]) == "--input-test";
            if (capture || inputTest) {
                if (capture) {
                    if (std::string(argv[1]) == "--capture-plugin") {
                        demo.ribbon->selectTab("plugin:플러그인");
                    } else {
                        demo.showModal();
                    }
                }
                const std::filesystem::path capturePath = argv[2];
                bool captured = false;
                for (int tick = 0; tick < 240 && !CAD_ShouldClose(); ++tick) {
                    if (inputTest && tick == 5) {
                        demo.ribbon->selectTab("controls");
                        demo.layout(1280, 800);
                        const auto bounds = demo.openButton->bounds();
                        const double x = bounds.x + bounds.width * 0.5;
                        const double y = bounds.y + bounds.height * 0.5;
                        CAD_OnMouseMove(x, y);
                        CAD_OnMouseDown(0, x, y, 0);
                    }
                    if (inputTest && tick == 6) {
                        if ((demo.lastInputFlags &
                                CAD_OVERLAY_CAPTURE_POINTER) == 0) {
                            throw std::runtime_error(
                                "LotUI did not capture the pointer press");
                        }
                        const auto bounds = demo.openButton->bounds();
                        CAD_OnMouseUp(0,
                            bounds.x + bounds.width * 0.5,
                            bounds.y + bounds.height * 0.5, 0);
                    }
                    if (inputTest && tick == 7 &&
                        !demo.host->hasModal()) {
                        throw std::runtime_error(
                            "LotUI button did not open the modal");
                    }
                    if (inputTest && tick == 7) {
                        demo.host->dismissModal();
                    }
                    if (inputTest && tick == 8) {
                        const auto bounds = demo.slider->bounds();
                        const double x = bounds.x + bounds.width * 0.2;
                        const double y = bounds.y + bounds.height * 0.5;
                        CAD_OnMouseMove(x, y);
                        CAD_OnMouseDown(0, x, y, 0);
                    }
                    if (inputTest && tick == 9) {
                        const auto bounds = demo.slider->bounds();
                        CAD_OnMouseMove(bounds.x + bounds.width * 0.95,
                            bounds.y + bounds.height * 0.5);
                    }
                    if (inputTest && tick == 10) {
                        const auto bounds = demo.slider->bounds();
                        CAD_OnMouseUp(0,
                            bounds.x + bounds.width * 0.95,
                            bounds.y + bounds.height * 0.5, 0);
                    }
                    if (inputTest && tick == 11 &&
                        demo.slider->value() < 90.0) {
                        throw std::runtime_error(
                            "LotUI slider did not receive the drag");
                    }
                    if (inputTest && tick == 12) {
                        demo.ribbon->selectTab("plugin:플러그인");
                        demo.layout(1280, 800);
                        if (!demo.pluginButton) {
                            throw std::runtime_error("plugin button missing");
                        }
                        const auto bounds = demo.pluginButton->bounds();
                        const double x = bounds.x + bounds.width * 0.5;
                        const double y = bounds.y + bounds.height * 0.5;
                        CAD_OnMouseMove(x, y);
                        CAD_OnMouseDown(0, x, y, 0);
                    }
                    if (inputTest && tick == 13) {
                        const auto bounds = demo.pluginButton->bounds();
                        CAD_OnMouseUp(0,
                            bounds.x + bounds.width * 0.5,
                            bounds.y + bounds.height * 0.5, 0);
                    }
                    if (inputTest && tick == 14 && pluginClicks != 1) {
                        const auto bounds = demo.pluginButton->bounds();
                        std::cerr << "plugin button bounds: "
                                  << bounds.x << ',' << bounds.y << ' '
                                  << bounds.width << 'x' << bounds.height
                                  << ", capture=" << demo.lastInputFlags
                                  << ", clicks=" << pluginClicks << '\n';
                        throw std::runtime_error(
                            "plugin ribbon command was not dispatched");
                    }
                    if (inputTest && tick == 15) {
                        const auto bounds = demo.pluginCheckboxes.at(toggleId)->bounds();
                        const double x = bounds.x + 8.0;
                        const double y = bounds.y + bounds.height * 0.5;
                        CAD_OnMouseMove(x, y);
                        CAD_OnMouseDown(0, x, y, 0);
                    }
                    if (inputTest && tick == 16) {
                        const auto bounds = demo.pluginCheckboxes.at(toggleId)->bounds();
                        CAD_OnMouseUp(0, bounds.x + 8.0,
                            bounds.y + bounds.height * 0.5, 0);
                    }
                    if (inputTest && tick == 17) {
                        double value = 0.0;
                        if (!CAD_GetUiControlValue(toggleId, &value,
                                nullptr, nullptr) || value != 1.0 ||
                            toggleProbe.calls != 1 ||
                            toggleProbe.observed != 1.0) {
                            throw std::runtime_error(
                                "plugin checkbox did not update its command");
                        }
                    }
                    if (inputTest && tick == 18) {
                        const auto bounds = demo.pluginSliders.at(sliderId)->bounds();
                        const double x = bounds.x + bounds.width * 0.2;
                        const double y = bounds.y + bounds.height * 0.5;
                        CAD_OnMouseMove(x, y);
                        CAD_OnMouseDown(0, x, y, 0);
                    }
                    if (inputTest && tick == 19) {
                        const auto bounds = demo.pluginSliders.at(sliderId)->bounds();
                        CAD_OnMouseMove(bounds.x + bounds.width * 0.95,
                            bounds.y + bounds.height * 0.5);
                    }
                    if (inputTest && tick == 20) {
                        const auto bounds = demo.pluginSliders.at(sliderId)->bounds();
                        CAD_OnMouseUp(0,
                            bounds.x + bounds.width * 0.95,
                            bounds.y + bounds.height * 0.5, 0);
                    }
                    if (inputTest && tick == 21) {
                        double value = 0.0;
                        if (!CAD_GetUiControlValue(sliderId, &value,
                                nullptr, nullptr) || value < 8.0 ||
                            sliderProbe.calls == 0 ||
                            sliderProbe.observed < 8.0) {
                            throw std::runtime_error(
                                "plugin slider did not update its command");
                        }
                    }
                    if (inputTest && tick == 22) {
                        if (CAD_RemoveUiItemsByOwner(pluginOwner) != 3) {
                            throw std::runtime_error(
                                "plugin ribbon unload failed");
                        }
                        demo.refreshPluginUi();
                        if (demo.pluginButton != nullptr ||
                            demo.pluginCheckboxes.count(toggleId) != 0 ||
                            demo.pluginSliders.count(sliderId) != 0 ||
                            CAD_GetUiControlValue(toggleId, nullptr,
                                nullptr, nullptr) ||
                            CAD_GetUiControlValue(sliderId, nullptr,
                                nullptr, nullptr)) {
                            throw std::runtime_error(
                                "plugin ribbon did not reflect unload");
                        }
                    }
                    if (tick == 30 &&
                        !CAD_CaptureViewport(argv[2], false)) {
                        throw std::runtime_error("capture request failed");
                    }
                    if (!CAD_Tick()) {
                        break;
                    }
                    if (tick > 30 &&
                        std::filesystem::exists(capturePath) &&
                        std::filesystem::file_size(capturePath) > 1024) {
                        captured = true;
                        break;
                    }
                }
                if (!captured) {
                    throw std::runtime_error("capture was not written");
                }
            } else {
                while (!CAD_ShouldClose() && CAD_Tick()) {}
            }
            CAD_SetOverlayInputCallback(nullptr, nullptr);
            CAD_SetOverlayRenderCallback(nullptr, nullptr);
            CAD_SetOverlayRibbonHeight(0.0F);
            CAD_RemoveUiItemsByOwner(pluginOwner);
            CAD_UnregisterCommandsByOwner(pluginOwner);
        }
        // Demo owns Vulkan resources borrowed from the engine. It must die
        // before CAD_DestroyEngine destroys the device.
        CAD_DestroyEngine();
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        CAD_SetOverlayInputCallback(nullptr, nullptr);
        CAD_SetOverlayRenderCallback(nullptr, nullptr);
        std::cerr << "VulkanAppLotGUI failed: " << error.what() << '\n';
        CAD_DestroyEngine();
        return EXIT_FAILURE;
    }
}
