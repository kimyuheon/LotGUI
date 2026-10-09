#include "examples/ribbon_demo/plugin_loader.h"

#include <dlfcn.h>

#include <stdexcept>
#include <string>

namespace lotui::example {

PluginLibrary::PluginLibrary(const std::filesystem::path& path) {
    void* module = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (module == nullptr) {
        const char* error = dlerror();
        throw std::runtime_error(std::string("failed to load ribbon plugin: ") +
            (error != nullptr ? error : path.string()));
    }
    dlerror();
    auto entry = reinterpret_cast<LotuiDemoGetPluginV1>(
        dlsym(module, "lotui_demo_get_plugin_v1"));
    if (const char* error = dlerror(); error != nullptr) {
        dlclose(module);
        throw std::runtime_error(std::string(
            "ribbon plugin entry point is missing: ") + error);
    }
    handle_ = module;
    entry_ = entry;
}

PluginLibrary::~PluginLibrary() {
    if (handle_ != nullptr) {
        dlclose(handle_);
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
