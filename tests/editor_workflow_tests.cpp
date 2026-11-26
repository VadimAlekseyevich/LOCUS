#include "app/chart_playback.h"
#include "app/direction_input_events.h"
#include "app/demo_transport.h"
#include "app/editor_file_actions.h"
#include "app/editor_set_controller.h"
#include "app/editor_discard_guard.h"
#include "app/editor_confirmation.h"
#include "app/editor_session.h"
#include "app/game_session.h"
#include "app/local_music.h"
#include "app/library_playback.h"
#include "app/menu_ui.h"
#include "app/play_screen_actions.h"
#include "app/play_intro_skip.h"
#include "app/run_results.h"
#include "app/screen_flow.h"
#include "app/session_transition.h"
#include "app/song_library.h"
#include "core/chart_editor.h"
#include "core/chart_revision.h"
#include "core/judgement.h"
#include "core/route.h"
#include "song_fixture.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <iostream>
#include <memory>
#include <vector>
#include <string>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define CloseWindow Win32CloseWindow
#define ShowCursor Win32ShowCursor
#include <windows.h>
#undef CloseWindow
#undef ShowCursor
#endif

namespace {
using namespace locus;
namespace fs = std::filesystem;

bool Check(bool value, const char* step) {
    if (!value) std::cerr << "EDITOR_WORKFLOW_FAIL at " << step << '\n';
    return value;
}

std::uint32_t Le32(const std::uint8_t* data) {
    return static_cast<std::uint32_t>(data[0]) |
        (static_cast<std::uint32_t>(data[1]) << 8) |
        (static_cast<std::uint32_t>(data[2]) << 16) |
        (static_cast<std::uint32_t>(data[3]) << 24);
}

// Only the OS audio output is replaced. The production LocalMusic,
// ChartPlayback, EditorFileActions, GameSession and disk code still run.
class SilentOutputStream final : public app::MusicOutputStream {
public:
    explicit SilentOutputStream(double duration) : duration_(duration) {}
    double DurationSeconds() const override { return duration_; }
    double PlayedSeconds() const override { return position_; }
    bool Playing() const override { return running_ && !paused_; }
    void SetVolume(float) override {}
    void SetPitch(float) override {}
    void Play() override { running_ = true; paused_ = false; }
    void Seek(float seconds) override { position_ = seconds; }
    void Pause() override { paused_ = true; }
    void Resume() override { paused_ = false; }
    void Stop() override { running_ = false; paused_ = false; position_ = 0; }
    void Update() override {}
private:
    double duration_ = 0.0;
    double position_ = 0.0;
    bool running_ = false;
    bool paused_ = false;
};

class SilentAudioBackend final : public app::MusicOutputBackend {
public:
    bool Ready() const override { return true; }
    bool SupportsPitchPreserving() const noexcept override { return false; }
    bool HasConsumedAudioCursor() const noexcept override { return false; }
    std::unique_ptr<app::MusicOutputStream> Open(
        const char* format, const std::uint8_t* bytes, int count) override {
        if (std::strcmp(format, ".wav") != 0 || count < 44 ||
            std::memcmp(bytes, "RIFF", 4) != 0 ||
            std::memcmp(bytes + 8, "WAVE", 4) != 0 ||
            Le32(bytes + 28) == 0) return {};
        const double duration = static_cast<double>(Le32(bytes + 40)) /
                                Le32(bytes + 28);
        return duration > 0.0 ?
            std::make_unique<SilentOutputStream>(duration) : nullptr;
    }
};

app::ImportPickResult CancelImport() { return {{}, {}, true}; }
app::MusicPickResult CancelPick() { return {{}, {}, true}; }
app::MusicPickResult CancelSave(const std::wstring&) {
    return {{}, {}, true};
}
double TestClock() { return 10.0; }

struct TempProfile {
    fs::path root;
    TempProfile() {
        root = fs::temp_directory_path() /
            ("locus-editor-workflow-" + std::to_string(
                std::chrono::steady_clock::now().time_since_epoch().count()));
        fs::create_directories(root);
    }
    ~TempProfile() {
        std::error_code ec;
        fs::remove_all(root, ec);
    }
};

} // namespace

