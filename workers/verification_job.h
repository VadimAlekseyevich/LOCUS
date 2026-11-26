#pragma once
#include "app/publication_revision.h"
#include "core/score_verification.h"
#include "core/performance.h"

namespace locus::worker {
// Supplied by the trusted orchestrator from immutable publication and
// attempt records. None of these fields are assertions of a client.
struct ScoreJob {
    std::string difficultyId;
    std::string gameplaySha256;
    std::int64_t audioDurationMs=0; // independently decoded by media worker
    int ratePercent=100;
    core::ModFlags modFlags=0;
    std::uint32_t modSeed=0;
};
struct ScoreEvaluation {
    core::VerifiedReplayScore score;
    core::PerformanceDifficulty difficulty;
    double pp=0.0;
};
// Private, stable staging; no authentication, anti-cheat or rank approval.
// Recompute digests before deriving score; no client-provided grades/stars.
bool EvaluateScoreJob(const std::filesystem::path& revisionFolder,
                      const ScoreJob& job,std::string_view replayBytes,
                      ScoreEvaluation& result,std::string& error);
} // namespace locus::worker
