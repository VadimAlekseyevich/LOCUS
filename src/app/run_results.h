#pragma once

#include "app/run_result_model.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace locus::app {

// One JSON file per *completed* attempt, outside the Song Set and chart.
// Disk I/O happens only on completion/selection; never per judging frame.
class RunResultsStore {
public:
    RunResultsStore(std::filesystem::path root,std::string playerId)
        : root_(std::move(root)), playerId_(std::move(playerId)) {}

    // Creates a new attempt file without overwriting an existing one.
    bool Save(RunResult& result, std::string& error,
              const core::ReplayTrace* replay = nullptr) const;
    // The summary is the commit marker. Missing/corrupt sidecars never
    // invalidate the score history; payloads are read only on demand.
    bool LoadReplay(const RunResult& run, core::ReplayTrace& replay,
                    std::string& error) const;

    // Most recent first. Malformed files are skipped independently.
    std::vector<RunResult> Load(const RunKey& key,
                                std::string& error,
                                const std::atomic<bool>* cancelled = nullptr) const;

private:
    std::filesystem::path Directory(const RunKey& key) const;
    std::filesystem::path root_;
    const std::string playerId_;
};

// Returns true only after a durable result is written. On ineligible calls
// returns false with empty error, leaving saved unchanged. A storage failure
// returns false with its real error, which the application displays.
bool CaptureCompletedRun(const RunResultsStore& store,
                         const CompletedRunContext& completion,
                         RunResult& saved, std::string& error);

std::string FormatRunMods(const RunResult& result);
std::string FormatRunTimestamp(std::int64_t unixMs);
// Denominator belongs to the recorded chart revision, not the current chart.
std::string FormatRunCombo(const RunResult& result);

} // namespace locus::app
