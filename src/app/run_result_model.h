#pragma once
#include "core/judgement.h"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace locus::app {

// Immutable compact summary. Replay payloads are separate, loaded on demand.
// Both Song Set and difficulty IDs survive title and filename changes. The
// gameplay revision marks which recorded attempts used older authored notes.
struct RunResult {
    std::string id;
    std::string songSetId;
    std::string difficultyId;
    std::string gameplayRevision;
    int judgementVersion = core::kJudgementRulesVersion;
    std::string songTitle;
    std::string chartTitle;
    std::int64_t completedAtMs = 0; // UTC Unix milliseconds.
    core::GradeCounts grades;
    int notesJudged = 0;
    double accuracyPercent = 0.0;
    double practiceRate = 1.0;
    // The saved rate is the FINAL rate. True means one or more earlier
    // segments were played at another rate; it is not a fixed-rate result.
    bool variablePracticeRate = false;
    // Missing for attempts created before timing telemetry was introduced.
    std::optional<core::RunStatistics> statistics;
    bool hasReplay = false;
    core::ModFlags modFlags = 0;
    std::uint32_t modSeed = 0;
};

struct RunKey {
    std::string songSetId;
    std::string difficultyId;
    bool operator==(const RunKey& other) const noexcept {
        return songSetId == other.songSetId &&
               difficultyId == other.difficultyId;
    }
    bool operator!=(const RunKey& other) const noexcept {
        return !(*this == other);
    }
    bool Empty() const noexcept {
        return songSetId.empty() || difficultyId.empty();
    }
};

// Increment when gameplay judgement windows, grade semantics or scoring
// rules change. Keep historical scores visible but never mark them current.
inline constexpr int kJudgementRulesVersion = core::kJudgementRulesVersion;

enum class RunRevisionState { Current, OlderChart, OlderRules, Unknown };

inline RunRevisionState CompareRunRevision(
        const RunResult& run, const std::string& currentGameplayRevision) noexcept {
    if (currentGameplayRevision.empty()) return RunRevisionState::Unknown;
    if (run.judgementVersion != kJudgementRulesVersion)
        return RunRevisionState::OlderRules;
    return run.gameplayRevision == currentGameplayRevision
        ? RunRevisionState::Current : RunRevisionState::OlderChart;
}

// Snapshot at the completed-outro transition. Consumers supply the exact
// scored judge counts; this is the sole source for the local attempt snapshot.
// Invalid or ineligible transitions are silently ignored with no disk writes.
struct CompletedRunContext {
    bool completedThisFrame = false;
    bool editorTest = false;
    bool launchedFromEditor = false;
    RunKey key;
    std::string songTitle;
    std::string chartTitle;
    core::GradeCounts grades;
    std::size_t scorableCount = 0;
    double practiceRate = 1.0;
    std::string gameplayRevision;
    bool variablePracticeRate = false;
    // Missing for attempts created before timing telemetry was introduced.
    std::optional<core::RunStatistics> statistics;
    const core::ReplayTrace* replay = nullptr; // borrowed during capture
    bool spectator = false;
    core::ModFlags modFlags = 0;
    std::uint32_t modSeed = 0;
};

// Build an eligible attempt without touching the filesystem or assigning
// persistence identity/time. The storage adapter assigns the local ID.
// An ineligible transition returns nullopt with an empty error.
std::optional<RunResult> BuildCompletedRun(const CompletedRunContext& completion,
                                           std::string& error);
bool ValidRunResult(const RunResult& result) noexcept;
// Check a local summary against the shared authoritative scoring engine.
// This is consistency verification, not authentication or rank eligibility.
bool VerifyRunReplay(const RunResult& run, const core::ReplayTrace& replay,
                     const core::Chart& chart,
                     const std::string& loadedGameplayRevision,
                     double authoritativeOffsetSeconds);
} // namespace locus::app
