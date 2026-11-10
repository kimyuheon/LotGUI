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
    const LotuiDemoPluginV2* textExtension() const;
    const LotuiDemoPluginV3* fieldsExtension() const;

private:
    void* handle_{nullptr};
    LotuiDemoGetPluginV1 entry_{nullptr};
    LotuiDemoGetPluginV2 textEntry_{nullptr};
    LotuiDemoGetPluginV3 fieldsEntry_{nullptr};
};

} // namespace lotui::example
