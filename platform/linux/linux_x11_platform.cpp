#include "platform/platform_backend.h"

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>
#include <memory>
#include <stdexcept>
#include <utility>

namespace lotui {
namespace {

float displayDpiScale(Display* display, int screen) {
    const int widthPixels = DisplayWidth(display, screen);
    const int widthMillimeters = DisplayWidthMM(display, screen);
    if (widthMillimeters <= 0) {
        return 1.0F;
    }

    const float dpi = static_cast<float>(widthPixels) * 25.4F /
        static_cast<float>(widthMillimeters);
    return std::max(0.5F, dpi / 96.0F);
}

PointerButton pointerButton(unsigned int button) noexcept {
    switch (button) {
    case Button1:
        return PointerButton::Primary;
    case Button2:
        return PointerButton::Middle;
    case Button3:
        return PointerButton::Secondary;
    case 8:
        return PointerButton::Auxiliary1;
    case 9:
        return PointerButton::Auxiliary2;
    default:
        return PointerButton::Unspecified;
    }
}

KeyCode keyCode(KeySym key) noexcept {
    switch (key) {
    case XK_Tab:
    case XK_ISO_Left_Tab: return KeyCode::Tab;
    case XK_Return:
    case XK_KP_Enter: return KeyCode::Enter;
    case XK_space: return KeyCode::Space;
    case XK_Escape: return KeyCode::Escape;
    case XK_BackSpace: return KeyCode::Backspace;
    case XK_Delete:
    case XK_KP_Delete: return KeyCode::Delete;
    case XK_Left:
    case XK_KP_Left: return KeyCode::Left;
    case XK_Right:
    case XK_KP_Right: return KeyCode::Right;
    case XK_Up:
    case XK_KP_Up: return KeyCode::Up;
    case XK_Down:
    case XK_KP_Down: return KeyCode::Down;
    case XK_Home:
    case XK_KP_Home: return KeyCode::Home;
    case XK_End:
    case XK_KP_End: return KeyCode::End;
    case XK_Page_Up:
    case XK_KP_Page_Up: return KeyCode::PageUp;
    case XK_Page_Down:
    case XK_KP_Page_Down: return KeyCode::PageDown;
    default: return KeyCode::Unknown;
    }
}

KeyModifiers keyModifiers(unsigned int state) noexcept {
    return {
        (state & ShiftMask) != 0,
        (state & ControlMask) != 0,
        (state & Mod1Mask) != 0,
        (state & Mod4Mask) != 0,
    };
}

class X11Window final : public PlatformWindow {
public:
    explicit X11Window(const WindowOptions& options);
    ~X11Window() override;

    void show() override;
    bool pollEvent(PlatformEvent& event) override;
    bool setPointerCapture(bool enabled) override;
    WindowMetrics metrics() const override;
    NativeWindowHandle nativeHandle() const override;

private:
    void processEvent(const XEvent& nativeEvent);

