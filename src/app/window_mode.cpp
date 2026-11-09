#include "app/window_mode.h"

#include "raylib.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

namespace locus::app {
namespace {
bool borderless = false;
LONG_PTR originalStyle = 0;
LONG_PTR originalExStyle = 0;
WINDOWPLACEMENT originalPlacement{sizeof(WINDOWPLACEMENT)};
}
bool SetBorderlessDesktop(bool enabled) noexcept {
    if (enabled == borderless) return true;
    const HWND hwnd = static_cast<HWND>(GetWindowHandle());
    if (!hwnd) return false;
    if (enabled) {
        MONITORINFO monitor{sizeof(MONITORINFO)};
        if (!GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST),
                             &monitor) ||
            !GetWindowPlacement(hwnd, &originalPlacement)) return false;
        originalStyle = GetWindowLongPtrW(hwnd, GWL_STYLE);
        originalExStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
        const LONG_PTR style = (originalStyle & ~WS_OVERLAPPEDWINDOW) | WS_POPUP;
        SetWindowLongPtrW(hwnd, GWL_STYLE, style);
        SetWindowLongPtrW(hwnd, GWL_EXSTYLE,
                          originalExStyle & ~WS_EX_TOPMOST);
        const RECT r = monitor.rcMonitor;
        if (!SetWindowPos(hwnd, HWND_NOTOPMOST, r.left, r.top,
                          r.right - r.left, r.bottom - r.top,
                          SWP_FRAMECHANGED | SWP_NOACTIVATE)) {
            SetWindowLongPtrW(hwnd, GWL_STYLE, originalStyle);
            SetWindowLongPtrW(hwnd, GWL_EXSTYLE, originalExStyle);
            SetWindowPlacement(hwnd, &originalPlacement);
            return false;
        }
    } else {
        SetWindowLongPtrW(hwnd, GWL_STYLE, originalStyle);
        SetWindowLongPtrW(hwnd, GWL_EXSTYLE,
                          originalExStyle & ~WS_EX_TOPMOST);
        SetWindowPlacement(hwnd, &originalPlacement);
        SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED | SWP_NOACTIVATE);
    }
    borderless = enabled;
    return true;
}
} // namespace locus::app
#else
namespace locus::app {
namespace { bool borderless = false; }
bool SetBorderlessDesktop(bool enabled) noexcept {
    if (borderless != enabled) {
        ToggleBorderlessWindowed();
        borderless = enabled;
    }
    return true;
}
} // namespace locus::app
#endif
