#include "app/session_transition.h"

#include "app/chart_editor_ui.h"
#include "app/chart_playback.h"
#include "app/game_session.h"
#include "app/local_music.h"
#include "core/judgement.h"
#include "core/route.h"

namespace locus::app {

void ApplySessionTransition(const SessionTransitionContext& context,
                            Screen previous, Screen next,
                            bool loadedNewChart, double now) {
    if (previous == next) return;

    if (next == Screen::Editor) {
        // Menu key presses may also be rebound gameplay directions. Re-arm
        // placement only after those held keys have been released.
        context.editorUi.pendingDirection = 0;
        context.editorUi.pendingDirectionSeconds = 0.0f;
        context.editorUi.directionKeysHandled = true;
    }
    // The Results screen owns the unmapped song outro. Only explicit
    // navigation AWAY from Play stops the underlying audio immediately.
    // For menu navigation the stream keeps playing through the short
    // output fade-out. Editor and Listen reuse the decoder and stop now.
    if ((previous == Screen::Play || previous == Screen::Results) &&
        (next == Screen::Editor || next == Screen::Listen))
        context.gameSession.Stop(context.customChart);
    if (previous == Screen::Listen)
        context.song.Stop();
    if (previous == Screen::Editor)
        context.chartPlayback.Stop();

    if (next == Screen::Play) {
        context.sessionCompleted = false;
        if (previous != Screen::Results && !loadedNewChart &&
            previous != Screen::Editor) {
            // Entering Play without a selected chart uses the original demo.
            // Replay from Results and TEST from Editor keep their chart.
            context.customChart = false;
            context.activeChart = context.demoChart;
            context.chartTitle = "BUILT-IN DEMO";
            context.gameSession.ClearTestOrigin();
            context.judge.SetOffsetSeconds(0.0);
        } else {
            context.judge.Reset();
        }
        context.gameSession.Start(context.customChart, now);
    } else if (next == Screen::Listen) {
        // Preview slowdown must never leak into ordinary music listening.
        context.song.SetSpeed(1.0, now);
        context.song.Start(now);
    }
}

} // namespace locus::app
