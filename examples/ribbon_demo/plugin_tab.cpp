#include "examples/ribbon_demo/plugin_tab.h"

#include "widgets/button.h"
#include "widgets/checkbox.h"
#include "widgets/label.h"
#include "widgets/linear_layout.h"
#include "widgets/slider.h"

#include <cmath>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace lotui::example {

std::vector<Widget*> addPluginTab(
    Ribbon& ribbon,
    const std::shared_ptr<const TextEngine>& textEngine,
    const LotuiDemoPluginV1& descriptor,
    PluginActions actions) {
    if (descriptor.abiVersion != LOTUI_DEMO_PLUGIN_ABI_VERSION ||
        descriptor.structSize < sizeof(LotuiDemoPluginV1) ||
        descriptor.tabId == nullptr || descriptor.tabTitle == nullptr ||
        descriptor.groupTitle == nullptr || descriptor.controls == nullptr ||
        descriptor.controlCount == 0 || descriptor.controlCount > 32) {
        throw std::runtime_error("ribbon plugin descriptor is invalid");
    }
    TextStyle textStyle;
    textStyle.fontFamilies = {"Noto Sans KR"};
    textStyle.fontSize = 15.0F;

    auto controls = std::make_unique<Row>();
    LinearLayoutOptions controlsOptions;
    controlsOptions.spacing = 8.0F;
    controlsOptions.mainAxisSize = MainAxisSize::Min;
    controlsOptions.crossAxisAlignment = CrossAxisAlignment::Start;
    controls->setOptions(controlsOptions);
    LinearLayoutOptions columnOptions;
    columnOptions.spacing = 2.0F;
    columnOptions.mainAxisSize = MainAxisSize::Min;
    columnOptions.crossAxisAlignment = CrossAxisAlignment::Stretch;

    ButtonStyle buttonStyle;
    buttonStyle.normal = {0.0F, 0.0F, 0.0F, 0.0F};
    buttonStyle.hovered = {0.32F, 0.45F, 0.48F, 0.6F};
    buttonStyle.pressed = {0.35F, 0.53F, 0.54F, 0.8F};
    buttonStyle.disabled = {0.0F, 0.0F, 0.0F, 0.0F};
    buttonStyle.cornerRadius = 3.0F;
    buttonStyle.contentPadding = EdgeInsets::all(2.0F);
    CheckboxStyle checkboxStyle;
    checkboxStyle.boxSize = 16.0F;
    checkboxStyle.spacing = 5.0F;
    checkboxStyle.cornerRadius = 2.0F;

    std::vector<Widget*> controlWidgets;
    controlWidgets.reserve(descriptor.controlCount);
    Column* currentColumn = nullptr;
    for (uint32_t index = 0; index < descriptor.controlCount; ++index) {
        const LotuiDemoControlV1& control = descriptor.controls[index];
        if (control.structSize < sizeof(LotuiDemoControlV1) ||
            control.id == nullptr || control.label == nullptr ||
            control.action == nullptr ||
            (control.kind != LOTUI_DEMO_BUTTON &&
             control.kind != LOTUI_DEMO_CHECKBOX &&
             control.kind != LOTUI_DEMO_SLIDER)) {
            throw std::runtime_error("ribbon plugin control is invalid");
        }
        if (index % 3 == 0) {
            auto column = std::make_unique<Column>();
            column->setOptions(columnOptions);
            currentColumn = column.get();
            controls->addChild(std::move(column));
        }
        const auto dispatch = [control, onChanged = actions.onControlChanged]
            (double value) {
            control.action(control.context, value);
            if (onChanged) {
                onChanged(control.id, value);
            }
        };
        auto label = std::make_unique<Label>(
            textEngine, control.label, textStyle);
        label->setVerticalAlignment(VerticalTextAlignment::Center);
        if (control.kind == LOTUI_DEMO_BUTTON) {
            label->setHorizontalAlignment(HorizontalTextAlignment::Center);
            auto button = std::make_unique<Button>(
                std::move(label), Size{72.0F, 23.0F},
                [dispatch] { dispatch(1.0); }, buttonStyle);
            controlWidgets.push_back(button.get());
            currentColumn->addChild(std::move(button));
        } else if (control.kind == LOTUI_DEMO_CHECKBOX) {
            auto checkbox = std::make_unique<Checkbox>(
                std::move(label), control.initialValue != 0.0,
                [dispatch](bool checked) {
                    dispatch(checked ? 1.0 : 0.0);
                }, checkboxStyle);
            controlWidgets.push_back(checkbox.get());
            currentColumn->addChild(std::move(checkbox));
        } else {
            if (!std::isfinite(control.minimum) ||
                !std::isfinite(control.maximum) ||
                !std::isfinite(control.initialValue) ||
                control.minimum >= control.maximum ||
                control.initialValue < control.minimum ||
                control.initialValue > control.maximum) {
                throw std::runtime_error("ribbon plugin slider is invalid");
            }
            auto sliderRow = std::make_unique<Row>();
            LinearLayoutOptions rowOptions;
            rowOptions.spacing = 5.0F;
            rowOptions.mainAxisSize = MainAxisSize::Min;
            rowOptions.crossAxisAlignment = CrossAxisAlignment::Center;
            sliderRow->setOptions(rowOptions);
            sliderRow->addChild(std::move(label));
            SliderOptions options;
            options.minimum = control.minimum;
            options.maximum = control.maximum;
            options.value = control.initialValue;
            options.preferredSize = {116.0F, 18.0F};
            auto slider = std::make_unique<Slider>(options, dispatch);
            controlWidgets.push_back(slider.get());
            sliderRow->addChild(std::move(slider));
            currentColumn->addChild(std::move(sliderRow));
        }
    }

    RibbonGroupStyle groupStyle;
    groupStyle.cornerRadius = 0.0F;
    groupStyle.titleHeight = 18.0F;
    groupStyle.contentPadding = EdgeInsets::all(2.0F);
    groupStyle.titleColor = {0.87F, 0.91F, 0.96F, 1.0F};
    TextStyle titleStyle = textStyle;
    titleStyle.fontSize = 13.0F;
    auto groups = std::make_unique<Row>();
    LinearLayoutOptions groupsOptions;
    groupsOptions.crossAxisAlignment = CrossAxisAlignment::Stretch;
    groups->setOptions(groupsOptions);
    groups->addChild(std::make_unique<RibbonGroup>(
        textEngine, descriptor.groupTitle,
        std::move(controls), groupStyle, titleStyle));
    ribbon.addTab(std::make_unique<RibbonTab>(
        descriptor.tabId, descriptor.tabTitle, std::move(groups)));
    return controlWidgets;
}

} // namespace lotui::example