    Display* display_{nullptr};
    int screen_{0};
    ::Window window_{0};
    Atom deleteMessage_{0};
    WindowMetrics metrics_{};
    std::deque<PlatformEvent> events_;
    bool pointerCaptured_{false};
};

X11Window::X11Window(const WindowOptions& options) {
    display_ = XOpenDisplay(nullptr);
    if (display_ == nullptr) {
        throw std::runtime_error("failed to open the X11 display");
    }

    screen_ = DefaultScreen(display_);
    window_ = XCreateSimpleWindow(
        display_,
        RootWindow(display_, screen_),
        0,
        0,
        static_cast<unsigned int>(options.width),
        static_cast<unsigned int>(options.height),
        0,
        BlackPixel(display_, screen_),
        BlackPixel(display_, screen_));

    if (window_ == 0) {
        XCloseDisplay(display_);
        display_ = nullptr;
        throw std::runtime_error("failed to create the X11 window");
    }

    XStoreName(display_, window_, options.title.c_str());
    XSelectInput(
        display_,
        window_,
        StructureNotifyMask |
            PointerMotionMask |
            ButtonPressMask |
            ButtonReleaseMask |
            KeyPressMask |
            KeyReleaseMask |
            FocusChangeMask);

    deleteMessage_ = XInternAtom(display_, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(display_, window_, &deleteMessage_, 1);

    if (!options.resizable) {
        XSizeHints hints{};
        hints.flags = PMinSize | PMaxSize;
        hints.min_width = options.width;
        hints.max_width = options.width;
        hints.min_height = options.height;
        hints.max_height = options.height;
        XSetWMNormalHints(display_, window_, &hints);
    }

    metrics_.width = options.width;
    metrics_.height = options.height;
    metrics_.framebufferWidth = options.width;
    metrics_.framebufferHeight = options.height;
    metrics_.dpiScale = displayDpiScale(display_, screen_);
}

X11Window::~X11Window() {
    if (display_ != nullptr) {
        if (pointerCaptured_) {
            XUngrabPointer(display_, CurrentTime);
            pointerCaptured_ = false;
        }
        if (window_ != 0) {
            XDestroyWindow(display_, window_);
            window_ = 0;
        }
        XCloseDisplay(display_);
        display_ = nullptr;
    }
}

void X11Window::show() {
    XMapWindow(display_, window_);
    XFlush(display_);
}

bool X11Window::pollEvent(PlatformEvent& event) {
    while (events_.empty() && XPending(display_) > 0) {
        XEvent nativeEvent{};
        XNextEvent(display_, &nativeEvent);
        processEvent(nativeEvent);
    }

    if (events_.empty()) {
        return false;
    }

    event = events_.front();
    events_.pop_front();
    return true;
}

bool X11Window::setPointerCapture(bool enabled) {
    if (display_ == nullptr || window_ == 0) {
        return false;
    }

    if (enabled) {
        const int result = XGrabPointer(
            display_,
            window_,
            False,
            PointerMotionMask | ButtonPressMask | ButtonReleaseMask,
            GrabModeAsync,
            GrabModeAsync,
            None,
            None,
            CurrentTime);
        pointerCaptured_ = result == GrabSuccess;
        XFlush(display_);
        return pointerCaptured_;
    }

    if (pointerCaptured_) {
        XUngrabPointer(display_, CurrentTime);
        XFlush(display_);
        pointerCaptured_ = false;
    }
    return true;
}

WindowMetrics X11Window::metrics() const {
    return metrics_;
}

NativeWindowHandle X11Window::nativeHandle() const {
    return {
        NativeWindowSystem::X11,
        display_,
        static_cast<std::uintptr_t>(window_),
    };
}

void X11Window::processEvent(const XEvent& nativeEvent) {
    switch (nativeEvent.type) {
    case ClientMessage:
        if (static_cast<Atom>(nativeEvent.xclient.data.l[0]) == deleteMessage_) {
            events_.push_back({PlatformEventType::CloseRequested});
        }
        break;

    case ConfigureNotify: {
        metrics_.width = nativeEvent.xconfigure.width;
        metrics_.height = nativeEvent.xconfigure.height;
        metrics_.framebufferWidth = metrics_.width;
        metrics_.framebufferHeight = metrics_.height;
        PlatformEvent event{PlatformEventType::Resized};
        event.width = metrics_.width;
        event.height = metrics_.height;
        events_.push_back(event);
        break;
    }

    case MotionNotify: {
        PlatformEvent event{PlatformEventType::MouseMoved};
        const float scale = metrics_.dpiScale > 0.0F
            ? metrics_.dpiScale
            : 1.0F;
        event.x = static_cast<float>(nativeEvent.xmotion.x) / scale;
        event.y = static_cast<float>(nativeEvent.xmotion.y) / scale;
        events_.push_back(event);
        break;
    }

    case ButtonPress:
    case ButtonRelease: {
        const PointerButton button = pointerButton(nativeEvent.xbutton.button);
        if (button == PointerButton::Unspecified) {
            break;
        }
        const float scale = metrics_.dpiScale > 0.0F
            ? metrics_.dpiScale
            : 1.0F;
        PlatformEvent event{
            nativeEvent.type == ButtonPress
                ? PlatformEventType::MouseButtonPressed
                : PlatformEventType::MouseButtonReleased};
        event.x = static_cast<float>(nativeEvent.xbutton.x) / scale;
        event.y = static_cast<float>(nativeEvent.xbutton.y) / scale;
        event.button = button;
        events_.push_back(event);
        break;
    }

    case KeyPress:
    case KeyRelease: {
        XKeyEvent keyEvent = nativeEvent.xkey;
        PlatformEvent event{
            nativeEvent.type == KeyPress
                ? PlatformEventType::KeyPressed
                : PlatformEventType::KeyReleased};
        event.key = keyCode(XLookupKeysym(&keyEvent, 0));
        event.modifiers = keyModifiers(nativeEvent.xkey.state);
        event.repeat = false;
        events_.push_back(event);
        break;
    }

    case FocusIn:
        metrics_.focused = true;
        events_.push_back({PlatformEventType::FocusGained});
        break;

    case FocusOut:
        metrics_.focused = false;
        events_.push_back({PlatformEventType::FocusLost});
        break;

    default:
        break;
    }
}

class LinuxPlatformBackend final : public PlatformBackend {
public:
    std::unique_ptr<PlatformWindow> createWindow(
        const WindowOptions& options) override {
        return std::make_unique<X11Window>(options);
    }
};

} // namespace

std::unique_ptr<PlatformBackend> createPlatformBackend() {
    return std::make_unique<LinuxPlatformBackend>();
}

} // namespace lotui
