#include "app/chart_playback.h"
#include "app/demo_transport.h"
#include "app/editor_file_actions.h"
#include "app/editor_session.h"
#include "app/game_session.h"
#include "app/local_music.h"
#include "app/menu_ui.h"
#include "app/play_screen_actions.h"
#include "app/run_results.h"
#include "app/screen_flow.h"
#include "app/session_transition.h"
#include "app/song_library.h"
#include "core/chart_editor.h"
#include "core/judgement.h"
#include "core/route.h"
#include "song_fixture.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>

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
                chartPath.empty() && audioPath.empty(),
                "Home > Editor opens a blank document");

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
    const bool easyBeatSelected = editor.SelectBeat(3);
    if (easyBeatSelected) editor.PutTurn(core::Heading::DownLeft);
    ok &= Check(easyBeatSelected && editor.Dirty(),
                "Easy > author a diagonal note");
    ok &= Check(files.FocusChart(installedHard, error) &&
                editor.Data().title == "Hard" &&
                files.ParkedEditor(installedEasy) &&
                files.ParkedEditor(installedEasy)->Dirty(),
                "Easy > Focus Hard parks Easy and its independent undo history");
    const bool hardBeatSelected = editor.SelectBeat(5);
    if (hardBeatSelected) editor.PutTurn(core::Heading::Down);
    ok &= Check(hardBeatSelected && editor.Dirty() &&
                files.AnyDirty() && files.HasDirtyParked(),
                "Hard > author second unsaved difficulty");
    ok &= Check(files.FocusChart(installedEasy, error) &&
                editor.Data().title == "Easy" && editor.Dirty() &&
                files.ParkedEditor(installedHard) &&
                files.ParkedEditor(installedHard)->Dirty(),
                "Hard > Focus Easy restores both unsaved histories");

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
                files.HasDirtyParked() && !playback.Playing(),
                "TEST > Editor keeps both unsaved documents");
    ok &= Check(files.SaveAll(now) && !files.AnyDirty(),
                "Save All persists both documents");

    app::SongLibrary restarted(profile.root / "Songs");
    ok &= Check(restarted.Refresh(note) && restarted.ChartCount() == 2,
                "Restart > Library sees both saved difficulties");
    const auto& songs = restarted.Songs();
    const auto findTitle = [&](const char* title)
        -> const app::LibraryChart* {
        for (const auto& set : songs)
            for (const auto& item : set.charts)
                if (item.data.title == title) return &item;
        return nullptr;
    };
    const auto* savedA = findTitle("Easy");
    const auto* savedB = findTitle("Hard");
    ok &= Check(savedA && savedB &&
                savedA->data.route.turns.size() == 2 &&
                savedB->data.route.turns.size() == 3 &&
                savedB->data.route.tempoMap.size() == 2 &&
                savedB->data.route.tempoMap[1].bpm == 180.0,
                "Saved drafts survive restart with shared BPM and both note edits");
    if (!savedB || songs.empty()) return 1;

    // Start the real scored PlayScreenActions controller, deliver device
    // events at their authored audio timestamps, then traverse the normal
    // post-judgement animation and results boundary.
    core::Chart scored = savedB->data.route;
    core::TurnJudge scoredJudge(scored);
    app::GameSession scoredGame(scored, scoredJudge, demo, playback);
    app::AppState scoredNav;
    scoredNav.screen = Screen::Play;
    bool scoredCustom = true;
    bool scoredCompleted = false;
    app::PlayOutroTransition outro;
    app::PlayScreenActions scoredActions{
        {scoredNav, scoredGame, scoredJudge, demo, playback,
         scoredCustom, scoredCompleted, scored, outro}
    };
    playback.Configure(scored.bpm, savedB->data.offsetSeconds,
                       scored.tempoMap, scored.ticksPerBeat);
    constexpr double startAt = 20.0;
    scoredGame.Start(true, startAt);
    ok &= Check(playback.Playing() && scoredJudge.Counts().Resolved() == 0,
                "Play > starts saved difficulty through scored session");

    for (const auto& turn : scored.turns) {
        app::FrameInput keyDown;
        const double songTime = static_cast<double>(
            core::EventTimeMs(scored, turn, savedB->data.offsetSeconds)) / 1000.0;
        const double frameNow = startAt + songTime;
        const unsigned physicalArrows = core::HeadingArrowMask(turn.direction);
        for (std::size_t lane = 0; lane < keyDown.arrowPressed.size(); ++lane) {
            if ((physicalArrows & (1u << lane)) == 0) continue;
            keyDown.arrowPressed[lane] = true;
            keyDown.arrowCapturedAt[lane] = frameNow;
        }
        scoredActions.Update(keyDown, frameNow);
    }
    ok &= Check(scoredJudge.AllScorableJudged() && outro.Active() &&
                scoredJudge.Counts().perfect ==
                    static_cast<int>(scoredJudge.ScorableCount()),
                "Play > authored notes including diagonal are all judged Perfect");
    scoredActions.Update({}, startAt + 5.0);
    ok &= Check(scoredCompleted && scoredNav.screen == Screen::Results &&
                !outro.Active(), "last judgement > outro > Results screen");

    app::RunResultsStore results(profile.root / "Results");
    app::CompletedRunContext finish{
        scoredCompleted, false, false,
        {songs[0].id, savedB->file.filename().u8string()},
        songs[0].title, savedB->data.title,
        scoredJudge.Counts(), scoredJudge.ScorableCount(),
        scoredActions.Transport(startAt + 5.0).practiceSpeed
    };
    app::RunResult record;
    ok &= Check(app::CaptureCompletedRun(results, finish, record, error) &&
                results.Load(finish.key, error).size() == 1,
                "Play > last judgement > persisted Results on clean profile");
    finish.completedThisFrame = false;
    ok &= Check(!app::CaptureCompletedRun(results, finish, record, error) &&
                results.Load(finish.key, error).size() == 1,
                "Results > historical reentry cannot duplicate completed run");
    return ok ? 0 : 1;
}
