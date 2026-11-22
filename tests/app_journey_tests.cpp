#include "app/run_results.h"
#include "app/screen_flow.h"
#include "core/judgement.h"
#include "core/route.h"

#include <chrono>
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

    // The TEST view can use the same judge and chart, but is never an attempt.
    const core::Chart chart = core::MakeDemoChart();
    core::TurnJudge judge(chart);
    const app::RunKey key{"fixture-song-set", "Hard.locus.json"};
    app::CompletedRunContext completion;
    completion.key = key;
    completion.songTitle = u8"Тестовая песня";
    completion.chartTitle = u8"Сложная";
    completion.practiceRate = 0.5;
    completion.scorableCount = judge.ScorableCount();
    std::string error;
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

    // Real judgement: physical arrows count individually. A diagonal
    // contributes two notes, so ScorableCount is authoritative.
    judge.Reset();
    const auto first = chart.turns.front();
    const double firstTime = first.timeMs >= 0
        ? first.timeMs / 1000.0
        : first.beat * 60.0 / chart.bpm;
    judge.PressHeading(first.direction, firstTime);
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
    ok &= Check(app::CaptureCompletedRun(disk, completion, captured, error) &&
                error.empty() && !captured.id.empty() &&
                captured.notesJudged == static_cast<int>(judge.ScorableCount()) &&
                captured.grades.Resolved() == captured.notesJudged &&
                captured.practiceRate == 0.5,
                "Play > final judgement persists exactly one complete attempt");

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
