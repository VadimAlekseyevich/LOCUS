#pragma once

#include "core/route.h"

#include <cstddef>
#include <array>

namespace locus::core {

// Difficulty V4: physical key strain only, on an uncapped nonnegative scale.
// Reading and peak density remain diagnostics; neither affects overall.
// Not a grade, a player skill rating or a promise of sight-readability.
inline constexpr int kChartDifficultyVersion = 4;
// No skin, key binding or song duration dependence. Default is native rate.
struct ChartDifficulty {
    double overall = 0.0;
    double speed = 0.0;
    double fingerStrain = 0.0; // individual retrigger and anchor pressure
    double rhythm = 0.0;
    double coordination = 0.0;
    double reading = 0.0;
    double stamina = 0.0;
    double peak = 0.0;
    double notesPerSecondPeak = 0.0;
    double keysPerSecondPeak = 0.0;
    double activeSeconds = 0.0; // Gaps >1.5s don't train stamina.
    std::size_t notes = 0;
    std::size_t keyPresses = 0;
};

// Bounded song-time visualization; local physical strain, not final stars.
// Max-pooling preserves hard sections when several fall in one display bin.
struct DifficultyTimeline {
    static constexpr std::size_t kBins=256;
    std::array<double,kBins> peaks{};
    double maximum=0.0;
};

// Exact note timestamps win over ticks; only fallback notes use offsetSeconds.
// Invalid/unsorted events are skipped without impacting the rest of the app.
// Complexity: O(n log n), with bounded local geometry comparisons per note.
ChartDifficulty CalculateChartDifficulty(
    const Chart& chart, double offsetSeconds = 0.0,
    DifficultyTimeline* timeline = nullptr, double durationSeconds = 0.0,
    double clockRate = 1.0);

} // namespace locus::core
