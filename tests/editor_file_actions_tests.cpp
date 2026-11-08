#include "app/editor_file_actions.h"
#include "app/editor_set_controller.h"
#include "app/chart_editor_ui.h"
#include "app/chart_playback.h"
#include "app/editor_document.h"
#include "app/local_music.h"
#include "app/menu_ui.h"
#include "app/song_library.h"
#include "core/chart_editor.h"

#include <filesystem>
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
    if (!ok) return 1;
    std::cout << "EDITOR_FILE_ACTIONS_PASS cancel=1 open=1 attach=1 save=1 new=1\n";
    return 0;
}
