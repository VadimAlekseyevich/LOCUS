#pragma once

namespace locus::app {
// Full-desktop borderless *window*, not exclusive monitor fullscreen. It
// remains in the task switcher and never steals focus from another app.
bool SetBorderlessDesktop(bool enabled) noexcept;
}
