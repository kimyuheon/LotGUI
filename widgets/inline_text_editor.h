#pragma once

#include "widgets/popup.h"
#include "widgets/text_field.h"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace lotui {

using InlineTextValidator =
    std::function<std::optional<std::string>(std::string_view)>;

struct InlineTextEditHandlers {
    std::function<void(std::string)> committed;
    std::function<void(PopupCloseReason)> cancelled;
    InlineTextValidator validate;
    std::function<void(std::string)> validationFailed;
};

struct InlineTextEditorOptions {
    bool commitOnFocusLoss{true};
    Color invalidFocusRing{0.94F, 0.28F, 0.32F, 1.0F};
};

class InlineTextEditor final {
public:
    InlineTextEditor() = delete;

    static PopupId showAt(
        PopupHost& host,
        std::shared_ptr<const TextEngine> textEngine,
        Rect anchor,
        std::string initialText,
        InlineTextEditHandlers handlers,
        InlineTextEditorOptions options = {},
        TextFieldStyle fieldStyle = {},
        TextStyle textStyle = {});
};

} // namespace lotui
