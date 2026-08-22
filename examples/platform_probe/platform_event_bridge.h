#pragma once

#include "core/widget_tree.h"
#include "platform/platform_backend.h"
#include "platform/platform_event.h"

namespace lotui::example {

// Returns false after a native close request.
bool dispatchPlatformEvent(
    const PlatformEvent& event,
    WidgetTree& tree,
    PlatformWindow& window);

} // namespace lotui::example
