#include "platform/platform_backend.h"

#include <windows.h>
#include <windowsx.h>
#include <imm.h>

#include <deque>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace lotui {
namespace {

constexpr wchar_t kWindowClassName[] = L"LotUI.PlatformWindow";

KeyCode keyCode(WPARAM key) noexcept {
    switch (key) {
    case VK_TAB: return KeyCode::Tab;
    case VK_RETURN: return KeyCode::Enter;
    case VK_SPACE: return KeyCode::Space;
    case VK_ESCAPE: return KeyCode::Escape;
    case VK_BACK: return KeyCode::Backspace;
    case VK_DELETE: return KeyCode::Delete;
    case VK_LEFT: return KeyCode::Left;
    case VK_RIGHT: return KeyCode::Right;
    case VK_UP: return KeyCode::Up;
    case VK_DOWN: return KeyCode::Down;
    case VK_HOME: return KeyCode::Home;
    case VK_END: return KeyCode::End;
    case VK_PRIOR: return KeyCode::PageUp;
    case VK_NEXT: return KeyCode::PageDown;
    default: return KeyCode::Unknown;
    }
}

KeyModifiers keyModifiers() noexcept {
    return {
        (GetKeyState(VK_SHIFT) & 0x8000) != 0,
        (GetKeyState(VK_CONTROL) & 0x8000) != 0,
        (GetKeyState(VK_MENU) & 0x8000) != 0,
        (GetKeyState(VK_LWIN) & 0x8000) != 0 ||
            (GetKeyState(VK_RWIN) & 0x8000) != 0,
    };
}

std::wstring utf8ToWide(const std::string& value) {
    if (value.empty()) {
        return {};
    }

    const int length = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
        static_cast<int>(value.size()), nullptr, 0);
    if (length <= 0) {
        throw std::runtime_error("window title is not valid UTF-8");
    }

    std::wstring result(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
        static_cast<int>(value.size()), result.data(), length);
    return result;
}

std::string wideToUtf8(std::wstring_view value) {
    if (value.empty()) {
        return {};
    }
    const int length = WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
        static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (length <= 0) {
        return {};
    }
    std::string result(static_cast<std::size_t>(length), '\0');
    WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
        static_cast<int>(value.size()), result.data(), length,
        nullptr, nullptr);
    return result;
}

std::wstring compositionString(HIMC context, DWORD index) {
    const LONG byteCount = ImmGetCompositionStringW(
        context, index, nullptr, 0);
    if (byteCount <= 0) {
        return {};
    }
    std::wstring result(
        static_cast<std::size_t>(byteCount) / sizeof(wchar_t), L'\0');
    const LONG copied = ImmGetCompositionStringW(
        context, index, result.data(), static_cast<DWORD>(byteCount));
    if (copied < 0) {
        return {};
    }
    result.resize(static_cast<std::size_t>(copied) / sizeof(wchar_t));
    return result;
}

void enablePerMonitorDpiAwareness() {
    using SetDpiAwarenessContextFn = BOOL(WINAPI*)(DPI_AWARENESS_CONTEXT);

    const HMODULE user32 = GetModuleHandleW(L"user32.dll");
    const auto setDpiAwarenessContext = reinterpret_cast<SetDpiAwarenessContextFn>(
        GetProcAddress(user32, "SetProcessDpiAwarenessContext"));

    if (setDpiAwarenessContext != nullptr) {
        setDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    } else {
        SetProcessDPIAware();
    }
}

class WindowsWindow final : public PlatformWindow {
public:
    explicit WindowsWindow(const WindowOptions& options);
    ~WindowsWindow() override;

