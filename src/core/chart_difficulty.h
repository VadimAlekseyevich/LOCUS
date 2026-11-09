#pragma once

#include "core/route.h"

#include <cstddef>

namespace locus::core {

// Difficulty v2 is an unbounded, nonnegative chart-only estimate.
// Not a grade, a player skill rating or a promise of sight-readability.
// No skin, key binding, playback speed or song duration dependence.
struct ChartDifficulty {
    double overall = 0.0;
    double speed = 0.0;
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

// Exact note timestamps win over ticks; only fallback notes use offsetSeconds.
// Invalid/unsorted events are skipped without impacting the rest of the app.
// Complexity: O(n log n), with bounded local geometry comparisons per note.
ChartDifficulty CalculateChartDifficulty(
    const Chart& chart, double offsetSeconds = 0.0);

} // namespace locus::core
