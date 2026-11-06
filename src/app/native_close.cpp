#include "app/native_close.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

// raylib 6's GetWindowHandle() returns the Win32 HWND on Windows.
extern "C" void* GetWindowHandle(void);

namespace {
HWND nativeWindow = nullptr;
WNDPROC originalWindowProcedure = nullptr;
bool closeRequested = false;

LRESULT CALLBACK GuardedWindowProcedure(HWND hwnd, UINT message,
                                        WPARAM wparam, LPARAM lparam) {
    if (message == WM_CLOSE) {
        closeRequested = true;
        return 0; // Never forward to GLFW: its close flag would remain latched.
    }
    return CallWindowProcW(originalWindowProcedure, hwnd,
                           message, wparam, lparam);
}
} // namespace

namespace locus::app {
bool InstallNativeCloseGuard() noexcept {
    nativeWindow = static_cast<HWND>(GetWindowHandle());
    if (!nativeWindow) return false;
    SetLastError(0);
    const LONG_PTR oldProcedure = SetWindowLongPtrW(
        nativeWindow, GWLP_WNDPROC,
        reinterpret_cast<LONG_PTR>(&GuardedWindowProcedure));
    if (oldProcedure == 0 && GetLastError() != 0) {
        nativeWindow = nullptr;
        return false;
    }
    originalWindowProcedure = reinterpret_cast<WNDPROC>(oldProcedure);
    closeRequested = false;
    return true;
}

bool ConsumeNativeCloseRequest() noexcept {
    const bool value = closeRequested;
    closeRequested = false;
    return value;
}

void RemoveNativeCloseGuard() noexcept {
    if (nativeWindow && originalWindowProcedure) {
        SetWindowLongPtrW(nativeWindow, GWLP_WNDPROC,
                          reinterpret_cast<LONG_PTR>(originalWindowProcedure));
    }
    originalWindowProcedure = nullptr;
    nativeWindow = nullptr;
    closeRequested = false;
}
} // namespace locus::app

#else

#include "raylib.h"

namespace locus::app {
bool InstallNativeCloseGuard() noexcept { return true; }
bool ConsumeNativeCloseRequest() noexcept { return WindowShouldClose(); }
void RemoveNativeCloseGuard() noexcept {}
} // namespace locus::app

#endif
