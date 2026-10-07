#include "examples/ribbon_demo/plugin_tab.h"

#include "widgets/button.h"
#include "widgets/checkbox.h"
#include "widgets/label.h"
#include "widgets/linear_layout.h"
#include "widgets/slider.h"

#include <memory>
#include <utility>

namespace lotui::example {

void addPluginTab(
    Ribbon& ribbon,
    const std::shared_ptr<const TextEngine>& textEngine,
    PluginActions actions) {
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
    auto runLabel = std::make_unique<Label>(
        textEngine, "실행", textStyle);
    runLabel->setHorizontalAlignment(HorizontalTextAlignment::Center);
    runLabel->setVerticalAlignment(VerticalTextAlignment::Center);
    controls->addChild(std::make_unique<Button>(
        std::move(runLabel), Size{72.0F, 23.0F},
        std::move(actions.run), buttonStyle));

    CheckboxStyle checkboxStyle;
    checkboxStyle.boxSize = 16.0F;
    checkboxStyle.spacing = 5.0F;
    checkboxStyle.cornerRadius = 2.0F;
    auto enabledLabel = std::make_unique<Label>(
        textEngine, "스냅", textStyle);
    enabledLabel->setVerticalAlignment(VerticalTextAlignment::Center);
    controls->addChild(std::make_unique<Checkbox>(
        std::move(enabledLabel), false,
        std::move(actions.setEnabled), checkboxStyle));

    auto sizeLabel = std::make_unique<Label>(
        textEngine, "크기", textStyle);
    sizeLabel->setVerticalAlignment(VerticalTextAlignment::Center);
    controls->addChild(std::move(sizeLabel));
    SliderOptions sliderOptions;
    sliderOptions.value = 35.0;
    sliderOptions.preferredSize = {116.0F, 18.0F};
    controls->addChild(std::make_unique<Slider>(
        sliderOptions, std::move(actions.setSize)));

    RibbonGroupStyle groupStyle;
    groupStyle.cornerRadius = 0.0F;
    groupStyle.titleHeight = 14.0F;
    groupStyle.contentPadding = EdgeInsets::all(2.0F);
    groupStyle.titleColor = {0.87F, 0.91F, 0.96F, 1.0F};
    TextStyle titleStyle = textStyle;
    titleStyle.fontSize = 12.0F;
    auto groups = std::make_unique<Row>();
    groups->addChild(std::make_unique<RibbonGroup>(
        textEngine, "도구", std::move(controls), groupStyle, titleStyle));
    ribbon.addTab(std::make_unique<RibbonTab>(
        "plugin", "플러그인", std::move(groups)));
}

} // namespace lotui::example
