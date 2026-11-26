#pragma once
#include "app/run_results.h"
#include "app/player_profile_view.h"
#include <chrono>
#include <future>
#include <memory>

namespace locus::app {
// One coalesced off-thread projection, requested on Home or Profile. A save
// invalidates an in-flight snapshot; no stale result can be published.
class AsyncPlayerSummary {
public:
    explicit AsyncPlayerSummary(const RunResultsStore& store):store_(store) {}
    ~AsyncPlayerSummary() { CancelAndJoin(); }
    void Invalidate() noexcept {
        dirty_=true;
        if (cancel_) cancel_->store(true,std::memory_order_relaxed);
    }
    // True only when the display state changes. No filesystem I/O here.
    bool Update(PlayerProfileView& view);
    void CancelAndJoin() noexcept;
private:
    struct Outcome { PlayerProfileSnapshot snapshot; std::string error; bool success=false; };
    RunResultsStore store_;
    bool dirty_=true;
    std::future<Outcome> worker_;
    std::shared_ptr<std::atomic<bool>> cancel_;
};
} // namespace locus::app
