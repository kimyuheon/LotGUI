#pragma once

#include "core/widget_tree.h"
#include "platform/platform_backend.h"
#include "renderer/texture.h"
#include "text/text_layout.h"

#include <memory>

namespace lotui::example {

TextureImage createDemoMask();

std::unique_ptr<WidgetTree> createDemoUi(
    TextureId demoMaskTexture,
    const std::shared_ptr<const TextEngine>& textEngine);

void updateDemoLayout(
    WidgetTree& tree,
    const WindowMetrics& metrics);

} // namespace lotui::example
