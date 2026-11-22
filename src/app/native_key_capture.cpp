#include "app/native_key_capture.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

namespace locus::app {
namespace {
// Windows virtual keys -> GLFW/raylib physical key identifiers. Printable
// symbols use US positions (as raylib key codes do), never translated WM_CHAR.
int RaylibKeyFromVirtualKey(WPARAM vk, LPARAM details) noexcept {
    if ((vk >= 0x30 && vk <= 0x39) || (vk >= 0x41 && vk <= 0x5a))
        return static_cast<int>(vk);
    const bool extended = (static_cast<std::uintptr_t>(details) &
                           (1ull << 24)) != 0;
    switch (vk) {
        case VK_SPACE: return 32;
        case VK_OEM_7: return 39;
        case VK_OEM_COMMA: return 44;
        case VK_OEM_MINUS: return 45;
        case VK_OEM_PERIOD: return 46;
        case VK_OEM_2: return 47;
        case VK_OEM_1: return 59;
        case VK_OEM_PLUS: return 61;
        case VK_OEM_4: return 91;
        case VK_OEM_5: return 92;
        case VK_OEM_6: return 93;
        case VK_OEM_3: return 96;
        case VK_ESCAPE: return 256;
        case VK_RETURN: return extended ? 335 : 257;
        case VK_TAB: return 258;
        case VK_BACK: return 259;
        // Physical numpad remains the same key with NumLock OFF; Win32 then
        // sends navigation VKs, distinguished by the extended-key scan flag.
        case VK_INSERT: return extended ? 260 : 320; // KP 0
        case VK_DELETE: return extended ? 261 : 330; // KP decimal
        case VK_RIGHT: return extended ? 262 : 326;  // KP 6
        case VK_LEFT: return extended ? 263 : 324;   // KP 4
        case VK_DOWN: return extended ? 264 : 322;   // KP 2
        case VK_UP: return extended ? 265 : 328;     // KP 8
        case VK_PRIOR: return extended ? 266 : 329;  // KP 9
        case VK_NEXT: return extended ? 267 : 323;   // KP 3
        case VK_HOME: return extended ? 268 : 327;   // KP 7
        case VK_END: return extended ? 269 : 321;    // KP 1
        case VK_CLEAR: return 325;                  // KP 5
        case VK_CAPITAL: return 280;
        case VK_SCROLL: return 281;
        case VK_NUMLOCK: return 282;
        case VK_SNAPSHOT: return 283;
        case VK_PAUSE: return 284;
        case VK_LWIN: return 343;
        case VK_RWIN: return 347;
        case VK_APPS: return 348;
        case VK_SHIFT: {
            const auto scan = static_cast<UINT>(
                (static_cast<std::uintptr_t>(details) >> 16) & 0xff);
            return scan == MapVirtualKeyW(VK_RSHIFT, MAPVK_VK_TO_VSC)
                ? 344 : 340;
        }
        case VK_CONTROL: return extended ? 345 : 341;
        case VK_MENU: return extended ? 346 : 342;
        case VK_LSHIFT: return 340;
        case VK_RSHIFT: return 344;
        case VK_LCONTROL: return 341;
        case VK_RCONTROL: return 345;
        case VK_LMENU: return 342;
        case VK_RMENU: return 346;
        case VK_DECIMAL: return 330;
        case VK_DIVIDE: return 331;
        case VK_MULTIPLY: return 332;
        case VK_SUBTRACT: return 333;
        case VK_ADD: return 334;
        default: break;
    }
    if (vk >= VK_F1 && vk <= VK_F24)
        return 290 + static_cast<int>(vk - VK_F1);
    if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9)
        return 320 + static_cast<int>(vk - VK_NUMPAD0);
    return -1;
}

DirectionInputEvents* active = nullptr;
double monoAnchorSeconds = 0.0;
std::uint64_t uptimeAnchorMs = 0;

double MonotonicMessageTime(std::uint32_t queuedMs) noexcept {
    // GetMessageTime: Windows uptime milliseconds, 32-bit wrapped.
    // Restore its nearest 64-bit epoch via GetTickCount64 and convert into
    // the same monotonic origin as the audio/game clock.
    const auto now = static_cast<std::int64_t>(GetTickCount64());
    const auto delta = static_cast<std::int32_t>(
        queuedMs - static_cast<std::uint32_t>(now));
    return monoAnchorSeconds + static_cast<double>(
        now + static_cast<std::int64_t>(delta) -
        static_cast<std::int64_t>(uptimeAnchorMs)) / 1000.0;
}
} // namespace

void BindNativeDirectionEvents(DirectionInputEvents* sink,
                               double monotonicNow) noexcept {
    active = sink;
    uptimeAnchorMs = GetTickCount64();
    monoAnchorSeconds = monotonicNow;
    if (active) active->Reset();
}
void CaptureNativeDirectionMessage(unsigned message, std::uintptr_t wparam,
                                   std::intptr_t lparam,
                                   std::uint32_t queuedMs) noexcept {
    if (!active) return;
    if (message == WM_KILLFOCUS ||
        (message == WM_ACTIVATEAPP && !wparam)) {
        active->Reset();
        return;
    }
    const bool down = message == WM_KEYDOWN || message == WM_SYSKEYDOWN;
    const bool up = message == WM_KEYUP || message == WM_SYSKEYUP;
    if (!down && !up) return;
    const int key = RaylibKeyFromVirtualKey(
        static_cast<WPARAM>(wparam), static_cast<LPARAM>(lparam));
    if (key < 0) return;
    const bool repeat = down &&
        (static_cast<std::uintptr_t>(lparam) & (1ull << 30)) != 0;
    active->Observe(key, down, MonotonicMessageTime(queuedMs), repeat);
}
} // namespace locus::app

#else
namespace locus::app {
void BindNativeDirectionEvents(DirectionInputEvents*, double) noexcept {}
void CaptureNativeDirectionMessage(unsigned, std::uintptr_t, std::intptr_t,
                                   std::uint32_t) noexcept {}
} // namespace locus::app
#endif
