#pragma once

#include "core/timed_route.h"
#include "core/play_mods.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace locus::app {

// A single song-time snapshot shared by the camera, route ribbon and turn
// markers. Owned copies survive any reallocation of a source route.
struct RoutePreviewFrame {
    bool ready = false;
    double songSeconds = 0.0;
    double horizonSeconds = 0.0;
    int ticksPerBeat = 1;
    double hiddenFadeSeconds = 0.0;
    core::GridPoint marker{};
    std::vector<core::TimedRouteSpan> spans;
    // Draw distant events first. Imminent notes win at world-space overlaps.
    std::vector<core::TimedTurn> turnsFarToNear;
};

// Caller-owned storage reuses capacity across successive render frames.
inline void PrepareRoutePreview(const core::TimedRoute& route,
                                double songSeconds, float lookaheadMs,
                                RoutePreviewFrame& out,
                                core::ModFlags mods = 0, double clockRate = 1.0) {
    out.ready = route.Ready() && std::isfinite(songSeconds);
    out.songSeconds = songSeconds;
    out.horizonSeconds = 0.0;
    out.hiddenFadeSeconds = 0.0;
    out.ticksPerBeat = route.TicksPerBeat();
    out.marker = route.Ready() ? route.PositionAt(songSeconds)
                               : core::GridPoint{};
    out.spans.clear();
    out.turnsFarToNear.clear();
    if (!out.ready || !std::isfinite(lookaheadMs) ||
        lookaheadMs <= 0.0f) return;
    out.horizonSeconds = static_cast<double>(lookaheadMs) / 1000.0;
    if (!std::isfinite(clockRate) || clockRate <= 0.0) clockRate = 1.0;
    if (core::HasMod(mods,core::PlayMod::Hidden))
        out.hiddenFadeSeconds = std::min(out.horizonSeconds*0.5,0.45*clockRate);
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
}

// Hidden fades both the ribbon and the junctions before reaching the player.
inline float ModVisibility(const RoutePreviewFrame& preview,double ahead) noexcept {
    if (preview.hiddenFadeSeconds <= 0.0) return 1.0f;
    return static_cast<float>(std::clamp((ahead-preview.hiddenFadeSeconds*0.35)/
        (preview.hiddenFadeSeconds*0.65),0.0,1.0));
}

inline RoutePreviewFrame MakeRoutePreview(const core::TimedRoute& route,
                                          double songSeconds,
                                          float lookaheadMs) {
    RoutePreviewFrame result;
    PrepareRoutePreview(route, songSeconds, lookaheadMs, result);
    return result;
}

} // namespace locus::app
