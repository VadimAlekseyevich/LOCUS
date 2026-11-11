#pragma once

#include "core/route.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace locus::core {

// All functions accept a single-marker map as well as an empty fallback.
// A segment begins exactly at timeMs, and its BPM continues until the next
// entry. The musical beat phase never resets at a tempo change.
inline std::vector<TempoMarker> EffectiveTempoMap(
    const Chart& chart, double offsetSeconds) {
    if (!chart.tempoMap.empty()) return chart.tempoMap;
    return {{static_cast<std::int64_t>(std::llround(offsetSeconds * 1000.0)),
             chart.bpm}};
}

inline bool ValidTempoMap(const std::vector<TempoMarker>& map,
                          std::string& error) {
    if (map.empty() || map.size() > 256) {
        error = "Tempo map requires 1 to 256 BPM markers.";
        return false;
    }
    for (std::size_t i = 0; i < map.size(); ++i) {
        const auto& item = map[i];
        if (!std::isfinite(item.bpm) || item.bpm <= 0.0 ||
            item.bpm > 10000.0 || item.timeMs < -3600000 ||
            item.timeMs > 86400000 ||
            (i == 0 && item.timeMs > 3600000) ||
            (i && item.timeMs <= map[i-1].timeMs)) {
            error = "BPM markers must have increasing millisecond offsets and valid tempos.";
            return false;
        }
    }
    error.clear();
    return true;
}

inline double TempoBeatAtMs(const std::vector<TempoMarker>& map,
                            double timeMs) noexcept {
    if (map.empty() || !std::isfinite(timeMs)) return 0.0;
    double beats = 0.0;
    if (timeMs < static_cast<double>(map.front().timeMs))
        return (timeMs - map.front().timeMs) * map.front().bpm / 60000.0;
    for (std::size_t i = 0; i < map.size(); ++i) {
        const double from = static_cast<double>(map[i].timeMs);
        const double to = i + 1 < map.size()
            ? static_cast<double>(map[i+1].timeMs) : timeMs;
        if (timeMs <= to) return beats +
            (timeMs - from) * map[i].bpm / 60000.0;
        beats += (to - from) * map[i].bpm / 60000.0;
    }
    return beats;
}

inline double TempoMsAtBeat(const std::vector<TempoMarker>& map,
                            double beat) noexcept {
    if (map.empty() || !std::isfinite(beat)) return 0.0;
    if (beat < 0.0)
        return map.front().timeMs + beat * 60000.0 / map.front().bpm;
    double passed = 0.0;
    for (std::size_t i = 0; i < map.size(); ++i) {
        const double beatsInSegment = i + 1 < map.size()
            ? (map[i+1].timeMs - map[i].timeMs) * map[i].bpm / 60000.0
            : std::numeric_limits<double>::infinity();
        if (beat <= passed + beatsInSegment || i + 1 == map.size())
            return map[i].timeMs + (beat - passed) * 60000.0 / map[i].bpm;
        passed += beatsInSegment;
    }
    return map.back().timeMs;
}

inline double TempoBeatAtSeconds(const Chart& chart,
                                 double offsetSeconds,
                                 double songSeconds) noexcept {
    if (chart.tempoMap.empty())
        return (songSeconds - offsetSeconds) * chart.bpm / 60.0;
    return TempoBeatAtMs(chart.tempoMap, songSeconds * 1000.0);
}

inline double TempoSecondsAtTick(const Chart& chart,
                                 double offsetSeconds, double tick) noexcept {
    if (chart.tempoMap.empty())
        return offsetSeconds + tick * 60.0 /
            (chart.bpm * chart.ticksPerBeat);
    return TempoMsAtBeat(chart.tempoMap,
        tick / chart.ticksPerBeat) / 1000.0;
}

inline int TempoTickAtMs(const Chart& chart,
                         double offsetSeconds, std::int64_t timeMs) noexcept {
    const double beat = TempoBeatAtSeconds(chart, offsetSeconds,
                                           timeMs / 1000.0);
    const double ticks = beat * chart.ticksPerBeat;
    if (!std::isfinite(ticks)) return 0;
    return static_cast<int>(std::llround(std::clamp(ticks,
        static_cast<double>(std::numeric_limits<int>::min()),
        static_cast<double>(std::numeric_limits<int>::max()))));
}

inline bool TempoTickRange(const Chart& chart, double offsetSeconds,
                           double durationSeconds, int& first, int& last,
                           std::string& error) {
    if (!std::isfinite(durationSeconds) || durationSeconds <= 0.0) {
        error = "Song duration is invalid.";
        return false;
    }
    const double earliest = TempoBeatAtSeconds(chart, offsetSeconds, 0.0)
        * chart.ticksPerBeat;
    const double latest = TempoBeatAtSeconds(chart, offsetSeconds,
        durationSeconds) * chart.ticksPerBeat;
    const double begin = std::min(0.0, std::floor(earliest));
    const double end = std::ceil(latest);
    if (!std::isfinite(begin) || !std::isfinite(end) ||
        begin < -1000000.0 || end > 1000000.0 ||
        end - begin < 1.0 || end - begin > 1000000.0) {
        error = "Tempo map exceeds playable song tick limits.";
        return false;
    }
    first = static_cast<int>(begin);
    last = static_cast<int>(end);
    error.clear();
    return true;
}

} // namespace locus::core
