#pragma once

#include "raylib.h"

#include <algorithm>
#include <cmath>

namespace locus::app {

// Logical 960x540 coordinates are independent of physical window pixels.
// Gameplay fits its 540x540 square to the window's shorter physical side.
struct PixelRect { int x = 0, y = 0, width = 0, height = 0; };

struct ViewportTransform {
    float scale = 1.0f, offsetX = 0.0f, offsetY = 0.0f;

    static ViewportTransform Menu(int width, int height) noexcept {
        const float scale = std::min(width / 960.0f, height / 540.0f);
        return {scale, (width - 960.0f * scale) * 0.5f,
                       (height - 540.0f * scale) * 0.5f};
    }

    static ViewportTransform Gameplay(int width, int height) noexcept {
        const float scale = width > 0 && height > 0
            ? static_cast<float>(std::min(width, height)) / 540.0f : 1.0f;
        return {scale, (width - 960.0f * scale) * 0.5f,
                       (height - 540.0f * scale) * 0.5f};
    }

    Vector2 ToVirtual(Vector2 physical) const noexcept {
        if (scale <= 0.0f) return {-1000.0f, -1000.0f};
        return {(physical.x - offsetX) / scale,
                (physical.y - offsetY) / scale};
    }

    // Round transformed edges, not width/height alone. Fractional offsets
    // otherwise cut pixels from the playfield or editor preview.
    PixelRect Scissor(Rectangle bounds) const noexcept {
        const int left = static_cast<int>(std::floor(offsetX + bounds.x * scale));
        const int top = static_cast<int>(std::floor(offsetY + bounds.y * scale));
        const int right = static_cast<int>(std::ceil(
            offsetX + (bounds.x + bounds.width) * scale));
        const int bottom = static_cast<int>(std::ceil(
            offsetY + (bounds.y + bounds.height) * scale));
        return {left, top, right - left, bottom - top};
    }
};

} // namespace locus::app
