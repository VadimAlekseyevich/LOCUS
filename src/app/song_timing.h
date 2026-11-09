#pragma once

#include "core/chart_json.h"
#include "core/route.h"

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace locus::app {

// Song Set timing is authoritative. Absolute note milliseconds stay fixed,
// even when older difficulty files have different BPM/offset values.
inline bool ApplySongTiming(core::LoadedChart& chart, double bpm, int offsetMs,
                            std::string& error) {
    if (!std::isfinite(bpm) || bpm <= 0.0 ||
        offsetMs < -3600000 || offsetMs > 3600000) {
        error = "Song BPM or offset is outside supported limits.";
        return false;
    }
    auto& route = chart.route;
    const int oldTicks = route.ticksPerBeat;
    if (oldTicks <= 0 || oldTicks > 48) {
        error = "Difficulty has an unsupported tick resolution.";
        return false;
    }
    if (route.bpm == bpm &&
        static_cast<int>(std::llround(chart.offsetSeconds * 1000.0)) == offsetMs) {
        error.clear();
        return true;
    }
    std::vector<std::int64_t> times;
    times.reserve(route.turns.size());
    for (const auto& note : route.turns)
        times.push_back(core::EventTimeMs(route, note, chart.offsetSeconds));

    // The time domain is authoritative. Raise tick resolution if a changed
    // BPM would otherwise collapse two notes onto a single grid point.
    for (int multiplier = 1; multiplier <= 48 / oldTicks; ++multiplier) {
        const int ticks = oldTicks * multiplier;
        int previous = -1000001;
        std::vector<int> positions;
        positions.reserve(times.size());
        bool fits = true;
        for (const auto time : times) {
            const int at = core::TickAtTimeMs(bpm, ticks,
                offsetMs / 1000.0, time);
            if (at <= previous || at < -1000000 || at > 999999) {
                fits = false;
                break;
            }
            previous = at;
            positions.push_back(at);
        }
        if (!fits) continue;
        for (std::size_t i = 0; i < positions.size(); ++i) {
            route.turns[i].beat = positions[i];
            route.turns[i].timeMs = times[i];
        }
        route.ticksPerBeat = ticks;
        route.bpm = bpm;
        chart.offsetSeconds = offsetMs / 1000.0;
        error.clear();
        return true;
    }
    error = "Shared song timing would merge two notes. "
            "No existing note positions were changed.";
    return false;
}

} // namespace locus::app
