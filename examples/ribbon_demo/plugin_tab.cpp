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
    textStyle.fontSize = 14.0F;

    auto controls = std::make_unique<Row>();
    LinearLayoutOptions rowOptions;
    rowOptions.spacing = 8.0F;
    rowOptions.mainAxisSize = MainAxisSize::Min;
    rowOptions.crossAxisAlignment = CrossAxisAlignment::Center;
    controls->setOptions(rowOptions);

    ButtonStyle buttonStyle;
    buttonStyle.normal = {0.24F, 0.37F, 0.47F, 1.0F};
    buttonStyle.hovered = {0.31F, 0.48F, 0.57F, 1.0F};
    buttonStyle.cornerRadius = 3.0F;
    buttonStyle.contentPadding = EdgeInsets::all(2.0F);
    CheckboxStyle checkboxStyle;
    checkboxStyle.boxSize = 16.0F;
    checkboxStyle.spacing = 5.0F;
    checkboxStyle.cornerRadius = 2.0F;

    std::vector<Widget*> controlWidgets;
    controlWidgets.reserve(descriptor.controlCount);
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
            controls->addChild(std::move(button));
        } else if (control.kind == LOTUI_DEMO_CHECKBOX) {
            auto checkbox = std::make_unique<Checkbox>(
                std::move(label), control.initialValue != 0.0,
                [dispatch](bool checked) {
                    dispatch(checked ? 1.0 : 0.0);
                }, checkboxStyle);
            controlWidgets.push_back(checkbox.get());
            controls->addChild(std::move(checkbox));
        } else {
            if (!std::isfinite(control.minimum) ||
                !std::isfinite(control.maximum) ||
                !std::isfinite(control.initialValue) ||
                control.minimum >= control.maximum ||
                control.initialValue < control.minimum ||
                control.initialValue > control.maximum) {
                throw std::runtime_error("ribbon plugin slider is invalid");
            }
            controls->addChild(std::move(label));
            SliderOptions options;
            options.minimum = control.minimum;
            options.maximum = control.maximum;
            options.value = control.initialValue;
            options.preferredSize = {116.0F, 18.0F};
            auto slider = std::make_unique<Slider>(options, dispatch);
            controlWidgets.push_back(slider.get());
            controls->addChild(std::move(slider));
        }
    }

    RibbonGroupStyle groupStyle;
    groupStyle.cornerRadius = 0.0F;
    groupStyle.titleHeight = 14.0F;
    groupStyle.contentPadding = EdgeInsets::all(2.0F);
    groupStyle.titleColor = {0.87F, 0.91F, 0.96F, 1.0F};
    TextStyle titleStyle = textStyle;
    titleStyle.fontSize = 12.0F;
    auto groups = std::make_unique<Row>();
    groups->addChild(std::make_unique<RibbonGroup>(
        textEngine, descriptor.groupTitle,
        std::move(controls), groupStyle, titleStyle));
    ribbon.addTab(std::make_unique<RibbonTab>(
        descriptor.tabId, descriptor.tabTitle, std::move(groups)));
    return controlWidgets;
}

} // namespace lotui::example
