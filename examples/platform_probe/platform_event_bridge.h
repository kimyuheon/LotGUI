#pragma once

#include "core/widget_tree.h"
#include "platform/platform_backend.h"

#include <functional>
#include "platform/platform_event.h"

namespace lotui::example {

// Returns false after a native close request.
using PlatformLayoutHandler =
    std::function<void(WidgetTree&, const WindowMetrics&)>;

bool dispatchPlatformEvent(
    const PlatformEvent& event,
    WidgetTree& tree,
    PlatformWindow& window,
    const PlatformLayoutHandler& updateLayout);

} // namespace lotui::example
