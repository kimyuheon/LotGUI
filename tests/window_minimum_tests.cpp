#include "platform/platform_backend.h"

#include <windows.h>

#include <cstdlib>
#include <iostream>

int main() {
    auto platform = lotui::createPlatformBackend();
    auto window = platform->createWindow(
        {"LotUI minimum window test", 320, 240, true, 680, 900});
    const auto metrics = window->metrics();
    if (metrics.width < 680 || metrics.height < 900) {
        std::cerr << "initial window did not respect the minimum\n";
        return EXIT_FAILURE;
    }

    const auto handle = reinterpret_cast<HWND>(window->nativeHandle().window);
    MINMAXINFO limits{};
    SendMessageW(handle, WM_GETMINMAXINFO, 0,
        reinterpret_cast<LPARAM>(&limits));
    RECT expected{0, 0, 680, 900};
    const DWORD style = static_cast<DWORD>(GetWindowLongPtrW(handle, GWL_STYLE));
    const UINT dpi = GetDpiForWindow(handle);
    if (AdjustWindowRectExForDpi(&expected, style, FALSE, 0,
            dpi > 0 ? dpi : 96) == FALSE ||
        limits.ptMinTrackSize.x < expected.right - expected.left ||
        limits.ptMinTrackSize.y < expected.bottom - expected.top) {
        std::cerr << "native resize limit did not respect the minimum\n";
        return EXIT_FAILURE;
    }
    std::cout << "window_minimum_tests passed\n";
    return EXIT_SUCCESS;
}
