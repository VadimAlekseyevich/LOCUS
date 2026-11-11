#pragma once

#include "core/timed_route.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace locus::app {

// A single song-time snapshot shared by the camera, route ribbon and turn
// markers. Owned copies survive any reallocation of a source route.
struct RoutePreviewFrame {
    bool ready = false;
    double songSeconds = 0.0;
    double horizonSeconds = 0.0;
    int ticksPerBeat = 1;
    core::GridPoint marker{};
    std::vector<core::TimedRouteSpan> spans;
    // Draw distant events first. Imminent notes win at world-space overlaps.
    std::vector<core::TimedTurn> turnsFarToNear;
    // Nearest authored neighbor within the local 1.2s pattern, measured
    // in world units, in the identical order as turnsFarToNear.
    // Uses the COMPILED full route, so marker size does not pop when a
    // neighbor enters or leaves the current visual preview window.
    std::vector<float> turnSpacingWorld;
};

// Caller-owned storage reuses capacity across successive render frames.
inline void PrepareRoutePreview(const core::TimedRoute& route,
                                double songSeconds, float lookaheadMs,
                                RoutePreviewFrame& out) {
    out.ready = route.Ready() && std::isfinite(songSeconds);
    out.songSeconds = songSeconds;
    out.horizonSeconds = 0.0;
    out.ticksPerBeat = route.TicksPerBeat();
    out.marker = route.Ready() ? route.PositionAt(songSeconds)
                               : core::GridPoint{};
    out.spans.clear();
    out.turnsFarToNear.clear();
    out.turnSpacingWorld.clear();
    if (!out.ready || !std::isfinite(lookaheadMs) ||
        lookaheadMs <= 0.0f) return;
    out.horizonSeconds = static_cast<double>(lookaheadMs) / 1000.0;
    route.Window(songSeconds, songSeconds + out.horizonSeconds, out.spans);
    const auto& turns = route.Turns();
    const auto first = std::lower_bound(
        turns.begin(), turns.end(), songSeconds,
        [](const core::TimedTurn& turn, double time) {
            return turn.songSeconds < time;
        });
    const auto last = std::lower_bound(
        first, turns.end(), songSeconds + out.horizonSeconds,
        [](const core::TimedTurn& turn, double time) {
            return turn.songSeconds < time;
        });
    const auto count = static_cast<std::size_t>(last - first);
    out.turnsFarToNear.reserve(count);
    out.turnSpacingWorld.reserve(count);
    for (auto it = last; it != first;) {
        --it;
        out.turnsFarToNear.push_back(*it);

        // Nearby loopbacks can revisit a target four or eight notes later.
        // Compare eight neighbors on either side in a 1.2s local pattern.
        // O(visible notes), not quadratic in the full chart length.
        const auto index = static_cast<std::size_t>(it - turns.begin());
        const auto beginIndex = index > 8 ? index - 8 : 0;
        const auto endIndex = std::min(turns.size(), index + 9);
        double distanceSquared = std::numeric_limits<double>::infinity();
        for (auto neighbor = beginIndex; neighbor < endIndex; ++neighbor) {
            if (neighbor == index ||
                std::abs(turns[neighbor].songSeconds - it->songSeconds) > 1.2)
                continue;
            const double dx =
                turns[neighbor].position.x - it->position.x;
            const double dy =
                turns[neighbor].position.y - it->position.y;
            distanceSquared = std::min(distanceSquared, dx * dx + dy * dy);
            if (distanceSquared == 0.0) break;
        }
        out.turnSpacingWorld.push_back(
            static_cast<float>(std::sqrt(distanceSquared)));
    }
}

inline RoutePreviewFrame MakeRoutePreview(const core::TimedRoute& route,
                                          double songSeconds,
                                          float lookaheadMs) {
    RoutePreviewFrame result;
    PrepareRoutePreview(route, songSeconds, lookaheadMs, result);
    return result;
}

} // namespace locus::app
