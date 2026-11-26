#include "app/play_screen_actions.h"

#include "app/app_runtime_policy.h"
#include "app/chart_playback.h"
#include "app/demo_transport.h"
#include "app/game_session.h"
#include "core/judgement.h"

namespace locus::app {

bool PlayScreenActions::HandleControls(const FrameInput& input, double now) {
    const bool custom = context_.customChart;
    if (context_.app.quitConfirmation || context_.app.levelExitConfirmation || input.closeRequested || input.leaveRequested || input.homePressed) {
        dragging_=false;
        return false;
    }
    if (input.restartPressed ||
        (input.requestedScreen && *input.requestedScreen == Screen::Play)) {
        context_.sessionCompleted = false;
        context_.outro.Reset();
        context_.game.Restart(custom, now);
        dragging_=false;
        return true;
    } else if (input.pausePressed && !context_.outro.Active()) {
        context_.game.TogglePause(custom, now);
    }
    if (!context_.game.IsSpectator()) { dragging_=false; return false; }
    if (input.spectatorTimelinePressed) dragging_=true;
    if (dragging_) {
        dragFraction_=input.spectatorPointerFraction;
        if (input.spectatorPointerDown) return false;
        dragging_=false;
        if (context_.game.SeekSpectator(custom,dragFraction_*TimelineDuration(),now)) {
            context_.outro.Reset(); context_.sessionCompleted=false; return true;
        }
    } else if (input.spectatorSeekStep) {
        if (context_.game.SeekSpectator(custom,context_.game.RawSongSeconds(custom,now)+5.0*input.spectatorSeekStep,now)) {
            context_.outro.Reset(); context_.sessionCompleted=false; return true;
        }
    }
    return false;
}

double PlayScreenActions::TimelineSeconds(double now) const noexcept {
    return dragging_ ? dragFraction_*TimelineDuration() : context_.game.RawSongSeconds(context_.customChart,now);
}
double PlayScreenActions::TimelineDuration() const noexcept {
    return context_.game.DurationSeconds(context_.customChart);
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
    ArrowInput arrows{input.arrowPressed, input.arrowCapturedAt, input.arrowHeld};
    arrows.events = &input.directionEvents;
    arrows.precise = input.exactDirectionEvents;
    // Modal, retry and pause consume all coincident keys.
    if (input.pausePressed || input.restartPressed ||
        input.leaveRequested || input.homePressed || input.requestedScreen) {
        arrows.pressed.fill(false);
        arrows.events = nullptr;
        arrows.precise = true;
    }
    context_.game.Update(custom, now, arrows);
    if (context_.judge.Failed()) {
        context_.game.Stop(custom);
        context_.app.screen=Screen::Results;
        context_.sessionCompleted=false;
        return;
    }
    const bool audioFinished = custom
        ? context_.local.Finished() : context_.demo.Finished();
    // A spectator may seek through the whole song, even after its final note.
    if (context_.game.IsSpectator() && (!audioFinished || !context_.game.SpectatorEventsDone() || dragging_)) return;
    if (ChartReadyForResults(context_.judge, audioFinished)) {
        const auto transport = Transport(now);
        context_.outro.Begin(context_.judge.EffectiveChart(), transport.songSeconds,
                             transport.offsetSeconds, now);
    }
}

PlayTransportView PlayScreenActions::Transport(double now) const {
    if (!context_.customChart) {
        const double raw = context_.demo.SongSeconds(now);
        const double song = context_.game.CalibratedTime(raw, context_.demo.Speed());
        return {
            song,
            0.0, // Demo song offset (not a snap-grid beat).
            context_.demo.Paused(),
            context_.demo.Finished(),
            context_.demo.Speed(), context_.demo.Status(), song
        };
    }
    // The audio device exposes a stepped cursor at reduced pitch. The
    // visual beat is interpolated for camera/marker rendering only; the
    // judge still reads raw consumed-audio timestamps from ChartPlayback.
    const ChartClockFrame clock = context_.local.Snapshot(now);
    const double song = context_.game.CalibratedTime(
        clock.visualSeconds, clock.practiceRate);
    const bool finished = clock.finished;
    const bool paused = clock.paused;
    const char* status = finished ? "CHART FINISHED"
        : paused ? "PAUSED  |  PRESS PAUSE KEY TO RESUME"
        : song < 0.0 ? "SILENT PREROLL  |  ARROWS TO TURN"
                     : "CUSTOM SONG  |  ARROWS TO TURN";
    return {song, context_.local.OffsetSeconds(), paused, finished,
            clock.practiceRate, status,
            context_.game.CalibratedTime(
                clock.judgeSeconds, clock.practiceRate)};
}

} // namespace locus::app
