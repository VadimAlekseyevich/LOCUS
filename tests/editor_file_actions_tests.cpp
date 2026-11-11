#include "app/editor_file_actions.h"
#include "app/chart_filename.h"
#include "app/editor_set_controller.h"
#include "app/chart_editor_ui.h"
#include "app/chart_playback.h"
#include "app/editor_document.h"
#include "app/local_music.h"
#include "app/menu_ui.h"
#include "app/song_library.h"
#include "app/song_set_storage.h"
#include "core/chart_editor.h"

#include <filesystem>
#include <fstream>
#include <chrono>
#include <iostream>
#include <string>

using namespace locus;

namespace {
int openCalls = 0, audioCalls = 0, saveCalls = 0;
double mockedNow() { return 42.75; }
app::MusicPickResult CanceledChart() { ++openCalls; return {{}, {}, true}; }
app::MusicPickResult MissingChart() {
    ++openCalls; return {L"missing-chart-for-editor-actions.locus.json", {}, false};
}
app::MusicPickResult MissingAudio() {
    ++audioCalls; return {L"missing-audio-for-editor-actions.wav", {}, false};
}
app::MusicPickResult CanceledSave(const std::wstring&) {
    ++saveCalls; return {{}, {}, true};
}
bool Check(bool ok, const char* description) {
    if (!ok) std::cerr << "EDITOR_FILE_ACTIONS_FAIL " << description << '\n';
    return ok;
}
}

