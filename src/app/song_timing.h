#pragma once

#include "core/chart_json.h"
#include "core/tempo_map.h"

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace locus::app {

// Song Set timing is authoritative. Existing note timestamps never move;
// increase the editor tick density if a tempo change would merge two notes.
inline bool ApplySongTiming(core::LoadedChart& chart,
                            const std::vector<core::TempoMarker>& markers,
                            std::string& error) {
    if (!core::ValidTempoMap(markers, error)) return false;
    auto& route = chart.route;
    const int oldTicks = route.ticksPerBeat;
    if (oldTicks <= 0 || oldTicks > 48) {
        error = "Difficulty has an unsupported tick resolution.";
        return false;
    }
    std::vector<std::int64_t> times;
    times.reserve(route.turns.size());
    for (const auto& note : route.turns)
        times.push_back(core::EventTimeMs(route, note, chart.offsetSeconds));
    for (int multiplier = 1; multiplier <= 48 / oldTicks; ++multiplier) {
        core::Chart mapped = route;
        mapped.bpm = markers.front().bpm;
        mapped.tempoMap = markers;
        mapped.ticksPerBeat = oldTicks * multiplier;
        const double offsetSeconds = markers.front().timeMs / 1000.0;
        int previous = -1000001;
        std::vector<int> positions;
        positions.reserve(times.size());
        bool fits = true;
        for (const auto time : times) {
            const int tick = core::TempoTickAtMs(mapped, offsetSeconds, time);
            if (tick <= previous || tick < -1000000 || tick > 999999) {
                fits = false;
                break;
            }
            previous = tick;
            positions.push_back(tick);
        }
        if (!fits) continue;
        for (std::size_t i = 0; i < positions.size(); ++i) {
            route.turns[i].beat = positions[i];
            route.turns[i].timeMs = times[i];
        }
        route.ticksPerBeat = mapped.ticksPerBeat;
        route.bpm = mapped.bpm;
        route.tempoMap = markers;
        chart.offsetSeconds = offsetSeconds;
        error.clear();
        return true;
    }
    error = "New BPM markers would merge notes; shared timing remains unchanged.";
    return false;
}

inline bool ApplySongTiming(core::LoadedChart& chart,
                             double bpm, int offsetMs,
                             std::string& error) {
    return ApplySongTiming(chart,
        {{static_cast<std::int64_t>(offsetMs), bpm}}, error);
}

} // namespace locus::app
