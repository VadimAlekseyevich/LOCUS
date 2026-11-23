#pragma once

#include "app/run_results.h"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <future>
#include <memory>
#include <string>
#include <vector>

namespace locus::app {

// One isolated history read at a time; the newest selected RunKey always
// wins. No disk I/O or sorting on the UI frame thread. New completed run
// writes remain handled by RunResultsStore's atomic publication contract.
class AsyncResultHistory {
public:
    enum class PollState { Idle, Pending, Ready, Failed };

    explicit AsyncResultHistory(std::filesystem::path root)
        : root_(std::move(root)) {}
    ~AsyncResultHistory() { CancelAndJoin(); }
    AsyncResultHistory(const AsyncResultHistory&) = delete;
    AsyncResultHistory& operator=(const AsyncResultHistory&) = delete;

    bool Request(RunKey key, std::string& status);
    PollState Poll(const RunKey& selected, std::vector<RunResult>& records,
                   std::string& status);
    bool Busy() const noexcept { return worker_.valid(); }
    void CancelAndJoin() noexcept;

private:
    struct Outcome {
        RunKey key;
        std::vector<RunResult> records;
        std::string status;
        std::uint64_t ticket = 0;
        bool success = false;
    };
    bool Start(std::string& status);

    const std::filesystem::path root_;
    RunKey latest_;
    std::future<Outcome> worker_;
    std::shared_ptr<std::atomic<bool>> cancellation_;
    std::uint64_t requested_ = 0;
};

} // namespace locus::app
