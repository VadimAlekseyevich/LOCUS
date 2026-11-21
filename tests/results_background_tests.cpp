#include "app/results_background.h"

#include <filesystem>
#include <iostream>
#include <vector>

int main() {
    namespace fs = std::filesystem;
    using namespace locus::app;
    std::vector<LibrarySong> songs(2);
    songs[0].id = "set-first";
    songs[0].backgroundImage = fs::path("first") / "cover.png";
    songs[1].id = "set-second";
    songs[1].backgroundImage = fs::path("second") / "background.jpg";

    RunResult saved;
    saved.songSetId = "set-second";
    saved.difficultyFile = "Hard.json";
    if (BackgroundForSavedResult(songs, saved) != songs[1].backgroundImage) {
        std::cerr << "HISTORICAL_BACKGROUND_FAIL: last played song must not win\n";
        return 1;
    }
    saved.difficultyFile = "deleted-difficulty.json";
    if (BackgroundForSavedResult(songs, saved) != songs[1].backgroundImage) {
        std::cerr << "HISTORICAL_BACKGROUND_FAIL: art is Song Set scoped\n";
        return 1;
    }
    songs[1].backgroundImage = fs::path("second") / "new-cover.png";
    if (BackgroundForSavedResult(songs, saved) != songs[1].backgroundImage) {
        std::cerr << "HISTORICAL_BACKGROUND_FAIL: cover changes must apply\n";
        return 1;
    }
    saved.songSetId = "removed-song";
    if (!BackgroundForSavedResult(songs, saved).empty()) {
        std::cerr << "HISTORICAL_BACKGROUND_FAIL: unknown song must be blank\n";
        return 1;
    }
    saved.songSetId.clear();
    if (!BackgroundForSavedResult(songs, saved).empty()) {
        std::cerr << "HISTORICAL_BACKGROUND_FAIL: empty identity must be blank\n";
        return 1;
    }
    std::cout << "HISTORICAL_BACKGROUND_PASS\n";
    return 0;
}
