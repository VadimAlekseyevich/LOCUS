#pragma once

#include "core/timed_route.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace locus::app {

// A single song-time snapshot shared by the camera, route ribbon and turn
// markers. Owned copies survive any reallocation of a source route.
struct HoldPreview {
    double fromSeconds = 0.0, toSeconds = 0.0;
    core::GridPoint from{}, to{};
    bool tailVisible = false;
};
struct RoutePreviewFrame {
    bool ready = false;
    double songSeconds = 0.0;
    double horizonSeconds = 0.0;
    int ticksPerBeat = 1;
    core::GridPoint marker{};
    std::vector<core::TimedRouteSpan> spans;
    // Draw distant events first. Imminent notes win at world-space overlaps.
    std::vector<core::TimedTurn> turnsFarToNear;
    std::vector<HoldPreview> holdsFarToNear;
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
    out.holdsFarToNear.clear();
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
    out.turnsFarToNear.reserve(static_cast<std::size_t>(last - first));
    for (auto it = last; it != first;) {
        --it;
        out.turnsFarToNear.push_back(*it);
    }
    // Include a hold whose head is already behind the player. Intervals
    // never overlap, so at most one predecessor can cross the viewport.
    const auto holdFirst = first == turns.begin() ? first : first - 1;
    for (auto it = last; it != holdFirst;) {
        --it;
        if (it->holdSeconds <= 0.0) continue;
        const double tail = it->songSeconds + it->holdSeconds;
        const double from = std::max(songSeconds, it->songSeconds);
        const double to = std::min(songSeconds + out.horizonSeconds, tail);
        if (to <= from) continue;
        out.holdsFarToNear.push_back({from, to, route.PositionAt(from),
            route.PositionAt(to), tail <= songSeconds + out.horizonSeconds});
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
