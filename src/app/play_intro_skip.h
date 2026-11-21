#pragma once

#include "core/route.h"
#include "core/tempo_map.h"

#include <algorithm>
#include <cmath>
#include <optional>

namespace locus::app {

// Both intro skipping and automatic startup use one shared musical lead-in.
// At least one second before the first authored note, rounded to full 4/4
// measures and integrated across tempo changes. May be negative: the scored
// transport supports SILENT preroll before audio file timestamp zero.
inline constexpr double kMinimumFirstNoteLeadSeconds = 1.0;

inline std::optional<double> FirstNoteLeadStartSeconds(
    const core::Chart& chart, double offsetSeconds) {
    if (chart.turns.empty() || !std::isfinite(chart.bpm) ||
        chart.bpm <= 0.0 || !std::isfinite(offsetSeconds))
        return std::nullopt;

    constexpr double kBeatsPerMeasure = 4.0;
    const auto tempo = core::EffectiveTempoMap(chart, offsetSeconds);
    const double firstNoteMs = static_cast<double>(
        core::EventTimeMs(chart, chart.turns.front(), offsetSeconds));

    // Whole measures have different real durations when BPM changes.
    const double firstBeat = core::TempoBeatAtMs(tempo, firstNoteMs);
    const double beforeLead = core::TempoBeatAtMs(
        tempo, firstNoteMs - kMinimumFirstNoteLeadSeconds * 1000.0);
    double measures = std::max(1.0, std::ceil(
        (firstBeat - beforeLead - 1e-9) / kBeatsPerMeasure));
    double start = core::TempoMsAtBeat(
        tempo, firstBeat - measures * kBeatsPerMeasure) / 1000.0;
    // BPM markers are new local downbeats. Their reserved fractional-beat
    // indices can make a beat-distance calculation slightly shorter in REAL
    // time than requested; guarantee the full second on the audio clock.
    for (int guard = 0; guard < 4 &&
         std::isfinite(start) &&
         start > firstNoteMs / 1000.0 - kMinimumFirstNoteLeadSeconds + 1e-8;
         ++guard) {
        measures += 1.0;
        start = core::TempoMsAtBeat(
            tempo, firstBeat - measures * kBeatsPerMeasure) / 1000.0;
    }
    if (!std::isfinite(start)) return std::nullopt;
    return start;
}

// Normal scored starts preserve the existing negative chart-offset preroll.
// Only if the first note would arrive within one second of that start do we
// begin earlier. This never shifts the MP3, chart timestamps or judgement
// windows; the transport simply counts up from a negative audio coordinate.
// Editor TEST seeks deliberately start at the playhead and bypass this.
inline double ScoredChartStartSeconds(
    const core::Chart& chart, double offsetSeconds) {
    const double defaultStart = std::min(0.0, offsetSeconds);
    if (!std::isfinite(defaultStart) || chart.turns.empty())
        return 0.0;
    const double firstNote = static_cast<double>(
        core::EventTimeMs(chart, chart.turns.front(), offsetSeconds)) / 1000.0;
    if (firstNote - defaultStart >= kMinimumFirstNoteLeadSeconds)
        return defaultStart;
    const auto lead = FirstNoteLeadStartSeconds(chart, offsetSeconds);
    return lead ? std::min(defaultStart, *lead) : defaultStart;
}

// Manual skip is forward-only. Its destination is the same whole-measure
// lead position used by an early chart's automatic startup, but it must stay
// within the actual audio file instead of seeking to a negative timestamp.
inline std::optional<double> IntroSkipDestination(
    const core::Chart& chart, double offsetSeconds,
    double currentSongSeconds) {
    if (!std::isfinite(currentSongSeconds)) return std::nullopt;
    const auto seekTo = FirstNoteLeadStartSeconds(chart, offsetSeconds);
    if (!seekTo || *seekTo <= 0.0 ||
        currentSongSeconds + 1e-6 >= *seekTo) return std::nullopt;
    return seekTo;
}

} // namespace locus::app
