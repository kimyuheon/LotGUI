#include "examples/platform_probe/platform_event_bridge.h"
#include "examples/ribbon_demo/desktop_palette.h"

#include "platform/platform_backend.h"
#include "platform/runtime_paths.h"
#include "renderer/vulkan/vulkan_renderer.h"
#include "text/freetype/freetype_text_engine.h"
#include "widgets/button.h"
#include "widgets/checkbox.h"
#include "widgets/color_editor.h"
#include "widgets/label.h"
#include "widgets/linear_layout.h"
#include "widgets/numeric_input.h"
#include "widgets/text_field.h"
#include "widgets/tree_view.h"

#include <array>
#include <chrono>
#include <cmath>
#include <exception>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace {

using lotui::example::desktopColor;

struct CadObject {
    std::string id;
    std::string name;
    std::string kind;
    std::array<double, 3> position{};
    lotui::Color color{};
    bool visible{true};
};

lotui::TextStyle textStyle(float size = 14.0F) {
    lotui::TextStyle style;
    style.fontFamilies = {"Noto Sans KR"};
    style.fontSize = size;
    return style;
}

lotui::ChildLayout height(float value) {
    return {0.0F, {0.0F, value},
        {lotui::unboundedLayoutSize, value}};
}

std::unique_ptr<lotui::Label> label(
    const std::shared_ptr<const lotui::TextEngine>& text,
    std::string value, float size = 14.0F) {
    auto widget = std::make_unique<lotui::Label>(
        text, std::move(value), textStyle(size));
    widget->setColor(desktopColor(0xe1e5e9));
    widget->setVerticalAlignment(lotui::VerticalTextAlignment::Center);
    return widget;
}

struct InspectorState {
    std::shared_ptr<const lotui::TextEngine> text;
    std::vector<CadObject> objects{
        {"object:column", "기둥 A", "솔리드", {0.0, 0.0, 2.5},
            desktopColor(0x82a8c5), true},
        {"object:wall", "벽체 B", "솔리드", {4.0, 0.0, 0.0},
            desktopColor(0xc1a782), true},
        {"object:door", "문 C", "블록", {4.0, 1.2, 0.0},
            desktopColor(0x94b69e), false},
    };
    std::string selected{"object:column"};
    lotui::TreeView* tree{nullptr};
    lotui::Label* selectionTitle{nullptr};
    lotui::Label* kindValue{nullptr};
    lotui::Label* status{nullptr};
    lotui::TextField* nameField{nullptr};
    lotui::Checkbox* visibleField{nullptr};
    std::array<lotui::NumericInput*, 3> positionFields{};
    lotui::ColorEditor* colorField{nullptr};

    CadObject* current() {
        for (auto& object : objects) {
            if (object.id == selected) return &object;
        }
        return nullptr;
    }

    void select(std::string_view id) {
        selected = std::string(id);
        const auto* object = current();
        if (!object) return;
        selectionTitle->setText(object->name);
        kindValue->setText(object->kind);
        nameField->setText(object->name);
        visibleField->setChecked(object->visible);
        for (std::size_t axis = 0; axis < positionFields.size(); ++axis)
            positionFields[axis]->setValue(object->position[axis]);
        colorField->setColor(object->color);
        status->setText("선택: " + object->name);
    }

    void rename(const std::string& name) {
        auto* object = current();
        if (!object || name.empty()) return;
        object->name = name;
        tree->setLabel(object->id, name);
        selectionTitle->setText(name);
        status->setText("이름 변경: " + name);
    }

    void setVisible(bool visible) {
        auto* object = current();
        if (!object) return;
        object->visible = visible;
        tree->setChecked(object->id, visible);
        status->setText(visible ? "객체 표시" : "객체 숨김");
    }

    void setPosition(std::size_t axis, double value) {
        auto* object = current();
        if (!object) return;
        object->position[axis] = value;
        status->setText("위치 변경: " + object->name);
    }

    void resetPosition() {
        auto* object = current();
        if (!object) return;
        object->position = {0.0, 0.0, 0.0};
        for (auto* input : positionFields) input->setValue(0.0);
        status->setText("위치 초기화: " + object->name);
    }
};

std::unique_ptr<lotui::Row> fieldRow(
    const std::shared_ptr<const lotui::TextEngine>& text,
    std::string title, std::unique_ptr<lotui::Widget> control,
    float rowHeight = 32.0F) {
    auto row = std::make_unique<lotui::Row>();
    lotui::LinearLayoutOptions options;
    options.spacing = 8.0F;
    options.crossAxisAlignment = lotui::CrossAxisAlignment::Center;
    row->setOptions(options);
    row->addChild(label(text, std::move(title)),
        {0.0F, {78.0F, rowHeight}, {78.0F, rowHeight}});
    row->addChild(std::move(control),
        {1.0F, {180.0F, rowHeight},
            {lotui::unboundedLayoutSize, rowHeight}});
    return row;
}

