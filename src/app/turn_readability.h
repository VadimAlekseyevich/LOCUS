#pragma once

#include "core/route.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace locus::app {

// Pure, GPU-free legibility policy. Changes glyph sizes only; never edits
// route points, timestamps, judged inputs, or the fixed-speed camera.
struct DenseTurnGlyph {
    float radius = 0.0f;
    float reverseCapScale = 1.0f;
    bool showHeading = false;
};

// "spacingPx" is the closest nearby authored target (not a BPM guess)
// projected with the same camera as the trajectory. Infinity is an
// isolated note. Distances of zero occur on real overlapping paths.
inline DenseTurnGlyph LayoutDenseTurn(
    float baseRadius, float spacingPx, float requestedReverseCapScale) noexcept {
    if (!std::isfinite(baseRadius) || baseRadius <= 0.0f)
        return {};
    if (!std::isfinite(spacingPx) || spacingPx < 0.0f)
        spacingPx = std::numeric_limits<float>::infinity();

    const float scale = std::clamp(spacingPx / (2.25f * baseRadius),
                                   0.35f, 1.0f);
    const float radius = baseRadius * scale;
    const float capScale = std::min(
        std::max(0.0f, requestedReverseCapScale),
        std::max(1.1f, spacingPx * 0.44f / radius));
    return {radius, capScale, scale < 0.80f};
}

struct TurnDirection {
    float x = 0.0f;
    float y = 0.0f;
};

inline constexpr TurnDirection OutgoingVector(core::Heading direction) noexcept {
    constexpr float d = 0.7071067811865475244f;
    using core::Heading;
    switch (direction) {
        case Heading::Up: return {0.0f, -1.0f};
        case Heading::UpRight: return {d, -d};
        case Heading::Right: return {1.0f, 0.0f};
        case Heading::DownRight: return {d, d};
        case Heading::Down: return {0.0f, 1.0f};
        case Heading::DownLeft: return {-d, d};
        case Heading::Left: return {-1.0f, 0.0f};
        case Heading::UpLeft: return {-d, -d};
    }
    return {};
}

} // namespace locus::app
