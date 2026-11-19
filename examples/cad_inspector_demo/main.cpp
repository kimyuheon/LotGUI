#include "examples/platform_probe/platform_event_bridge.h"
#include "examples/ribbon_demo/desktop_palette.h"

#include "platform/platform_backend.h"
#include "platform/runtime_paths.h"
#include "renderer/vulkan/vulkan_renderer.h"
#include "text/freetype/freetype_text_engine.h"
#include "widgets/button.h"
#include "widgets/box.h"
#include "widgets/checkbox.h"
#include "widgets/color_editor.h"
#include "widgets/label.h"
#include "widgets/linear_layout.h"
#include "widgets/numeric_input.h"
#include "widgets/property_field.h"
#include "widgets/text_field.h"
#include "widgets/tree_view.h"

#include <array>
#include <chrono>
#include <cmath>
#include <exception>
#include <fstream>
#include <functional>
#include <iostream>
#include <locale>
#include <memory>
#include <optional>
#include <sstream>
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
    double length{12.0};
    std::string lastCommand;
};

// The standalone demo's small scene surface. An engine supplies the picked
// world point through PropertyField::acceptPoint instead of using this widget.
class PointPad final : public lotui::Widget {
public:
    using PickHandler = std::function<void(lotui::PropertyPoint)>;
    explicit PointPad(PickHandler onPick) : onPick_(std::move(onPick)) {}
    lotui::Size measure(const lotui::LayoutConstraints& constraints) const override {
        return constraints.constrain({200.0F, 300.0F});
    }
    void setArmed(bool armed) noexcept { armed_ = armed; }
    bool armed() const noexcept { return armed_; }
    lotui::PropertyPoint pointAt(lotui::Point point) const noexcept {
        const auto area = bounds();
        return {(point.x - area.x) / area.width * 100.0 - 50.0,
            50.0 - (point.y - area.y) / area.height * 100.0, 0.0};
    }
protected:
    void onPaint(std::vector<lotui::PaintCommand>& commands) const override {
        const auto area = bounds();
        const auto visible = clip();
        commands.push_back({area, visible,
            desktopColor(armed_ ? 0x223943 : 0x1b252e),
            lotui::invalidTextureId, 5.0F});
        const float spacing = 32.0F;
        const float centerX = area.x + area.width * 0.5F;
        const float centerY = area.y + area.height * 0.5F;
        for (float x = centerX; x < area.x + area.width; x += spacing) {
            commands.push_back({{x, area.y, 1.0F, area.height}, visible,
                desktopColor(0x32414d), lotui::invalidTextureId, 0.0F});
        }
        for (float x = centerX - spacing; x > area.x; x -= spacing)
            commands.push_back({{x, area.y, 1.0F, area.height}, visible,
                desktopColor(0x32414d), lotui::invalidTextureId, 0.0F});
        for (float y = centerY; y < area.y + area.height; y += spacing) {
            commands.push_back({{area.x, y, area.width, 1.0F}, visible,
                desktopColor(0x32414d), lotui::invalidTextureId, 0.0F});
        }
        for (float y = centerY - spacing; y > area.y; y -= spacing)
            commands.push_back({{area.x, y, area.width, 1.0F}, visible,
                desktopColor(0x32414d), lotui::invalidTextureId, 0.0F});
        commands.push_back({{centerX, area.y, 1.0F, area.height}, visible,
            desktopColor(0x668697), lotui::invalidTextureId, 0.0F});
        commands.push_back({{area.x, centerY, area.width, 1.0F}, visible,
            desktopColor(0x668697), lotui::invalidTextureId, 0.0F});
        commands.push_back({{centerX - 3.0F, centerY - 3.0F, 6.0F, 6.0F},
            visible, desktopColor(0x99bdc8), lotui::invalidTextureId, 3.0F});
        if (picked_) {
            commands.push_back({{picked_->x - 3.0F, picked_->y - 3.0F,
                6.0F, 6.0F}, visible, desktopColor(0xf0ca8d),
                lotui::invalidTextureId, 3.0F});
        }
    }
    bool acceptsPointerEvents() const noexcept override { return true; }
    bool onPointerEvent(const lotui::WidgetPointerEvent& event) override {
        if (!armed_ || event.type != lotui::WidgetPointerEventType::Press ||
                event.button != lotui::PointerButton::Primary) return false;
        picked_ = event.position;
        armed_ = false;
        if (onPick_) onPick_(pointAt(event.position));
        return true;
    }
private:
    PickHandler onPick_;
    std::optional<lotui::Point> picked_;
    bool armed_{false};
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

std::unique_ptr<lotui::Label> mutedLabel(
    const std::shared_ptr<const lotui::TextEngine>& text,
    std::string value, float size = 12.0F) {
    auto widget = label(text, std::move(value), size);
    widget->setColor(desktopColor(0x9baab7));
    return widget;
}

std::unique_ptr<lotui::Row> sectionHeading(
    const std::shared_ptr<const lotui::TextEngine>& text,
    std::string title) {
    auto row = std::make_unique<lotui::Row>();
    lotui::LinearLayoutOptions layout;
    layout.spacing = 8.0F;
    layout.crossAxisAlignment = lotui::CrossAxisAlignment::Center;
    row->setOptions(layout);
    row->addChild(std::make_unique<lotui::Box>(
        lotui::Size{3.0F, 15.0F}, desktopColor(0x81b9c6), 1.0F),
        {0.0F, {3.0F, 15.0F}, {3.0F, 15.0F}});
    auto textLabel = label(text, std::move(title), 14.0F);
    textLabel->setColor(desktopColor(0xcbd9df));
    row->addChild(std::move(textLabel),
        {1.0F, {80.0F, 23.0F},
            {lotui::unboundedLayoutSize, 23.0F}});
    return row;
}

lotui::PropertyFieldStyle propertyStyle() {
    lotui::PropertyFieldStyle style;
    style.input.normal = desktopColor(0x313b45);
    style.input.hovered = desktopColor(0x3a4854);
    style.input.text = desktopColor(0xf0f3f5);
    style.input.focusRing = desktopColor(0x83b8c4);
    style.input.focusRingWidth = 1.0F;
    style.readOnlyInput = style.input;
    style.readOnlyInput.normal = desktopColor(0x29323b);
    style.readOnlyInput.hovered = style.readOnlyInput.normal;
    style.readOnlyInput.text = desktopColor(0xafbdc7);
    style.action.normal = desktopColor(0x344b57);
    style.action.hovered = desktopColor(0x42616e);
    style.action.pressed = desktopColor(0x28414d);
    style.action.focusRing = desktopColor(0x83b8c4);
    style.action.focusRingWidth = 1.0F;
    style.label = desktopColor(0xc5d0d7);
    style.readOnlyLabel = desktopColor(0x9baab7);
    style.actionText = desktopColor(0xe4f2f3);
    return style;
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
    lotui::Label* pointHint{nullptr};
    lotui::TextField* nameField{nullptr};
    lotui::Checkbox* visibleField{nullptr};
    std::array<lotui::NumericInput*, 3> positionFields{};
    lotui::ColorEditor* colorField{nullptr};
    lotui::PropertyField* idField{nullptr};
    lotui::PropertyField* commandField{nullptr};
    lotui::PropertyField* lengthField{nullptr};
    lotui::PropertyField* pointField{nullptr};
    PointPad* pointPad{nullptr};

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
        idField->setValue(object->id);
        lengthField->setValue(std::to_string(object->length));
        pointField->setValue(std::to_string(object->position[0]) + ", " +
            std::to_string(object->position[1]) + ", " +
            std::to_string(object->position[2]));
        pointPad->setArmed(false);
        pointHint->setText("X " + std::to_string(object->position[0]) +
            "   Y " + std::to_string(object->position[1]));
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
        pointField->setValue(std::to_string(object->position[0]) + ", " +
            std::to_string(object->position[1]) + ", " +
            std::to_string(object->position[2]));
        pointHint->setText("X " + std::to_string(object->position[0]) +
            "   Y " + std::to_string(object->position[1]));
        status->setText("위치 변경: " + object->name);
    }

