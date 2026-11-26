#pragma once
#include "core/judgement.h"

namespace locus::core {

// Derived data only, not an authenticated or ranked server score.
// The caller must supply a previously validated authoritative chart and
// its effective song-set offset; never derive that offset from the replay.
struct VerifiedReplayScore {
    GradeCounts grades;
    RunStatistics statistics;
};

// Reuse the production judge without desktop, filesystem, UI or a claimed
// result summary. Reject incompatible traces, mismatched offset, empty,
// failed and incomplete runs. Mathematical consistency is NOT anti-cheat.
// Online submission policy (identity, timestamp/transport bounds, mods,
// Finish operations and authorization) belongs to a separate boundary.
std::optional<VerifiedReplayScore> EvaluateCompletedReplay(
    const Chart& chart, double authoritativeOffsetSeconds,
    const ReplayTrace& trace);

} // namespace locus::core
