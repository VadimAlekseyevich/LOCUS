#pragma once
#include "app/viewport.h"
#include "raylib.h"

namespace locus::app {
// Shared clickable controls in the 960x540 GAMEPLAY virtual viewport.
inline constexpr Rectangle kPlayResumeButton{286.0f, 297.0f, 120.0f, 46.0f};
inline constexpr Rectangle kPlayRetryButton{419.0f, 297.0f, 120.0f, 46.0f};
inline constexpr Rectangle kPlayLeaveButton{552.0f, 297.0f, 120.0f, 46.0f};
inline constexpr Rectangle kLeaveLevelConfirmButton{286, 319, 177, 51};
inline constexpr Rectangle kLeaveLevelCancelButton{497, 319, 177, 51};

inline bool PlayButtonHit(Rectangle button, Vector2 pointer,
                          int width, int height) noexcept {
    if (width <= 0 || height <= 0) return false;
    const auto viewport = ViewportTransform::Gameplay(width, height);
    if (viewport.scale <= 0.0f) return false;
    const Vector2 mouse = viewport.ToVirtual(pointer);
    return mouse.x >= button.x && mouse.y >= button.y &&
           mouse.x < button.x + button.width &&
           mouse.y < button.y + button.height;
}
} // namespace locus::app
