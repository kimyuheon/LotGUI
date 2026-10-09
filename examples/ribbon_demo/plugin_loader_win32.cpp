#include "examples/ribbon_demo/plugin_loader.h"

#include <windows.h>

#include <stdexcept>
#include <string>

namespace lotui::example {

PluginLibrary::PluginLibrary(const std::filesystem::path& path) {
    HMODULE module = LoadLibraryW(path.c_str());
    if (module == nullptr) {
        throw std::runtime_error(
            "failed to load ribbon plugin: " + path.u8string());
    }
    auto entry = reinterpret_cast<LotuiDemoGetPluginV1>(
        GetProcAddress(module, "lotui_demo_get_plugin_v1"));
    if (entry == nullptr) {
        FreeLibrary(module);
        throw std::runtime_error("ribbon plugin entry point is missing");
    }
    handle_ = module;
    entry_ = entry;
}

PluginLibrary::~PluginLibrary() {
    if (handle_ != nullptr) {
        FreeLibrary(static_cast<HMODULE>(handle_));
    }
}

const LotuiDemoPluginV1& PluginLibrary::descriptor() const {
    const LotuiDemoPluginV1* result = entry_();
    if (result == nullptr) {
        throw std::runtime_error("ribbon plugin returned no descriptor");
    }
    return *result;
}

} // namespace lotui::example
