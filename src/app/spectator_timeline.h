#pragma once
#include "app/viewport.h"

namespace locus::app {
// Shared virtual geometry for pointer hit-testing and spectator rendering.
inline Rectangle SpectatorTimelineBounds() noexcept {
    return {240.0f, 494.0f, 480.0f, 36.0f};
}
inline Rectangle SpectatorTimelineTrack() noexcept {
    return {252.0f, 515.0f, 456.0f, 4.0f};
}
inline float SpectatorTimelineFraction(Vector2 pointer) noexcept {
    const auto track=SpectatorTimelineTrack();
    return std::clamp((pointer.x-track.x)/track.width,0.0f,1.0f);
}
} // namespace locus::app
