#include "core/score_verification.h"

#include <cmath>

namespace locus::core {

std::optional<VerifiedReplayScore> EvaluateCompletedReplay(
    const Chart& chart, double authoritativeOffsetSeconds,
    const ReplayTrace& trace) {
    if (!std::isfinite(authoritativeOffsetSeconds) ||
        trace.offsetSeconds != authoritativeOffsetSeconds)
        return std::nullopt;
    TurnJudge judge(chart);
    if (!RejudgeReplay(trace,judge) || judge.Failed() ||
        !judge.AllScorableJudged() ||
        static_cast<std::size_t>(judge.Counts().Resolved()) != judge.ScorableCount())
        return std::nullopt;
    return VerifiedReplayScore{judge.Counts(),judge.Statistics()};
}

} // namespace locus::core