int main() {
    using namespace locus;
    using app::Screen;
    bool ok = true;
    const auto noConfirm = app::EditorConfirmationView(nullptr,app::EditorPending::None,false,"");
    app::EditorSetRequest deletion;
    deletion.action = app::EditorSetAction::DeleteSongSet;
    auto cleanConfirm = app::EditorConfirmationView(&deletion,app::EditorPending::None,false,"");
    deletion.discardsDraft = true;
    auto dirtyConfirm = app::EditorConfirmationView(&deletion,app::EditorPending::None,false,"SAVE FAILED: disk");
    auto closeConfirm = app::EditorConfirmationView(nullptr,app::EditorPending::None,true,"");
    ok &= Check(!noConfirm.visible && cleanConfirm.visible && !cleanConfirm.canSave &&
        dirtyConfirm.canSave && dirtyConfirm.notice=="SAVE FAILED: disk" &&
        closeConfirm.canSave && closeConfirm.message=="Save before closing this chart?" &&
        app::ChooseConfirmation(cleanConfirm,true,false,false)==app::ConfirmationChoice::None &&
        app::ChooseConfirmation(dirtyConfirm,true,true,true)==app::ConfirmationChoice::Cancel &&
        app::ChooseConfirmation(dirtyConfirm,true,false,false)==app::ConfirmationChoice::Save,
        "editor confirmation model distinguishes clean deletion, dirty save and pane close");
    TempProfile profile;
    const fs::path sources = profile.root / "external";
    fs::create_directories(sources);
    const auto easy = sources / "Easy.locus.json";
    const auto hard = sources / "Hard.locus.json";
    ok &= Check(test::WriteSilentWav(sources / "song.wav") &&
                test::WriteDifficulty(easy, "Easy",
                    "[{\"timeMs\":1000,\"heading\":\"u\"}]") &&
                test::WriteDifficulty(hard, "Hard",
                    "[{\"timeMs\":1000,\"heading\":\"ru\"},"
                    "{\"timeMs\":3000,\"heading\":\"l\"}]"),
                "fixture: WAV + two charts + BPM switch");

    app::SongLibrary library(profile.root / "Songs");
    std::string note, error;
    fs::path installedEasy, installedHard;
    ok &= Check(library.Refresh(note) &&
                library.Import(easy, installedEasy, error) &&
                library.Import(hard, installedHard, error),
                "Library > import both difficulties");
    if (!ok) { std::cerr << "FIXTURE: " << error << '\n'; return 1; }

    SilentAudioBackend fakeDevice;
    app::LocalMusic song(fakeDevice);
    app::ChartPlayback playback(song);
    core::ChartEditor editor;
    app::EditorUiState editorUi;
    app::SongSelectUi libraryUi;
    std::wstring chartPath, audioPath, loadedAudioPath;
    int previewBeat = 0;
    std::string status, menuStatus;
    app::EditorFileActions files{
        {editor, editorUi, song, playback, chartPath, audioPath,
         loadedAudioPath, previewBeat, status, library, libraryUi, menuStatus},
        {CancelPick, CancelPick, CancelSave, TestClock}
    };

    app::AppState nav;
    app::NavigationInput edit;
    edit.openEditorPressed = true;
    app::UpdateScreenFlow(nav, edit, false);
    files.NewDraft();
    ok &= Check(nav.screen == Screen::Editor && !editor.Dirty() &&
                editor.Data().route.turns.empty() &&
                chartPath.empty() && audioPath.empty() &&
                editorUi.tab == app::EditorTab::Set,
                "Home > Editor opens blank Song Setup");

    // Canceling the real file action changes neither the selected document
    // nor the underlying library, even after its native dialog callback.
    double now = 10.0;
    files.Open(now, error);
    ok &= Check(nav.screen == Screen::Editor && chartPath.empty() &&
                library.ChartCount() == 2,
                "Editor > cancel Open preserves blank draft");

    // Open through the actual controller; only sound output is simulated.
    ok &= Check(files.OpenFromLibrary(installedEasy.wstring(), error) &&
                song.Ready() && song.DurationSeconds() == 4.0 &&
                editor.Data().title == "Easy" &&
                chartPath == installedEasy.wstring(),
                "Library > Edit difficulty decodes WAV without a sound device");
    if (!song.Ready()) return 1;
    app::EditorSetController sets{
        {library, files, editor, editorUi, chartPath, status, libraryUi, menuStatus},
        {CancelImport, CancelSave}};
    const auto indexFor = [&](const fs::path& path) {
        const auto view = sets.View();
        for (std::size_t i = 0; i < view.comparisonOptions.size(); ++i)
            if (view.comparisonOptions[i].file == path) return static_cast<int>(i);
        return -1;
    };
    const auto& initialView=sets.FrameView();
    const auto* cachedRows=initialView.comparisonOptions.data();
    const auto initialCatalogRevision=initialView.catalogRevision;
    bool reused=cachedRows && initialView.comparisonOptions.size()==2;
    for (int i=0;i<128;++i) {
        editorUi.setSelected=i%2;
        const auto& frame=sets.FrameView();
        reused &= frame.comparisonOptions.data()==cachedRows &&
            frame.catalogRevision==initialCatalogRevision &&
            frame.selected==i%2;
    }
    editorUi.setSelected=-1;
    ok &= Check(reused,"frame view reuses catalog rows across cursor and selection changes");
    editorUi.tab = app::EditorTab::Set;
    editorUi.comparisonFiles.push_back(installedHard);
    ok &= Check(sets.FocusDifficulty(indexFor(installedHard)) &&
                !sets.HasPending() && chartPath == installedEasy.wstring() &&
                editorUi.tab == app::EditorTab::Set,
                "first click selects Hard without opening or changing comparison");
    // Naming the selected but unopened chart cannot switch documents or
    // change the current chart. The stable ID survives metadata refresh.
    const auto selectedId = editorUi.setSelectedId;
    ok &= Check(sets.CommitDifficultyTitle("Hard selection probe", now) &&
                chartPath == installedEasy.wstring() && editor.Data().title == "Easy" &&
                editorUi.setSelectedId == selectedId &&
                sets.View().difficulties[static_cast<std::size_t>(indexFor(installedHard))] ==
                    "Hard selection probe" &&
                sets.CommitDifficultyTitle("Hard", now),
                "rename selected inactive difficulty without opening or saving current chart");
    ok &= Check(sets.FocusDifficulty(indexFor(installedHard)) &&
                !sets.HasPending() && chartPath == installedHard.wstring() &&
                editorUi.tab == app::EditorTab::Route &&
                editorUi.comparisonFiles.empty() && !files.ParkedEditor(installedEasy),
                "Song Setup > Hard opens standalone, not a comparison pane");
    editorUi.tab = app::EditorTab::Set;
    ok &= Check(sets.FocusDifficulty(indexFor(installedEasy)) &&
                editorUi.tab == app::EditorTab::Set &&
                sets.EnterChart(now) &&
                chartPath == installedEasy.wstring() &&
                editorUi.comparisonFiles.empty() &&
                editorUi.tab == app::EditorTab::Route,
                "Song Setup > Easy switches standalone chart");
    const bool easyBeatSelected = editor.SelectBeat(3);
    if (easyBeatSelected) editor.PutTurn(core::Heading::DownLeft);
    ok &= Check(easyBeatSelected && editor.Dirty(),
                "Easy > author a diagonal note");
    editorUi.layout.native = true;
    editorUi.layout.coordinates = app::ViewportTransform::NativeUi(1.5f);
    editorUi.layout.preview = {15,70,820,450};
    ok &= Check(files.FocusChart(installedHard, error) &&
                editorUi.layout.native && editorUi.layout.preview.width == 820 &&
                editorUi.layout.coordinates.scale == 1.5f &&
                editor.Data().title == "Hard" &&
                files.ParkedEditor(installedEasy) &&
                files.ParkedEditor(installedEasy)->Dirty(),
                "Easy > Focus Hard parks Easy and its independent undo history");
    const bool hardBeatSelected = editor.SelectBeat(5);
    if (hardBeatSelected) editor.PutTurn(core::Heading::Down);
    ok &= Check(hardBeatSelected && editor.Dirty() &&
                files.AnyDirty() && files.HasDirtyParked(),
                "Hard > author second unsaved difficulty");
    editorUi.layout.preview.width = 1000; // the window grew while Easy was parked
    ok &= Check(files.FocusChart(installedEasy, error) &&
                editorUi.layout.native && editorUi.layout.preview.width == 1000 &&
                editor.Data().title == "Easy" && editor.Dirty() &&
                files.ParkedEditor(installedHard) &&
                files.ParkedEditor(installedHard)->Dirty(),
                "Hard > Focus Easy restores both unsaved histories");

    // Parked unsaved names are metadata too: no catalog publish is needed
    // to refresh the view after editing a parked draft.
    const auto parkedCatalogRevision=sets.FrameView().catalogRevision;
    ok &= Check(files.RenameInactiveDifficulty(installedHard,"Hard parked probe",error) &&
                sets.FrameView().catalogRevision==parkedCatalogRevision &&
                sets.FrameView().difficulties[static_cast<std::size_t>(indexFor(installedHard))]=="Hard parked probe" &&
                files.RenameInactiveDifficulty(installedHard,"Hard",error) &&
                sets.FrameView().difficulties[static_cast<std::size_t>(indexFor(installedHard))]=="Hard",
                "cached view invalidates for parked title edits without catalog refresh");

    app::EditorControls compare;
    compare.nativeChrome = true;
    compare.compareAction = app::ComparePickerAction::Toggle;
    sets.HandleControls(compare);
    compare.compareAction = app::ComparePickerAction::Select;
    compare.compareFile = installedHard;
    sets.HandleControls(compare);
    ok &= Check(editorUi.comparisonPickerOpen && files.PendingClose()==installedHard &&
                files.HasDirtyParked(),
                "native minus uses dirty reference guard without closing picker");
    ok &= Check(files.ResolveClose(false,false,now) && editorUi.comparisonPickerOpen &&
                files.PendingClose().empty() && files.HasDirtyParked(),
                "cancel dirty reference removal restores native picker");
    compare.compareAction = app::ComparePickerAction::Dismiss;
    sets.HandleControls(compare);
    const auto* parkedHard = files.ParkedEditor(installedHard);
    const int selectedBeforeTest = editor.SelectedBeat();
    ok &= Check(editor.CanUndo() && parkedHard && parkedHard->CanUndo(),
                "Both difficulties keep independent Undo histories after focus changes");

    // The real editor guard suppresses unrelated shortcut actions while
    // its modal is active. Cancel/Esc returns to the two dirty documents.
    app::EditorDiscardGuard discardGuard;
    app::FrameInput attemptLeave;
    attemptLeave.homePressed = true;
    attemptLeave.editorTest = true; // must not be passed beneath the modal.
    ok &= Check(discardGuard.Handle(
                    attemptLeave, files.AnyDirty(), false, false) ==
                    app::EditorDiscardOutcome::PromptOpened &&
                discardGuard.Pending() == app::EditorPending::Home &&
                !attemptLeave.homePressed && !attemptLeave.editorTest,
                "Editor > Home with unsaved work opens exclusive leave guard");
    app::FrameInput canceledLeave;
    canceledLeave.editorNew = true;
    ok &= Check(discardGuard.Handle(
                    canceledLeave, files.AnyDirty(), false, true) ==
                    app::EditorDiscardOutcome::ChangesKept &&
                discardGuard.Pending() == app::EditorPending::None &&
                !canceledLeave.editorNew && files.AnyDirty(),
                "Editor > Esc cancel keeps both dirty histories");

    // Native OS close takes precedence over the editor discard prompt.
    // Canceling the quit dialog must leave the dirty workspace untouched.
    app::FrameInput windowClose;
    windowClose.closeRequested = true;
    ok &= Check(discardGuard.Handle(windowClose, files.AnyDirty(), false, false) ==
                    app::EditorDiscardOutcome::None &&
                discardGuard.Pending() == app::EditorPending::None,
                "Editor > Windows close bypasses the editor-specific modal");
    app::NavigationInput requestQuit;
    requestQuit.closeRequested = windowClose.closeRequested;
    app::UpdateScreenFlow(nav, requestQuit, false);
    ok &= Check(nav.quitConfirmation && nav.running &&
                nav.screen == Screen::Editor && files.AnyDirty(),
                "Editor > OS close enters application quit guard");
    app::NavigationInput keepEditing;
    keepEditing.quitCanceled = true;
    app::UpdateScreenFlow(nav, keepEditing, false);
    ok &= Check(!nav.quitConfirmation && files.HasDirtyParked(),
                "Editor > cancel OS quit keeps parked unsaved work");

    const core::Chart builtinDemo = core::MakeDemoChart();
    core::Chart activeChart = builtinDemo;
    core::TurnJudge judge(activeChart);
    app::DemoTransport demo;
    app::GameSession game(activeChart, judge, demo, playback);
    bool custom = false, completed = false;
    std::string activeTitle = "BUILT-IN DEMO";
    app::EditorSession author{
        editor, editorUi, playback, song, audioPath, previewBeat, status};
    app::SessionTransitionContext transition{
        activeChart, builtinDemo, judge, game, playback, song, editorUi,
        custom, completed, activeTitle
    };
    ok &= Check(author.BeginTest(
                    {activeChart, activeTitle, judge, game, custom},
                    now, error) && game.IsEditorTest() && custom,
                "Easy > TEST starts from unsaved in-memory chart");
    app::NavigationInput toPlay;
    toPlay.requestedScreen = Screen::Play;
    app::UpdateScreenFlow(nav, toPlay, false);
    app::ApplySessionTransition(transition, Screen::Editor, nav.screen,
                                false, now);
    ok &= Check(nav.screen == Screen::Play && playback.Playing(),
                "TEST > Play runs scored transport");
    game.TogglePause(true, now + 0.01);
    ok &= Check(playback.Paused(), "TEST > pause preserves playhead");
    game.TogglePause(true, now + 0.2);
    ok &= Check(!playback.Paused(), "TEST > resume");
    app::NavigationInput leave;
    leave.requestedScreen = Screen::Editor;
    app::UpdateScreenFlow(nav, leave, false);
    app::ApplySessionTransition(transition, Screen::Play, nav.screen,
                                false, now + 0.3);
    ok &= Check(nav.screen == Screen::Editor && files.AnyDirty() &&
                files.HasDirtyParked() && !playback.Playing() &&
                editor.SelectedBeat() == selectedBeforeTest &&
                editor.CanUndo() && files.ParkedEditor(installedHard) &&
                files.ParkedEditor(installedHard)->CanUndo(),
                "TEST > Editor keeps exact tick, both dirty documents and Undo histories");
    // An actual EditorSession::BeginTest gives GameSession a TestOrigin;
    // even a completely resolved judge is not eligible for a saved attempt.
    judge.FinishRemainingMisses();
    app::RunResultsStore transientResults(profile.root / "Results","11111111111111111111111111111111");
    const app::RunKey testKey{
        library.Songs().front().id,
        library.Songs().front().charts[0].metadata.difficultyId};
    app::CompletedRunContext finishedTest{
        true, game.IsEditorTest(), true, testKey,
        library.Songs().front().title, editor.Data().title,
        judge.Counts(), judge.ScorableCount(), playback.Speed(),
        core::ChartGameplayRevision(editor.Data())
    };
    app::RunResult rejectedTest;
    ok &= Check(game.IsEditorTest() &&
                !app::CaptureCompletedRun(
                    transientResults, finishedTest, rejectedTest, error) &&
                error.empty() && transientResults.Load(testKey, error).empty(),
                "TEST completion cannot publish a result on the real profile");

#ifdef _WIN32
    // Inject a real file-sharing denial on the *second* dirty difficulty.
    // The first chart is allowed to commit; the failed second write must
    // preserve its parked undo history and keep the leave guard active.
    {
        HANDLE held = CreateFileW(installedHard.c_str(), GENERIC_READ,
                                  FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                  FILE_ATTRIBUTE_NORMAL, nullptr);
        ok &= Check(held != INVALID_HANDLE_VALUE,
                    "fixture can block second difficulty replacement");
        if (held != INVALID_HANDLE_VALUE) {
            ok &= Check(!files.SaveAll(now) &&
                        chartPath == installedEasy.wstring() &&
                        !editor.Dirty() &&
                        files.ParkedEditor(installedHard) &&
                        files.ParkedEditor(installedHard)->Dirty() &&
                        files.AnyDirty() &&
                        status.find("SAVE FAILED: ") == 0,
                        "partial Save All commits first and preserves second dirty draft");
            CloseHandle(held);
            app::SongLibrary partialRestart(profile.root / "Songs");
            ok &= Check(partialRestart.Refresh(note) &&
                        partialRestart.ChartCount() == 2 &&
                        partialRestart.Songs()[0].charts[0].metadata.noteCount == 2 &&
                        partialRestart.Songs()[0].charts[1].metadata.noteCount == 2,
                        "first difficulty survives restart; blocked second is unchanged");
        }
    }
#endif
    ok &= Check(files.SaveAll(now) && !files.AnyDirty(),
                "Save All retries remaining dirty document without duplicating first");

    // Force a real failure AFTER a chart file has been renamed but BEFORE
    // the new JSON can publish: the backup destination is replaced by a
    // regular file. The original filename, bytes and dirty draft must survive.
    {
        const fs::path history = installedEasy.parent_path() / ".locus-history";
        const fs::path heldHistory = installedEasy.parent_path() /
            ".locus-history-test-held";
        std::error_code ec;
        ok &= Check(fs::is_directory(history, ec) && !ec,
                    "first Save All created recoverable chart backups");
        if (!ec && fs::is_directory(history, ec)) {
            fs::rename(history, heldHistory, ec);
            ok &= Check(!ec, "backup folder can be temporarily occupied");
            if (!ec) {
                {
                    std::ofstream blocker(history, std::ios::binary);
                    blocker << "not a directory";
                }
                const auto read = [](const fs::path& file) {
                    std::ifstream in(file, std::ios::binary);
                    return std::string(std::istreambuf_iterator<char>(in),
                                       std::istreambuf_iterator<char>());
                };
                const std::string unchanged = read(installedEasy);
                const fs::path renamed = installedEasy.parent_path() /
                    "Rollback Probe.locus.json";
                editor.SetTitle("Rollback Probe");
                ok &= Check(!files.Save(false, now) &&
                            chartPath == installedEasy.wstring() &&
                            editor.Dirty() && fs::exists(installedEasy) &&
                            !fs::exists(renamed) &&
                            read(installedEasy) == unchanged,
                            "post-rename backup failure restores original filename and bytes");
                fs::remove(history, ec);
                ec.clear();
                fs::rename(heldHistory, history, ec);
                ok &= Check(!ec, "original backup history survives failed Save");
                editor.SetTitle("Easy");
                ok &= Check(files.Save(false, now) && !editor.Dirty() &&
                            chartPath == installedEasy.wstring(),
                            "retry after backup failure saves the retained draft");
            }
        }
    }

    // A saved difficulty may have a new filename while the library still
    // holds its old index. Saving it must first rescan, not create a copy.
    {
        const fs::path relocated = installedEasy.parent_path() /
            "Easy-relocated.locus.json";
        std::error_code renameError;
        fs::rename(installedEasy, relocated, renameError);
        ok &= Check(!renameError,
                    "fixture can rename the committed chart externally");
        if (!renameError) {
            chartPath = relocated.wstring();
            ok &= Check(files.Save(false, now) &&
                        chartPath == relocated.wstring() &&
                        library.ChartCount() == 2,
                        "Save rescans a stale library before updating chart");
            int songIndex = -1, difficultyIndex = -1;
            ok &= Check(library.FindChart(relocated, songIndex, difficultyIndex),
                        "committed difficulty found under actual filename");
        }
    }

    app::SongLibrary restarted(profile.root / "Songs");
    ok &= Check(restarted.Refresh(note) && restarted.ChartCount() == 2,
                "Restart > Library sees both saved difficulties");
    const auto& songs = restarted.Songs();
    const auto findTitle = [&](const char* title)
        -> const app::LibraryChart* {
        for (const auto& set : songs)
            for (const auto& item : set.charts)
                if (item.metadata.title == title) return &item;
        return nullptr;
    };
    const auto* savedA = findTitle("Easy");
    const auto* savedB = findTitle("Hard");
    core::LoadedChart savedPayload;
    ok &= Check(savedA && savedB &&
                savedA->metadata.noteCount==2 &&
                savedB->CopyPayload(savedPayload,error) &&
                savedPayload.route.turns.size()==3 &&
                savedPayload.route.tempoMap.size()==2 &&
                savedPayload.route.tempoMap[1].bpm==180.0,
                "Saved drafts survive restart with shared BPM and both note edits");
    if (!savedB || songs.empty() || !ok) return 1;

    // Start the real scored PlayScreenActions controller, deliver device
    // events at their authored audio timestamps, then traverse the normal
    // post-judgement animation and results boundary.
    // The compact catalog holds metadata; chart length cannot be inferred
    // until the selected chart and its audio have been loaded.
    // Exercise the exact production activation path that prepares duration,
    // transfers the BPM map and updates GameSession's stable chart address.
    core::Chart scored = core::MakeDemoChart();
    core::TurnJudge scoredJudge(scored);
    app::GameSession scoredGame(scored, scoredJudge, demo, playback);
    std::string scoredTitle;
    bool scoredCustom = false;
    ok &= Check(app::ActivateLibraryDifficulty(
                    *savedB, {song, loadedAudioPath, scored, scoredTitle,
                              playback, scoredJudge, scoredGame, scoredCustom},
                    now, error) &&
                scoredCustom && scoredTitle == "Hard" &&
                scored.lengthBeats > scored.turns.back().beat &&
                scored.tempoMap.size() == 2 &&
                std::abs(playback.BeatAtAudioSeconds(3.0) - 7.0) < 1e-7,
                "Library > Play prepares audio duration and exact BPM map");
    if (!scoredCustom) {
        std::cerr << "LIBRARY_ACTIVATION_FAIL " << error << '\n';
        return 1;
    }
    app::AppState scoredNav;
    scoredNav.screen = Screen::Play;
    bool scoredCompleted = false;
    app::PlayOutroTransition outro;
    app::PlayScreenActions scoredActions{
        {scoredNav, scoredGame, scoredJudge, demo, playback,
         scoredCustom, scoredCompleted, scored, outro}
    };
    constexpr double startAt = 20.0;
    scoredGame.Start(true, startAt);
    ok &= Check(playback.Playing() && scoredJudge.Counts().Resolved() == 0,
                "Play > starts saved difficulty through scored session");

    std::vector<double> audioSampledAtPress;
    std::vector<bool> outroAfterNote;
    app::DirectionInputEvents hardwareEvents;
    constexpr std::array<int, 4> bindings{{74, 75, 70, 68}}; // J K F D
    hardwareEvents.Configure(bindings);
    double firstDiagonalError = 0.0;
    for (std::size_t noteIndex = 0; noteIndex < scored.turns.size(); ++noteIndex) {
        const auto& turn = scored.turns[noteIndex];
        app::FrameInput keyDown;
        const double songTime = static_cast<double>(
            core::EventTimeMs(scored, turn, savedPayload.offsetSeconds)) / 1000.0;
        const double frameNow = startAt + songTime;
        const unsigned physicalArrows = core::HeadingArrowMask(turn.direction);
        // Physical diagonal components arrive 7ms apart. No second snapshot
        // of IsKeyPressed can reconstruct this when both are in one frame.
        for (std::size_t lane = 0; lane < bindings.size(); ++lane) {
            if ((physicalArrows & (1u << lane)) == 0) continue;
            const double skew = noteIndex == 0 && lane == 1 ? 0.007 : 0.0;
            hardwareEvents.Observe(bindings[lane], true, frameNow + skew);
        }
        keyDown.exactDirectionEvents = true;
        hardwareEvents.DrainInto(keyDown.directionEvents);
        const double deliveredAt = frameNow + 0.008;
        audioSampledAtPress.push_back(playback.AudioSeconds(deliveredAt));
        scoredActions.Update(keyDown, deliveredAt);
        if (noteIndex == 0 && scoredJudge.Last())
            firstDiagonalError = scoredJudge.Last()->timingErrorSeconds;
        outroAfterNote.push_back(outro.Active());
        // Real key-up messages must arrive before a subsequent tap can score.
        for (std::size_t lane = 0; lane < bindings.size(); ++lane)
            if (physicalArrows & (1u << lane))
                hardwareEvents.Observe(bindings[lane], false, deliveredAt + 0.001);
    }
    ok &= Check(std::abs(firstDiagonalError - 0.007) < 1e-7,
                "Windows event stream > GameSession preserves 7ms diagonal skew");
    if (!scoredJudge.AllScorableJudged() || !outro.Active() ||
        scoredJudge.Counts().perfect !=
            static_cast<int>(scoredJudge.ScorableCount())) {
        core::TimedRoute debugRoute;
        const bool routeValid = debugRoute.Compile(
            scored, savedPayload.offsetSeconds);
        std::cerr << "EDITOR_WORKFLOW_DIAGNOSTIC routeValid=" << routeValid
                  << " outroActive=" << outro.Active()
                  << " completed=" << scoredCompleted
                  << " state=" << static_cast<int>(scoredNav.screen)
                  << " judgeReady=" << scoredJudge.AllScorableJudged()
                  << " start=" << startAt
                  << " speed=" << playback.Speed()
                  << " playing=" << playback.Playing()
                  << " paused=" << playback.Paused()
                  << " scorable=" << scoredJudge.ScorableCount()
                  << " resolved=" << scoredJudge.Counts().Resolved()
                  << " perfect=" << scoredJudge.Counts().perfect
                  << " good=" << scoredJudge.Counts().good
                  << " bad=" << scoredJudge.Counts().bad
                  << " miss=" << scoredJudge.Counts().miss
                  << " chartNotes=" << scored.turns.size()
                  << " bpm=" << scored.bpm
                  << " offset=" << savedPayload.offsetSeconds
                  << " introStart="
                  << app::ScoredChartStartSeconds(scored, savedPayload.offsetSeconds)
                  << '\n';
        for (std::size_t i = 0; i < scored.turns.size(); ++i)
            std::cerr << "EDITOR_WORKFLOW_NOTE " << i
                      << " beat=" << scored.turns[i].beat
                      << " ms=" << core::EventTimeMs(
                          scored, scored.turns[i], savedPayload.offsetSeconds)
                      << " audioSampled=" << audioSampledAtPress[i]
                      << " outroAfter=" << outroAfterNote[i]
                      << " heading=" << static_cast<int>(scored.turns[i].direction)
                      << '\n';
    }
    ok &= Check(scoredJudge.AllScorableJudged() && outro.Active() &&
                scoredJudge.Counts().perfect ==
                    static_cast<int>(scoredJudge.ScorableCount()),
                "Play > authored notes including diagonal are all judged Perfect");
    scoredActions.Update({}, startAt + 5.0);
    ok &= Check(scoredCompleted && scoredNav.screen == Screen::Results &&
                !outro.Active(), "last judgement > outro > Results screen");

    const auto* trace=scoredJudge.CompletedReplay();
    const core::ReplayTrace spectatorTrace=trace?*trace:core::ReplayTrace{};
    const auto expectedStats=scoredJudge.Statistics();
    ok &= Check(trace!=nullptr,"completed attempt retains spectator scoring trace");

    app::RunResultsStore results(profile.root / "Results","11111111111111111111111111111111");
    app::CompletedRunContext finish{
        scoredCompleted, false, false,
        {songs[0].id, savedPayload.difficultyId},
        songs[0].title, savedPayload.title,
        scoredJudge.Counts(), scoredJudge.ScorableCount(),
        scoredActions.Transport(startAt + 5.0).practiceSpeed,
        core::ChartGameplayRevision(savedPayload)
    };
    app::RunResult record;
    ok &= Check(app::CaptureCompletedRun(results, finish, record, error) &&
                results.Load(finish.key, error).size() == 1,
                "Play > last judgement > persisted Results on clean profile");
    finish.completedThisFrame = false;
    ok &= Check(!app::CaptureCompletedRun(results, finish, record, error) &&
                results.Load(finish.key, error).size() == 1,
                "Results > historical reentry cannot duplicate completed run");

    // End-to-end identity contract: PLAY saves a result, the real editor
    // renames the difficulty (including its file), then a library restart
    // still resolves the exact same result. Copy is a separate difficulty.
    const std::string playedDifficultyId = finish.key.difficultyId;
    const std::string playedRevision = finish.gameplayRevision;
    ok &= Check(files.OpenFromLibrary(installedHard.wstring(), error) &&
                editor.Data().difficultyId == playedDifficultyId,
                "Open played difficulty before rename");
    editorUi.tab = app::EditorTab::Set;
    sets.FocusDifficulty(indexFor(installedHard));
    ok &= Check(editorUi.tab == app::EditorTab::Set && !sets.HasPending() &&
                sets.CommitDifficultyTitle("Hard Renamed", now),
                "Song Setup > Rename saves installed clean difficulty after completed PLAY");
    editorUi.active = app::EditorField::None;
    const fs::path renamedHard(chartPath);
    const bool renamed = renamedHard != installedHard;
    app::SongLibrary afterRename(profile.root / "Songs");
    int renamedSong = -1, renamedChart = -1;
    ok &= Check(renamed &&
                afterRename.Refresh(note) &&
                afterRename.FindChart(renamedHard, renamedSong, renamedChart) &&
                afterRename.Songs()[static_cast<std::size_t>(renamedSong)]
                    .charts[static_cast<std::size_t>(renamedChart)]
                    .metadata.difficultyId == playedDifficultyId &&
                afterRename.Songs()[static_cast<std::size_t>(renamedSong)].charts[
                    static_cast<std::size_t>(renamedChart)].gameplayRevision == playedRevision &&
                results.Load({songs[0].id, playedDifficultyId}, error).size() == 1,
                "PLAY result survives actual chart rename and library restart");
    if (renamed) {
        fs::path copied;
        int copySong = -1, copyChart = -1;
        ok &= Check(library.DuplicateDifficulty(renamedHard, copied, error) &&
                    library.FindChart(copied, copySong, copyChart) &&
                    library.Songs()[static_cast<std::size_t>(copySong)].charts[
                        static_cast<std::size_t>(copyChart)].metadata.difficultyId !=
                        playedDifficultyId &&
                    results.Load({songs[0].id,
                        library.Songs()[static_cast<std::size_t>(copySong)].charts[
                        static_cast<std::size_t>(copyChart)].metadata.difficultyId},
                        error).empty(),
                    "Copy difficulty receives independent identity and empty results");
    }
    // A configured rate applies only at an attempt boundary, never to live audio.
    ok &= Check(!scoredGame.VariablePracticeRate(),
                "completed constant-rate attempt remains a fixed-rate result");
    scoredGame.SetRatePercent(50);
    ok &= Check(playback.Speed() == 1.0,
                "pending HT does not mutate the existing attempt");
    scoredGame.Restart(true, startAt + 6.2);
    ok &= Check(playback.Speed() == 0.5 && !scoredGame.VariablePracticeRate(),
                "new HT attempt freezes its rate");
    scoredGame.SetRatePercent(125);
    ok &= Check(playback.Speed() == 0.5,
                "pending DT does not mutate active HT");
    scoredGame.Restart(true, startAt + 7.0);
    ok &= Check(playback.Speed() == 1.25 && !scoredGame.VariablePracticeRate(),
                "new DT attempt freezes its rate");
    // Seek the real GameSession/transport, including paused reconstruction
    // of diagonals, hit statistics and the same non-rescoring replay cursor.
    const double endSeek=static_cast<double>(core::EventTimeMs(
        scored,scored.turns.back(),savedPayload.offsetSeconds))/1000.0+0.1;
    const double seekNow=startAt+10.0;
    ok &= Check(!scoredGame.SeekSpectator(true,0.0,seekNow),
                "live scored play cannot seek");
    core::PlayMods autoplay;
    autoplay.Toggle(core::PlayMod::Autoplay);
    scoredGame.SetMods(autoplay);
    scoredGame.Start(true,seekNow);
    ok &= Check(scoredGame.SeekSpectator(true,endSeek,seekNow+0.1) &&
                scoredJudge.AllScorableJudged() &&
                scoredJudge.Counts().perfect==static_cast<int>(scoredJudge.ScorableCount()) &&
                scoredGame.TakeHitSoundCount()==0,
                "Autoplay forward seek reconstructs exact hits without sounds");
    scoredGame.TogglePause(true,seekNow+0.2);
    ok &= Check(scoredGame.SeekSpectator(true,0.0,seekNow+0.3) &&
                playback.Paused() && scoredJudge.Counts().Resolved()==0 &&
                scoredJudge.MaxCombo()==0,
                "Autoplay backward seek resets judge while retaining pause");
    scoredGame.SetReplay(spectatorTrace);
    scoredGame.Start(true,seekNow+1);
    ok &= Check(scoredGame.SeekSpectator(true,endSeek,seekNow+1.1) &&
                scoredJudge.AllScorableJudged() &&
                scoredJudge.Statistics().maxCombo==expectedStats.maxCombo &&
                std::abs(scoredJudge.Statistics().unstableRate-expectedStats.unstableRate)<1e-7 &&
                scoredGame.TakeHitSoundCount()==0,
                "Replay seek restores recorded timing and combo");
    scoredGame.TogglePause(true,seekNow+1.2);
    ok &= Check(scoredGame.SeekSpectator(true,0.0,seekNow+1.3) &&
                playback.Paused() && scoredJudge.Counts().Resolved()==0 &&
                scoredGame.SeekSpectator(true,endSeek,seekNow+1.4) &&
                scoredJudge.Statistics().maxCombo==expectedStats.maxCombo &&
                scoredJudge.AllScorableJudged() && scoredGame.TakeHitSoundCount()==0,
                "Replay rewind then forward seek reproduces the same result");
    scoredGame.Stop(true);
    scoredGame.ClearReplay();
    return ok ? 0 : 1;
}