    void resetPosition() {
        auto* object = current();
        if (!object) return;
        object->position = {0.0, 0.0, 0.0};
        for (auto* input : positionFields) input->setValue(0.0);
        pointField->setValue("0, 0, 0");
        pointHint->setText("X 0   Y 0");
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
        desktopColor(0x171c22), 0.0F});
    lotui::LinearLayoutOptions rootOptions;
    rootOptions.spacing = 8.0F;
    rootOptions.padding = {12.0F, 12.0F, 12.0F, 10.0F};
    rootOptions.crossAxisAlignment = lotui::CrossAxisAlignment::Stretch;
    root->setOptions(rootOptions);
    root->addChild(sectionHeading(state.text, "LOTUI  ·  객체 속성"), height(29.0F));

    auto body = std::make_unique<lotui::Row>();
    lotui::LinearLayoutOptions bodyOptions;
    bodyOptions.spacing = 10.0F;
    bodyOptions.crossAxisAlignment = lotui::CrossAxisAlignment::Stretch;
    body->setOptions(bodyOptions);

    auto treePanel = std::make_unique<lotui::Column>();
    treePanel->setDecoration(lotui::BoxDecoration{
        desktopColor(0x222a31), 5.0F});
    lotui::LinearLayoutOptions panelOptions;
    panelOptions.spacing = 7.0F;
    panelOptions.padding = lotui::EdgeInsets::all(10.0F);
    panelOptions.crossAxisAlignment = lotui::CrossAxisAlignment::Stretch;
    treePanel->setOptions(panelOptions);
    treePanel->addChild(sectionHeading(state.text, "장면"), height(28.0F));
    treePanel->addChild(mutedLabel(state.text, "모델  /  건축 객체"), height(20.0F));

    std::vector<lotui::TreeNode> nodes{
        {"scene", "CAD 장면", 0, true, false, false, false},
        {"group:architecture", "건축", 1, true, false, false, false},
    };
    for (const auto& object : state.objects)
        nodes.push_back({object.id, object.name, 2, false, true,
            true, object.visible});
    lotui::TreeViewStyle treeStyle;
    treeStyle.background = desktopColor(0x222a31);
    treeStyle.selected = desktopColor(0x344c59);
    treeStyle.hovered = desktopColor(0x2b3943);
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
        {0.0F, {200.0F, 150.0F}, {200.0F, 150.0F}});
    treePanel->addChild(sectionHeading(state.text, "작업 평면"), height(28.0F));
    treePanel->addChild(mutedLabel(state.text,
        "◎ 선택 후 평면을 클릭하세요"), height(22.0F));
    auto pointPad = std::make_unique<PointPad>([&state](lotui::PropertyPoint p) {
        state.pointField->acceptPoint(p);
    });
    state.pointPad = pointPad.get();
    treePanel->addChild(std::move(pointPad),
        {1.0F, {200.0F, 200.0F},
            {200.0F, lotui::unboundedLayoutSize}});
    auto pointHint = mutedLabel(state.text, "", 12.0F);
    state.pointHint = pointHint.get();
    treePanel->addChild(std::move(pointHint), height(22.0F));
    body->addChild(std::move(treePanel),
        {0.0F, {220.0F, 300.0F},
            {220.0F, lotui::unboundedLayoutSize}});

    auto inspector = std::make_unique<lotui::Column>();
    inspector->setDecoration(lotui::BoxDecoration{
        desktopColor(0x222a31), 5.0F});
    inspector->setOptions(panelOptions);
    inspector->addChild(sectionHeading(state.text, "기본 정보"), height(28.0F));
    auto selected = label(state.text, "", 15.0F);
    state.selectionTitle = selected.get();
    inspector->addChild(std::move(selected), height(29.0F));
    auto kind = label(state.text, "");
    state.kindValue = kind.get();
    inspector->addChild(fieldRow(state.text, "종류", std::move(kind)),
        height(32.0F));
    auto id = std::make_unique<lotui::PropertyField>(state.text,
        lotui::PropertyFieldOptions{"객체 ID", lotui::PropertyAccess::ReadOnly},
        "", lotui::PropertyField::ValueHandler{},
        lotui::PropertyField::PickHandler{},
        lotui::PropertyField::ErrorHandler{}, textStyle());
    state.idField = id.get();
    id->setStyle(propertyStyle());
    inspector->addChild(std::move(id), height(32.0F));
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
    auto command = std::make_unique<lotui::PropertyField>(state.text,
        lotui::PropertyFieldOptions{"명령 입력", lotui::PropertyAccess::WriteOnly},
        "", [&state](std::string_view value) {
            if (auto* object = state.current()) object->lastCommand = value;
            state.status->setText("명령 값을 전달했습니다");
        }, lotui::PropertyField::PickHandler{},
        lotui::PropertyField::ErrorHandler{}, textStyle());
    state.commandField = command.get();
    command->setStyle(propertyStyle());
    inspector->addChild(std::move(command), height(32.0F));
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
    auto length = std::make_unique<lotui::PropertyField>(state.text,
        lotui::PropertyFieldOptions{"길이 계산", lotui::PropertyAccess::ReadWrite,
            false, true}, "12", [&state](std::string_view value) {
            if (auto* object = state.current())
                object->length = std::stod(std::string(value));
            state.status->setText("계산 결과를 속성에 적용했습니다");
        }, lotui::PropertyField::PickHandler{},
        [&state](std::string_view) {
            state.status->setText("수식을 확인하세요");
        }, textStyle());
    state.lengthField = length.get();
    length->setStyle(propertyStyle());
    inspector->addChild(std::move(length), height(32.0F));
    inspector->addChild(sectionHeading(state.text, "위치  ·  월드 좌표"),
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
    auto point = std::make_unique<lotui::PropertyField>(state.text,
        lotui::PropertyFieldOptions{"클릭 좌표", lotui::PropertyAccess::ReadWrite,
            true, false}, "", [&state](std::string_view value) {
            double x = 0.0, y = 0.0, z = 0.0;
            char first = 0, second = 0, trailing = 0;
            std::istringstream input(std::string{value});
            input.imbue(std::locale::classic());
            if (!(input >> x >> first >> y >> second >> z) ||
                    first != ',' || second != ',' || input >> trailing ||
                    !std::isfinite(x) || !std::isfinite(y) ||
                    !std::isfinite(z)) {
                state.status->setText("좌표 형식: x, y, z");
                return;
            }
            if (auto* object = state.current()) object->position = {x, y, z};
            for (std::size_t axis = 0; axis < 3; ++axis)
                state.positionFields[axis]->setValue(
                    state.current()->position[axis]);
            state.status->setText("장면 클릭 좌표를 적용했습니다");
        }, [&state] {
            state.pointPad->setArmed(true);
            state.status->setText("왼쪽 장면에서 좌표를 클릭하세요");
        }, lotui::PropertyField::ErrorHandler{}, textStyle());
    state.pointField = point.get();
    point->setStyle(propertyStyle());
    inspector->addChild(std::move(point), height(32.0F));
    lotui::ButtonStyle resetStyle;
    resetStyle.normal = desktopColor(0x343c45);
    resetStyle.hovered = desktopColor(0x45515d);
    resetStyle.pressed = desktopColor(0x2e3945);
    auto reset = std::make_unique<lotui::Button>(
        label(state.text, "위치 초기화"), lotui::Size{116.0F, 29.0F},
        [&state] { state.resetPosition(); }, resetStyle);
    inspector->addChild(std::move(reset),
        {0.0F, {116.0F, 29.0F}, {116.0F, 29.0F}});
    inspector->addChild(sectionHeading(state.text, "외관  ·  객체 색상"), height(25.0F));
    lotui::ColorEditorStyle colorStyle;
    colorStyle.background = desktopColor(0x222a31);
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
        {1.0F, {600.0F, 340.0F},
            {lotui::unboundedLayoutSize, lotui::unboundedLayoutSize}});
    auto status = label(state.text, "", 13.0F);
    status->setColor(desktopColor(0xa5b7c1));
    state.status = status.get();
    root->addChild(std::move(status), height(20.0F));
    auto widgets = std::make_unique<lotui::WidgetTree>(std::move(root));
    state.tree->setSelectedId(state.selected);
    state.select(state.selected);
    return widgets;
}

