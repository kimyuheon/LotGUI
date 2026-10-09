#pragma once

#include "examples/ribbon_demo/plugin_api.h"

#include "text/text_layout.h"
#include "widgets/ribbon.h"

#include <functional>
#include <memory>
#include <string_view>
#include <vector>

namespace lotui::example {

struct PluginActions {
    std::function<void(std::string_view, double)> onControlChanged;
};

std::vector<Widget*> addPluginTab(
    Ribbon& ribbon,
    const std::shared_ptr<const TextEngine>& textEngine,
    const LotuiDemoPluginV1& descriptor,
    PluginActions actions);

} // namespace lotui::example
