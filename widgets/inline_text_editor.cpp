#include "widgets/inline_text_editor.h"

#include <stdexcept>
#include <utility>

namespace lotui {
namespace {

struct EditorState {
    PopupHost* host{nullptr};
    TextField* field{nullptr};
    std::string text;
    InlineTextEditHandlers handlers;
    InlineTextEditorOptions options;
    TextFieldStyle normalStyle;
    bool terminalCallbackDelivered{false};
    bool invalid{false};
};

std::optional<std::string> validate(const EditorState& state) {
    return state.handlers.validate
        ? state.handlers.validate(state.text)
        : std::nullopt;
}

void restoreNormalStyle(EditorState& state) {
    if (!state.invalid || state.field == nullptr) {
        return;
    }
    state.invalid = false;
    state.field->setStyle(state.normalStyle);
}

void reportValidationFailure(
    EditorState& state,
    std::string message) {
    state.invalid = true;
    if (state.field != nullptr) {
        TextFieldStyle invalidStyle = state.normalStyle;
        invalidStyle.focusRing = state.options.invalidFocusRing;
        state.field->setStyle(invalidStyle);
    }
    if (state.handlers.validationFailed) {
        auto callback = state.handlers.validationFailed;
        callback(std::move(message));
    }
}

void cancel(EditorState& state, PopupCloseReason reason) {
    if (state.handlers.cancelled) {
        auto callback = state.handlers.cancelled;
        callback(reason);
    }
}

void commit(EditorState& state) {
    if (state.handlers.committed) {
        auto callback = state.handlers.committed;
        callback(state.text);
    }
}

} // namespace

PopupId InlineTextEditor::showAt(
    PopupHost& host,
    std::shared_ptr<const TextEngine> textEngine,
    Rect anchor,
    std::string initialText,
    InlineTextEditHandlers handlers,
    InlineTextEditorOptions options,
    TextFieldStyle fieldStyle,
    TextStyle textStyle) {
    if (!textEngine) {
        throw std::invalid_argument(
            "InlineTextEditor requires a TextEngine");
    }

    auto state = std::make_shared<EditorState>();
    state->host = &host;
    state->text = std::move(initialText);
    state->handlers = std::move(handlers);
    state->options = options;
    state->normalStyle = fieldStyle;

    auto field = std::make_unique<TextField>(
        std::move(textEngine),
        state->text,
        Size{anchor.width, anchor.height},
        [state](const std::string& text) {
            state->text = text;
            restoreNormalStyle(*state);
        },
        [state](const std::string& text) {
            state->text = text;
            if (const auto error = validate(*state)) {
                reportValidationFailure(*state, *error);
                return;
            }
            state->terminalCallbackDelivered = true;
            if (state->host != nullptr && state->host->acceptPopup()) {
                commit(*state);
            } else {
                state->terminalCallbackDelivered = false;
            }
        },
        fieldStyle,
        std::move(textStyle));
    state->field = field.get();

    PopupOptions popupOptions;
    popupOptions.placement = PopupPlacement::Anchor;
    popupOptions.gap = 0.0F;
    popupOptions.matchAnchorWidth = true;
    popupOptions.exactAnchorWidth = true;
    popupOptions.exactAnchorHeight = true;
    popupOptions.dismissOnOutsidePress = true;
    popupOptions.dismissOnEscape = true;
    return host.showPopup(
        std::move(field),
        anchor,
        popupOptions,
        [state](PopupCloseReason reason) {
            state->field = nullptr;
            if (state->terminalCallbackDelivered) {
                return;
            }
            state->terminalCallbackDelivered = true;
            if (reason == PopupCloseReason::Dismissed &&
                state->options.commitOnFocusLoss) {
                if (const auto error = validate(*state)) {
                    reportValidationFailure(*state, *error);
                    cancel(*state, reason);
                } else {
                    commit(*state);
                }
                return;
            }
            cancel(*state, reason);
        });
}

} // namespace lotui
