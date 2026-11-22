#include "app/native_key_capture.h"
#include "app/direction_input_events.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <cmath>
#include <cstdint>
#include <iostream>

namespace {
bool Check(bool ok, const char* caseName) {
    if (!ok) std::cerr << "WIN32_EVENT_FAIL " << caseName << '\n';
    return ok;
}
} // namespace

int main() {
    bool ok = true;
    locus::app::DirectionInputEvents receiver;
    receiver.Configure({74, 75, 70, 68});
    locus::app::BindNativeDirectionEvents(&receiver, 12.5);

    const auto at = static_cast<std::uint32_t>(GetTickCount64());
    // VK_J=0x4A, VK_K=0x4B; synthetic messages traverse the SAME platform
    // translation and timestamp function as the live window procedure.
    locus::app::CaptureNativeDirectionMessage(WM_KEYDOWN, 0x4a, 0, at);
    locus::app::CaptureNativeDirectionMessage(WM_KEYDOWN, 0x4b, 0, at + 7);
    locus::app::CaptureNativeDirectionMessage(
        WM_KEYDOWN, 0x4a, 1ull << 30, at + 8);
    locus::app::CaptureNativeDirectionMessage(WM_KEYUP, 0x4a, 0, at + 10);
    locus::app::CaptureNativeDirectionMessage(WM_KEYDOWN, 0x4a, 0, at + 14);
    const auto batch = receiver.Drain();
    ok &= Check(batch.size == 4,
                "Win32 preserves down-down-up-down and excludes autorepeat");
    if (batch.size == 4) {
        ok &= Check(batch.events[0].arrow == locus::core::Heading::Up &&
                    batch.events[1].arrow == locus::core::Heading::Right &&
                    !batch.events[2].pressed && batch.events[3].pressed,
                    "physical direction mapping, order and release");
        ok &= Check(std::abs(
                    batch.events[1].capturedAt -
                    batch.events[0].capturedAt - 0.007) < 1e-8 &&
                    std::abs(batch.events[3].capturedAt -
                             batch.events[0].capturedAt - 0.014) < 1e-8,
                    "different queued message times survive same render frame");
    }
    locus::app::CaptureNativeDirectionMessage(WM_KILLFOCUS, 0, 0, at + 15);
    ok &= Check(receiver.Drain().Empty() && !receiver.Held()[0],
                "focus lost resets held state and pending input");
    locus::app::CaptureNativeDirectionMessage(WM_KEYDOWN, 0x4a, 0, at + 18);
    locus::app::CaptureNativeDirectionMessage(WM_ACTIVATEAPP, 0, 0, at + 19);
    ok &= Check(receiver.Drain().Empty(),
                "inactive window cannot retain queued gameplay presses");
    locus::app::BindNativeDirectionEvents(nullptr, 0);
    return ok ? 0 : 1;
}
