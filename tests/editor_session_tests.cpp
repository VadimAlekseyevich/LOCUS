#include "app/editor_session.h"
#include "app/editor_screen_actions.h"
#include "app/chart_playback.h"
#include "app/demo_transport.h"
#include "app/game_session.h"
#include "app/local_music.h"
#include "core/judgement.h"
#include "core/route.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <string>

namespace {
bool Check(bool value, const char* message) {
    if (!value) std::cerr << "EDITOR_SESSION_FAIL " << message << '\n';
    return value;
}
} // namespace

int main() {
    using namespace locus;
    bool ok = true;
    core::LoadedChart source;
    source.route = core::MakeDemoChart();
    source.title = "Practice fixture";
    source.audioPath = "song.wav";
    source.offsetSeconds = 0.25;

    app::EditorTestPlan plan;
    plan.chart.title = "UNMODIFIED";
    std::string error;
    ok &= Check(app::PrepareEditorTest(source, 2.0, 24.0, plan, error) &&
                error.empty() &&
                plan.chart.title == "Practice fixture" &&
                std::abs(plan.exactBeat - 3.5) < 1e-8 &&
                plan.firstScoredBeat == 4 &&
                plan.audioSeconds == 2.0,
                "TEST maps audio timestamp to next scored turn without rounding audio");
    const auto validPlan = plan.chart.title;
    ok &= Check(!app::PrepareEditorTest(
                    source, 2.0, 0.0, plan, error) &&
                !error.empty() && plan.chart.title == validPlan,
                "invalid audio duration leaves the prepared test plan intact");
    ok &= Check(!app::PrepareEditorTest(
                    source, 24.0, 24.0, plan, error) &&
                !error.empty() && plan.chart.title == validPlan,
                "TEST at song end is invalid without mutating plan");
    ok &= Check(!app::PrepareEditorTest(
                    source, std::numeric_limits<double>::quiet_NaN(),
                    24.0, plan, error) && !error.empty(),
                "NaN playhead cannot enter scored TEST");

    core::ChartEditor editor;
    app::EditorUiState ui;
    app::LocalMusic music;
    app::ChartPlayback playback(music);
    std::wstring editorAudioPath;
    int previewOrigin = 0;
    std::string status;
    app::EditorSession session{
        editor, ui, playback, music, editorAudioPath, previewOrigin, status
    };

    // No sound device exists in CTest. Preview must reject rather than
    // starting a fake stream or moving the current editor timeline.
    app::EditorControls toggle;
    toggle.previewToggle = true;
    const auto controls = session.ApplyControls(toggle, 1.0);
    ok &= Check(!controls.chooseAudio && !playback.Playing() &&
                status == "NO SONG - CLICK LOAD SONG / PRESS O" &&
                previewOrigin == 0,
                "preview without attached audio leaves timeline and decoder untouched");

    const core::Chart stableChart = core::MakeDemoChart();
    core::Chart activeChart = stableChart;
    core::TurnJudge judge(activeChart);
    app::DemoTransport demo;
    app::GameSession game(activeChart, judge, demo, playback);
    game.SetTestOrigin({3.0, 4});
    bool customChart = false;
    std::string title = "EXISTING CHART";
    app::EditorTestTarget target{activeChart, title, judge, game, customChart};
    ok &= Check(!session.BeginTest(target, 3.0, error) &&
                status == "TEST NEEDS AUDIO - CLICK LOAD SONG" &&
                activeChart.turns.size() == stableChart.turns.size() &&
                title == "EXISTING CHART" && !customChart &&
                game.IsEditorTest(),
                "TEST without audio preserves active scoring state");

    // A snapped editor selection is the preview's restart origin. Merely
    // changing the UI without a loaded song must never begin a stream.
    editor.SelectBeat(5);
    session.ApplyControls({}, 4.0);
    ok &= Check(previewOrigin == 0 || previewOrigin == 5,
                "idle update cannot make preview position invalid");
    session.UpdatePreview(4.5);
    ok &= Check(!playback.Playing(), "editor transport stays stopped on idle tick");
    // Editor file actions have a pure priority policy, testable without UI.
    app::FrameInput command{};
    app::EditorUiAction action{};
    ok &= Check(app::ChooseEditorCommand(command, action) ==
                    app::EditorFileCommand::None,
                "idle editor frame performs no file operation");
    action.chooseAudio = true;
    ok &= Check(app::ChooseEditorCommand(command, action) ==
                    app::EditorFileCommand::AttachAudio,
                "LOAD SONG requests audio attachment");
    action = {};
    action.testPlay = true;
    command.editorSave = true;
    ok &= Check(app::ChooseEditorCommand(command, action) ==
                    app::EditorFileCommand::Save,
                "save wins over simultaneous TEST button");
    command.editorSaveAs = true;
    ok &= Check(app::ChooseEditorCommand(command, action) ==
                    app::EditorFileCommand::SaveAs,
                "Save As is a distinct operation");
    command.editorNew = true;
    ok &= Check(app::ChooseEditorCommand(command, action) ==
                    app::EditorFileCommand::NewDraft,
                "New Draft takes precedence over competing shortcuts");
    command = {};
    ok &= Check(app::ChooseEditorCommand(command, action) ==
                    app::EditorFileCommand::TestPlay,
                "editor TEST command is dispatched independently");
    if (!ok) return 1;
    std::cout << "EDITOR_SESSION_PASS test-plan=1 preview=1 failed-test=1\n";
    return 0;
}
