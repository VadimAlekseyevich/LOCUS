#include "verification_job.h"
#include <algorithm>
#include <cmath>

namespace locus::worker {
bool EvaluateScoreJob(const std::filesystem::path& revisionFolder,
                      const ScoreJob& job,std::string_view replayBytes,
                      ScoreEvaluation& result,std::string& error) {
    error.clear();
    const auto validHash=[](const std::string& hash) {
        return hash.size()==64 && std::all_of(hash.begin(),hash.end(),[](char c) {
            return (c>='0' && c<='9') || (c>='a' && c<='f');
        });
    };
    if (!core::ValidDifficultyId(job.difficultyId) || !validHash(job.gameplaySha256) ||
        job.audioDurationMs<=0 || job.audioDurationMs>86400000 ||
        !core::ValidModRate(job.ratePercent) || !core::ValidModFlags(job.modFlags) ||
        core::Assisted(job.modFlags) ||
        (!core::HasMod(job.modFlags,core::PlayMod::Random) && job.modSeed!=0)) {
        error="Invalid trusted score job parameters."; return false;
    }
    core::ReplayTrace trace;
    if (replayBytes.size()>core::kMaxReplayBytes || !core::DecodeReplay(replayBytes,trace) ||
        trace.events.empty() || trace.judgementVersion!=core::kJudgementRulesVersion ||
        trace.ratePercent!=job.ratePercent || trace.modFlags!=job.modFlags ||
        trace.modSeed!=job.modSeed) {
        error="Replay format, rules or attempt configuration mismatch."; return false;
    }
    app::PackageRevisionContent revision;
    app::ValidatedPackage package;
    if (!app::DescribePackageRevision(revisionFolder,revision,error,&package)) return false;
    const auto digest=revision.difficulties.find(job.difficultyId);
    const auto selected=package.difficulties.find(job.difficultyId);
    if (digest==revision.difficulties.end() || selected==package.difficulties.end() ||
        digest->second.gameplaySha256!=job.gameplaySha256) {
        error="Published gameplay content does not match the expected revision."; return false;
    }
    auto& chart=selected->second.chart;
    const double duration=job.audioDurationMs/1000.0;
    if (!core::PrepareChartForSong(chart,duration,error)) return false;
    if (trace.offsetSeconds!=chart.offsetSeconds) {
        error="Replay offset does not match authoritative timing."; return false;
    }
    // Private prototype policy: require timestamped completion. Legacy
    // Finish has no dispatch timestamp and cannot establish elapsed time.
    // Allow the current +/-1s real calibration scaled by rate, plus a
    // silent tail for the final judgement. Do not sort dispatch order:
    // genuine native input can arrive with earlier captured timestamps.
    const double margin=job.ratePercent/100.0+1.0;
    for (const auto& event : trace.events) {
        if (event.action==core::ReplayAction::Finish ||
            event.songSeconds < -margin || event.songSeconds > duration+margin) {
            error="Replay contains an untimed finish or out-of-range event."; return false;
        }
    }
    const auto score=core::EvaluateCompletedReplay(chart.route,chart.offsetSeconds,trace);
    if (!score) { error="Replay does not reproduce an eligible completed score."; return false; }
    const auto difficulty=core::CalculatePerformanceDifficulty(chart.route,chart.offsetSeconds,
        job.ratePercent,job.modFlags,job.modSeed);
    if (!difficulty) { error="Unsupported performance difficulty."; return false; }
    const auto pp=core::CalculatePerformance(*difficulty,score->grades,job.ratePercent,job.modFlags);
    if (!pp || !std::isfinite(*pp)) { error="Cannot derive performance."; return false; }
    result={*score,*difficulty,*pp};
    return true;
}
} // namespace locus::worker
