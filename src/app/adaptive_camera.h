#pragma once

#include "app/movement_speed.h"
#include "core/route.h"
#include "core/route_view.h"

#include <algorithm>
#include <cmath>

namespace locus::app {

// Experimental: one flag to restore the centered presentation, with no
// chart format or judgement changes. The whole feature is one Git commit.
inline constexpr bool kExperimentalAdaptiveCamera = true;

// The camera looks a little AHEAD along the authored path, placing the
// marker slightly BEHIND center to preserve forward reading room. This
// remains deterministic at every song timestamp (no FPS-dependent easing).
inline constexpr double kAdaptiveCameraLeadSeconds = 0.18;
inline constexpr double kAdaptiveCameraMaxOffsetPx = 72.0;

// Return the world position projected onto the play square center.
// Speed changes the camera's absolute movement speed linearly on straight
// segments, and (until the safety limit) linearly increases the offset.
// Practice speed changes the tick delta and px/tick inversely so identical
// on-screen speeds have identical physical camera offsets.
inline core::GridPoint AdaptiveCameraAnchor(const core::Chart& chart,
                                            double beat, float pixelsPerTick,
                                            double practiceRate = 1.0) {
    const core::GridPoint marker = core::RoutePositionAt(chart, beat);
    if (!kExperimentalAdaptiveCamera ||
        !std::isfinite(beat) || !std::isfinite(pixelsPerTick) ||
        pixelsPerTick <= 0.0f || !std::isfinite(practiceRate) ||
        practiceRate <= 0.0 || !std::isfinite(chart.bpm) ||
        chart.bpm <= 0.0 || chart.ticksPerBeat <= 0)
        return marker;

    const double leadTicks = kAdaptiveCameraLeadSeconds * practiceRate *
        (chart.bpm * chart.ticksPerBeat / 60.0);
    const core::GridPoint ahead = core::RoutePositionAt(chart, beat + leadTicks);
    const double dx = (ahead.x - marker.x) * pixelsPerTick;
    const double dy = (ahead.y - marker.y) * pixelsPerTick;
    const double length = std::hypot(dx, dy);
    if (!std::isfinite(length) || length <= 0.0) return marker;
    const double fraction = length > kAdaptiveCameraMaxOffsetPx
        ? kAdaptiveCameraMaxOffsetPx / length : 1.0;
    return {marker.x + (ahead.x - marker.x) * fraction,
            marker.y + (ahead.y - marker.y) * fraction};
}

} // namespace locus::app
