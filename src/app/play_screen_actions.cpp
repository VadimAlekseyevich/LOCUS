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
        context_.outro.Reset();
        context_.game.Restart(custom, now);
    } else if (input.pausePressed && !context_.outro.Active()) {
        context_.game.TogglePause(custom, now);
    }
}

void PlayScreenActions::Update(const FrameInput& input, double now) {
    const bool custom = context_.customChart;
    if (context_.outro.Active()) {
        // Stop accepting notes but keep music streaming during the departure.
        if (custom) context_.local.Update(now);
        else context_.demo.Update(now);
        if (context_.outro.Done(now)) {
            FinishSession(context_.app, context_.judge);
            context_.sessionCompleted = true;
            context_.outro.BeginResults(now);
        }
        return;
    }
    context_.game.Update(custom, now,
                         {input.arrowPressed, input.arrowCapturedAt, input.arrowHeld});
    const bool audioFinished = custom
        ? context_.local.Finished() : context_.demo.Finished();
    if (ChartReadyForResults(context_.judge, audioFinished))
        context_.outro.Begin(context_.chart, Transport(now).beat, now);
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
    // The audio device exposes a stepped cursor at reduced pitch. The
    // visual beat is interpolated for camera/marker rendering only; the
    // judge still reads raw consumed-audio timestamps from ChartPlayback.
    const double song = context_.local.VisualAudioSeconds(now);
    const bool finished = context_.local.Finished();
    const bool paused = context_.local.Paused();
    const char* status = finished ? "CHART FINISHED"
        : paused ? "PAUSED  |  PRESS PAUSE KEY TO RESUME"
        : song < 0.0 ? "SILENT PREROLL  |  ARROWS TO TURN"
                     : "CUSTOM SONG  |  ARROWS TO TURN";
    return {song, context_.local.BeatAtAudioSeconds(song), paused, finished,
            context_.local.Speed(), status};
}

} // namespace locus::app
