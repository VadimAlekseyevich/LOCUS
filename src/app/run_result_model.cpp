#include "app/run_result_model.h"
#include "core/chart_json.h"
#include "core/score_verification.h"
#include "core/utf8.h"
#include <cmath>
#include <algorithm>
#include <map>
#include <functional>

namespace locus::app {
namespace {
bool ValidResultData(const RunResult& r) noexcept {
    if (r.songSetId.empty() || r.songSetId.size() > 256 ||
        !core::ValidDifficultyId(r.difficultyId) ||
        !core::ValidDifficultyId(r.gameplayRevision) ||
        r.judgementVersion < 1 || r.judgementVersion > 1000 ||
        !core::utf8::ValidName(r.songTitle, 256, 1024) ||
        !core::utf8::ValidName(r.chartTitle, 256, 1024) ||
        r.grades.perfect < 0 || r.grades.good < 0 ||
        r.grades.bad < 0 || r.grades.miss < 0 ||
        r.notesJudged <= 0 || r.notesJudged > 2000000 ||
        r.grades.perfect > r.notesJudged ||
        r.grades.good > r.notesJudged ||
        r.grades.bad > r.notesJudged ||
        r.grades.miss > r.notesJudged ||
        r.grades.Resolved() != r.notesJudged ||
        !std::isfinite(r.accuracyPercent) ||
        !std::isfinite(r.practiceRate) ||
        r.practiceRate <= 0.0 || r.practiceRate > 1.5 || !core::ValidModFlags(r.modFlags) || core::Assisted(r.modFlags) ||
        (!core::HasMod(r.modFlags,core::PlayMod::Random) && r.modSeed!=0) ||
        (core::HasMod(r.modFlags,core::PlayMod::SuddenDeath) && r.grades.miss) ||
        (core::HasMod(r.modFlags,core::PlayMod::Perfect) && (r.grades.good || r.grades.bad || r.grades.miss))) return false;
    if (r.statistics) {
        const auto& s = *r.statistics;
        const int hits = r.notesJudged - r.grades.miss;
        // At most miss+1 streaks; a nonempty run needs a nonempty best streak.
        const int minCombo = (hits + r.grades.miss) / (r.grades.miss + 1);
        const double limitMs = core::kBadWindowSeconds * core::WindowScale(r.modFlags) * 1000.0;
        // Historical rules may have different windows; only apply the
        // current window bound to results with the current rules revision.
        if (s.timedHits != hits || s.maxCombo < minCombo || s.maxCombo > hits ||
            !std::isfinite(s.meanErrorMs) || !std::isfinite(s.unstableRate) ||
            s.unstableRate < 0.0 ||
            (s.timedHits == 0 && s.meanErrorMs != 0.0) ||
            (s.timedHits < 2 && s.unstableRate != 0.0) ||
            (r.judgementVersion == core::kJudgementRulesVersion &&
             (std::abs(s.meanErrorMs) > limitMs + 1e-6 ||
              s.unstableRate > 10.0 * limitMs + 1e-6))) return false;
        if (s.histogram) {
            std::int64_t sum=0;
            for (int count : *s.histogram) {
                if (count<0 || count>s.timedHits) return false;
                sum+=count;
            }
            if (sum!=s.timedHits) return false;
        }
    }
    if (r.performanceDifficulty &&
        (!std::isfinite(r.performanceDifficulty->stars) ||
         r.performanceDifficulty->stars<0.0 ||
         r.performanceDifficulty->stars>100000.0 ||
         r.performanceDifficulty->difficultyVersion<1 ||
         r.performanceDifficulty->keyPresses!=static_cast<std::size_t>(r.notesJudged))) return false;
    const double computed = r.grades.AccuracyPercent();
    return std::abs(r.accuracyPercent - computed) <= 0.011;
}

} // namespace

bool ValidRunResult(const RunResult& r) noexcept {
    return core::ValidDifficultyId(r.id) && core::ValidDifficultyId(r.playerId) &&
        r.completedAtMs>0 && ValidResultData(r);
}

std::optional<RunResult> BuildCompletedRun(const CompletedRunContext& completion,
                                           std::string& error) {
    error.clear();
    // A Results screen or a repeated frame is NOT a new completed attempt.
    // Never publish TEST, spectator playback, abandoned or partial sessions.
    if (!completion.completedThisFrame || completion.editorTest ||
        completion.spectator || core::Assisted(completion.modFlags) ||
        completion.launchedFromEditor || completion.key.Empty() ||
        completion.scorableCount == 0 ||
        completion.scorableCount>2000000)
        return std::nullopt;
    // Sum in 64 bits before invoking GradeCounts::Resolved on claims.
    const auto& grades=completion.grades;
    if (grades.perfect<0 || grades.good<0 || grades.bad<0 || grades.miss<0 ||
        static_cast<std::int64_t>(grades.perfect)+grades.good+grades.bad+grades.miss !=
            static_cast<std::int64_t>(completion.scorableCount))
        return std::nullopt;

    RunResult record;
    record.songSetId = completion.key.songSetId;
    record.difficultyId = completion.key.difficultyId;
    record.gameplayRevision = completion.gameplayRevision;
    record.songTitle = completion.songTitle;
    record.chartTitle = completion.chartTitle;
    record.grades = completion.grades;
    record.notesJudged = static_cast<int>(completion.scorableCount);
    record.practiceRate = completion.practiceRate;
    record.variablePracticeRate = completion.variablePracticeRate;
    record.modFlags = completion.modFlags;
    record.modSeed = completion.modSeed;
    record.statistics = completion.statistics;
    record.performanceDifficulty=completion.performanceDifficulty;
    record.accuracyPercent=record.grades.AccuracyPercent();
    if (!ValidResultData(record)) {
        error="Invalid completed result.";
        return std::nullopt;
    }
    return record;
}

std::optional<double> RunPerformance(const RunResult& r) noexcept {
    if (!ValidRunResult(r) || !r.performanceDifficulty || r.variablePracticeRate ||
        r.judgementVersion!=core::kJudgementRulesVersion || r.songSetId=="builtin-demo")
        return std::nullopt;
    const auto percent=static_cast<int>(std::lround(r.practiceRate*100.0));
    if (std::abs(percent/100.0-r.practiceRate)>1e-9) return std::nullopt;
    return core::CalculatePerformance(*r.performanceDifficulty,r.grades,percent,r.modFlags);
}

PlayerProfileSnapshot ProjectPlayerProfile(
    const std::string& playerId,const std::vector<RunResult>& results) {
    PlayerProfileSnapshot snapshot;
    auto& summary=snapshot.summary;
    if (!core::ValidDifficultyId(playerId)) return snapshot;
    // Equal projection inputs with the same attempt ID count once.
    // Conflicting duplicates cannot make aggregation order-dependent.
    const auto sameProjection=[](const RunResult& a,const RunResult& b) {
        return a.songSetId==b.songSetId && a.difficultyId==b.difficultyId &&
            a.gameplayRevision==b.gameplayRevision && a.notesJudged==b.notesJudged &&
            a.songTitle==b.songTitle && a.chartTitle==b.chartTitle &&
            a.completedAtMs==b.completedAtMs && a.practiceRate==b.practiceRate &&
            a.modFlags==b.modFlags && a.modSeed==b.modSeed &&
            a.grades.perfect==b.grades.perfect && a.grades.good==b.grades.good &&
            a.grades.bad==b.grades.bad && a.grades.miss==b.grades.miss &&
            RunPerformance(a)==RunPerformance(b);
    };
    std::map<std::string,const RunResult*> unique;
    for (const auto& r : results) {
        if (r.playerId!=playerId || !ValidRunResult(r)) continue;
        const auto inserted=unique.emplace(r.id,&r);
        if (!inserted.second && inserted.first->second &&
            !sameProjection(*inserted.first->second,r)) inserted.first->second=nullptr;
    }
    struct Candidate { const RunResult* run=nullptr; double pp=0.0; };
    const auto better=[](const Candidate& a,const Candidate& b) {
        if (a.pp!=b.pp) return a.pp>b.pp;
        const auto aa=a.run->grades.AccuracyPercent(), ba=b.run->grades.AccuracyPercent();
        if (aa!=ba) return aa>ba;
        if (a.run->completedAtMs!=b.run->completedAtMs)
            return a.run->completedAtMs>b.run->completedAtMs;
        return a.run->id<b.run->id;
    };
    std::map<std::pair<std::string,std::string>,Candidate> best;
    double accuracySum=0.0,keys=0.0;
    for (const auto& entry : unique) {
        if (!entry.second) continue;
        const auto& r=*entry.second;
        ++summary.playCount;
        keys+=r.notesJudged;
        accuracySum+=r.grades.AccuracyPercent()*r.notesJudged;
        if (const auto pp=RunPerformance(r)) {
            auto& value=best[{r.songSetId,r.difficultyId}];
            const Candidate candidate{&r,*pp};
            if (!value.run || better(candidate,value)) value=candidate;
        }
    }
    if (keys>0.0) summary.weightedAccuracy=accuracySum/keys;
    std::vector<Candidate> values;
    values.reserve(best.size());
    for (const auto& entry : best) values.push_back(entry.second);
    std::sort(values.begin(),values.end(),better);
    summary.ratedDifficulties=values.size();
    snapshot.bestScores.reserve(values.size());
    double weight=1.0;
    for (const auto& value : values) {
        const double weighted=weight*value.pp;
        summary.totalPp+=weighted;
        snapshot.bestScores.push_back({*value.run,value.pp,weighted});
        weight*=0.95;
    }
    return snapshot;
}

PlayerPerformanceSummary SummarizePlayerPerformance(
    const std::string& playerId,const std::vector<RunResult>& results) {
    return ProjectPlayerProfile(playerId,results).summary;
}

bool VerifyRunReplay(const RunResult& run, const core::ReplayTrace& replay,
                     const core::Chart& chart,
                     const std::string& loadedGameplayRevision,
                     double authoritativeOffsetSeconds) {
    if (!ValidRunResult(run) || run.variablePracticeRate || replay.modFlags != run.modFlags || replay.modSeed != run.modSeed ||
        std::abs(replay.ratePercent / 100.0 - run.practiceRate) > 1e-9 ||
        CompareRunRevision(run, loadedGameplayRevision) != RunRevisionState::Current)
        return false;
    const auto verified=core::EvaluateCompletedReplay(chart,authoritativeOffsetSeconds,replay);
    if (!verified) return false;
    const auto& a = verified->grades;
    const auto& b = run.grades;
    if (a.perfect != b.perfect || a.good != b.good ||
        a.bad != b.bad || a.miss != b.miss ||
        a.Resolved() != run.notesJudged) return false;
    if (run.statistics) {
        const auto& actual = verified->statistics;
        const auto& expected = *run.statistics;
        if (actual.maxCombo != expected.maxCombo ||
            actual.timedHits != expected.timedHits ||
            std::abs(actual.meanErrorMs - expected.meanErrorMs) > 1e-6 ||
            std::abs(actual.unstableRate - expected.unstableRate) > 1e-6 ||
            (expected.histogram && actual.histogram != expected.histogram))
            return false;
    }
    return true;
}

} // namespace locus::app
