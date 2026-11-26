#pragma once

#include "declarative/lotml.h"

namespace lotui::designer::layout_settings {

inline void handle_settingsChanged_3696b6cec8() {
    // Refresh derived settings or mark the form as edited.
}

inline void handle_save_157dca92e4() {
    // Persist the project settings.
}

inline void handle_cancel_2374d91794() {
    // Close the settings dialog.
}

// LotUI handler insertion point

inline void bindEvents(lotui::declarative::LoadOptions& options) {
    options.events.try_emplace("settingsChanged", &handle_settingsChanged_3696b6cec8);
    options.events.try_emplace("save", &handle_save_157dca92e4);
    options.events.try_emplace("cancel", &handle_cancel_2374d91794);
    // LotUI binding insertion point
}

} // namespace lotui::designer::layout_settings
