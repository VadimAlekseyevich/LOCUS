#pragma once

#include "app/song_library.h"

#include <atomic>
#include <cstdint>
#include <future>
#include <memory>
#include <string>

namespace locus::app {

// Exactly one active scan, entirely isolated from the main-thread catalog.
// Multiple refresh requests coalesce. A generation comparison prevents stale
// background results from overwriting later imports or editor saves.
class AsyncLibraryRefresh {
public:
    enum class PollState { Idle, Pending, Published, Failed };

    AsyncLibraryRefresh() = default;
    ~AsyncLibraryRefresh() { CancelAndJoin(); }
    AsyncLibraryRefresh(const AsyncLibraryRefresh&) = delete;
    AsyncLibraryRefresh& operator=(const AsyncLibraryRefresh&) = delete;

    bool Request(const SongLibrary& current, std::string& status);
    PollState Poll(SongLibrary& current, std::string& status);
    bool Busy() const noexcept { return worker_.valid(); }
    void CancelAndJoin() noexcept;

private:
    struct Outcome {
        std::unique_ptr<SongLibrary> index;
        std::string status;
        std::uint64_t launchedRevision = 0;
        std::uint64_t launchedDiskEpoch = 0;
        std::uint64_t ticket = 0;
        bool success = false;
    };
    bool Start(const SongLibrary& current, std::string& status);

    std::future<Outcome> worker_;
    // Keep the displaced, previously published index for the next worker.
    // Its compact summaries can be reused by file fingerprints without
    // borrowing anything from the live main-thread library.
    std::unique_ptr<SongLibrary> reuseIndex_;
    std::shared_ptr<std::atomic<bool>> cancellation_;
    std::uint64_t requested_ = 0;
};

} // namespace locus::app
