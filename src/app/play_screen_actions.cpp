#include "app/play_screen_actions.h"

#include "app/app_runtime_policy.h"
#include "app/chart_playback.h"
#include "app/demo_transport.h"
#include "app/game_session.h"
#include "core/judgement.h"

namespace locus::app {

double PracticeRateForIndex(int index) noexcept {
    constexpr double speeds[] = {1.0, 0.75, 0.5, 0.25, 0.1};
    return index >= 0 && index < 5 ? speeds[index] : -1.0;
}

void PlayScreenActions::HandleControls(const FrameInput& input, double now) {
    const bool custom = context_.customChart;
    if (custom) {
        const double speed = PracticeRateForIndex(input.practiceSpeedIndex);
        if (speed > 0.0)
            context_.game.SetPracticeSpeed(custom, speed, now);
    }
    if (input.restartPressed ||
        (input.requestedScreen && *input.requestedScreen == Screen::Play)) {
        context_.sessionCompleted = false;
        context_.game.Restart(custom, now);
    } else if (input.pausePressed) {
        context_.game.TogglePause(custom, now);
    }
}

void PlayScreenActions::Update(const FrameInput& input, double now) {
    const bool custom = context_.customChart;
    context_.game.Update(custom, now,
                         {input.arrowPressed, input.arrowCapturedAt, input.arrowHeld});
    const bool audioFinished = custom
        ? context_.local.Finished() : context_.demo.Finished();
    if (ChartReadyForResults(context_.judge, audioFinished)) {
        FinishSession(context_.app, context_.judge);
        context_.sessionCompleted = true;
        // A final eligible note can finish before the audio track does.
        context_.game.Stop(custom);
    }
}

PlayTransportView PlayScreenActions::Transport(double now) const {
    if (!context_.customChart) {
        return {
            context_.demo.SongSeconds(now),
            context_.demo.Beat(now),
            context_.demo.Paused(),
            context_.demo.Finished(),
            1.0, context_.demo.Status()
        };
    }
    const double song = context_.local.AudioSeconds(now);
    const bool finished = context_.local.Finished();
    const bool paused = context_.local.Paused();
    const char* status = finished ? "CHART FINISHED"
        : paused ? "PAUSED  |  SPACE RESUME"
        : song < 0.0 ? "SILENT PREROLL  |  ARROWS TO TURN"
                     : "CUSTOM SONG  |  ARROWS TO TURN";
    return {song, context_.local.Beat(now), paused, finished,
            context_.local.Speed(), status};
}

} // namespace locus::app
