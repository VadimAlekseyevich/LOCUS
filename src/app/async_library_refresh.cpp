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
    const auto ticket = requested_;
    auto stop = std::make_shared<std::atomic<bool>>(false);
    try {
        worker_ = std::async(std::launch::async,
            [root, revision, ticket, stop]() -> Outcome {
                Outcome result;
                result.launchedRevision = revision;
                result.ticket = ticket;
                result.index = std::make_unique<SongLibrary>(root);
                try {
                    result.success = result.index->Refresh(
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
        result.launchedRevision != current.Revision()) {
        if (result.ticket == requested_) ++requested_;
        return Start(current, status) ? PollState::Pending : PollState::Failed;
    }
    status = std::move(result.status);
    if (!result.success || !result.index) return PollState::Failed;
    if (!current.PublishScannedIndex(std::move(*result.index))) {
        status = "Library root changed during background refresh.";
        return PollState::Failed;
    }
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
}

} // namespace locus::app
