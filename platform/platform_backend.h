#pragma once

#include "platform/platform_event.h"

#include <cstdint>
#include <memory>
#include <string>

namespace lotui {

struct WindowOptions {
    std::string title{"LotUI Platform Probe"};
    int width{960};
    int height{640};
    bool resizable{true};
};

struct WindowMetrics {
    int width{0};
    int height{0};
    int framebufferWidth{0};
    int framebufferHeight{0};
    float dpiScale{1.0F};
    bool focused{false};
};

enum class NativeWindowSystem {
    Win32,
    MetalLayer,
    X11,
};

struct NativeWindowHandle {
    NativeWindowSystem system{NativeWindowSystem::Win32};
    void* display{nullptr};
    std::uintptr_t window{0};
};

class PlatformWindow {
public:
    virtual ~PlatformWindow() = default;

    PlatformWindow(const PlatformWindow&) = delete;
    PlatformWindow& operator=(const PlatformWindow&) = delete;
    PlatformWindow(PlatformWindow&&) = delete;
    PlatformWindow& operator=(PlatformWindow&&) = delete;

    virtual void show() = 0;
    virtual bool pollEvent(PlatformEvent& event) = 0;
    virtual bool setPointerCapture(bool enabled) = 0;
    virtual WindowMetrics metrics() const = 0;
    virtual NativeWindowHandle nativeHandle() const = 0;

protected:
    PlatformWindow() = default;
};

class PlatformBackend {
public:
    virtual ~PlatformBackend() = default;

    PlatformBackend(const PlatformBackend&) = delete;
    PlatformBackend& operator=(const PlatformBackend&) = delete;
    PlatformBackend(PlatformBackend&&) = delete;
    PlatformBackend& operator=(PlatformBackend&&) = delete;

    virtual std::unique_ptr<PlatformWindow> createWindow(
        const WindowOptions& options) = 0;

protected:
    PlatformBackend() = default;
};

std::unique_ptr<PlatformBackend> createPlatformBackend();

} // namespace lotui
