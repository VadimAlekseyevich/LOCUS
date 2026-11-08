#pragma once

#include "raylib.h"

namespace locus::app {
// The global quit dialog is rendered in app_frame_renderer and hit-tested in
// the application input loop. Keep both sides on the SAME canonical hitboxes.
inline constexpr Rectangle kQuitConfirmButton{263.0f, 329.0f, 196.0f, 54.0f};
inline constexpr Rectangle kQuitCancelButton{502.0f, 329.0f, 196.0f, 54.0f};
} // namespace locus::app
