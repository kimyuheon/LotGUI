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

void onMode(void*, double value) {
    std::printf("ribbon plugin: mode=%.0f\n", value);
}

void onCount(void*, double value) {
    std::printf("ribbon plugin: count=%.0f\n", value);
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

const char* const modes[] = {"전체", "표시만", "검토"};
const LotuiDemoChoiceControlV3 choices[] = {
    {sizeof(LotuiDemoChoiceControlV3), "mode", "표시 모드", 3,
        modes, 0, nullptr, &onMode},
};
const LotuiDemoNumberControlV3 numbers[] = {
    {sizeof(LotuiDemoNumberControlV3), "count", "행 높이", 20.0,
        40.0, 28.0, 2.0, nullptr, &onCount},
};
const LotuiDemoPluginV3 fieldsExtension{
    LOTUI_DEMO_PLUGIN_FIELDS_ABI_VERSION,
    sizeof(LotuiDemoPluginV3),
    &textExtension,
    1, choices,
    1, numbers,
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

extern "C" LOTUI_DEMO_EXPORT const LotuiDemoPluginV3*
lotui_demo_get_plugin_v3(void) {
    return &fieldsExtension;
}
