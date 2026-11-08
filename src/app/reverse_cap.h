#pragma once

#include "core/route.h"

namespace locus::app {

struct ReverseCapArc {
    float startDegrees = 0.0f;
    float endDegrees = 180.0f;
};

// Raylib angles: 0 = right, 90 = down in screen coordinates.
// The hemisphere BULGES in the incoming direction, opposite the
// NEW outgoing heading. Its straight diameter crosses the turn point
// perpendicular to the trajectory, not parallel to the route line.
constexpr ReverseCapArc ReverseCapForOutgoing(core::Heading outgoing) noexcept {
    using core::Heading;
    switch (outgoing) {
        case Heading::Left:      return {-90.0f, 90.0f};   // Right-facing cap.
        case Heading::UpLeft:    return {-45.0f, 135.0f};
        case Heading::Up:        return {0.0f, 180.0f};
        case Heading::UpRight:   return {45.0f, 225.0f};
        case Heading::Right:     return {90.0f, 270.0f};
        case Heading::DownRight: return {135.0f, 315.0f};
        case Heading::Down:      return {-180.0f, 0.0f};
        case Heading::DownLeft:  return {-135.0f, 45.0f};
    }
    return {};
}

constexpr bool NeedsReverseCap(core::Turn turn) noexcept {
    return turn == core::Turn::Reverse;
}

} // namespace locus::app