int main() {
    bool ok = true;
    const auto namingFolder = std::filesystem::temp_directory_path() /
        ("locus-name-plan-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(namingFolder);
    const auto originalFile = namingFolder / L"Original.locus.json";
    { std::ofstream(originalFile) << "original"; }
    { std::ofstream(namingFolder / L"Changed.locus.json") << "other"; }
    { std::ofstream(namingFolder / L"Changed-2.locus.json") << "other2"; }
    std::error_code filenameError;
    ok &= Check(app::RenamedChartFilePath(
                    originalFile, "Changed", filenameError) ==
                    namingFolder / L"Changed-3.locus.json" &&
                !filenameError &&
                app::RenamedChartFilePath(
                    originalFile, u8"Сложность Ё", filenameError).filename() ==
                    std::filesystem::u8path(u8"Сложность Ё.locus.json") &&
                app::RenamedChartFilePath(
                    originalFile, "Original", filenameError) == originalFile,
                "rename creates a collision-free UTF-8 title filename");
    std::filesystem::remove_all(namingFolder, filenameError);

    core::ChartEditor editor;
    editor.SetTitle("UNSAVED CHART");
    app::EditorUiState ui;
    ui.tab = app::EditorTab::Audio;
    app::LocalMusic song;
    app::ChartPlayback playback(song);
    std::wstring chartPath = L"original.locus.json";
    std::wstring audioPath = L"original.wav";
    std::wstring loadedAudioPath = L"previous-song.wav";
    int previewBeat = 6;
    std::string status = "WORKING DRAFT";
    app::SongLibrary library(
        std::filesystem::temp_directory_path() / "locus-editor-file-actions-test-uninstalled");
    app::SongSelectUi libraryUi;
    libraryUi.notice = "OLD LIBRARY NOTICE";
    std::string menuStatus = "OLD LIBRARY STATUS";
    const app::EditorFileContext context{
        editor, ui, song, playback, chartPath, audioPath, loadedAudioPath,
        previewBeat, status, library, libraryUi, menuStatus
    };
    double now = 1.0;
    std::string error;

    app::EditorFileActions canceled{
        context, {CanceledChart, MissingAudio, CanceledSave, mockedNow}};
    canceled.Open(now, error);
    ok &= Check(openCalls == 1 && now == 42.75 &&
                status == "WORKING DRAFT" && editor.Dirty() &&
                editor.Data().title == "UNSAVED CHART" &&
                chartPath == L"original.locus.json" &&
                audioPath == L"original.wav" && previewBeat == 6,
                "canceling Open preserves the unsaved editor and selection");

    app::EditorFileActions failed{
        context, {MissingChart, MissingAudio, CanceledSave, mockedNow}};
    failed.Open(now, error);
    ok &= Check(openCalls == 2 && !error.empty() &&
                status.find("OPEN FAILED: ") == 0 &&
                editor.Dirty() && editor.Data().title == "UNSAVED CHART" &&
                chartPath == L"original.locus.json" &&
                loadedAudioPath == L"previous-song.wav",
                "failed Open cannot replace the active draft or song");

    failed.AttachAudio(now);
    ok &= Check(audioCalls == 1 && status.find("SONG FAILED: ") == 0 &&
                editor.Data().title == "UNSAVED CHART" &&
                audioPath == L"original.wav" &&
                loadedAudioPath == L"previous-song.wav" &&
                !song.Ready(),
                "failed audio attachment is non-destructive");

    const int beforePicker = saveCalls;
    failed.Save(true, now);
    ok &= Check(saveCalls == beforePicker && status.find("SAVE FAILED: ") == 0 &&
                chartPath == L"original.locus.json" && editor.Dirty() &&
                menuStatus == "OLD LIBRARY STATUS" &&
                libraryUi.notice == "OLD LIBRARY NOTICE",
                "missing audio rejects save before displaying native Save As");

    int previewMs = -9;
    ok &= Check(app::ParseSongPreviewMs("12567", previewMs) &&
                previewMs == 12567 &&
                app::ParseSongPreviewMs("3600000", previewMs) &&
                previewMs == 3600000 &&
                !app::ParseSongPreviewMs("-1", previewMs) &&
                !app::ParseSongPreviewMs("12.5", previewMs) &&
                !app::ParseSongPreviewMs("3600001", previewMs) &&
                !app::ParseSongPreviewMs("", previewMs) &&
                previewMs == 3600000,
                "preview field accepts exact whole milliseconds and rejects invalid values");

    // The global dirty-document guard approves this action BEFORE calling
    // NewDraft(). The operation itself must fully detach the old song.
    failed.NewDraft();
    ok &= Check(!editor.Dirty() && editor.Data().title != "UNSAVED CHART" &&
                !song.Ready() && chartPath.empty() && audioPath.empty() &&
                loadedAudioPath.empty() && previewBeat == 0 &&
                status == "NEW CHART - CLICK LOAD SONG" &&
                ui.tab == app::EditorTab::Route,
                "New Draft resets files, preview and editor UI together");

    // Loading a library chart is also non-destructive on failure, and never
    // invokes a native picker (the library already has the chart path).
    editor.SetTitle("SECOND DRAFT");
    ok &= Check(!failed.OpenFromLibrary(
                    L"missing-library-chart.locus.json", error) &&
                editor.Data().title == "SECOND DRAFT" && editor.Dirty() &&
                chartPath.empty() && status == "NEW CHART - CLICK LOAD SONG",
                "failed library edit retains the current unsaved draft");
    app::EditorSetController setControl{
        {library, failed, editor, ui, chartPath, status, libraryUi, menuStatus},
        {CanceledChart, CanceledSave}
    };
    ok &= Check(!setControl.View().managed &&
                failed.ActiveSetFolder().empty(),
                "uninstalled draft has no Song Set");
    setControl.Request(app::EditorSetAction::DeleteSongSet);
    ok &= Check(!setControl.HasPending() &&
                status.find("SET FAILED: ") == 0,
                "uninstalled chart cannot accidentally delete library files");
    ok &= Check(app::EditorSetNeedsConfirmation({
                    app::EditorSetAction::Open, {}, {}, true}) &&
                app::EditorSetNeedsConfirmation({
                    app::EditorSetAction::DeleteSongSet, {}, {}, false}) &&
                !app::EditorSetNeedsConfirmation({
                    app::EditorSetAction::Export, {}, {}, false}),
                "destructive and dirty-draft actions require confirmation");
    // Actual controller wiring: a managed Song Set must open a chart from
    // one row click (with unsaved confirmation), and COMPARE+ must respond
    // through the same native pointer path, not only a pure helper.
    const auto fixture = std::filesystem::temp_directory_path() /
        ("locus-editor-set-click-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto source = fixture / "source";
    std::filesystem::create_directories(source);
    { std::ofstream(source / "song.wav", std::ios::binary) <<
          "RIFF-song-set-test-audio-v1"; }
    const auto writeDifficulty = [&](const char* file, const char* title) {
        const std::string json =
            std::string("{") +
            "\"formatVersion\":3," +
            "\"title\":\"" + title + "\"," +
            "\"audioPath\":\"song.wav\"," +
            "\"bpm\":120,\"offsetMs\":0," +
            "\"ticksPerBeat\":1,\"snapDivision\":1," +
            "\"grid\":{\"width\":8,\"height\":8}," +
            "\"start\":{\"x\":3,\"y\":3,\"heading\":\"r\"}," +
            "\"previewLeadBeats\":4," +
            "\"turns\":[{\"timeMs\":1000,\"heading\":\"u\"}]}";
        std::ofstream(source / file, std::ios::binary) << json;
    };
    writeDifficulty("Easy.locus.json", "Easy");
    writeDifficulty("Hard.locus.json", "Hard");
    app::SongLibrary managedLibrary(fixture / "Songs");
    std::string managedNote, managedError;
    std::filesystem::path installedEasy, installedHard;
    const bool setup =
        managedLibrary.Refresh(managedNote) &&
        managedLibrary.Import(source / "Easy.locus.json",
                              installedEasy, managedError) &&
        managedLibrary.Import(source / "Hard.locus.json",
                              installedHard, managedError);
    if (!setup) std::cerr << "EDITOR_FIXTURE_SETUP_ERROR " << managedError << '\n';
    ok &= Check(setup && managedLibrary.ChartCount() == 2,
                "managed two-chart fixture is ready");
    if (setup) {
        std::wstring activePath = installedEasy.wstring();
        ui.tab = app::EditorTab::Set;
        const app::EditorFileContext managedContext{
            editor, ui, song, playback, activePath, audioPath,
            loadedAudioPath, previewBeat, status, managedLibrary,
            libraryUi, menuStatus};
        app::EditorFileActions managedFiles{
            managedContext,
            {CanceledChart, MissingAudio, CanceledSave, mockedNow}};
        app::EditorSetController managedSet{
            {managedLibrary, managedFiles, editor, ui, activePath,
             status, libraryUi, menuStatus},
            {CanceledChart, CanceledSave}};
        const auto view = managedSet.View();
        ok &= Check(view.managed && view.difficulties.size() == 2 &&
                    view.current == 0 &&
                    view.bpm == managedLibrary.Songs()[0].bpm &&
                    view.offsetMs == managedLibrary.Songs()[0].offsetMs,
                    "active Song Set is available on SET and ROUTE");
        app::EditorControls clickDifficulty;
        clickDifficulty.mouseClick = true;
        clickDifficulty.mouse = {548.0f, 252.0f}; // Hard difficulty row
        managedSet.HandleControls(clickDifficulty);
        ok &= Check(managedSet.HasPending() &&
                    managedSet.Pending()->chart == installedHard &&
                    managedSet.Pending()->discardsDraft && editor.Dirty(),
                    "single SET difficulty click requests safe Open");
        managedSet.Confirm(false, true);
        ok &= Check(!managedSet.HasPending() && editor.Dirty(),
                    "canceling direct switch leaves unsaved chart intact");

        // The controller (not just the pure helper) must commit AUDIO
        // table edits to the actual Song Set manifest and all difficulties.
        ui.tab = app::EditorTab::Audio;
        ok &= Check(editor.SetSongDuration(10.0), "tempo edit has audio duration");
        app::EditorControls tempoClick;
        tempoClick.mouseClick = true;
        tempoClick.mouse = {410.0f, 298.0f};
        managedSet.HandleControls(tempoClick);
        ok &= Check(ui.tempoEditingRow == 0 && ui.tempoEditingBpm,
                    "managed AUDIO BPM cell enters editing");
        app::EditorControls tempoTyping;
        tempoTyping.typedUtf8 = "170.5";
        managedSet.HandleControls(tempoTyping);
        app::EditorControls tempoEnter;
        tempoEnter.enter = true;
        managedSet.HandleControls(tempoEnter);
        const auto& firstTempo = managedLibrary.Songs()[0].tempoMap;
        ok &= Check(ui.tempoEditingRow == -1 &&
                    firstTempo.size() == 1 && firstTempo[0].bpm == 170.5 &&
                    editor.Data().route.bpm == 170.5,
                    "editing BPM updates whole pack and current draft");
        editor.SelectBeat(4);
        tempoClick.mouse = {730.0f, 447.0f};
        managedSet.HandleControls(tempoClick);
        const auto& twoTempo = managedLibrary.Songs()[0].tempoMap;
        ok &= Check(twoTempo.size() == 2 &&
                    twoTempo[1].timeMs > twoTempo[0].timeMs &&
                    twoTempo[1].bpm == 170.5,
                    "ADD creates pack-wide BPM marker at selected timestamp");
        app::SongSetManifest storedTempo;
        ok &= Check(app::LoadSongSetManifest(
                         installedEasy.parent_path() / "set.locus.json",
                         storedTempo, managedError) &&
                    storedTempo.tempoMap == twoTempo,
                    "AUDIO BPM table writes every marker to disk");

        ui.tab = app::EditorTab::Route;
        app::EditorControls clickCompare;
        clickCompare.mouseClick = true;
        clickCompare.mouse = {664.0f, 216.0f};
        managedSet.HandleControls(clickCompare);
        ok &= Check(ui.comparisonPickerOpen,
                    "real managed Song Set COMPARE+ opens on first click");
        clickCompare.mouse = {548.0f, 307.0f};
        managedSet.HandleControls(clickCompare);
        ok &= Check(ui.comparisonFiles.size() == 1 &&
                    ui.comparisonFiles.front() == installedHard,
                    "real comparison picker selects sibling chart by click");

        // A rejected shared tempo update after user Undo must restore the
        // same action via Redo, not accidentally undo an earlier note edit.
        editor.NewDraft();
        editor.SetSongDuration(10.0);
        ok &= Check(editor.SetBpm(150.0) &&
                    managedLibrary.SetSongTempoMap(
                        0, {{0, 150.0}}, nullptr, managedError),
                    "create a shared tempo history for rollback regression");
        {
            std::ofstream changed(installedEasy, std::ios::app);
            changed << ' '; // invalidate the library's size/mtime cache
        }
        ok &= Check(editor.Undo() &&
                    editor.Data().route.bpm == 120.0 &&
                    !managedSet.SyncSongTiming(app::EditorHistoryOperation::Undo) &&
                    editor.Data().route.bpm == 150.0 &&
                    !editor.CanRedo(),
                    "failed tempo publication reverses user Undo via Redo");
    }
    std::error_code fixtureError;
    std::filesystem::remove_all(fixture, fixtureError);

    if (!ok) return 1;
    std::cout << "EDITOR_FILE_ACTIONS_PASS cancel=1 open=1 attach=1 save=1 new=1\n";
    return 0;
}