std::unique_ptr<lotui::WidgetTree> buildUi(InspectorState& state) {
    auto root = std::make_unique<lotui::Column>();
    root->setDecoration(lotui::BoxDecoration{
        desktopColor(0x1b1d21), 0.0F});
    lotui::LinearLayoutOptions rootOptions;
    rootOptions.spacing = 8.0F;
    rootOptions.padding = {12.0F, 12.0F, 12.0F, 10.0F};
    rootOptions.crossAxisAlignment = lotui::CrossAxisAlignment::Stretch;
    root->setOptions(rootOptions);
    root->addChild(label(state.text, "CAD 객체 속성 검사", 18.0F), height(29.0F));

    auto body = std::make_unique<lotui::Row>();
    lotui::LinearLayoutOptions bodyOptions;
    bodyOptions.spacing = 10.0F;
    bodyOptions.crossAxisAlignment = lotui::CrossAxisAlignment::Stretch;
    body->setOptions(bodyOptions);

    auto treePanel = std::make_unique<lotui::Column>();
    treePanel->setDecoration(lotui::BoxDecoration{
        desktopColor(0x24282e), 3.0F});
    lotui::LinearLayoutOptions panelOptions;
    panelOptions.spacing = 7.0F;
    panelOptions.padding = lotui::EdgeInsets::all(10.0F);
    panelOptions.crossAxisAlignment = lotui::CrossAxisAlignment::Stretch;
    treePanel->setOptions(panelOptions);
    treePanel->addChild(label(state.text, "장면 트리", 15.0F), height(28.0F));

    std::vector<lotui::TreeNode> nodes{
        {"scene", "CAD 장면", 0, true, false, false, false},
        {"group:architecture", "건축", 1, true, false, false, false},
    };
    for (const auto& object : state.objects)
        nodes.push_back({object.id, object.name, 2, false, true,
            true, object.visible});
    lotui::TreeViewStyle treeStyle;
    treeStyle.background = desktopColor(0x24282e);
    treeStyle.selected = desktopColor(0x394959);
    treeStyle.hovered = desktopColor(0x303941);
    treeStyle.rowHeight = 28.0F;
    auto tree = std::make_unique<lotui::TreeView>(
        state.text, std::move(nodes), lotui::Size{280.0F, 380.0F},
        treeStyle, textStyle());
    state.tree = tree.get();
    tree->setOnSelectionChanged([&state](auto id) {
        if (id) state.select(*id);
    });
    tree->setOnCheckedChanged([&state](std::string_view id, bool checked) {
        for (auto& object : state.objects) {
            if (object.id != id) continue;
            object.visible = checked;
            if (state.selected == id) state.visibleField->setChecked(checked);
            state.status->setText(checked ? "트리에서 표시" : "트리에서 숨김");
            break;
        }
    });
    treePanel->addChild(std::move(tree),
        {1.0F, {280.0F, 200.0F},
            {280.0F, lotui::unboundedLayoutSize}});
    body->addChild(std::move(treePanel),
        {0.0F, {300.0F, 300.0F},
            {300.0F, lotui::unboundedLayoutSize}});

    auto inspector = std::make_unique<lotui::Column>();
    inspector->setDecoration(lotui::BoxDecoration{
        desktopColor(0x24282e), 3.0F});
    inspector->setOptions(panelOptions);
    inspector->addChild(label(state.text, "속성", 15.0F), height(28.0F));
    auto selected = label(state.text, "", 15.0F);
    state.selectionTitle = selected.get();
    inspector->addChild(std::move(selected), height(29.0F));
    auto kind = label(state.text, "");
    state.kindValue = kind.get();
    inspector->addChild(fieldRow(state.text, "종류", std::move(kind)),
        height(32.0F));
    auto name = std::make_unique<lotui::TextField>(
        state.text, "", lotui::Size{260.0F, 30.0F},
        lotui::TextField::ChangedHandler{},
        [&state](const std::string& value) { state.rename(value); },
        [] {
            lotui::TextFieldStyle style;
            style.normal = desktopColor(0x343c46);
            style.hovered = desktopColor(0x3d4854);
            style.text = desktopColor(0xf0f2f4);
            style.focusRing = desktopColor(0x809db5);
            style.focusRingWidth = 1.0F;
            style.cornerRadius = 3.0F;
            style.contentPadding = {8.0F, 4.0F, 8.0F, 4.0F};
            return style;
        }(), textStyle());
    state.nameField = name.get();
    inspector->addChild(fieldRow(state.text, "이름", std::move(name)),
        height(32.0F));
    lotui::CheckboxStyle checkboxStyle;
    checkboxStyle.boxSize = 16.0F;
    checkboxStyle.checked = desktopColor(0x79acd4);
    checkboxStyle.unchecked = desktopColor(0x38434c);
    checkboxStyle.focusRing = desktopColor(0x809db5);
    checkboxStyle.focusRingWidth = 1.0F;
    auto visible = std::make_unique<lotui::Checkbox>(
        true, [&state](bool value) { state.setVisible(value); },
        checkboxStyle);
    state.visibleField = visible.get();
    inspector->addChild(fieldRow(state.text, "표시", std::move(visible)),
        height(32.0F));
    inspector->addChild(label(state.text, "위치 (월드 좌표)", 14.0F),
        height(25.0F));
    lotui::NumericInputStyle numericStyle;
    numericStyle.normal = desktopColor(0x343c46);
    numericStyle.hovered = desktopColor(0x3d4854);
    numericStyle.stepButton = desktopColor(0x3c4b59);
    numericStyle.stepButtonPressed = desktopColor(0x4c6881);
    numericStyle.text = desktopColor(0xf0f2f4);
    numericStyle.indicator = desktopColor(0xcbd9e5);
    numericStyle.focusRing = desktopColor(0x809db5);
    numericStyle.focusRingWidth = 1.0F;
    numericStyle.cornerRadius = 3.0F;
    numericStyle.stepButtonWidth = 22.0F;
    numericStyle.contentPadding = {8.0F, 4.0F, 4.0F, 4.0F};
    for (std::size_t axis = 0; axis < 3; ++axis) {
        lotui::NumericInputOptions inputOptions;
        inputOptions.minimum = -10000.0;
        inputOptions.maximum = 10000.0;
        inputOptions.step = 0.25;
        inputOptions.decimalPlaces = 2;
        inputOptions.preferredSize = {205.0F, 30.0F};
        auto input = std::make_unique<lotui::NumericInput>(
            state.text, inputOptions,
            [&state, axis](double value) { state.setPosition(axis, value); },
            numericStyle, textStyle());
        state.positionFields[axis] = input.get();
        inspector->addChild(fieldRow(state.text,
            std::string(1, "XYZ"[axis]), std::move(input)), height(32.0F));
    }
    lotui::ButtonStyle resetStyle;
    resetStyle.normal = desktopColor(0x343c45);
    resetStyle.hovered = desktopColor(0x45515d);
    resetStyle.pressed = desktopColor(0x2e3945);
    auto reset = std::make_unique<lotui::Button>(
        label(state.text, "위치 초기화"), lotui::Size{116.0F, 29.0F},
        [&state] { state.resetPosition(); }, resetStyle);
    inspector->addChild(std::move(reset),
        {0.0F, {116.0F, 29.0F}, {116.0F, 29.0F}});
    inspector->addChild(label(state.text, "객체 색상", 14.0F), height(25.0F));
    lotui::ColorEditorStyle colorStyle;
    colorStyle.background = desktopColor(0x24282e);
    colorStyle.track = desktopColor(0x1d2228);
    colorStyle.text = desktopColor(0xe1e5e9);
    colorStyle.focusRing = desktopColor(0x809db5);
    auto color = std::make_unique<lotui::ColorEditor>(
        state.text, lotui::Color{},
        [&state](lotui::Color value) {
            if (auto* object = state.current()) {
                object->color = value;
                state.status->setText("색상 변경: " + object->name);
            }
        }, lotui::Size{300.0F, 94.0F}, colorStyle,
        textStyle(13.0F));
    state.colorField = color.get();
    inspector->addChild(std::move(color),
        {0.0F, {300.0F, 94.0F},
            {lotui::unboundedLayoutSize, 94.0F}});
    body->addChild(std::move(inspector),
        {1.0F, {360.0F, 300.0F},
            {lotui::unboundedLayoutSize, lotui::unboundedLayoutSize}});
    root->addChild(std::move(body),
        {1.0F, {660.0F, 340.0F},
            {lotui::unboundedLayoutSize, lotui::unboundedLayoutSize}});
    auto status = label(state.text, "", 13.0F);
    state.status = status.get();
    root->addChild(std::move(status), height(20.0F));
    auto widgets = std::make_unique<lotui::WidgetTree>(std::move(root));
    state.tree->setSelectedId(state.selected);
    state.select(state.selected);
    return widgets;
}

