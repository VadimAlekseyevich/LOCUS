#pragma once

#include "core/route.h"
#include "core/tempo_map.h"

#include <cmath>
#include <optional>

namespace locus::app {

// A chart has a 4/4 measure by default. Skip to the first note minus the
// smallest *whole* number of measures whose duration is at least one second.
// The authoring timestamp is authoritative (cached tick positions may change).
// Never seek backward, past the first note or into negative song time.
inline std::optional<double> IntroSkipDestination(
    const core::Chart& chart, double offsetSeconds,
    double currentSongSeconds) noexcept {
    if (chart.turns.empty() || !std::isfinite(chart.bpm) ||
        chart.bpm <= 0.0 || !std::isfinite(offsetSeconds) ||
        !std::isfinite(currentSongSeconds)) return std::nullopt;

    constexpr double kBeatsPerMeasure = 4.0;
    constexpr double kMinimumLeadMs = 1000.0;
    const auto tempo = core::EffectiveTempoMap(chart, offsetSeconds);
    const double firstNoteMs = static_cast<double>(
        core::EventTimeMs(chart, chart.turns.front(), offsetSeconds));
    // Measure length is musical, not a fixed interval in seconds after a
    // BPM change. Find the number of prior *whole measures* needed to leave
    // at least one real second of audio, integrating every tempo segment.
    const double firstBeat = core::TempoBeatAtMs(tempo, firstNoteMs);
    const double oneSecondBefore = core::TempoBeatAtMs(
        tempo, firstNoteMs - kMinimumLeadMs);
    const double measures = std::max(1.0, std::ceil(
        (firstBeat - oneSecondBefore - 1e-9) / kBeatsPerMeasure));
    const double seekTo = core::TempoMsAtBeat(
        tempo, firstBeat - measures * kBeatsPerMeasure) / 1000.0;
    if (!std::isfinite(seekTo) || seekTo <= 0.0 ||
        currentSongSeconds + 1e-6 >= seekTo) return std::nullopt;
    return seekTo;
}

} // namespace locus::app
