#include "app/run_results.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using locus::app::RunResultsStore;
using locus::app::RunResult;
using locus::app::RunKey;

bool Check(bool ok, const char* message) {
    if (!ok) std::cerr << "RUN_RESULTS_FAIL: " << message << '\n';
    return ok;
}

int main() {
    const fs::path root = fs::temp_directory_path() /
        ("locus-run-test-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    std::error_code ec;
    fs::create_directories(root, ec);
    if (ec) return 1;
    bool ok = true;
    RunResultsStore store(root / "Results");
    std::string error;
    RunResult first;
    first.songSetId = "set-123";
    first.difficultyFile = "easy.locus.json";
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
    const auto history = store.Load({"set-123", "easy.locus.json"}, error);
    ok &= Check(error.empty() && history.size() == 2 &&
                history[0].id == second.id && history[1].id == first.id &&
                history[0].practiceRate == 0.75 &&
                history[1].songTitle == first.songTitle &&
                history[0].grades.perfect == 6,
                "multiple runs sorted newest first with UTF-8 and counts");

    RunResult other = first;
    other.id.clear();
    other.difficultyFile = "hard.locus.json";
    ok &= Check(store.Save(other, error) &&
                store.Load({"set-123", "hard.locus.json"}, error).size() == 1 &&
                store.Load({"set-123", "easy.locus.json"}, error).size() == 2 &&
                store.Load({"different-set", "easy.locus.json"}, error).empty(),
                "records are isolated by set identity and difficulty filename");

    RunResult renamedSong = first;
    renamedSong.id.clear();
    renamedSong.songTitle = "Renamed Set";
    renamedSong.chartTitle = "Renamed Display Title";
    renamedSong.completedAtMs += 3000;
    ok &= Check(store.Save(renamedSong, error) &&
                store.Load({"set-123", "easy.locus.json"}, error).size() == 3,
                "display-title edits preserve the chart history identity");

    // Deliberate corruption and abandoned .pending writes cannot create
    // phantom attempts nor prevent valid results from loading.
    fs::path easyFolder;
    for (const auto& item : fs::directory_iterator(root / "Results")) {
        if (fs::is_directory(item.path())) {
            bool containsEasy = false;
            for (const auto& file : fs::directory_iterator(item.path())) {
                if (file.path().filename() == (first.id + ".json"))
                    containsEasy = true;
            }
            if (containsEasy) easyFolder = item.path();
        }
    }
    ok &= Check(!easyFolder.empty(), "record files exist on disk");
    if (!easyFolder.empty()) {
        std::ofstream(easyFolder / "corrupt.json") << "{ broken json";
        std::ofstream(easyFolder / "stale.pending") << "unfinished";
        ok &= Check(store.Load({"set-123", "easy.locus.json"}, error).size() == 3,
                    "corrupt and pending files are ignored independently");
    }

    RunResult invalid = first;
    invalid.id.clear();
    invalid.grades.miss = -1;
    const std::size_t before = store.Load(
        {"set-123", "easy.locus.json"}, error).size();
    ok &= Check(!store.Save(invalid, error) &&
                store.Load({"set-123", "easy.locus.json"}, error).size() == before,
                "invalid judgement counts are never written");

    ok &= Check(locus::app::ValidRunResult(first) &&
                locus::app::FormatRunTimestamp(first.completedAtMs).find("UTC") !=
                    std::string::npos,
                "accuracy and UTC timestamp preserve completed-run values");
    // These records are result summaries, not playbacks: no timeline events.
    fs::remove_all(root, ec);
    return ok ? 0 : 1;
}