void selfTest(lotui::WidgetTree& widgets, InspectorState& state) {
    if (state.nameField->bounds().x +
            state.nameField->bounds().width > 828.5F ||
        state.colorField->bounds().y +
            state.colorField->bounds().height > 682.5F)
        throw std::runtime_error("inspector controls overflow the panel");
    const auto row = state.tree->rowBounds("object:wall");
    const lotui::Point center{row.x + row.width * 0.5F,
        row.y + row.height * 0.5F};
    widgets.pointerMoved(center);
    widgets.pointerPressed(center, lotui::PointerButton::Primary);
    widgets.pointerReleased(center, lotui::PointerButton::Primary);
    if (state.selected != "object:wall" ||
        state.nameField->text() != "벽체 B" ||
        std::abs(state.positionFields[0]->value() - 4.0) > 0.001)
        throw std::runtime_error("tree selection did not refresh inspector");
    state.positionFields[0]->increment();
    if (std::abs(state.objects[1].position[0] - 4.25) > 0.001)
        throw std::runtime_error("position edit did not reach the model");
    state.visibleField->toggle();
    if (state.objects[1].visible || state.tree->isChecked("object:wall"))
        throw std::runtime_error("visibility edit did not update the tree");
    state.rename("벽체 수정");
    if (state.objects[1].name != "벽체 수정" ||
        state.tree->nodes()[3].label != "벽체 수정")
        throw std::runtime_error("name edit did not update the tree");
    state.resetPosition();
    if (state.objects[1].position[0] != 0.0 ||
        state.positionFields[0]->value() != 0.0)
        throw std::runtime_error("reset did not update model and controls");
}

