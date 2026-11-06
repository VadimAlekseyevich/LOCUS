#pragma once

namespace locus::app {

// On Windows, intercept WM_CLOSE before GLFW/raylib latches a permanent
// "should close" flag. This lets the player cancel an X/Alt+F4 request
// and later try closing again without a stuck confirmation dialog.
bool InstallNativeCloseGuard() noexcept;
bool ConsumeNativeCloseRequest() noexcept;
void RemoveNativeCloseGuard() noexcept;

} // namespace locus::app
