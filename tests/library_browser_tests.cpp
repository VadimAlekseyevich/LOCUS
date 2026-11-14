#include "app/library_browser.h"
#include "app/library_file_actions.h"
#include "app/library_launch_actions.h"
#include "app/song_set_storage.h"

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
    stream << "{\"formatVersion\":4,\"title\":\"" << title
           << "\",\"audioPath\":\"" << audioName
           << "\",\"bpm\":120,\"offsetMs\":0,\"tempoMap\":[{\"timeMs\":0,\"bpm\":120}],"
              "\"grid\":{\"width\":4,\"height\":4},"
              "\"start\":{\"x\":1,\"y\":1,\"heading\":\"r\"},"
              "\"previewLeadBeats\":4,\"turns\":[]}";
    return static_cast<bool>(stream);
}
bool WriteAudio(const fs::path& file) {
    std::ofstream stream(file, std::ios::binary);
    stream << "RIFF-fixture-audio-bytes";
    return static_cast<bool>(stream);
}

fs::path pickedChart, exportedPackage;
bool cancelImport = false;
int importPickerCalls = 0, exportPickerCalls = 0, clockCalls = 0;
MusicPickResult PickImport() {
    ++importPickerCalls;
    return cancelImport ? MusicPickResult{{}, {}, true}
                        : MusicPickResult{pickedChart.wstring(), {}, false};
}
MusicPickResult PickExport(const std::wstring& suggestion) {
    ++exportPickerCalls;
    return suggestion.empty() ? MusicPickResult{{}, "EMPTY SUGGESTION", false}
        : MusicPickResult{exportedPackage.wstring(), {}, false};
}
double FakeClock() {
    ++clockCalls;
    return 44.5;
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
    // Native results UI observes a single revision rather than hashing
    // an unbounded number of run records every frame.
    const RunKey firstRunKey{"first-set", "easy.locus.json"};
    const RunKey secondRunKey{"other-set", "hard.locus.json"};
    ui.resultScroll = 4;
    ui.ReplaceResults(firstRunKey, {});
    ok &= Check(ui.resultsRevision == 1 && ui.resultKey == firstRunKey &&
                ui.resultScroll == 0 && ui.results.empty(),
                "results replacement advances revision and resets history scroll");
    ui.ReplaceResults(secondRunKey, {});
    ok &= Check(ui.resultsRevision == 2 && ui.resultKey == secondRunKey,
                "same list content for another chart still invalidates results UI");
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
    const auto expanded = LibraryTreeRows(library, ui);
    ok &= Check(expanded.size() == 4 &&
                expanded[0].song == 0 && expanded[0].chart == -1 &&
                expanded[1].song == 1 && expanded[1].chart == -1 &&
                expanded[2].song == 1 && expanded[2].chart == 0 &&
                expanded[3].song == 1 && expanded[3].chart == 1,
                "Song Library displays charts indented directly beneath selected song");
    // The native PLAY browser now filters song and difficulty titles. A
    // difficulty-name hit must not expose unrelated sibling difficulties.
    ui.search = "Hard";
    const auto hardFiltered = LibraryTreeRows(library, ui);
    ok &= Check(hardFiltered.size() == 2 &&
                hardFiltered[0].song == 1 && hardFiltered[0].chart == -1 &&
                hardFiltered[1].song == 1 && hardFiltered[1].chart == 1,
                "search isolates matching difficulty with its parent Song Set");
    ui.search = "ZZ-SONG";
    const auto songFiltered = LibraryTreeRows(library, ui);
    ok &= Check(songFiltered.size() == 3 &&
                songFiltered[0].song == 1 &&
                songFiltered[1].chart == 0 && songFiltered[2].chart == 1,
                "song-name search displays all difficulties case-insensitively");
    ui.search = "no-such-song";
    ok &= Check(LibraryTreeRows(library, ui).empty(),
                "unmatched song query yields empty scroll tree");
    ui.search.clear();
    ui.treeScroll = 0;

    // Drawing and mouse input use identical 960x540 hitboxes. The expanded
    // right tree cannot overlap the Play/Edit buttons in the left column.
    const Rectangle treeFirst = LibrarySongRow(0);
    const Rectangle treeLast = LibraryChartRow(kLibraryTreeVisibleRows - 1);
    const Rectangle playButton = LibraryPlayButton();
    const Rectangle editButton = LibraryEditButton();
    ok &= Check(kLibraryTreeVisibleRows == 8 &&
                treeFirst.x >= 382.0f && treeLast.x >= 382.0f &&
                treeLast.y + treeLast.height <= 470.0f &&
                playButton.x + playButton.width < treeFirst.x &&
                editButton.x + editButton.width < treeFirst.x,
                "native PLAY layout has eight scroll rows and a separate action column");

    ui.MoveTree(-1, library);
    ok &= Check(ui.selectedSong == 1 && ui.selectedChart == 0 &&
                ui.treeChartFocused,
                "tree keyboard selection moves between child difficulties");
    ui.MoveTree(-1, library);
    ok &= Check(ui.selectedSong == 1 && !ui.treeChartFocused,
                "tree keyboard selection returns from chart to parent song");
    ui.SelectChart(1, library);
    // Pure selection policy: no raylib window, file picker or audio device.
    {
        MenuEvents action{};
        action.playSelected = true;
        const auto play = ChooseLibraryLaunch(action, library, ui);
        ok &= Check(play.kind == LibraryLaunchKind::PlayChart &&
                    play.chart == ui.Selected(library)->file,
                    "play intent captures the selected chart path");
        action = {};
        action.editSelected = true;
        const auto edit = ChooseLibraryLaunch(action, library, ui);
        ok &= Check(edit.kind == LibraryLaunchKind::EditChart &&
                    edit.chart == play.chart,
                    "edit intent identifies the same difficulty");
        action = {};
        action.newDifficulty = true;
        const auto create = ChooseLibraryLaunch(action, library, ui);
        ok &= Check(create.kind == LibraryLaunchKind::NewDifficulty &&
                    create.managed &&
                    !create.audio.empty() && !create.folder.empty(),
                    "new difficulty captures managed Set identity and audio");
        ui.SelectSong(0, library);
        action = {};
        action.playSelected = true;
        ok &= Check(ChooseLibraryLaunch(action, library, ui).kind ==
                        LibraryLaunchKind::PlayDemo,
                    "demo playback does not depend on selected chart");
        action = {};
        action.editSelected = true;
        ok &= Check(ChooseLibraryLaunch(action, library, ui).kind ==
                        LibraryLaunchKind::EditDraft,
                    "demo editor opens the existing draft");
        ui.SelectSong(1, library);
        ui.SelectChart(1, library);
    }
    const fs::path selectedFile = ui.Selected(library)->file;

    ok &= Check(RefreshLibraryBrowser(library, ui, menuStatus) &&
                ui.selectedSong == 1 && ui.selectedChart == 1 &&
                ui.Selected(library) && ui.Selected(library)->file == selectedFile,
                "refresh keeps the selected difficulty, not chart zero");

    // Add a new earlier-sorting song directly to Songs without changing the
    // cached index; refresh must preserve selection by file after reordering.
    const fs::path earlier = installed / "other-album";
    fs::create_directories(earlier, ec);
    SongSetManifest earlierSet;
    earlierSet.id = "external-bonus-set";
    earlierSet.title = "aa-song";
    earlierSet.audioFile = "aa-song.wav";
    earlierSet.bpm = 120.0;
    earlierSet.offsetMs = 0;
    earlierSet.tempoMap = {{0, 120.0}};
    std::string manifestError;
    ok &= Check(!ec && WriteAudio(earlier / "aa-song.wav") &&
                WriteChart(earlier / "bonus.locus.json", "Bonus", "aa-song.wav") &&
                WriteNewSongSetManifest(earlier / "set.locus.json",
                                        earlierSet, manifestError),
                "add valid managed Song Set on disk externally");
    ok &= Check(RefreshLibraryBrowser(library, ui, menuStatus) &&
                library.Songs().size() == 2 && ui.selectedSong == 2 &&
                ui.Selected(library) && ui.Selected(library)->file == selectedFile &&
                ui.selectedChart == 1,
                "refresh preserves identity when a new song reorders the list");


    // File workflows can now be exercised without the game's main loop,
    // a real file-picker window or a real clock/audio device.
    pickedChart = easy;
    exportedPackage = root / "exported-song.locuspack";
    std::string fileError;
    LibraryFileActions commands{
        library, ui, menuStatus, fileError,
        {PickImport, PickExport, FakeClock}
    };
    MenuEvents request;
    double eventTime = 1.0;
    cancelImport = true;
    request.import = true;
    const int beforeCancel = library.ChartCount();
    commands.Apply(request, false, eventTime);
    ok &= Check(importPickerCalls == 1 && clockCalls == 1 &&
                eventTime == 44.5 && beforeCancel == library.ChartCount() &&
                ui.Selected(library) && ui.Selected(library)->file == selectedFile,
                "cancelled import stays in library UI and refreshes time");
    cancelImport = false;
    commands.Apply(request, false, eventTime);
    ok &= Check(importPickerCalls == 2 && library.ChartCount() == 3 &&
                ui.notice.find("IMPORTED") != std::string::npos,
                "injected import picker updates library without main.cpp");

    request = MenuEvents{};
    request.exportSelected = true;
    commands.Apply(request, false, eventTime);
    ok &= Check(exportPickerCalls == 1 && fs::exists(exportedPackage) &&
                ui.notice == "SONG SET EXPORTED",
                "controller exports selected song with native dialog injected");

    // Explicit actions reuse the existing recoverable Trash semantics.
    ui.SelectSong(2, library);
    ui.SelectChart(1, library);
    request = MenuEvents{};
    request.trashDifficulty = true;
    commands.Apply(request, false, eventTime);
    ok &= Check(library.ChartCount() == 2 &&
                !fs::exists(selectedFile),
                "file command moves only selected difficulty to Trash");
    request = MenuEvents{};
    request.restoreDeleted = true;
    commands.Apply(request, false, eventTime);
    ok &= Check(library.ChartCount() == 3 &&
                fs::exists(selectedFile),
                "file command can restore a trashed difficulty after rescan");
    ui.SelectSong(2, library);
    ui.SelectChart(1, library);

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
