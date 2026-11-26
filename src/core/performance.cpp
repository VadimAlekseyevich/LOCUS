#include "core/performance.h"
#include <cmath>

namespace locus::core {
bool ValidPerformanceDifficulty(const PerformanceDifficulty& d) noexcept {
    return d.difficultyVersion==kChartDifficultyVersion && std::isfinite(d.stars) &&
        d.stars>=0.0 && d.stars<=100000.0 && d.keyPresses>0 && d.keyPresses<=2000000;
}

std::optional<PerformanceDifficulty> CalculatePerformanceDifficulty(
    const Chart& chart,double offsetSeconds,int ratePercent,ModFlags flags,
    std::uint32_t seed) {
    if (!ValidModRate(ratePercent) || !ValidModFlags(flags) || Assisted(flags) ||
        (!HasMod(flags,PlayMod::Random) && seed!=0)) return std::nullopt;
    const auto rate=ratePercent/100.0;
    // Mirror is a key permutation with invariant physical strain.
    const auto difficulty=HasMod(flags,PlayMod::Random)
        ? CalculateChartDifficulty(TransformModChart(chart,flags,seed),offsetSeconds,nullptr,0.0,rate)
        : CalculateChartDifficulty(chart,offsetSeconds,nullptr,0.0,rate);
    PerformanceDifficulty result{kChartDifficultyVersion,difficulty.overall,difficulty.keyPresses};
    if (!ValidPerformanceDifficulty(result)) return std::nullopt;
    return result;
}

std::optional<double> CalculatePerformance(const PerformanceDifficulty& d,
        const GradeCounts& g,int ratePercent,ModFlags flags) noexcept {
    if (!ValidPerformanceDifficulty(d) || !ValidModRate(ratePercent) ||
        !ValidModFlags(flags) || Assisted(flags) ||
        g.perfect<0 || g.good<0 || g.bad<0 || g.miss<0 ||
        static_cast<std::int64_t>(g.perfect)+g.good+g.bad+g.miss!=static_cast<std::int64_t>(d.keyPresses) ||
        (HasMod(flags,PlayMod::SuddenDeath) && g.miss) ||
        (HasMod(flags,PlayMod::Perfect) && (g.good || g.bad || g.miss)))
        return std::nullopt;
    const double accuracy=g.AccuracyPercent()/100.0;
    const double hitFraction=1.0-static_cast<double>(g.miss)/d.keyPresses;
    // Saturating short-chart confidence; no bonus for padding past 200 keys.
    const double length=std::min(1.0,std::sqrt(d.keyPresses/200.0));
    const double timing=HasMod(flags,PlayMod::Easy)?0.65:1.0;
    return 12.0*std::pow(d.stars,2.2)*std::pow(accuracy,6.0)*
        hitFraction*hitFraction*length*timing;
}
} // namespace locus::core
