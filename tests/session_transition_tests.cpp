#include "app/session_transition.h"
#include "app/play_screen_actions.h"
#include "app/play_intro_skip.h"
#include "app/play_outro_transition.h"
#include "app/chart_editor_ui.h"
#include "app/chart_playback.h"
#include "app/demo_transport.h"
#include "app/game_session.h"
#include "app/local_music.h"
#include "core/judgement.h"
#include "core/route.h"

#include <iostream>
#include <cmath>
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
    // Only forward skip while in an unmapped intro; a 4/4 whole-measure
    // lead must be at least one second, and note timeMs beats cached ticks.
    core::Chart intro;
    intro.bpm = 120.0;
    intro.turns = {{12, core::Heading::Up, 11000}};
    const auto oneBar = app::IntroSkipDestination(intro, 0.0, 1.0);
    ok &= Check(oneBar && std::abs(*oneBar - 9.0) < 1e-9,
                "120 BPM seeks to one 2-second measure before first note");
    intro.bpm = 250.0;
    intro.turns.front().timeMs = 20000;
    const auto twoBars = app::IntroSkipDestination(intro, 0.0, 1.0);
    ok &= Check(twoBars && std::abs(*twoBars - 18.08) < 1e-9,
                "250 BPM needs two 0.96-second measures (1.92 seconds)");
    ok &= Check(!app::IntroSkipDestination(intro, 0.0, 18.1) &&
                !app::IntroSkipDestination(intro, 0.0, 19.9),
                "skip never seeks backward or steals near-note input");
    intro.turns.front().timeMs = 1000;
    ok &= Check(!app::IntroSkipDestination(intro, 0.0, 0.2),
                "no skip if the lead-in cannot fit before song zero");
    // Across a BPM change, the skipped interval is a whole number of
    // musical measures, not a fixed number of seconds at the first BPM.
    intro.bpm = 120.0;
    intro.tempoMap = {{0, 120.0}, {8000, 240.0}};
    intro.turns = {{1, core::Heading::Up, 8500}};
    const auto variableTempo = app::IntroSkipDestination(intro, 0.0, 0.5);
    ok &= Check(variableTempo &&
                std::abs(*variableTempo - 7.0) < 1e-6,
                "intro skip integrates tempo changes while preserving a whole measure");
    intro.tempoMap.clear();
    intro.turns.clear();
    ok &= Check(!app::IntroSkipDestination(intro, 0.0, 0.0),
                "no notes means no intro skip");

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
    app::AppState editFromResults;
    editFromResults.screen = Screen::Results;
    app::NavigationInput editShortcut;
    editShortcut.openEditorPressed = true;
    app::UpdateScreenFlow(editFromResults, editShortcut, false);
    ok &= Check(editFromResults.screen == Screen::Editor,
                "E from Results opens Editor instead of replaying");
    // TEST starts from Editor with the chart and test origin already selected.
    activeChart = demoChart;
    activeChart.turns = {{1, core::Heading::DownRight}, {3, core::Heading::Up}};
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

    app::ApplySessionTransition(context, Screen::Play, Screen::Editor,
                                false, 13.0);
    ok &= Check(customChart && title == "CUSTOM TEST",
                "returning to Editor retains chart without keyboard-note state");

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
    activeChart.turns = {{2, core::Heading::UpRight}};
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