void selfTest(lotui::WidgetTree& widgets, InspectorState& state,
        lotui::Size viewport) {
    if (state.nameField->bounds().x +
            state.nameField->bounds().width > viewport.width - 10.0F ||
        state.colorField->bounds().y +
            state.colorField->bounds().height > viewport.height - 20.0F ||
        state.pointPad->bounds().y + state.pointPad->bounds().height >
            viewport.height - 20.0F ||
        state.lengthField->childAt(2).bounds().x +
            state.lengthField->childAt(2).bounds().width > viewport.width - 10.0F ||
        state.pointField->childAt(2).bounds().x +
            state.pointField->childAt(2).bounds().width > viewport.width - 10.0F)
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
    if (state.idField->value() != "object:wall" ||
            !state.idField->input().isReadOnly() || state.idField->submit())
        throw std::runtime_error("read-only property must stay readable");
    state.commandField->input().setText("build wall");
    if (!state.commandField->submit() ||
            state.objects[1].lastCommand != "build wall" ||
            state.commandField->value() || !state.commandField->input().text().empty())
        throw std::runtime_error("write-only property must clear after submit");
    state.lengthField->input().setText("(10 + 2) * 3");
    if (!state.lengthField->submit() || state.objects[1].length != 36.0)
        throw std::runtime_error("calculator result must reach the model");
    state.lengthField->input().setText("1 / 0");
    if (state.lengthField->submit() || state.objects[1].length != 36.0)
        throw std::runtime_error("invalid expression must not change the model");
    state.pointField->requestPick();
    if (!state.pointPad->armed())
        throw std::runtime_error("point field must request a scene pick");
    const auto area = state.pointPad->bounds();
    const lotui::Point pick{area.x + area.width * 0.75F,
        area.y + area.height * 0.25F};
    widgets.pointerPressed(pick, lotui::PointerButton::Primary);
    widgets.pointerReleased(pick, lotui::PointerButton::Primary);
    if (state.pointPad->armed() || state.objects[1].position[0] != 25.0 ||
            state.objects[1].position[1] != 25.0)
        throw std::runtime_error("scene click must update world coordinates");
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
            {"LotUI CAD Inspector", 840, 1000, true});
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
            const float scale = metrics.dpiScale > 0.0F ?
                metrics.dpiScale : 1.0F;
            tree.layout({0.0F, 0.0F,
                static_cast<float>(metrics.framebufferWidth) / scale,
                static_cast<float>(metrics.framebufferHeight) / scale});
        };
        updateLayout(*widgets, window->metrics());
        if (test) {
            const auto metrics = window->metrics();
            const float scale = metrics.dpiScale > 0.0F ?
                metrics.dpiScale : 1.0F;
            selfTest(*widgets, state,
                {static_cast<float>(metrics.framebufferWidth) / scale,
                 static_cast<float>(metrics.framebufferHeight) / scale});
        }
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
