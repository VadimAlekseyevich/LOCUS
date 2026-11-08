#include "app/library_browser.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

using namespace locus::app;

namespace {
namespace fs = std::filesystem;

bool Check(bool ok, const char* label) {
    if (!ok) std::cerr << "LIBRARY_BROWSER_FAIL " << label << '\n';
    return ok;
}

bool WriteChart(const fs::path& file, const char* title,
                const char* audioName) {
    std::ofstream stream(file, std::ios::binary);
    stream << "{\"formatVersion\":1,\"title\":\"" << title
           << "\",\"audioPath\":\"" << audioName
           << "\",\"bpm\":120,\"offsetMs\":0,"
              "\"grid\":{\"width\":4,\"height\":4},"
              "\"start\":{\"x\":1,\"y\":1,\"heading\":\"right\"},"
              "\"previewLeadBeats\":4,\"turns\":[]}";
    return static_cast<bool>(stream);
}
bool WriteAudio(const fs::path& file) {
    std::ofstream stream(file, std::ios::binary);
    stream << "RIFF-fixture-audio-bytes";
    return static_cast<bool>(stream);
}
} // namespace

int main() {
    namespace fs = std::filesystem;
    bool ok = true;
    const auto unique = std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count());
    const fs::path root = fs::temp_directory_path() /
                          ("locus-library-browser-" + unique);
    const fs::path external = root / "external";
    const fs::path installed = root / "Songs";
    std::error_code ec;
    fs::create_directories(external, ec);
    if (ec) return 1;
    const fs::path music = external / "zz-song.wav";
    const fs::path easy = external / "easy.locus.json";
    const fs::path hard = external / "hard.locus.json";
    ok &= Check(WriteAudio(music) &&
                WriteChart(easy, "Easy", "zz-song.wav") &&
                WriteChart(hard, "Hard", "zz-song.wav"),
                "test fixtures are created");

    SongLibrary library(installed);
    SongSelectUi ui;
    std::string menuStatus = "PREVIOUS MENU STATUS";
    ok &= Check(RefreshLibraryBrowser(library, ui, menuStatus) &&
                library.Songs().empty() && ui.selectedSong == 0,
                "empty refresh preserves the demo row");

    ok &= Check(ImportLibraryChart(library, ui, easy, menuStatus) &&
                library.Songs().size() == 1 && library.ChartCount() == 1 &&
                ui.selectedSong == 1 && ui.selectedChart == 0 &&
                ui.Selected(library) && ui.Selected(library)->data.title == "Easy",
                "first import chooses installed difficulty");
    ok &= Check(ImportLibraryChart(library, ui, hard, menuStatus) &&
                library.ChartCount() == 2 && ui.selectedSong == 1 &&
                ui.selectedChart == 1 && ui.Selected(library) &&
                ui.Selected(library)->data.title == "Hard",
                "second import selects a different difficulty in same song");
    const fs::path selectedFile = ui.Selected(library)->file;

    ok &= Check(RefreshLibraryBrowser(library, ui, menuStatus) &&
                ui.selectedSong == 1 && ui.selectedChart == 1 &&
                ui.Selected(library) && ui.Selected(library)->file == selectedFile,
                "refresh keeps the selected difficulty, not chart zero");

    // Add a new earlier-sorting song directly to Songs without changing the
    // cached index; refresh must preserve selection by file after reordering.
    const fs::path earlier = installed / "other-album";
    fs::create_directories(earlier, ec);
    ok &= Check(!ec && WriteAudio(earlier / "aa-song.wav") &&
                WriteChart(earlier / "bonus.locus.json", "Bonus", "aa-song.wav"),
                "add on-disk song externally");
    ok &= Check(RefreshLibraryBrowser(library, ui, menuStatus) &&
                library.Songs().size() == 2 && ui.selectedSong == 2 &&
                ui.Selected(library) && ui.Selected(library)->file == selectedFile &&
                ui.selectedChart == 1,
                "refresh preserves identity when a new song reorders the list");

    const std::string oldMenu = menuStatus;
    const int oldSong = ui.selectedSong, oldChart = ui.selectedChart;
    const fs::path absent = external / "does-not-exist.locus.json";
    ok &= Check(!ImportLibraryChart(library, ui, absent, menuStatus) &&
                ui.notice.find("IMPORT FAILED: ") == 0 &&
                menuStatus == oldMenu && ui.selectedSong == oldSong &&
                ui.selectedChart == oldChart &&
                ui.Selected(library) && ui.Selected(library)->file == selectedFile,
                "failed import keeps selection and successful library status");

    // If a difficulty was removed, clamp to an existing chart rather than
    // pointing at a stale entry after Refresh replaces the cached index.
    fs::remove(selectedFile, ec);
    ok &= Check(!ec && RefreshLibraryBrowser(library, ui, menuStatus) &&
                library.ChartCount() == 2 && ui.selectedSong == 2 &&
                ui.selectedChart == 0 && ui.Selected(library) &&
                ui.Selected(library)->data.title == "Easy",
                "deleted selected difficulty falls back to a valid row");

    fs::remove_all(root, ec);
    if (!ok) return 1;
    std::cout << "LIBRARY_BROWSER_PASS import=1 refresh=1 identity=1 errors=1\n";
    return 0;
}
