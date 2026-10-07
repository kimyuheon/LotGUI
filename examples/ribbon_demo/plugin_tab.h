#pragma once

#include "text/text_layout.h"
#include "widgets/ribbon.h"

#include <functional>
#include <memory>

namespace lotui::example {

struct PluginActions {
    std::function<void()> run;
    std::function<void(bool)> setEnabled;
    std::function<void(double)> setSize;
};

void addPluginTab(
    Ribbon& ribbon,
    const std::shared_ptr<const TextEngine>& textEngine,
    PluginActions actions);

} // namespace lotui::example
