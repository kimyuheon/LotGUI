#include "platform/platform_backend.h"

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>

#include <algorithm>
#include <chrono>
#include <clocale>
#include <cmath>
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace lotui {
namespace {

constexpr std::size_t kMaximumClipboardBytes = 64 * 1024;

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
    case XK_a:
    case XK_A: return KeyCode::A;
    case XK_c:
    case XK_C: return KeyCode::C;
    case XK_v:
    case XK_V: return KeyCode::V;
    case XK_x:
    case XK_X: return KeyCode::X;
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

std::string lookupUtf8(
    XIC context,
    XKeyEvent& event,
    KeySym& symbol,
    Status& status) {
    std::vector<char> buffer(64);
    int length = Xutf8LookupString(
        context, &event, buffer.data(),
        static_cast<int>(buffer.size()), &symbol, &status);
    if (status == XBufferOverflow) {
        buffer.resize(static_cast<std::size_t>(length) + 1);
        length = Xutf8LookupString(
            context, &event, buffer.data(),
            static_cast<int>(buffer.size()), &symbol, &status);
    }
    return length > 0
        ? std::string(buffer.data(), static_cast<std::size_t>(length))
        : std::string{};
}

class X11Window final : public PlatformWindow {
public:
    explicit X11Window(const WindowOptions& options);
    ~X11Window() override;

    void show() override;
    bool pollEvent(PlatformEvent& event) override;
    bool setPointerCapture(bool enabled) override;
    void setTextInputState(const TextInputState& state) override;
    bool writeClipboardText(std::string_view text) override;
    std::optional<std::string> readClipboardText() override;
    WindowMetrics metrics() const override;
    NativeWindowHandle nativeHandle() const override;

private:
    void processEvent(const XEvent& nativeEvent);

    Display* display_{nullptr};
    int screen_{0};
    ::Window window_{0};
    Atom deleteMessage_{0};
    Atom clipboardAtom_{None};
    Atom utf8Atom_{None};
    Atom targetsAtom_{None};
    Atom clipboardPropertyAtom_{None};
    std::string clipboardText_;
    WindowMetrics metrics_{};
    std::deque<PlatformEvent> events_;
    bool pointerCaptured_{false};
    XIM inputMethod_{nullptr};
    XIC inputContext_{nullptr};
    XIMStyle inputStyle_{0};
    TextInputState textInputState_{};
};

X11Window::X11Window(const WindowOptions& options) {
    std::setlocale(LC_CTYPE, "");
    XSetLocaleModifiers("");
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
    clipboardAtom_ = XInternAtom(display_, "CLIPBOARD", False);
    utf8Atom_ = XInternAtom(display_, "UTF8_STRING", False);
    targetsAtom_ = XInternAtom(display_, "TARGETS", False);
    clipboardPropertyAtom_ = XInternAtom(
        display_, "LOTUI_CLIPBOARD_REPLY", False);

    inputMethod_ = XOpenIM(display_, nullptr, nullptr, nullptr);
    if (inputMethod_ != nullptr) {
        XIMStyles* styles = nullptr;
        if (XGetIMValues(
                inputMethod_, XNQueryInputStyle, &styles, nullptr) == nullptr &&
            styles != nullptr) {
            const XIMStyle positioned =
                XIMPreeditPosition | XIMStatusNothing;
            const XIMStyle external =
                XIMPreeditNothing | XIMStatusNothing;
            for (unsigned short index = 0; index < styles->count_styles; ++index) {
                if (styles->supported_styles[index] == positioned) {
                    inputStyle_ = positioned;
                    break;
                }
                if (styles->supported_styles[index] == external) {
                    inputStyle_ = external;
                }
            }
            XFree(styles);
        }
        if (inputStyle_ == (XIMPreeditPosition | XIMStatusNothing)) {
            XPoint spot{0, 0};
            XVaNestedList preedit = XVaCreateNestedList(
                0, XNSpotLocation, &spot, nullptr);
            inputContext_ = XCreateIC(
                inputMethod_,
                XNInputStyle, inputStyle_,
                XNClientWindow, window_,
                XNFocusWindow, window_,
                XNPreeditAttributes, preedit,
                nullptr);
            XFree(preedit);
        }
        if (inputContext_ == nullptr) {
            inputStyle_ = XIMPreeditNothing | XIMStatusNothing;
            inputContext_ = XCreateIC(
                inputMethod_,
                XNInputStyle, inputStyle_,
                XNClientWindow, window_,
                XNFocusWindow, window_,
                nullptr);
        }
    }

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
            if (inputContext_ != nullptr) {
                XDestroyIC(inputContext_);
                inputContext_ = nullptr;
            }
            if (inputMethod_ != nullptr) {
                XCloseIM(inputMethod_);
                inputMethod_ = nullptr;
            }
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
        if (nativeEvent.type != SelectionRequest &&
            nativeEvent.type != SelectionClear &&
            XFilterEvent(&nativeEvent, window_) != False) {
            continue;
        }
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

void X11Window::setTextInputState(const TextInputState& state) {
    const bool changed = textInputState_.enabled != state.enabled;
    textInputState_ = state;
    if (inputContext_ == nullptr) {
        return;
    }
    if (changed) {
        if (state.enabled && metrics_.focused) {
            XSetICFocus(inputContext_);
        } else if (!state.enabled) {
            XUnsetICFocus(inputContext_);
        }
    }
    if (state.enabled &&
        inputStyle_ == (XIMPreeditPosition | XIMStatusNothing)) {
        const float scale = metrics_.dpiScale > 0.0F
            ? metrics_.dpiScale
            : 1.0F;
        XPoint spot{
            static_cast<short>(std::lround(state.inputRect.x * scale)),
            static_cast<short>(std::lround(
                (state.inputRect.y + state.inputRect.height) * scale)),
        };
        XVaNestedList preedit = XVaCreateNestedList(
            0, XNSpotLocation, &spot, nullptr);
        XSetICValues(inputContext_, XNPreeditAttributes, preedit, nullptr);
        XFree(preedit);
    }
}

bool X11Window::writeClipboardText(std::string_view text) {
    if (text.size() > kMaximumClipboardBytes) {
        return false;
    }
    XSetSelectionOwner(display_, clipboardAtom_, window_, CurrentTime);
    XFlush(display_);
    if (XGetSelectionOwner(display_, clipboardAtom_) != window_) {
        return false;
    }
    clipboardText_.assign(text);
    return true;
}

std::optional<std::string> X11Window::readClipboardText() {
    const ::Window owner = XGetSelectionOwner(display_, clipboardAtom_);
    if (owner == None) {
        return std::nullopt;
    }
    if (owner == window_) {
        return clipboardText_;
    }

    XDeleteProperty(display_, window_, clipboardPropertyAtom_);
    XConvertSelection(
        display_, clipboardAtom_, utf8Atom_, clipboardPropertyAtom_,
        window_, CurrentTime);
    XFlush(display_);

    const auto deadline = std::chrono::steady_clock::now() +
        std::chrono::milliseconds(500);
    while (std::chrono::steady_clock::now() < deadline) {
        while (XPending(display_) > 0) {
            XEvent nativeEvent{};
            XNextEvent(display_, &nativeEvent);
            if (nativeEvent.type == SelectionNotify &&
                nativeEvent.xselection.requestor == window_ &&
                nativeEvent.xselection.selection == clipboardAtom_) {
                if (nativeEvent.xselection.property == None) {
                    return std::nullopt;
                }
                Atom actualType = None;
                int actualFormat = 0;
                unsigned long itemCount = 0;
                unsigned long bytesAfter = 0;
                unsigned char* data = nullptr;
                const int status = XGetWindowProperty(
                    display_, window_, clipboardPropertyAtom_, 0,
                    static_cast<long>(kMaximumClipboardBytes / 4), False,
                    utf8Atom_, &actualType, &actualFormat, &itemCount,
                    &bytesAfter, &data);
                std::optional<std::string> result;
                if (status == Success && actualType == utf8Atom_ &&
                    actualFormat == 8 && bytesAfter == 0) {
                    result = std::string{};
                    if (itemCount > 0 && data != nullptr) {
                        result->assign(
                            reinterpret_cast<const char*>(data), itemCount);
                    }
                }
                if (data != nullptr) {
                    XFree(data);
                }
                XDeleteProperty(display_, window_, clipboardPropertyAtom_);
                return result;
            }
            if (nativeEvent.type != SelectionRequest &&
                nativeEvent.type != SelectionClear &&
                XFilterEvent(&nativeEvent, window_) != False) {
                continue;
            }
            processEvent(nativeEvent);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    XDeleteProperty(display_, window_, clipboardPropertyAtom_);
    return std::nullopt;
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
    case SelectionRequest: {
        const XSelectionRequestEvent& request = nativeEvent.xselectionrequest;
        XEvent reply{};
        reply.xselection.type = SelectionNotify;
        reply.xselection.display = display_;
        reply.xselection.requestor = request.requestor;
        reply.xselection.selection = request.selection;
        reply.xselection.target = request.target;
        reply.xselection.time = request.time;
        reply.xselection.property = None;
        const Atom property = request.property != None
            ? request.property : request.target;
        if (request.selection == clipboardAtom_ &&
            XGetSelectionOwner(display_, clipboardAtom_) == window_) {
            if (request.target == targetsAtom_) {
                const Atom targets[]{targetsAtom_, utf8Atom_};
                XChangeProperty(
                    display_, request.requestor, property, XA_ATOM, 32,
                    PropModeReplace,
                    reinterpret_cast<const unsigned char*>(targets), 2);
                reply.xselection.property = property;
            } else if (request.target == utf8Atom_) {
                XChangeProperty(
                    display_, request.requestor, property, utf8Atom_, 8,
                    PropModeReplace,
                    reinterpret_cast<const unsigned char*>(
                        clipboardText_.data()),
                    static_cast<int>(clipboardText_.size()));
                reply.xselection.property = property;
            }
        }
        XSendEvent(display_, request.requestor, False, NoEventMask, &reply);
        XFlush(display_);
        break;
    }

    case SelectionClear:
        if (nativeEvent.xselectionclear.selection == clipboardAtom_) {
            clipboardText_.clear();
        }
        break;

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
        event.modifiers = keyModifiers(nativeEvent.xmotion.state);
        events_.push_back(event);
        break;
    }

    case ButtonPress:
    case ButtonRelease: {
        const unsigned int nativeButton = nativeEvent.xbutton.button;
        if (nativeButton >= 4 && nativeButton <= 7) {
            if (nativeEvent.type == ButtonPress) {
                const float scale = metrics_.dpiScale > 0.0F
                    ? metrics_.dpiScale
                    : 1.0F;
                PlatformEvent event{PlatformEventType::MouseWheel};
                event.x = static_cast<float>(nativeEvent.xbutton.x) / scale;
                event.y = static_cast<float>(nativeEvent.xbutton.y) / scale;
                event.scrollMode = ScrollDeltaMode::Line;
                if (nativeButton == 4) {
                    event.scrollY = -3.0F;
                } else if (nativeButton == 5) {
                    event.scrollY = 3.0F;
                } else if (nativeButton == 6) {
                    event.scrollX = -3.0F;
                } else {
                    event.scrollX = 3.0F;
                }
                events_.push_back(event);
            }
            break;
        }
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
        event.modifiers = keyModifiers(nativeEvent.xbutton.state);
        events_.push_back(event);
        break;
    }

    case KeyPress: {
        XKeyEvent keyEvent = nativeEvent.xkey;
        KeySym symbol = NoSymbol;
        Status status = XLookupNone;
        std::string text;
        if (inputContext_ != nullptr) {
            text = lookupUtf8(inputContext_, keyEvent, symbol, status);
        } else {
            std::vector<char> buffer(64);
            const int length = XLookupString(
                &keyEvent,
                buffer.data(),
                static_cast<int>(buffer.size()),
                &symbol,
                nullptr);
            if (length > 0) {
                text.assign(buffer.data(), static_cast<std::size_t>(length));
                status = XLookupBoth;
            } else {
                status = XLookupKeySym;
            }
        }
        PlatformEvent event{PlatformEventType::KeyPressed};
        event.key = keyCode(symbol);
        event.modifiers = keyModifiers(nativeEvent.xkey.state);
        event.repeat = false;
        events_.push_back(event);
        if (textInputState_.enabled && !text.empty() &&
            !event.modifiers.control && !event.modifiers.meta &&
            (status == XLookupChars || status == XLookupBoth)) {
            PlatformEvent textEvent{PlatformEventType::TextInput};
            textEvent.text = std::move(text);
            events_.push_back(std::move(textEvent));
        }
        break;
    }

    case KeyRelease: {
        XKeyEvent keyEvent = nativeEvent.xkey;
        PlatformEvent event{PlatformEventType::KeyReleased};
        event.key = keyCode(XLookupKeysym(&keyEvent, 0));
        event.modifiers = keyModifiers(nativeEvent.xkey.state);
        events_.push_back(event);
        break;
    }

    case FocusIn:
        metrics_.focused = true;
        if (inputContext_ != nullptr && textInputState_.enabled) {
            XSetICFocus(inputContext_);
        }
        events_.push_back({PlatformEventType::FocusGained});
        break;

    case FocusOut:
        metrics_.focused = false;
        if (inputContext_ != nullptr) {
            XUnsetICFocus(inputContext_);
        }
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
