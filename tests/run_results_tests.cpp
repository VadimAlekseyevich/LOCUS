#include "app/run_results.h"
#include "app/async_result_history.h"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <thread>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace fs = std::filesystem;
using locus::app::RunResultsStore;
using locus::app::RunResult;
using locus::app::RunKey;

constexpr const char* kEasyId = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
constexpr const char* kHardId = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
constexpr const char* kParallelId = "cccccccccccccccccccccccccccccccc";
constexpr const char* kRevision = "11111111111111111111111111111111";

bool Check(bool ok, const char* message) {
    if (!ok) std::cerr << "RUN_RESULTS_FAIL: " << message << '\n';
    return ok;
}

int main(int argc, char** argv) {
#ifdef _WIN32
    if (argc == 2 && std::string(argv[1]) == "--race-writer") {
        std::wstring parent(32768, L'\0');
        const DWORD length = GetEnvironmentVariableW(
            L"LOCUS_RESULTS_RACE_ROOT", parent.data(),
            static_cast<DWORD>(parent.size()));
        if (length == 0 || length >= parent.size()) return 2;
        parent.resize(length);
        RunResultsStore writer(fs::path(parent) / "Results");
        RunResult run;
        run.id = "1760000000000"; // deliberately identical between processes
        run.songSetId = "parallel-set";
        run.difficultyId = kParallelId;
        run.gameplayRevision = kRevision;
        run.songTitle = "Concurrent song";
        run.chartTitle = "Concurrent chart";
        run.completedAtMs = 1760000000000LL;
        run.grades = {1, 0, 0, 0};
        run.notesJudged = 1;
        run.practiceRate = 1.0;
        std::string failure;
        return writer.Save(run, failure) ? 0 : 3;
    }
#else
    (void)argc;
    (void)argv;
#endif
    const fs::path root = fs::temp_directory_path() /
        ("locus-run-test-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    std::error_code ec;
    fs::create_directories(root, ec);
    if (ec) return 1;
    bool ok = true;
    ok &= Check(RunResult{}.judgementVersion ==
                locus::core::kJudgementRulesVersion,
                "new score records inherit the authoritative judgement revision");
    RunResultsStore store(root / "Results");
    std::string error;
    RunResult first;
    first.songSetId = "set-123";
    first.difficultyId = kEasyId;
    first.gameplayRevision = kRevision;
    first.songTitle = u8"Ночная песня";
    first.chartTitle = u8"Лёгкая";
    first.completedAtMs = 1760000000000LL;
    first.grades = {4, 1, 1, 1};
    first.notesJudged = 7;
    first.practiceRate = 1.0;
    ok &= Check(store.Save(first, error) && error.empty() &&
                !first.id.empty(), "first completed attempt saved");

    RunResult second = first;
    second.id.clear();
    second.completedAtMs += 1000;
    second.grades = {6, 0, 0, 1};
    second.practiceRate = 0.75;
    ok &= Check(store.Save(second, error) && first.id != second.id,
                "another attempt creates a separate file");
    const auto history = store.Load({"set-123", kEasyId}, error);
    ok &= Check(error.empty() && history.size() == 2 &&
                history[0].id == second.id && history[1].id == first.id &&
                history[0].practiceRate == 0.75 &&
                history[1].songTitle == first.songTitle &&
                history[0].grades.perfect == 6,
                "multiple runs sorted newest first with UTF-8 and counts");

    RunResult other = first;
    other.id.clear();
    other.difficultyId = kHardId;
    ok &= Check(store.Save(other, error) &&
                // Equal filenames are legal in different RunKey directories.
                // Identifying the history directory by filename alone would
                // mistake Hard for Easy and break the obstruction regression.
                other.id == first.id &&
                store.Load({"set-123", kHardId}, error).size() == 1 &&
                store.Load({"set-123", kEasyId}, error).size() == 2 &&
                store.Load({"different-set", kEasyId}, error).empty(),
                "records are isolated by set identity and persistent difficulty ID");

    RunResult renamedSong = first;
    renamedSong.id.clear();
    renamedSong.songTitle = "Renamed Set";
    renamedSong.chartTitle = "Renamed Display Title";
    renamedSong.completedAtMs += 3000;
    ok &= Check(store.Save(renamedSong, error) &&
                store.Load({"set-123", kEasyId}, error).size() == 3,
                "display-title edits preserve the chart history identity");

    ok &= Check(locus::app::CompareRunRevision(first, kRevision) ==
                    locus::app::RunRevisionState::Current &&
                locus::app::CompareRunRevision(first, "") ==
                    locus::app::RunRevisionState::Unknown,
                "equal revision and missing reference are distinguished");
    RunResult differentRevision = first;
    differentRevision.id.clear();
    differentRevision.gameplayRevision = "22222222222222222222222222222222";
    differentRevision.completedAtMs += 4000;
    ok &= Check(store.Save(differentRevision, error) &&
                store.Load({"set-123", kEasyId}, error).size() == 4 &&
                locus::app::CompareRunRevision(differentRevision, kRevision) ==
                    locus::app::RunRevisionState::OlderChart,
                "old authored revision remains visible in same difficulty history");
    RunResult oldRules = first;
    oldRules.judgementVersion = 1;
    ok &= Check(locus::app::CompareRunRevision(oldRules, kRevision) ==
                    locus::app::RunRevisionState::OlderRules,
                "old scoring rules are shown separately from old chart content");

    // Deliberate corruption and abandoned .pending writes cannot create
    // phantom attempts nor prevent valid results from loading.
    fs::path easyFolder;
    for (const auto& item : fs::directory_iterator(root / "Results")) {
        if (fs::is_directory(item.path())) {
            bool containsEasy = false;
            for (const auto& file : fs::directory_iterator(item.path())) {
                // Result filenames need only be unique within one
                // difficulty. The first and the separate Hard result use
                // the same timestamp-based ID, so matching first.id here
                // can select the WRONG history directory on Windows.
                // second.id was saved only for the Easy difficulty.
                if (file.path().filename() == (second.id + ".json"))
                    containsEasy = true;
            }
            if (containsEasy) easyFolder = item.path();
        }
    }
    ok &= Check(!easyFolder.empty(), "record files exist on disk");
    if (!easyFolder.empty()) {
        std::ofstream(easyFolder / "corrupt.json") << "{ broken json";
        std::ofstream(easyFolder / "stale.pending") << "unfinished";
        ok &= Check(store.Load({"set-123", kEasyId}, error).size() == 4,
                    "corrupt and pending files are ignored independently");
    }

    // A deliberately reused ID must never replace an earlier completed
    // result, even if an abandoned legacy .pending file occupies its name.
    RunResult collision = first;
    collision.completedAtMs += 5000;
    collision.id = first.id;
    if (!easyFolder.empty())
        std::ofstream(easyFolder / (collision.id + "-0.pending")) << "old";
    ok &= Check(store.Save(collision, error) &&
                collision.id != first.id &&
                store.Load({"set-123", kEasyId}, error).size() == 5,
                "colliding completed attempt is published without overwriting old result");
    RunResult secondCollision = first;
    secondCollision.completedAtMs += 6000;
    secondCollision.id = first.id;
    ok &= Check(store.Save(secondCollision, error) &&
                secondCollision.id != collision.id &&
                store.Load({"set-123", kEasyId}, error).size() == 6,
                "repeated ID gets another unique result despite stale pending files");

#ifdef _WIN32
    // Real independent processes compete for the same record ID. Both
    // results must survive; a preflight exists() check would race here.
    {
        std::wstring executable(32768, L'\0');
        const DWORD length = GetModuleFileNameW(
            nullptr, executable.data(), static_cast<DWORD>(executable.size()));
        const bool validExecutable = length > 0 && length < executable.size();
        ok &= Check(validExecutable, "child executable path available");
        if (validExecutable) {
            executable.resize(length);
            const bool envReady =
                SetEnvironmentVariableW(L"LOCUS_RESULTS_RACE_ROOT",
                                        root.c_str()) != 0;
            ok &= Check(envReady, "child process root configured");
            if (envReady) {
                PROCESS_INFORMATION workers[2]{};
                bool launched[2]{false, false};
                for (int i = 0; i < 2; ++i) {
                    STARTUPINFOW startup{};
                    startup.cb = sizeof(startup);
                    std::wstring command = L"\"" + executable +
                                           L"\" --race-writer";
                    launched[i] = CreateProcessW(
                        nullptr, command.data(), nullptr, nullptr, FALSE,
                        CREATE_NO_WINDOW, nullptr, nullptr, &startup,
                        &workers[i]) != 0;
                }
                SetEnvironmentVariableW(L"LOCUS_RESULTS_RACE_ROOT", nullptr);
                bool passed = launched[0] && launched[1];
                for (int i = 0; i < 2; ++i) {
                    if (!launched[i]) continue;
                    const DWORD waited =
                        WaitForSingleObject(workers[i].hProcess, 30000);
                    DWORD code = 1;
                    if (waited != WAIT_OBJECT_0)
                        TerminateProcess(workers[i].hProcess, 1);
                    if (waited != WAIT_OBJECT_0 ||
                        !GetExitCodeProcess(workers[i].hProcess, &code) ||
                        code != 0)
                        passed = false;
                    CloseHandle(workers[i].hThread);
                    CloseHandle(workers[i].hProcess);
                }
                const auto parallel = store.Load(
                    {"parallel-set", kParallelId}, error);
                ok &= Check(passed && error.empty() &&
                            parallel.size() == 2 &&
                            parallel[0].id != parallel[1].id,
                            "two independent writers preserve both completed attempts");
            }
        }
    }
#endif

    RunResult variable = first;
    variable.id.clear();
    variable.completedAtMs += 7000;
    variable.practiceRate = 1.0;
    variable.variablePracticeRate = true;
    const bool variableSaved = store.Save(variable, error);
    const auto variableHistory = store.Load({"set-123", kEasyId}, error);
    ok &= Check(variableSaved && error.empty() &&
                !variableHistory.empty() &&
                variableHistory.front().variablePracticeRate &&
                variableHistory.front().practiceRate == 1.0,
                "mixed-rate run remains distinguishable after returning to 100%");

    // History loads on a worker and only the latest selected difficulty may
    // publish. Repeated requests must cancel/discard earlier results.
    {
        locus::app::AsyncResultHistory reader(root / "Results");
        std::string status;
        const RunKey easyKey{"set-123", kEasyId};
        const RunKey hardKey{"set-123", kHardId};
        ok &= Check(reader.Request(easyKey, status) && reader.Busy() &&
                    reader.Request(hardKey, status),
                    "background history reader accepts latest selection while busy");
        std::vector<RunResult> loaded;
        auto outcome = locus::app::AsyncResultHistory::PollState::Pending;
        const auto deadline = std::chrono::steady_clock::now() +
            std::chrono::seconds(15);
        while (std::chrono::steady_clock::now() < deadline) {
            outcome = reader.Poll(hardKey, loaded, status);
            if (outcome == locus::app::AsyncResultHistory::PollState::Ready ||
                outcome == locus::app::AsyncResultHistory::PollState::Failed)
                break;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        ok &= Check(outcome == locus::app::AsyncResultHistory::PollState::Ready &&
                    loaded.size() == 1 &&
                    loaded.front().difficultyId == kHardId &&
                    !reader.Busy(),
                    "stale history never leaks into newly selected difficulty");
        std::atomic<bool> stop{true};
        std::string cancelled;
        ok &= Check(store.Load(easyKey, cancelled, &stop).empty() &&
                    cancelled.find("cancelled") != std::string::npos,
                    "cancelled history read does not publish partial attempts");
        reader.CancelAndJoin();
    }

    RunResult invalid = first;
    invalid.id.clear();
    invalid.grades.miss = -1;
    const std::size_t before = store.Load(
        {"set-123", kEasyId}, error).size();
    ok &= Check(!store.Save(invalid, error) &&
                store.Load({"set-123", kEasyId}, error).size() == before,
                "invalid judgement counts are never written");

    ok &= Check(locus::app::ValidRunResult(first) &&
                locus::app::FormatRunTimestamp(first.completedAtMs).find("UTC") !=
                    std::string::npos,
                "accuracy and UTC timestamp preserve completed-run values");
    // A corrupt Results path is a hard failure, not an empty score history.
    // The data is restored intact after the check, even on Windows.
    if (!easyFolder.empty()) {
        const fs::path held = easyFolder.string() + ".temporarily-held";
        fs::rename(easyFolder, held, ec);
        const bool parked = !ec;
        if (parked) {
            std::ofstream(easyFolder, std::ios::binary) << "not a directory";
            std::string failure;
            const auto inaccessible = store.Load({"set-123", kEasyId}, failure);
            // Windows may diagnose a non-directory path either during
            // exists() or during the subsequent directory probe. Both are
            // valid errors; only an empty-success history would be wrong.
            if (failure.empty() || !inaccessible.empty())
                std::cerr << "RUN_RESULTS_STORAGE_DIAGNOSTIC error='"
                          << failure << "' records=" << inaccessible.size()
                          << " is_file=" << fs::is_regular_file(easyFolder)
                          << '\n';
            ok &= Check(inaccessible.empty() && !failure.empty(),
                        "inaccessible results storage is not reported as no scores");
            fs::remove(easyFolder, ec);
            fs::rename(held, easyFolder, ec);
        }
        ok &= Check(parked && !ec && fs::exists(easyFolder),
                    "saved results survive a temporary directory obstruction");
    }
    // These records are result summaries, not playbacks: no timeline events.
    fs::remove_all(root, ec);
    return ok ? 0 : 1;
}
