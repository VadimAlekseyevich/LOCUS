#include "app/session_transition.h"
#include "app/play_screen_actions.h"
#include "app/play_outro_transition.h"
#include "app/chart_editor_ui.h"
#include "app/chart_playback.h"
#include "app/demo_transport.h"
#include "app/game_session.h"
#include "app/local_music.h"
#include "core/judgement.h"
#include "core/route.h"

#include <iostream>
#include <string>

namespace {
bool Check(bool condition, const char* label) {
    if (!condition) std::cerr << "SESSION_TRANSITION_FAIL " << label << '\n';
    return condition;
}
} // namespace

int main() {
    using namespace locus;
    using app::Screen;
    bool ok = true;
    const core::Chart demoChart = core::MakeDemoChart();
    core::Chart activeChart = demoChart;
    core::TurnJudge judge(activeChart);
    app::DemoTransport demo;
    app::LocalMusic song;
    app::ChartPlayback preview(song);
    app::GameSession game(activeChart, judge, demo, preview);
    app::EditorUiState editorUi;
    bool customChart = false;
    bool completed = false;
    std::string title = "BUILT-IN DEMO";
    const app::SessionTransitionContext context{
        activeChart, demoChart, judge, game, preview, song, editorUi,
        customChart, completed, title
    };

    app::AppState resultsNavigation;
    resultsNavigation.screen = Screen::Results;
    app::NavigationInput returnToSelection;
    returnToSelection.backPressed = true;
    app::UpdateScreenFlow(resultsNavigation, returnToSelection, false);
    ok &= Check(resultsNavigation.screen == Screen::SongSelect,
                "Results Back returns to selected song menu listening");
    // TEST starts from Editor with the chart and test origin already selected.
    activeChart = demoChart;
    activeChart.turns = {{1, core::Turn::Right45}, {3, core::Turn::Left}};
    customChart = true;
    title = "CUSTOM TEST";
    game.SetTestOrigin({0.5, 1});
    app::ApplySessionTransition(context, Screen::Editor, Screen::Play, false, 10.0);
    ok &= Check(customChart && game.IsEditorTest() &&
                !completed && title == "CUSTOM TEST" &&
                activeChart.turns.size() == 2,
                "Editor to TEST preserves selected chart and practice origin");

    // Finishing gameplay must not erase the authoring document or change its
    // test origin. Results replay uses the same practice chart.
    completed = true;
    app::ApplySessionTransition(context, Screen::Play, Screen::Results,
                                false, 11.0);
    ok &= Check(completed && customChart && game.IsEditorTest(),
                "TEST to Results stops scoring without discarding test identity");
    judge.FinishRemainingMisses();
    ok &= Check(judge.Counts().Resolved() > 0, "fixture resolved chart notes");
    app::ApplySessionTransition(context, Screen::Results, Screen::Play,
                                false, 12.0);
    ok &= Check(!completed && customChart && game.IsEditorTest() &&
                title == "CUSTOM TEST" && judge.Counts().Resolved() == 0,
                "Results replay resets score but preserves tested difficulty");

    editorUi.pendingDirection = 5u;
    editorUi.pendingDirectionSeconds = 0.06f;
    editorUi.directionKeysHandled = false;
    app::ApplySessionTransition(context, Screen::Play, Screen::Editor,
                                false, 13.0);
    ok &= Check(editorUi.pendingDirection == 0 &&
                editorUi.pendingDirectionSeconds == 0.0f &&
                editorUi.directionKeysHandled && customChart &&
                title == "CUSTOM TEST",
                "returning to Editor clears pending input without losing chart");

    // Unselected Play from Home resets to the built-in demo and clears an old
    // TEST origin, without replacing the underlying chart reference.
    app::ApplySessionTransition(context, Screen::Editor, Screen::Home,
                                false, 14.0);
    app::ApplySessionTransition(context, Screen::Home, Screen::Play,
                                false, 15.0);
    ok &= Check(!customChart && !game.IsEditorTest() &&
                activeChart.turns.size() == demoChart.turns.size() &&
                title == "BUILT-IN DEMO" && !completed,
                "ordinary Play selects demo and clears stale test origin");
    app::ApplySessionTransition(context, Screen::Play, Screen::SongSelect,
                                false, 16.0);

    // Song library already loaded/validated a new chart before navigation.
    activeChart.turns = {{2, core::Turn::Left45}};
    customChart = true;
    title = "LIBRARY SONG";
    app::ApplySessionTransition(context, Screen::SongSelect, Screen::Play,
                                true, 17.0);
    ok &= Check(customChart && title == "LIBRARY SONG" &&
                activeChart.turns.size() == 1 && !game.IsEditorTest(),
                "Song Library to Play preserves newly loaded difficulty");

    const auto count = judge.Counts().Resolved();
    app::ApplySessionTransition(context, Screen::Play, Screen::Play,
                                false, 18.0);
    ok &= Check(customChart && title == "LIBRARY SONG" &&
                judge.Counts().Resolved() == count,
                "no screen boundary does not restart the session");
    // Play controls and status no longer depend on main's frame loop.
    app::AppState appState;
    app::PlayOutroTransition outro;
    app::PlayScreenActions play{
        {appState, game, judge, demo, preview, customChart, completed,
         activeChart, outro}
    };
    ok &= Check(app::PracticeRateForIndex(-1) < 0.0 &&
                app::PracticeRateForIndex(0) == 1.0 &&
                app::PracticeRateForIndex(4) == 0.1 &&
                app::PracticeRateForIndex(5) < 0.0,
                "practice presets reject out-of-range input");
    customChart = false;
    const auto demoView = play.Transport(20.0);
    ok &= Check(demoView.practiceSpeed == 1.0 &&
                demoView.status != nullptr,
                "demo transport snapshot never depends on custom audio");
    app::FrameInput noKeys;
    play.HandleControls(noKeys, 20.0);
    ok &= Check(appState.screen == Screen::Home,
                "idle play commands do not change navigation");
    customChart = true;
    const auto localView = play.Transport(21.0);
    ok &= Check(localView.practiceSpeed > 0.0 &&
                localView.status != nullptr,
                "custom transport snapshot available without audio device");
    game.Stop(customChart);
    if (!ok) return 1;
    std::cout << "SESSION_TRANSITION_PASS editor-test=1 results-replay=1 demo=1 library=1\n";
    return 0;
}