void saveCapture(const lotui::VulkanRenderer::CapturedFrame& frame,
    const char* path) {
    std::ofstream output(path, std::ios::binary);
    if (!output || frame.rgba.empty())
        throw std::runtime_error("inspector capture failed");
    output << "P6\n" << frame.width << ' ' << frame.height << "\n255\n";
    for (std::size_t i = 0; i < frame.rgba.size(); i += 4)
        output.write(reinterpret_cast<const char*>(
            frame.rgba.data() + i), 3);
    if (!output) throw std::runtime_error("inspector capture write failed");
}

} // namespace

int main(int argc, char** argv) {
    try {
        const bool smoke = argc == 2 &&
            std::string_view(argv[1]) == "--smoke";
        const bool test = argc == 2 &&
            std::string_view(argv[1]) == "--self-test";
        const bool capture = argc == 3 &&
            std::string_view(argv[1]) == "--capture";
        if (argc > 1 && !smoke && !test && !capture) {
            std::cerr << "usage: cad_inspector_demo "
                "[--smoke|--self-test|--capture output.ppm]\n";
            return 2;
        }
        auto platform = lotui::createPlatformBackend();
        auto window = platform->createWindow(
            {"LotUI CAD Inspector", 840, 710, true});
        window->show();
        lotui::VulkanRenderer renderer(*window);
        const auto font = lotui::executableDirectory() /
            "resources" / "fonts" / "NotoSansKR-Regular.ttf";
        InspectorState state;
        state.text = std::make_shared<lotui::FreetypeTextEngine>(
            renderer, std::vector<lotui::FontSource>{{"Noto Sans KR", font}});
        auto widgets = buildUi(state);
        const auto updateLayout = [](lotui::WidgetTree& tree,
                const lotui::WindowMetrics& metrics) {
            tree.layout({0.0F, 0.0F,
                static_cast<float>(metrics.width),
                static_cast<float>(metrics.height)});
        };
        updateLayout(*widgets, window->metrics());
        if (test) selfTest(*widgets, state);
        std::vector<lotui::PaintCommand> commands;
        if (smoke || test) {
            widgets->paint(commands);
            renderer.drawFrame(commands);
            std::cout << "cad-inspector: " << commands.size()
                      << " paint commands\n";
            return 0;
        }
        bool running = true;
        int frames = 0;
        while (running) {
            bool received = false;
            lotui::PlatformEvent event{};
            while (window->pollEvent(event)) {
                received = true;
                running = lotui::example::dispatchPlatformEvent(
                    event, *widgets, *window, updateLayout) && running;
            }
            if (!running) break;
            commands.clear();
            widgets->paint(commands);
            if (capture && frames == 6) {
                saveCapture(renderer.drawFrameCaptured(commands), argv[2]);
                return 0;
            }
            renderer.drawFrame(commands);
            ++frames;
            if (!received)
                std::this_thread::sleep_for(std::chrono::milliseconds(8));
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "cad_inspector_demo failed: " << error.what() << '\n';
        return 1;
    }
}
