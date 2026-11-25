#include "app/library_browser.h"
#include "app/async_library_refresh.h"
#include "app/library_file_actions.h"
#include "app/library_virtual_list.h"
#include "app/library_launch_actions.h"
#include "app/song_set_storage.h"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>

using namespace locus::app;

#ifdef LOCUS_TEST_NATIVE_PLAY
bool TestNativePlayPresenter(const SongLibrary& library);
bool TestNativeResultsVirtualHistory();
#endif

namespace {
namespace fs = std::filesystem;

bool Check(bool ok, const char* label) {
    if (!ok) std::cerr << "LIBRARY_BROWSER_FAIL " << label << '\n';
    return ok;
}

bool WriteChart(const fs::path& file, const char* title,
                const char* audioName, const char* difficultyId = nullptr) {
    std::ofstream stream(file, std::ios::binary);
    stream << "{\"formatVersion\":4,\"title\":\"" << title << "\"";
    if (difficultyId) stream << ",\"difficultyId\":\"" << difficultyId << "\"";
    stream << ",\"audioPath\":\"" << audioName
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
    const RunKey firstRunKey{"first-set", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"};
    const RunKey secondRunKey{"other-set", "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"};
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
                ui.Selected(library) && ui.Selected(library)->data->title == "Easy",
                "first import chooses installed difficulty");
    ok &= Check(ImportLibraryChart(library, ui, hard, menuStatus) &&
                library.ChartCount() == 2 && ui.selectedSong == 1 &&
                ui.selectedChart == 1 && ui.Selected(library) &&
                ui.Selected(library)->data->title == "Hard",
                "second import selects a different difficulty in same song");
#ifdef LOCUS_TEST_NATIVE_PLAY
    ok &= TestNativePlayPresenter(library);
    ok &= TestNativeResultsVirtualHistory();
#endif
    // A selected difficulty keeps its permanent ID when its disk file is
    // renamed outside the application. Refresh must not fall back to the
    // difficulty at the old numeric index after resorting.
    {
        const auto* selected = ui.Selected(library);
        const fs::path before = selected ? selected->file : fs::path{};
        const std::string id = selected ? selected->data->difficultyId : "";
        const fs::path renamed = before.parent_path() / "zz-renamed.locus.json";
        fs::rename(before, renamed, ec);
        const bool moved = !ec;
        const bool followed = moved &&
            RefreshLibraryBrowser(library, ui, menuStatus) &&
            ui.Selected(library) && ui.Selected(library)->file == renamed &&
            ui.Selected(library)->data->difficultyId == id;
        ok &= Check(followed,
                    "external filename rename retains selected difficulty by ID");
        if (moved) {
            fs::rename(renamed, before, ec);
            ok &= Check(!ec && RefreshLibraryBrowser(library, ui, menuStatus) &&
                        ui.Selected(library) && ui.Selected(library)->file == before,
                        "renaming back preserves the same logical selected chart");
        }
    }
    // An isolated worker may finish after an editor/import has already
    // published a newer catalog generation. The older scan must be retried,
    // never published over the authoritative in-memory result.
    {
        AsyncLibraryRefresh worker;
        std::string scanStatus;
        const auto initialGeneration = library.Revision();
        const auto retainedGameplay = library.Songs()[0].charts[0].data;
        ok &= Check(worker.Request(library, scanStatus) && worker.Busy(),
                    "background catalog scan starts without blocking the caller");
        ok &= Check(library.Refresh(scanStatus) &&
                    library.Revision() > initialGeneration,
                    "foreground edits can advance the index during a scan");
        ok &= Check(worker.Request(library, scanStatus),
                    "rapid second F5 coalesces with the active worker scan");
        const auto stableIndexRevision = library.Revision();
        const auto priorDiskEpoch = library.DiskMutationEpoch();
        library.MarkDiskMutation();
        ok &= Check(library.Revision() == stableIndexRevision &&
                    library.DiskMutationEpoch() == priorDiskEpoch + 1,
                    "disk commit invalidates worker independently of UI revision");
        auto result = AsyncLibraryRefresh::PollState::Pending;
        const auto deadline = std::chrono::steady_clock::now() +
            std::chrono::seconds(15);
        while (std::chrono::steady_clock::now() < deadline) {
            result = worker.Poll(library, scanStatus);
            if (result == AsyncLibraryRefresh::PollState::Published ||
                result == AsyncLibraryRefresh::PollState::Failed) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        ok &= Check(result == AsyncLibraryRefresh::PollState::Published &&
                    library.ChartCount() == 2 && !worker.Busy(),
                    "stale worker index is discarded and latest snapshot published");
        // The old *published* index was swapped out, not destroyed. A new
        // background worker can reuse all unchanged chart content, avoiding
        // a fresh JSON parse for every F5 on an established library.
        ok &= Check(worker.Request(library, scanStatus),
                    "second background scan starts with retained cache");
        result = AsyncLibraryRefresh::PollState::Pending;
        const auto warmDeadline = std::chrono::steady_clock::now() +
            std::chrono::seconds(15);
        while (std::chrono::steady_clock::now() < warmDeadline) {
            result = worker.Poll(library, scanStatus);
            if (result == AsyncLibraryRefresh::PollState::Published ||
                result == AsyncLibraryRefresh::PollState::Failed) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        ok &= Check(result == AsyncLibraryRefresh::PollState::Published &&
                    library.LastRefreshStats().reusedCharts == 2 &&
                    library.LastRefreshStats().calculatedDifficulties == 0 &&
                    library.LastRefreshStats().parsedCharts == 0 &&
                    library.Songs()[0].charts[0].data == retainedGameplay,
                    "background rescan shares immutable note payload across generations");
        worker.CancelAndJoin();
        ok &= Check(!worker.Busy(), "shutdown drains and invalidates scanner future");
        // Cancellation never discards or replaces the last published index.
        const auto publishedRevision = library.Revision();
        const auto publishedCount = library.ChartCount();
        std::atomic<bool> alreadyCancelled{true};
        ok &= Check(!library.Refresh(scanStatus, &alreadyCancelled) &&
                    scanStatus.find("cancelled") != std::string::npos &&
                    library.Revision() == publishedRevision &&
                    library.ChartCount() == publishedCount,
                    "cancelled scan preserves every published library entry");
        SongLibrary foreign(installed / "foreign-root");
        ok &= Check(!library.PublishScannedIndex(std::move(foreign)) &&
                    library.Revision() == publishedRevision &&
                    library.ChartCount() == publishedCount,
                    "worker cannot publish scan from another library root");
    }
    // The same bounded-window contract applies to long result histories.
    // The layout row step is supplied by measured RmlUi geometry.
    {
        constexpr std::size_t total = 25000;
        constexpr float step = 54.0f;
        const auto compact = VirtualRowsForViewport(200.0f,step);
        const auto tall = VirtualRowsForViewport(3200.0f,step);
        ok &= Check(compact < 32 && tall > 32 &&
                    (tall-10)*step >= 3200.0f,
                    "virtual working set grows with viewport rather than a fixed cap");
        const auto tallView = VisibleResultRows(total,13000.0f*step,step,tall);
        ok &= Check(tallView.first <= 13000 &&
                    tallView.end*step >= 13000.0f*step+3200.0f,
                    "large viewport fully covered with overscan");
        const auto first = VisibleResultRows(total, 0.0f, step);
        const auto middle = VisibleResultRows(total, 13000.0f * step, step);
        const auto last = VisibleResultRows(total, total * step, step);
        for (const float density : {1.0f, 1.25f, 1.5f, 2.0f, 2.5f}) {
            const float pixels = LibraryScrollPixels(13000.0f * step,density);
            const float measured = LibraryResultRowStepDp((step-5.0f)*density,density);
            const auto scaled = VisibleResultRows(total,LibraryScrollDp(pixels,density),measured);
            ok &= Check(scaled.first == middle.first && scaled.end == middle.end &&
                        std::abs(scaled.before-middle.before) < 0.1f &&
                        std::abs(scaled.after-middle.after) < 0.1f,
                        "virtual result window and dp spacers invariant across DPI");
        }
        const auto height = [&](const LibraryRowWindow& view) {
            const auto elements = (view.end - view.first) +
                (view.first > 0 ? 1 : 0) + (view.end < total ? 1 : 0);
            const float rowHeight = step - 5.0f;
            return (view.end - view.first) * rowHeight +
                view.before + view.after + (elements - 1) * 5.0f;
        };
        ok &= Check(first.first == 0 && first.end <= 32 &&
                    middle.first <= 13000 && middle.end > 13000 &&
                    middle.end - middle.first <= 32 &&
                    last.end == total && last.end - last.first <= 32 &&
                    height(first) == total * step - 5.0f &&
                    height(middle) == total * step - 5.0f &&
                    height(last) == total * step - 5.0f,
                    "25k saved results keep bounded buttons and invariant scroll height");
    }
    // Virtual RML list draws a bounded number of actual buttons even for
    // thousands of rows. Prefix heights preserve 45dp Song and 42dp chart
    // layout, plus the existing 6dp RmlUi gap.
    {
        std::vector<LibraryTreeEntry> synthetic(10000);
        for (std::size_t i = 0; i < synthetic.size(); ++i)
            synthetic[i] = {static_cast<int>(i), i % 3 ? -1 : 0};
        const auto offsets = LibraryRowOffsets(synthetic);
        const auto first = VisibleLibraryRows(offsets, 0.0f);
        const auto middle = VisibleLibraryRows(offsets, offsets[5000]);
        const auto last = VisibleLibraryRows(offsets, offsets.back());
        for (const float density : {1.0f, 1.25f, 1.5f, 2.0f, 2.5f}) {
            const float dp = offsets[5000] + 1.0f;
            const auto scaled = VisibleLibraryRows(offsets,
                LibraryScrollDp(LibraryScrollPixels(dp,density),density));
            ok &= Check(scaled.first == middle.first && scaled.end == middle.end &&
                        scaled.before == middle.before && scaled.after == middle.after,
                        "virtual library window and dp spacers invariant across DPI");
        }
        ok &= Check(offsets.size() == synthetic.size() + 1 &&
                    first.first == 0 && first.end <= 40 &&
                    middle.first <= 5000 && middle.end > 5000 &&
                    middle.end - middle.first <= 40 &&
                    middle.before > 0 && middle.after > 0 &&
                    last.first < synthetic.size() &&
                    last.end == synthetic.size(),
                    "RML browser bounds live rows independent of collection size");
        const auto height = [&](const LibraryRowWindow& view) {
            float rows = 0;
            for (std::size_t i = view.first; i < view.end; ++i)
                rows += (synthetic[i].chart >= 0 ? 42.0f : 45.0f);
            const float virtualSpaces =
                (view.first ? view.before : 0.0f) +
                (view.end < synthetic.size() ? view.after : 0.0f);
            const auto elements = (view.end - view.first) +
                (view.first ? 1 : 0) +
                (view.end < synthetic.size() ? 1 : 0);
            return rows + virtualSpaces + (elements - 1) * 6.0f;
        };
        const float expected = offsets.back() - 6.0f;
        ok &= Check(height(first) == expected &&
                    height(middle) == expected &&
                    height(last) == expected,
                    "virtual spacers preserve scroll height across window swaps");
    }
    const auto expanded = LibraryTreeRows(library, ui);
    ok &= Check(expanded.size() == 4 &&
                expanded[0].song == 0 && expanded[0].chart == -1 &&
                expanded[1].song == 1 && expanded[1].chart == -1 &&
                expanded[2].song == 1 && expanded[2].chart == 0 &&
                expanded[3].song == 1 && expanded[3].chart == 1,
                "Song Library displays charts indented directly beneath selected song");
    std::string unicodeError;
    // Search keys are normalized with full Unicode casefold and canonical
    // composition, not a hand-written ASCII-only lowercase conversion.
    ok &= Check(library.SetSongMetadata(
                    0, u8"Ёлка", u8"Café", u8"Straße", {}, unicodeError),
                "fixture can update searchable UTF-8 Song Set metadata");
    ui.search = u8"ёлка";
    ok &= Check(LibraryTreeRows(library, ui).size() == 3,
                "Cyrillic Ё/ё search matches both cases and displays children");
    ui.search = u8"Cafe\u0301"; // decomposed e + combining acute.
    ok &= Check(LibraryTreeRows(library, ui).size() == 3,
                "Unicode normalization matches decomposed and composed accents");
    ui.search = "STRASSE";
    ok &= Check(LibraryTreeRows(library, ui).size() == 3,
                "Unicode full casefold maps German sharp-S to ss");
    ok &= Check(library.SetSongMetadata(
                    0, "zz-song", "", "", {}, unicodeError),
                "search fixture restores original Song Set metadata");
    ui.search = u8"ёлка";
    ok &= Check(LibraryTreeRows(library, ui).empty(),
                "search cache invalidates when Song Set metadata revision changes");
    ui.search.clear();

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
                WriteChart(earlier / "bonus.locus.json", "Bonus", "aa-song.wav",
                            "dddddddddddddddddddddddddddddddd") &&
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
                ui.Selected(library)->data->title == "Easy",
                "deleted selected difficulty falls back to a valid row");

    fs::remove_all(root, ec);
    if (!ok) return 1;
    std::cout << "LIBRARY_BROWSER_PASS import=1 refresh=1 identity=1 errors=1\n";
    return 0;
}
