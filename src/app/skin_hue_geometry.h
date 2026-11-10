#pragma once
#include "raylib.h"
#include <cmath>

namespace locus::app {
// Screen angles follow atan2(dy, dx): 0=right, 90=down.
// raylib DrawRing instead uses sin(angle) for x and cos(angle) for y:
// 90=right, 0=down. Use the SAME mapping for drawing and picking.
inline constexpr float kSkinHuePi = 3.14159265358979323846f;

inline float SkinHueFromPointer(Vector2 mouse, Vector2 center) noexcept {
    float angle = std::atan2(mouse.y - center.y, mouse.x - center.x);
    if (angle < 0.0f) angle += 2.0f * kSkinHuePi;
    return angle / (2.0f * kSkinHuePi);
}

inline float SkinRingAngleForHue(float hue) noexcept {
    return 90.0f - 360.0f * hue;
}
} // namespace locus::app
