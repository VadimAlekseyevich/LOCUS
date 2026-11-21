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

// Every tempo marker is the first beat of a new local measure. We preserve
// absolute song milliseconds and exact tempo on both sides. When a marker
// falls between prior beats, reserve the remainder of the old beat-index
// interval and begin the next segment at the next whole beat. This avoids
// mapping a freshly inserted marker to a fractional/off-grid downbeat.
// The reserved indices have no song duration; inverse mapping clamps them
// to the boundary and chart editing never authors notes at those indices.
inline double TempoMarkerAnchorBeat(double previousBeat) noexcept {
    return std::ceil(previousBeat - 1e-9);
}

// Prefix at the START of one segment. Mainly for drawing the note ruler:
// one pass per visible tempo segment, never a full map scan per grid line.
inline double TempoSegmentStartBeat(const std::vector<TempoMarker>& map,
                                    std::size_t index) noexcept {
    if (map.empty()) return 0.0;
    index = std::min(index, map.size() - 1);
    double beat = 0.0;
    for (std::size_t i = 1; i <= index; ++i) {
        beat += (map[i].timeMs - map[i-1].timeMs) *
                map[i-1].bpm / 60000.0;
        beat = TempoMarkerAnchorBeat(beat);
    }
    return beat;
}

inline double TempoBeatAtMs(const std::vector<TempoMarker>& map,
                            double timeMs) noexcept {
    if (map.empty() || !std::isfinite(timeMs)) return 0.0;
    double beats = 0.0;
    if (timeMs < static_cast<double>(map.front().timeMs))
        return (timeMs - map.front().timeMs) * map.front().bpm / 60000.0;
    for (std::size_t i = 0; i < map.size(); ++i) {
        const double from = static_cast<double>(map[i].timeMs);
        if (i + 1 == map.size())
            return beats + (timeMs - from) * map[i].bpm / 60000.0;
        const double to = static_cast<double>(map[i+1].timeMs);
        // The exact marker belongs to the NEXT tempo and is a downbeat.
        if (timeMs < to)
            return beats + (timeMs - from) * map[i].bpm / 60000.0;
        beats = TempoMarkerAnchorBeat(
            beats + (to - from) * map[i].bpm / 60000.0);
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
        const double from = static_cast<double>(map[i].timeMs);
        if (i + 1 == map.size())
            return from + (beat - passed) * 60000.0 / map[i].bpm;
        const double to = static_cast<double>(map[i+1].timeMs);
        const double endBeat = passed + (to - from) * map[i].bpm / 60000.0;
        if (beat < endBeat)
            return from + (beat - passed) * 60000.0 / map[i].bpm;
        const double nextBeat = TempoMarkerAnchorBeat(endBeat);
        if (beat < nextBeat) return to; // Reserved, zero-duration gap.
        passed = nextBeat;
    }
    return map.back().timeMs;
}

// A ruler click snaps in LOCAL song-time, not against a global integer
// index whose phase can jump at a tempo marker. If a subdivision would
// cross the next marker, the marker's own downbeat wins. All authored
// placements remain exact integer subdivisions in their tempo segment.
inline int TempoSnapTickAtSeconds(const Chart& chart, double offsetSeconds,
                                 double songSeconds) {
    if (!std::isfinite(songSeconds) || chart.ticksPerBeat <= 0 ||
        chart.snapDivision <= 0 ||
        chart.ticksPerBeat % chart.snapDivision != 0)
        return chart.firstBeat;
    const auto map = EffectiveTempoMap(chart, offsetSeconds);
    if (map.empty()) return chart.firstBeat;
    const double songMs = songSeconds * 1000.0;
    const auto next = std::upper_bound(
        map.begin(), map.end(), songMs,
        [](double ms, const TempoMarker& marker) {
            return ms < static_cast<double>(marker.timeMs);
        });
    const std::size_t segment = next == map.begin() ? 0
        : static_cast<std::size_t>(next - map.begin() - 1);
    const auto& marker = map[segment];
    const double subdivisionMs =
        60000.0 / (marker.bpm * chart.snapDivision);
    const double subdivisions = std::round(
        (songMs - marker.timeMs) / subdivisionMs);
    const double snappedMs = marker.timeMs + subdivisions * subdivisionMs;
    double tick = (TempoSegmentStartBeat(map, segment) +
                   subdivisions / chart.snapDivision) * chart.ticksPerBeat;
    if (segment + 1 < map.size() &&
        snappedMs >= map[segment+1].timeMs) {
        // Prevent an old section's subdivision from floating past a new
        // downbeat, including the reserved fractional-beat prefix.
        tick = TempoSegmentStartBeat(map, segment + 1) * chart.ticksPerBeat;
    }
    if (!std::isfinite(tick)) return chart.firstBeat;
    const double step = chart.ticksPerBeat / chart.snapDivision;
    const double bounded = std::clamp(tick,
        static_cast<double>(chart.firstBeat),
        static_cast<double>(chart.lengthBeats - 1));
    // Keep the snapped tick aligned to the same discrete subdivision lattice.
    // The caller may clamp to the chart bounds, which can be non-snap aligned.
    const double aligned = std::round(bounded / step) * step;
    return static_cast<int>(std::clamp(aligned,
        static_cast<double>(chart.firstBeat),
        static_cast<double>(chart.lengthBeats - 1)));
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

// Move by MUSICAL BARS, not by adding four global tick indices. Every
// subsequent BPM marker is the beginning of a new bar, including markers
// that were inserted mid-measure. This is used by the note ruler's left/right
// bar navigation and held-key repeats.
inline int TempoAdjacentBarTick(const Chart& chart, double offsetSeconds,
                               int selectedTick, int direction) {
    if (direction == 0 || chart.ticksPerBeat <= 0)
        return selectedTick;
    const auto map = EffectiveTempoMap(chart, offsetSeconds);
    if (map.empty()) return selectedTick;
    const double cursor = TempoSecondsAtTick(
        chart, offsetSeconds, selectedTick);
    const double first = TempoSecondsAtTick(
        chart, offsetSeconds, chart.firstBeat);
    const double last = TempoSecondsAtTick(
        chart, offsetSeconds, chart.lengthBeats - 1);
    constexpr double eps = 1e-7;
    double best = direction > 0
        ? std::numeric_limits<double>::infinity()
        : -std::numeric_limits<double>::infinity();

    for (std::size_t i = 0; i < map.size(); ++i) {
        const double start = map[i].timeMs / 1000.0;
        const double duration = 240.0 / map[i].bpm; // Four quarter notes.
        const double next = i+1 < map.size()
            ? map[i+1].timeMs / 1000.0
            : std::numeric_limits<double>::infinity();
        if (direction > 0) {
            double bar = std::floor((cursor-start) / duration + eps) + 1.0;
            if (i > 0) bar = std::max(bar, 0.0);
            const double candidate = start + bar * duration;
            if (candidate <= cursor + eps ||
                candidate < first - eps || candidate > last + eps ||
                candidate >= next - eps) continue;
            best = std::min(best, candidate);
        } else {
            double bar = std::ceil((cursor-start) / duration - eps) - 1.0;
            if (i+1 < map.size())
                bar = std::min(bar, std::floor((next-start-eps) / duration));
            if (i > 0 && bar < 0.0) continue;
            const double candidate = start + bar * duration;
            if (candidate >= cursor - eps ||
                candidate < first - eps || candidate > last + eps ||
                candidate >= next - eps) continue;
            best = std::max(best, candidate);
        }
    }
    if (!std::isfinite(best)) return selectedTick;
    return TempoSnapTickAtSeconds(chart, offsetSeconds, best);
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
