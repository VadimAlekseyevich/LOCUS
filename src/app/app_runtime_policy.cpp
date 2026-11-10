#include "app/app_runtime_policy.h"

namespace locus::app {
// Platform-specific input is reduced to navigation actions. The screen
// transition policy stays independently testable without a window.
void UpdateState(AppState& state, const FrameInput& input, bool smokeWindow) {
    locus::app::UpdateScreenFlow(state, {
        input.closeRequested, input.requestedScreen,
        input.quitConfirmed, input.quitCanceled,
        input.startPressed, input.restartPressed,
        input.homePressed, input.backPressed,
        input.openEditorPressed, input.openSkinPressed,
        input.openSettingsPressed, input.openPlayPressed,
        input.chooseChartPressed,
        input.leaveRequested, input.levelExitConfirmed,
        input.levelExitCanceled
    }, smokeWindow);
}

// A scored chart ends when its LAST eligible note has been judged, not when
// the remaining song audio runs out. Empty charts and editor TEST positions
// after the last note have no scorable event: only these need the audio-end
// fallback. GameSession extends the scoring clock briefly past audio end
// so a final late hit or Miss is evaluated before showing Results.
bool ChartReadyForResults(const locus::core::TurnJudge& judge,
                          bool audioFinished) noexcept {
    // If there are scorable notes, all of them must actually be evaluated.
    // Audio can stop before the final note's 180ms Miss window expires;
    // GameSession keeps a silent judgement tail in that case.
    return judge.AllScorableJudged() ||
           (audioFinished && judge.ScorableCount() == 0);
}

void FinishSession(AppState& state, locus::core::TurnJudge& judge) {
    judge.FinishRemainingMisses();
    state.screen = Screen::Results;
}


} // namespace locus::app
