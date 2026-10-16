#include "examples/ribbon_demo/plugin_api.h"

#include <cstdio>

namespace {

void onRun(void*, double) {
    std::puts("ribbon plugin: run");
}

void onSnap(void*, double value) {
    std::printf("ribbon plugin: snap=%d\n", value != 0.0);
}

void onSize(void*, double value) {
    std::printf("ribbon plugin: size=%.0f\n", value);
}

void onName(void*, const char* value) {
    std::printf("ribbon plugin: name=%s\n", value);
}

const LotuiDemoControlV1 controls[] = {
    {sizeof(LotuiDemoControlV1), LOTUI_DEMO_BUTTON,
        "run", "실행", 0.0, 0.0, 0.0, nullptr, &onRun},
    {sizeof(LotuiDemoControlV1), LOTUI_DEMO_CHECKBOX,
        "snap", "열 고정", 0.0, 1.0, 0.0, nullptr, &onSnap},
    {sizeof(LotuiDemoControlV1), LOTUI_DEMO_SLIDER,
        "size", "크기", 0.0, 100.0, 35.0, nullptr, &onSize},
};

const LotuiDemoPluginV1 plugin{
    LOTUI_DEMO_PLUGIN_ABI_VERSION,
    sizeof(LotuiDemoPluginV1),
    "plugin", "플러그인", "도구",
    static_cast<uint32_t>(sizeof(controls) / sizeof(controls[0])),
    controls,
};

const LotuiDemoTextControlV2 textControls[] = {
    {sizeof(LotuiDemoTextControlV2), "name", "이름", "새 항목",
        nullptr, &onName},
};

const LotuiDemoPluginV2 textExtension{
    LOTUI_DEMO_PLUGIN_TEXT_ABI_VERSION,
    sizeof(LotuiDemoPluginV2),
    &plugin,
    static_cast<uint32_t>(sizeof(textControls) / sizeof(textControls[0])),
    textControls,
};

} // namespace

#if defined(_WIN32)
#define LOTUI_DEMO_EXPORT __declspec(dllexport)
#else
#define LOTUI_DEMO_EXPORT __attribute__((visibility("default")))
#endif

extern "C" LOTUI_DEMO_EXPORT const LotuiDemoPluginV1*
lotui_demo_get_plugin_v1(void) {
    return &plugin;
}

extern "C" LOTUI_DEMO_EXPORT const LotuiDemoPluginV2*
lotui_demo_get_plugin_v2(void) {
    return &textExtension;
}
