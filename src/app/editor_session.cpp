#include "app/editor_session.h"
#include "core/tempo_map.h"

#include "app/chart_playback.h"
#include "app/game_session.h"
#include "app/local_music.h"
#include "core/judgement.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace locus::app {

bool PrepareEditorTest(const core::LoadedChart& source, double audioSeconds,
                       double durationSeconds, EditorTestPlan& plan,
                       std::string& error) {
    core::LoadedChart checked = source;
    if (!core::PrepareChartForSong(checked, durationSeconds, error))
        return false;
    if (!std::isfinite(audioSeconds) || audioSeconds >= durationSeconds) {
        error = "SELECT A POSITION BEFORE SONG END";
        return false;
    }
    const double exactBeat = core::TempoBeatAtSeconds(
        checked.route, checked.offsetSeconds, audioSeconds) *
        checked.route.ticksPerBeat;
    if (!std::isfinite(exactBeat)) {
        error = "SELECTED POSITION IS INVALID";
        return false;
    }
    const int firstScored = std::clamp(
        static_cast<int>(std::ceil(exactBeat - 1e-6)),
        checked.route.firstBeat, checked.route.lengthBeats);
    plan = {std::move(checked), audioSeconds, exactBeat, firstScored};
    error.clear();
    return true;
}

EditorUiAction EditorSession::ApplyControls(const EditorControls& input,
                                            double now) {
    const int beforeBeat = editor_.SelectedBeat();
    const int beforeTicks = editor_.Data().route.ticksPerBeat;
    EditorControls controls = input;
    controls.timelineViewBeat = playback_.Playing()
        ? playback_.Beat(now) : static_cast<double>(beforeBeat);
    const EditorUiAction action = ApplyEditorControls(editor_, ui_, controls);

    if (editor_.Data().route.ticksPerBeat != beforeTicks) {
        // Snap resolution is a coordinate change, not an authored time edit.
        // Keep the existing preview origin on the same musical beat.
        const double songBeat = static_cast<double>(previewBeat_) / beforeTicks;
        previewBeat_ = std::clamp(
            static_cast<int>(std::llround(
                songBeat * editor_.Data().route.ticksPerBeat)),
            editor_.Data().route.firstBeat,
            editor_.Data().route.lengthBeats - 1);
    }
    constexpr double speeds[] = {1.0, 0.75, 0.5, 0.25, 0.1};
    if (action.speedIndex >= 0 && action.speedIndex < 5) {
        playback_.SetSpeed(speeds[action.speedIndex], now);
        status_ = "PREVIEW SPEED CHANGED - AUDIO PITCH WILL ALSO CHANGE";
    }
    const bool selectionChanged = editor_.SelectedBeat() != beforeBeat;
    if (selectionChanged) previewBeat_ = editor_.SelectedBeat();
    if ((action.changedChart || selectionChanged) &&
        (playback_.Playing() || playback_.Finished())) {
        playback_.Stop();
        status_ = "PREVIEW STOPPED - SPACE PLAYS FROM SELECTED BEAT";
    }
    if (action.previewRestart) {
        StartPreview(previewBeat_, now);
    } else if (action.previewToggle) {
        if (!playback_.Playing()) StartPreview(editor_.SelectedBeat(), now);
        else if (playback_.Paused()) playback_.Resume(now);
        else playback_.Pause(now);
    }
    return action;
}

void EditorSession::StartPreview(int beat, double now) {
    if (!music_.Ready() || audioPath_.empty()) {
        status_ = "NO SONG - CLICK LOAD SONG / PRESS O";
        return;
    }
    // Preserve negative-time visual preroll for positive song offsets.
    const double songTime = editor_.AudioSecondsAtBeat(beat);
    if (!std::isfinite(songTime) ||
        songTime >= music_.DurationSeconds()) {
        status_ = "PREVIEW FAILED: SELECTED BEAT / OFFSET IS AFTER SONG END";
        return;
    }
    playback_.Configure(editor_.Data().route.bpm,
                        editor_.Data().offsetSeconds,
                        editor_.Data().route.tempoMap,
                        editor_.Data().route.ticksPerBeat);
    if (!playback_.StartAtAudioSeconds(songTime, now)) {
        status_ = "PREVIEW FAILED: COULD NOT START AUDIO";
        return;
    }
    previewBeat_ = beat;
    status_ = "PREVIEW FROM BEAT " + std::to_string(beat) +
              " / SONG " + std::to_string(songTime) +
              "S - SPACE PAUSE / R RESTART";
}

bool EditorSession::BeginTest(EditorTestTarget target, double now,
                              std::string& error) {
    // Use the exact paused/current audio position, not the rounded UI tick.
    const double position = playback_.Playing()
        ? playback_.AudioSeconds(now)
        : editor_.AudioSecondsAtBeat(editor_.SelectedBeat());
    playback_.Stop();
    if (audioPath_.empty() || !music_.Ready()) {
        status_ = "TEST NEEDS AUDIO - CLICK LOAD SONG";
        return false;
    }
    EditorTestPlan plan;
    if (!PrepareEditorTest(editor_.Data(), position,
                           music_.DurationSeconds(), plan, error)) {
        status_ = "TEST FAILED: " + error;
        return false;
    }

    // Keep the Chart object identity: TurnJudge and GameSession borrow it.
    target.activeChart = std::move(plan.chart.route);
    target.title = std::move(plan.chart.title);
    playback_.Configure(target.activeChart.bpm,
                        plan.chart.offsetSeconds,
                        target.activeChart.tempoMap,
                        target.activeChart.ticksPerBeat);
    target.judge.SetOffsetSeconds(plan.chart.offsetSeconds);
    target.gameSession.SetTestOrigin(
        {plan.audioSeconds, plan.firstScoredBeat});
    target.customChart = true;
    status_ = "TEST FROM BEAT " + std::to_string(plan.exactBeat) +
              " - R RETRIES FROM HERE";
    error.clear();
    return true;
}

void EditorSession::UpdatePreview(double now) {
    if (!playback_.Playing()) return;
    playback_.Update(now);
    if (playback_.Finished()) {
        status_ = "PREVIEW FINISHED - SELECT BEAT OR R TO RESTART";
    } else if (!playback_.Paused()) {
        const double beatNow = playback_.Beat(now);
        const int lastBeat = editor_.Data().route.lengthBeats - 1;
        if (beatNow >= editor_.Data().route.lengthBeats) {
            playback_.Stop();
            editor_.SelectBeat(lastBeat);
            status_ = "PREVIEW REACHED CHART END - SELECT ANOTHER BEAT";
        } else {
            const int beat = std::clamp(
                static_cast<int>(std::floor(beatNow)),
                editor_.Data().route.firstBeat, lastBeat);
            editor_.SelectBeat(beat);
        }
    }
}

} // namespace locus::app
