#include "app/async_library_refresh.h"

#include <chrono>
#include <exception>
#include <filesystem>
#include <future>
#include <string>
#include <utility>

namespace locus::app {

bool AsyncLibraryRefresh::Start(const SongLibrary& current,
                                std::string& status) {
    const auto root = current.Root();
    const auto revision = current.Revision();
    const auto diskEpoch = current.DiskMutationEpoch();
    const auto ticket = requested_;
    auto stop = std::make_shared<std::atomic<bool>>(false);
    // The latest *displaced* published index is an independent copy of
    // the prior generation. Transfer ownership to the worker: only the
    // worker may read or mutate this instance while scanning.
    auto seed = std::move(reuseIndex_);
    try {
        worker_ = std::async(std::launch::async,
            [root, revision, diskEpoch, ticket, stop,
             seed = std::move(seed)]() mutable -> Outcome {
                Outcome result;
                result.launchedRevision = revision;
                result.launchedDiskEpoch = diskEpoch;
                result.ticket = ticket;
                result.index = seed && seed->Root() == root
                    ? std::move(seed) : std::make_unique<SongLibrary>(root);
                try {
                    result.success = result.index->RefreshDetached(
                        result.status, stop.get());
                } catch (const std::exception& e) {
                    result.status = std::string("Scan failed: ") + e.what();
                } catch (...) {
                    result.status = "Unexpected library scan failure.";
                }
                return result;
            });
        cancellation_ = std::move(stop);
        status = "LIBRARY: SCANNING IN BACKGROUND";
        return true;
    } catch (const std::exception& e) {
        cancellation_.reset();
        status = std::string("Cannot start library scanner: ") + e.what();
        return false;
    }
}

bool AsyncLibraryRefresh::Request(const SongLibrary& current,
                                  std::string& status) {
    ++requested_;
    if (worker_.valid()) {
        if (cancellation_)
            cancellation_->store(true, std::memory_order_relaxed);
        status = "LIBRARY: NEWER RESCAN QUEUED";
        return true;
    }
    return Start(current, status);
}

AsyncLibraryRefresh::PollState AsyncLibraryRefresh::Poll(
    SongLibrary& current, std::string& status) {
    if (!worker_.valid()) return PollState::Idle;
    if (worker_.wait_for(std::chrono::seconds(0)) !=
        std::future_status::ready) return PollState::Pending;

    Outcome result;
    try {
        result = worker_.get();
    } catch (const std::exception& e) {
        status = std::string("Library scanner failed: ") + e.what();
        cancellation_.reset();
        return PollState::Failed;
    } catch (...) {
        status = "Unexpected library scanner failure.";
        cancellation_.reset();
        return PollState::Failed;
    }
    cancellation_.reset();
    if (result.ticket != requested_ ||
        result.launchedRevision != current.Revision() ||
        result.launchedDiskEpoch != current.DiskMutationEpoch()) {
        // A superseded SUCCESSFUL scan can still seed a newer worker after
        // fingerprints are rechecked. A cancelled or failed detached scan
        // may have moved chart note vectors out of its cache, so retaining
        // it would incorrectly reuse empty/moved-from chart data.
        if (result.success) reuseIndex_ = std::move(result.index);
        else reuseIndex_.reset();
        if (result.ticket == requested_) ++requested_;
        return Start(current, status) ? PollState::Pending : PollState::Failed;
    }
    status = std::move(result.status);
    if (!result.success || !result.index) {
        // The isolated scan may have consumed old chart vectors before
        // detecting a bad file or cancellation. Never cache this potentially
        // moved-from index; the live published catalog remains intact.
        reuseIndex_.reset();
        return PollState::Failed;
    }
    if (!current.PublishScannedIndex(std::move(*result.index))) {
        status = "Library root changed during background refresh.";
        return PollState::Failed;
    }
    // PublishScannedIndex swaps the two vectors. The worker's index now
    // owns the *previous* main-thread snapshot, which stays valid after
    // the UI moves to the new one. Reuse it for the next background scan.
    reuseIndex_ = std::move(result.index);
    return PollState::Published;
}

void AsyncLibraryRefresh::CancelAndJoin() noexcept {
    if (cancellation_)
        cancellation_->store(true, std::memory_order_relaxed);
    if (worker_.valid()) {
        try { (void)worker_.get(); }
        catch (...) {}
    }
    cancellation_.reset();
    reuseIndex_.reset();
}

} // namespace locus::app
