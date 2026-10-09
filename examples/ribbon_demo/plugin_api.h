#pragma once

#include <stdint.h>

#define LOTUI_DEMO_PLUGIN_ABI_VERSION 1u

#ifdef __cplusplus
extern "C" {
#endif

enum LotuiDemoControlKind {
    LOTUI_DEMO_BUTTON = 1,
    LOTUI_DEMO_CHECKBOX = 2,
    LOTUI_DEMO_SLIDER = 3,
};

typedef void (*LotuiDemoControlAction)(void* context, double value);

typedef struct LotuiDemoControlV1 {
    uint32_t structSize;
    uint32_t kind;
    const char* id;
    const char* label;
    double minimum;
    double maximum;
    double initialValue;
    void* context;
    LotuiDemoControlAction action;
} LotuiDemoControlV1;

typedef struct LotuiDemoPluginV1 {
    uint32_t abiVersion;
    uint32_t structSize;
    const char* tabId;
    const char* tabTitle;
    const char* groupTitle;
    uint32_t controlCount;
    const LotuiDemoControlV1* controls;
} LotuiDemoPluginV1;

typedef const LotuiDemoPluginV1* (*LotuiDemoGetPluginV1)(void);

#ifdef __cplusplus
}
#endif
