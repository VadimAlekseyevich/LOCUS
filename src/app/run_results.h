#pragma once

#include "core/judgement.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace locus::app {

// Immutable summary only. No keypress events, movement or replay data.
// Both Song Set and difficulty IDs survive title and filename changes. The
// gameplay revision marks which recorded attempts used older authored notes.
struct RunResult {
    std::string id;
    std::string songSetId;
    std::string difficultyId;
    std::string gameplayRevision;
    int judgementVersion = 1;
    std::string songTitle;
    std::string chartTitle;
    std::int64_t completedAtMs = 0; // UTC Unix milliseconds.
    core::GradeCounts grades;
    int notesJudged = 0;
    double accuracyPercent = 0.0;
    double practiceRate = 1.0;
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
inline constexpr int kJudgementRulesVersion = 1;

enum class RunRevisionState { Current, OlderChart, OlderRules, Unknown };

inline RunRevisionState CompareRunRevision(
        const RunResult& run, const std::string& currentGameplayRevision) noexcept {
    if (currentGameplayRevision.empty()) return RunRevisionState::Unknown;
    if (run.judgementVersion != kJudgementRulesVersion)
        return RunRevisionState::OlderRules;
    return run.gameplayRevision == currentGameplayRevision
        ? RunRevisionState::Current : RunRevisionState::OlderChart;
}

// One JSON file per *completed* attempt, outside the Song Set and chart.
// Disk I/O happens only on completion/selection; never per judging frame.
class RunResultsStore {
public:
    explicit RunResultsStore(std::filesystem::path root)
        : root_(std::move(root)) {}

    // Creates a new attempt file without overwriting an existing one.
    bool Save(RunResult& result, std::string& error) const;

    // Most recent first. Malformed files are skipped independently.
    std::vector<RunResult> Load(const RunKey& key,
                                std::string& error) const;

private:
    std::filesystem::path Directory(const RunKey& key) const;
    std::filesystem::path root_;
};

// Snapshot at the completed-outro transition. Consumers supply the exact
// scored judge counts; this is the sole path for persistence of an attempt.
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
};

// Returns true only after a durable result is written. On ineligible calls
// returns false with empty error, leaving saved unchanged. A storage failure
// returns false with its real error, which the application displays.
bool CaptureCompletedRun(const RunResultsStore& store,
                         const CompletedRunContext& completion,
                         RunResult& saved, std::string& error);

std::string FormatRunTimestamp(std::int64_t unixMs);
bool ValidRunResult(const RunResult& result) noexcept;

} // namespace locus::app
