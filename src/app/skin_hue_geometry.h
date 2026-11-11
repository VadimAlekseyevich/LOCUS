#pragma once
#include "raylib.h"
#include <cmath>

namespace locus::app {
// Screen and raylib DrawRing use the same coordinates:
// x = center.x + cos(angle)*radius, y = center.y + sin(angle)*radius.
// 0 degrees is right, 90 is down. Hue increases clockwise.
inline constexpr float kSkinHuePi = 3.14159265358979323846f;

inline float SkinHueFromPointer(Vector2 mouse, Vector2 center) noexcept {
    float angle = std::atan2(mouse.y - center.y, mouse.x - center.x);
    if (angle < 0.0f) angle += 2.0f * kSkinHuePi;
    return angle / (2.0f * kSkinHuePi);
}

inline float SkinRingAngleForHue(float hue) noexcept {
    return 360.0f * hue;
}
} // namespace locus::app
