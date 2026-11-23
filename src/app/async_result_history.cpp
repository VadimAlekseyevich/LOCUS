#include "app/async_result_history.h"

#include <chrono>
#include <exception>
#include <future>
#include <string>
#include <utility>

namespace locus::app {

bool AsyncResultHistory::Start(std::string& status) {
    const auto root = root_;
    const RunKey key = latest_;
    const std::uint64_t ticket = requested_;
    auto stop = std::make_shared<std::atomic<bool>>(false);
    try {
        worker_ = std::async(std::launch::async,
            [root, key, ticket, stop]() -> Outcome {
                Outcome outcome;
                outcome.key = key;
                outcome.ticket = ticket;
                try {
                    RunResultsStore store(root);
                    outcome.records = store.Load(key, outcome.status, stop.get());
                    outcome.success = outcome.status.empty();
                } catch (const std::exception& e) {
                    outcome.status = std::string("Cannot load results: ") + e.what();
                } catch (...) {
                    outcome.status = "Unexpected results loading failure.";
                }
                return outcome;
            });
        cancellation_ = std::move(stop);
        status = "LOADING SAVED RESULTS";
        return true;
    } catch (const std::exception& e) {
        cancellation_.reset();
        status = std::string("Cannot start history reader: ") + e.what();
        return false;
    }
}

bool AsyncResultHistory::Request(RunKey key, std::string& status) {
    if (key.Empty()) {
        status = "No difficulty selected for result history.";
        return false;
    }
    latest_ = std::move(key);
    ++requested_;
    if (worker_.valid()) {
        if (cancellation_)
            cancellation_->store(true, std::memory_order_relaxed);
        status = "LOADING NEW SELECTION";
        return true;
    }
    return Start(status);
}

AsyncResultHistory::PollState AsyncResultHistory::Poll(
    const RunKey& selected, std::vector<RunResult>& records,
    std::string& status) {
    if (!worker_.valid()) return PollState::Idle;
    if (worker_.wait_for(std::chrono::seconds(0)) !=
        std::future_status::ready) return PollState::Pending;

    Outcome outcome;
    try {
        outcome = worker_.get();
    } catch (const std::exception& e) {
        status = std::string("History reader failed: ") + e.what();
        cancellation_.reset();
        return PollState::Failed;
    } catch (...) {
        status = "Unexpected history reader failure.";
        cancellation_.reset();
        return PollState::Failed;
    }
    cancellation_.reset();
    if (outcome.ticket != requested_ || outcome.key != selected) {
        // A selection disappeared entirely or changed again; the caller
        // will explicitly Request() any newly selected difficulty.
        if (selected != latest_) return PollState::Idle;
        return Start(status) ? PollState::Pending : PollState::Failed;
    }
    if (!outcome.success) {
        status = std::move(outcome.status);
        return PollState::Failed;
    }
    records = std::move(outcome.records);
    status.clear();
    return PollState::Ready;
}

void AsyncResultHistory::CancelAndJoin() noexcept {
    if (cancellation_)
        cancellation_->store(true, std::memory_order_relaxed);
    if (worker_.valid()) {
        try { (void)worker_.get(); }
        catch (...) {}
    }
    cancellation_.reset();
}

} // namespace locus::app