    void show() override;
    bool pollEvent(PlatformEvent& event) override;
    bool setPointerCapture(bool enabled) override;
    void setTextInputState(const TextInputState& state) override;
    WindowMetrics metrics() const override;
    NativeWindowHandle nativeHandle() const override;

private:
    static LRESULT CALLBACK windowProcedure(
        HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT handleMessage(UINT message, WPARAM wParam, LPARAM lParam);
    void pushEvent(PlatformEvent event);
    void updateClientSize();
    void updateDpiScale();

    HWND window_{nullptr};
    WindowMetrics metrics_{};
    std::deque<PlatformEvent> events_;
    bool closeRequested_{false};
    bool pointerCaptured_{false};
    TextInputState textInputState_{};
    wchar_t pendingHighSurrogate_{0};
};

WindowsWindow::WindowsWindow(const WindowOptions& options) {
    const HINSTANCE instance = GetModuleHandleW(nullptr);

    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = &WindowsWindow::windowProcedure;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.lpszClassName = kWindowClassName;

    if (RegisterClassExW(&windowClass) == 0 &&
        GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        throw std::runtime_error("failed to register the Win32 window class");
    }

    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    if (options.resizable) {
        style |= WS_THICKFRAME | WS_MAXIMIZEBOX;
    }

    RECT frame{0, 0, options.width, options.height};
    if (AdjustWindowRectEx(&frame, style, FALSE, 0) == FALSE) {
        throw std::runtime_error("failed to calculate the Win32 window frame");
    }

    const std::wstring title = utf8ToWide(options.title);
    window_ = CreateWindowExW(
        0,
        kWindowClassName,
        title.c_str(),
        style,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        frame.right - frame.left,
        frame.bottom - frame.top,
        nullptr,
        nullptr,
        instance,
        this);

    if (window_ == nullptr) {
        throw std::runtime_error("failed to create the Win32 window");
    }

    updateClientSize();
    updateDpiScale();
}

WindowsWindow::~WindowsWindow() {
    if (pointerCaptured_ && GetCapture() == window_) {
        pointerCaptured_ = false;
        ReleaseCapture();
    }
    if (window_ != nullptr) {
        DestroyWindow(window_);
        window_ = nullptr;
    }
}

void WindowsWindow::show() {
    ShowWindow(window_, SW_SHOW);
    UpdateWindow(window_);
}

bool WindowsWindow::pollEvent(PlatformEvent& event) {
    MSG message{};
    while (events_.empty() &&
           PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE) != FALSE) {
        if (message.message == WM_QUIT) {
            if (!closeRequested_) {
                closeRequested_ = true;
                pushEvent({PlatformEventType::CloseRequested});
            }
            continue;
        }
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    if (events_.empty()) {
        return false;
    }

    event = events_.front();
    events_.pop_front();
    return true;
}

bool WindowsWindow::setPointerCapture(bool enabled) {
    if (window_ == nullptr) {
        return false;
    }

    if (enabled) {
        SetCapture(window_);
        pointerCaptured_ = GetCapture() == window_;
        return pointerCaptured_;
    }

    pointerCaptured_ = false;
    if (GetCapture() == window_) {
        ReleaseCapture();
    }
    return GetCapture() != window_;
}

void WindowsWindow::setTextInputState(const TextInputState& state) {
    const bool wasEnabled = textInputState_.enabled;
    textInputState_ = state;
    if (window_ == nullptr) {
        return;
    }

    HIMC context = ImmGetContext(window_);
    if (context == nullptr) {
        return;
    }
    if (!state.enabled) {
        pendingHighSurrogate_ = 0;
        if (wasEnabled) {
            ImmNotifyIME(context, NI_COMPOSITIONSTR, CPS_CANCEL, 0);
        }
        ImmReleaseContext(window_, context);
        return;
    }

    const float scale = metrics_.dpiScale > 0.0F
        ? metrics_.dpiScale
        : 1.0F;
    const POINT position{
        static_cast<LONG>(std::lround(state.inputRect.x * scale)),
        static_cast<LONG>(std::lround(
            (state.inputRect.y + state.inputRect.height) * scale)),
    };
    COMPOSITIONFORM composition{};
    composition.dwStyle = CFS_POINT;
    composition.ptCurrentPos = position;
    ImmSetCompositionWindow(context, &composition);

    CANDIDATEFORM candidate{};
    candidate.dwIndex = 0;
    candidate.dwStyle = CFS_CANDIDATEPOS;
    candidate.ptCurrentPos = position;
    ImmSetCandidateWindow(context, &candidate);
    ImmReleaseContext(window_, context);
}

WindowMetrics WindowsWindow::metrics() const {
    return metrics_;
}

NativeWindowHandle WindowsWindow::nativeHandle() const {
    return {
        NativeWindowSystem::Win32,
        GetModuleHandleW(nullptr),
        reinterpret_cast<std::uintptr_t>(window_),
    };
}

LRESULT CALLBACK WindowsWindow::windowProcedure(
    HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    WindowsWindow* self = reinterpret_cast<WindowsWindow*>(
        GetWindowLongPtrW(window, GWLP_USERDATA));

    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<WindowsWindow*>(create->lpCreateParams);
        self->window_ = window;
        SetWindowLongPtrW(
            window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }

    if (self != nullptr) {
        return self->handleMessage(message, wParam, lParam);
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

LRESULT WindowsWindow::handleMessage(
    UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CLOSE:
        if (!closeRequested_) {
            closeRequested_ = true;
            pushEvent({PlatformEventType::CloseRequested});
        }
        DestroyWindow(window_);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_SIZE:
        metrics_.width = LOWORD(lParam);
        metrics_.height = HIWORD(lParam);
        metrics_.framebufferWidth = metrics_.width;
        metrics_.framebufferHeight = metrics_.height;
        if (wParam != SIZE_MINIMIZED) {
            PlatformEvent event{PlatformEventType::Resized};
            event.width = metrics_.width;
            event.height = metrics_.height;
            pushEvent(event);
        }
        return 0;

    case WM_MOUSEMOVE: {
        PlatformEvent event{PlatformEventType::MouseMoved};
        const float scale = metrics_.dpiScale > 0.0F
            ? metrics_.dpiScale
            : 1.0F;
        event.x = static_cast<float>(GET_X_LPARAM(lParam)) / scale;
        event.y = static_cast<float>(GET_Y_LPARAM(lParam)) / scale;
        pushEvent(event);
        return 0;
    }

    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_XBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MBUTTONUP:
    case WM_XBUTTONUP: {
        const bool pressed = message == WM_LBUTTONDOWN ||
            message == WM_RBUTTONDOWN || message == WM_MBUTTONDOWN ||
            message == WM_XBUTTONDOWN;
        PointerButton button = PointerButton::Unspecified;
        if (message == WM_LBUTTONDOWN || message == WM_LBUTTONUP) {
            button = PointerButton::Primary;
        } else if (message == WM_RBUTTONDOWN || message == WM_RBUTTONUP) {
            button = PointerButton::Secondary;
        } else if (message == WM_MBUTTONDOWN || message == WM_MBUTTONUP) {
            button = PointerButton::Middle;
        } else {
            button = GET_XBUTTON_WPARAM(wParam) == XBUTTON1
                ? PointerButton::Auxiliary1
                : PointerButton::Auxiliary2;
        }

        const float scale = metrics_.dpiScale > 0.0F
            ? metrics_.dpiScale
            : 1.0F;
        PlatformEvent event{
            pressed
                ? PlatformEventType::MouseButtonPressed
                : PlatformEventType::MouseButtonReleased};
        event.x = static_cast<float>(GET_X_LPARAM(lParam)) / scale;
        event.y = static_cast<float>(GET_Y_LPARAM(lParam)) / scale;
        event.button = button;
        pushEvent(event);
        if (pressed) {
            SetFocus(window_);
        }
        return message == WM_XBUTTONDOWN || message == WM_XBUTTONUP
            ? TRUE
            : 0;
    }

    case WM_CAPTURECHANGED:
        if (pointerCaptured_ &&
            reinterpret_cast<HWND>(lParam) != window_) {
            pointerCaptured_ = false;
            pushEvent({PlatformEventType::PointerCaptureLost});
        }
        return 0;

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN: {
        PlatformEvent event{PlatformEventType::KeyPressed};
        event.key = keyCode(wParam);
        event.modifiers = keyModifiers();
        event.repeat = (lParam & (1LL << 30)) != 0;
        pushEvent(event);
        return 0;
    }

    case WM_KEYUP:
    case WM_SYSKEYUP: {
        PlatformEvent event{PlatformEventType::KeyReleased};
        event.key = keyCode(wParam);
        event.modifiers = keyModifiers();
        pushEvent(event);
        return 0;
    }

    case WM_CHAR: {
        if (!textInputState_.enabled) {
            return DefWindowProcW(window_, message, wParam, lParam);
        }
        const wchar_t value = static_cast<wchar_t>(wParam);
        if (value < 0x20 || value == 0x7F) {
            return 0;
        }
        std::wstring text;
        if (value >= 0xD800 && value <= 0xDBFF) {
            pendingHighSurrogate_ = value;
            return 0;
        }
        if (value >= 0xDC00 && value <= 0xDFFF &&
            pendingHighSurrogate_ != 0) {
            text.push_back(pendingHighSurrogate_);
            text.push_back(value);
            pendingHighSurrogate_ = 0;
        } else {
            pendingHighSurrogate_ = 0;
            text.push_back(value);
        }
        PlatformEvent event{PlatformEventType::TextInput};
        event.text = wideToUtf8(text);
        if (!event.text.empty()) {
            pushEvent(std::move(event));
        }
        return 0;
    }

    case WM_IME_STARTCOMPOSITION:
        if (textInputState_.enabled) {
            pushEvent({PlatformEventType::TextComposition});
            return 0;
        }
        break;

    case WM_IME_COMPOSITION: {
        if (!textInputState_.enabled) {
            break;
        }
        HIMC context = ImmGetContext(window_);
        if (context == nullptr) {
            return 0;
        }
        if ((lParam & GCS_RESULTSTR) != 0) {
            PlatformEvent event{PlatformEventType::TextInput};
            event.text = wideToUtf8(
                compositionString(context, GCS_RESULTSTR));
            if (!event.text.empty()) {
                pushEvent(std::move(event));
            }
        }
        if ((lParam & GCS_COMPSTR) != 0) {
            const std::wstring composition =
                compositionString(context, GCS_COMPSTR);
            const LONG cursor = ImmGetCompositionStringW(
                context, GCS_CURSORPOS, nullptr, 0);
            const std::size_t cursorUnits = cursor < 0
                ? composition.size()
                : std::min(
                    static_cast<std::size_t>(cursor), composition.size());
            PlatformEvent event{PlatformEventType::TextComposition};
            event.text = wideToUtf8(composition);
            event.selectionStart = wideToUtf8(std::wstring_view(
                composition.data(), cursorUnits)).size();
            pushEvent(std::move(event));
        } else if ((lParam & GCS_RESULTSTR) == 0) {
            pushEvent({PlatformEventType::TextCompositionEnd});
        }
        ImmReleaseContext(window_, context);
        return 0;
    }

    case WM_IME_ENDCOMPOSITION:
        if (textInputState_.enabled) {
            pushEvent({PlatformEventType::TextCompositionEnd});
            return 0;
        }
        break;

    case WM_SETFOCUS:
        metrics_.focused = true;
        pushEvent({PlatformEventType::FocusGained});
        return 0;

    case WM_KILLFOCUS:
        metrics_.focused = false;
        pushEvent({PlatformEventType::FocusLost});
        return 0;

    case WM_DPICHANGED: {
        metrics_.dpiScale =
            static_cast<float>(HIWORD(wParam)) / 96.0F;
        const auto* suggested = reinterpret_cast<RECT*>(lParam);
        SetWindowPos(
            window_, nullptr,
            suggested->left,
            suggested->top,
            suggested->right - suggested->left,
            suggested->bottom - suggested->top,
            SWP_NOACTIVATE | SWP_NOZORDER);

        PlatformEvent event{PlatformEventType::DpiChanged};
        event.dpiScale = metrics_.dpiScale;
        pushEvent(event);
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT: {
        PAINTSTRUCT paint{};
        BeginPaint(window_, &paint);
        EndPaint(window_, &paint);
        return 0;
    }

    case WM_NCDESTROY: {
        const HWND nativeWindow = window_;
        SetWindowLongPtrW(nativeWindow, GWLP_USERDATA, 0);
        window_ = nullptr;
        return DefWindowProcW(nativeWindow, message, wParam, lParam);
    }

    default:
        return DefWindowProcW(window_, message, wParam, lParam);
    }
    return DefWindowProcW(window_, message, wParam, lParam);
}

void WindowsWindow::pushEvent(PlatformEvent event) {
    events_.push_back(std::move(event));
}

void WindowsWindow::updateClientSize() {
    RECT client{};
    if (GetClientRect(window_, &client) != FALSE) {
        metrics_.width = client.right - client.left;
        metrics_.height = client.bottom - client.top;
        metrics_.framebufferWidth = metrics_.width;
        metrics_.framebufferHeight = metrics_.height;
    }
}

void WindowsWindow::updateDpiScale() {
    const UINT dpi = GetDpiForWindow(window_);
    metrics_.dpiScale = dpi > 0 ? static_cast<float>(dpi) / 96.0F : 1.0F;
}

class WindowsPlatformBackend final : public PlatformBackend {
public:
    WindowsPlatformBackend() {
        enablePerMonitorDpiAwareness();
    }

    std::unique_ptr<PlatformWindow> createWindow(
        const WindowOptions& options) override {
        return std::make_unique<WindowsWindow>(options);
    }
};

} // namespace

std::unique_ptr<PlatformBackend> createPlatformBackend() {
    return std::make_unique<WindowsPlatformBackend>();
}

} // namespace lotui
