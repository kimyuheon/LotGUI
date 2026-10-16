#pragma once

#include "examples/ribbon_demo/plugin_api.h"

#include "text/text_layout.h"
#include "widgets/ribbon.h"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace lotui::example {

struct PluginActions {
    std::function<void(std::string_view, double)> onControlChanged;
    std::function<void(std::string_view, std::string_view)> onTextSubmitted;
    std::function<bool(std::string)> writeClipboard;
    std::function<std::optional<std::string>()> readClipboard;
};

std::vector<Widget*> addPluginTab(
    Ribbon& ribbon,
    const std::shared_ptr<const TextEngine>& textEngine,
    const LotuiDemoPluginV1& descriptor,
    const LotuiDemoPluginV2* textExtension,
    PluginActions actions);

} // namespace lotui::example
