#pragma once

#include "raylib.h"
#include "app/ui_scale.h"

#include <algorithm>
#include <cmath>

namespace locus::app {

inline constexpr int kVirtualWidth = 960;
inline constexpr int kVirtualHeight = 540;

// Native UI uses density-independent units and the full client area.
// The old 960x540 transform is retained only for legacy drawing and modal
// hitboxes; gameplay keeps its own square coordinate system.
struct PixelRect { int x = 0, y = 0, width = 0, height = 0; };

struct ViewportTransform {
    float scale = 1.0f, offsetX = 0.0f, offsetY = 0.0f;

    static ViewportTransform Menu(int width, int height) noexcept {
        const float scale = std::min(width / static_cast<float>(kVirtualWidth),
                                     height / static_cast<float>(kVirtualHeight));
        return {scale, (width - kVirtualWidth * scale) * 0.5f,
                       (height - kVirtualHeight * scale) * 0.5f};
    }

    static ViewportTransform NativeUi(float density) noexcept {
        return {std::isfinite(density) && density > 0.0f ? density : 1.0f,
                0.0f, 0.0f};
    }

    Camera2D Camera() const noexcept {
        Camera2D camera{};
        camera.offset = {offsetX, offsetY};
        camera.zoom = scale;
        return camera;
    }

    static ViewportTransform Gameplay(int width, int height) noexcept {
        const float scale = width > 0 && height > 0
            ? static_cast<float>(std::min(width, height)) / kVirtualHeight : 1.0f;
        return {scale, (width - kVirtualWidth * scale) * 0.5f,
                       (height - kVirtualHeight * scale) * 0.5f};
    }

    Vector2 ToVirtual(Vector2 physical) const noexcept {
        if (scale <= 0.0f) return {-1000.0f, -1000.0f};
        return {(physical.x - offsetX) / scale,
                (physical.y - offsetY) / scale};
    }

    Rectangle ToVirtualBounds(Rectangle physical) const noexcept {
        if (scale <= 0.0f) return {};
        const auto origin = ToVirtual({physical.x, physical.y});
        return {origin.x, origin.y, physical.width/scale, physical.height/scale};
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
