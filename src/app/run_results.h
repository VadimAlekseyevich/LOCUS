#pragma once

#include "core/judgement.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace locus::app {

// Immutable summary only. No keypress events, movement or replay data.
// Song Set identity survives pack title/artist changes. Difficulty identity
// currently uses its installed filename; display title is never a key.
struct RunResult {
    std::string id;
    std::string songSetId;
    std::string difficultyFile;
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
    std::string difficultyFile;
    bool operator==(const RunKey& other) const noexcept {
        return songSetId == other.songSetId &&
               difficultyFile == other.difficultyFile;
    }
    bool operator!=(const RunKey& other) const noexcept {
        return !(*this == other);
    }
    bool Empty() const noexcept {
        return songSetId.empty() || difficultyFile.empty();
    }
};

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

std::string FormatRunTimestamp(std::int64_t unixMs);
bool ValidRunResult(const RunResult& result) noexcept;

} // namespace locus::app
