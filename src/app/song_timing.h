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
    if (route.ticksPerBeat <= 0 || route.ticksPerBeat > 48) {
        error = "Difficulty has an unsupported tick resolution.";
        return false;
    }
    // Song Set timing changes are transactional: compute all projections
    // before touching the difficulty. A grid tick can contain multiple
    // different millisecond timestamps; tempo editing must never reject
    // or delete those distinct authored events.
    core::Chart projected = route;
    projected.bpm = markers.front().bpm;
    projected.tempoMap = markers;
    const double offsetSeconds = markers.front().timeMs / 1000.0;
    int previous = -1000001;
    for (auto& note : projected.turns) {
        const auto ms = core::EventTimeMs(route, note, chart.offsetSeconds);
        const int tick = core::TempoTickAtMs(projected, offsetSeconds, ms);
        if (tick < previous || tick < -1000000 || tick > 999999) {
            error = "New BPM markers project a note outside valid tick order.";
            return false;
        }
        previous = tick;
        note.beat = tick;
        note.timeMs = ms;
    }
    route = std::move(projected);
    chart.offsetSeconds = offsetSeconds;
    error.clear();
    return true;
}

inline bool ApplySongTiming(core::LoadedChart& chart,
                             double bpm, int offsetMs,
                             std::string& error) {
    return ApplySongTiming(chart,
        {{static_cast<std::int64_t>(offsetMs), bpm}}, error);
}

} // namespace locus::app
