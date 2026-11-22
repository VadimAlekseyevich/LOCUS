#pragma once

#include "app/direction_input_events.h"

#include <cstdint>

namespace locus::app {
// The existing Win32 subclass already used for WM_CLOSE also forwards
// keyboard messages to this per-window input sink. No global keyboard hook.
void BindNativeDirectionEvents(DirectionInputEvents* target,
                               double monotonicSecondsNow) noexcept;
void CaptureNativeDirectionMessage(unsigned message, std::uintptr_t wparam,
                                   std::intptr_t lparam,
                                   std::uint32_t queuedAtMilliseconds) noexcept;
} // namespace locus::app
