#pragma once

#include "examples/ribbon_demo/plugin_api.h"

#include <filesystem>

namespace lotui::example {

class PluginLibrary final {
public:
    explicit PluginLibrary(const std::filesystem::path& path);
    ~PluginLibrary();

    PluginLibrary(const PluginLibrary&) = delete;
    PluginLibrary& operator=(const PluginLibrary&) = delete;

    const LotuiDemoPluginV1& descriptor() const;

private:
    void* handle_{nullptr};
    LotuiDemoGetPluginV1 entry_{nullptr};
};

} // namespace lotui::example
