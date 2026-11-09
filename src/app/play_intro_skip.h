#pragma once

#include "core/route.h"

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
    constexpr double kMinimumLeadSeconds = 1.0;
    const double secondsPerMeasure = kBeatsPerMeasure * 60.0 / chart.bpm;
    const double measures = std::ceil(
        (kMinimumLeadSeconds - 1e-9) / secondsPerMeasure);
    const double firstNoteSeconds = static_cast<double>(
        core::EventTimeMs(chart, chart.turns.front(), offsetSeconds)) / 1000.0;
    const double seekTo = firstNoteSeconds - measures * secondsPerMeasure;
    if (!std::isfinite(seekTo) || seekTo <= 0.0 ||
        currentSongSeconds + 1e-6 >= seekTo) return std::nullopt;
    return seekTo;
}

} // namespace locus::app
