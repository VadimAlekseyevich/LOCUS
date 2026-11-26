#pragma once
#include "core/chart_difficulty.h"
#include "core/judgement.h"

namespace locus::core {

// Provisional local performance, independent of published/ranked status.
inline constexpr int kPerformanceVersion=1;
struct PerformanceDifficulty {
    int difficultyVersion=kChartDifficultyVersion;
    double stars=0.0;
    std::size_t keyPresses=0;
};

// Uses source chart with authoritative timing, not a pre-transformed route.
// Rate and Random seed are applied exactly once.
std::optional<PerformanceDifficulty> CalculatePerformanceDifficulty(
    const Chart& chart,double offsetSeconds,int ratePercent,ModFlags flags,
    std::uint32_t seed);

bool ValidPerformanceDifficulty(const PerformanceDifficulty& difficulty) noexcept;
// Empty for unsupported versions/mods, partial or invalid scores; zero is
// a valid performance for an all-miss attempt. No reading or combo bonus.
std::optional<double> CalculatePerformance(
    const PerformanceDifficulty& difficulty,const GradeCounts& grades,
    int ratePercent,ModFlags flags) noexcept;
} // namespace locus::core
