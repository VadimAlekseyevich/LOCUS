#include "app/run_results.h"
#include "app/song_library.h"
#include "song_fixture.h"
#include "app/screen_flow.h"
#include "core/judgement.h"
#include "core/chart_revision.h"
#include "core/route.h"

#include <chrono>
#include <cstdint>
#include <fstream>
#include <vector>
#include <filesystem>
#include <iostream>
#include <string>

namespace {
using namespace locus;

bool Check(bool condition, const char* command) {
    if (!condition)
        std::cerr << "APP_JOURNEY_FAIL at command: " << command << '\n';
    return condition;
}

struct TempResults {
    std::filesystem::path root;
    TempResults() {
        root = std::filesystem::temp_directory_path() /
            ("locus-journey-" + std::to_string(
                std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(root);
    }
    ~TempResults() {
        std::error_code ignored;
        std::filesystem::remove_all(root, ignored);
    }
};

} // namespace

int main() {
    using namespace locus;
    using app::Screen;

    bool ok = true;
    TempResults profile;
    app::RunResultsStore disk(profile.root / "Results");
    app::AppState nav;

    // Real Home navigation: selecting the editor is not implicitly Play.
    app::NavigationInput toEditor;
    toEditor.openEditorPressed = true;
    app::UpdateScreenFlow(nav, toEditor, false);
    ok &= Check(nav.screen == Screen::Editor, "Home > Editor");

    // Import a true two-difficulty song, restart the library index and
    // play the same chart bytes that production LOAD/PLAY will use.
    const auto source = profile.root / "source";
    std::filesystem::create_directories(source);
    const auto easy = source / "Easy.locus.json";
    const auto hard = source / "Hard.locus.json";
    ok &= Check(locus::test::WriteSilentWav(source / "song.wav") &&
                locus::test::WriteDifficulty(easy, "Easy",
                    "[{\"timeMs\":1000,\"heading\":\"u\"}]") &&
                locus::test::WriteDifficulty(hard, "Hard",
                    "[{\"timeMs\":1000,\"heading\":\"ru\"},"
                    "{\"timeMs\":3000,\"heading\":\"l\"}]"),
                "create decodable WAV and two difficulties with BPM change");
    app::SongLibrary library(profile.root / "Songs");
    std::string message, error;
    std::filesystem::path savedEasy, savedHard;
    ok &= Check(library.Refresh(message) &&
                library.Import(easy, savedEasy, error) &&
                library.Import(hard, savedHard, error) &&
                savedEasy.parent_path() == savedHard.parent_path(),
                "Library > Import Easy > Import Hard into one Song Set");

    app::SongLibrary restarted(profile.root / "Songs");
    ok &= Check(restarted.Refresh(message) &&
                restarted.Songs().size() == 1 &&
                restarted.ChartCount() == 2,
                "Library > application restart > same two difficulties");
    const auto* selected = restarted.Songs().empty() ? nullptr :
        &restarted.Songs().front();
    const auto* hardChart = selected && selected->charts.size() >= 2
        ? &selected->charts[1] : nullptr;
    core::LoadedChart hardPayload;
    ok &= Check(hardChart && hardChart->CopyPayload(hardPayload,error) &&
                hardPayload.title == "Hard" &&
                hardPayload.route.tempoMap.size() == 2 &&
                hardPayload.route.tempoMap[1].timeMs == 2000 &&
                hardPayload.route.tempoMap[1].bpm == 180.0 &&
                hardPayload.route.turns.size() == 2 &&
                hardPayload.route.turns[0].direction == core::Heading::UpRight &&
                hardPayload.route.turns[1].timeMs == 3000 &&
                selected->charts[0].audioFile == hardChart->audioFile,
                "Play > selected difficulty uses absolute notes and shared tempo map");
    if (!hardChart || !ok) return 1;
    const core::Chart chart = hardPayload.route;
    core::TurnJudge judge(chart);
    const app::RunKey key{
        selected->id, hardPayload.difficultyId};
    app::CompletedRunContext completion;
    completion.key = key;
    completion.gameplayRevision = core::ChartGameplayRevision(hardPayload);
    completion.songTitle = u8"Тестовая песня";
    completion.chartTitle = u8"Сложная";
    completion.practiceRate = 0.5;
    completion.scorableCount = judge.ScorableCount();
    app::RunResult captured;
    const auto history = [&]() { return disk.Load(key, error); };

    completion.editorTest = true;
    completion.launchedFromEditor = true;
    completion.completedThisFrame = true;
    judge.FinishRemainingMisses();
    completion.grades = judge.Counts();
    ok &= Check(!app::CaptureCompletedRun(disk, completion, captured, error) &&
                error.empty() && history().empty(),
                "Editor > TEST > completion does not write a result");

    // Return from TEST without destroying the editor's authoring session.
    app::NavigationInput backToEditor;
    backToEditor.requestedScreen = Screen::Editor;
    app::UpdateScreenFlow(nav, backToEditor, false);
    ok &= Check(nav.screen == Screen::Editor, "TEST > Editor");

    // Start from Home, select a difficulty and play. The pure navigation
    // policy is the same one consumed by main.cpp.
    app::NavigationInput home;
    home.requestedScreen = Screen::Home;
    app::UpdateScreenFlow(nav, home, false);
    app::NavigationInput select;
    select.startPressed = true;
    app::UpdateScreenFlow(nav, select, false);
    ok &= Check(nav.screen == Screen::SongSelect, "Home > Library");

    app::NavigationInput start;
    start.requestedScreen = Screen::Play;
    app::UpdateScreenFlow(nav, start, false);
    ok &= Check(nav.screen == Screen::Play, "Library > Play");

    // Actual two-key diagonal at 1000 ms and one left key after the
    // 2000 ms BPM transition. This path exercises the production judge.
    judge.SetOffsetSeconds(hardPayload.offsetSeconds);
    judge.BeginReplayCapture(50);
    const auto up = judge.PressHeading(core::Heading::Up, 1.000);
    const auto right = judge.PressHeading(core::Heading::Right, 1.007);
    const auto left = judge.PressHeading(core::Heading::Left, 3.000);
    ok &= Check(up && right && left &&
                up->grade == core::Grade::Perfect &&
                right->grade == core::Grade::Perfect &&
                left->grade == core::Grade::Perfect &&
                judge.ScorableCount() == 3,
                "Play > diagonal physical presses and later BPM note judged");
    judge.FinishRemainingMisses();
    completion.grades = judge.Counts();
    completion.scorableCount = judge.ScorableCount();
    completion.editorTest = false;
    completion.launchedFromEditor = false;
    completion.completedThisFrame = false;
    ok &= Check(!app::CaptureCompletedRun(disk, completion, captured, error) &&
                error.empty() && history().empty(),
                "unfinished Play does not write a result");

    completion.completedThisFrame = true;
    completion.statistics = judge.Statistics();
    completion.replay = judge.CompletedReplay();
    ok &= Check(completion.replay != nullptr, "normal Play records a complete trace");
    ok &= Check(app::CaptureCompletedRun(disk, completion, captured, error) &&
                error.empty() && !captured.id.empty() &&
                captured.notesJudged == static_cast<int>(judge.ScorableCount()) &&
                captured.grades.Resolved() == captured.notesJudged &&
                captured.practiceRate == 0.5 &&
                captured.difficultyId == hardPayload.difficultyId &&
                captured.gameplayRevision == completion.gameplayRevision,
                "Play > final judgement persists exactly one complete attempt");

    core::ReplayTrace restored;
    ok &= Check(captured.hasReplay && disk.LoadReplay(captured,restored,error) &&
                app::VerifyRunReplay(captured,restored,chart,completion.gameplayRevision,
                                     hardPayload.offsetSeconds),
                "Results > recorded replay survives disk roundtrip and rejudges");
    completion.spectator = true;
    completion.replay = nullptr;
    ok &= Check(!app::CaptureCompletedRun(disk,completion,captured,error) &&
                error.empty() && history().size() == 1,
                "Replay > completion cannot publish a new attempt");
    completion.spectator = false;
    nav.screen = Screen::Results;
    completion.completedThisFrame = false;
    ok &= Check(!app::CaptureCompletedRun(disk, completion, captured, error) &&
                error.empty() && history().size() == 1,
                "Results > repeated frame cannot duplicate attempt");

    // Opening and closing a saved Results page must remain read-only.
    app::NavigationInput back;
    back.backPressed = true;
    app::UpdateScreenFlow(nav, back, false);
    ok &= Check(nav.screen == Screen::SongSelect, "Results > Library");
    nav.screen = Screen::Results;
    ok &= Check(history().size() == 1 &&
                history().front().id == captured.id,
                "Library > historical Results does not create a second file");

    // Close/Esc guards come from the actual application navigation policy.
    app::NavigationInput close;
    close.closeRequested = true;
    app::UpdateScreenFlow(nav, close, false);
    ok &= Check(nav.quitConfirmation && nav.running,
                "close window opens confirmation instead of exiting");
    app::NavigationInput cancel;
    cancel.quitCanceled = true;
    app::UpdateScreenFlow(nav, cancel, false);
    ok &= Check(!nav.quitConfirmation && nav.running &&
                history().size() == 1,
                "cancel close leaves the attempt history unchanged");

    return ok ? 0 : 1;
}
