#include "app/editor_session.h"
#include "app/editor_audio_analysis.h"
#include <chrono>
#include "core/tempo_map.h"

#include "app/chart_playback.h"
#include "app/chart_editor_layout.h"
#include "app/game_session.h"
#include "app/local_music.h"
#include "core/judgement.h"

#include <algorithm>
#include <cmath>
#include <limits>
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
    // The shared transport may have been configured by another difficulty
    // with a different tick resolution. Project its SONG SECONDS into the
    // currently focused chart's own grid instead of calling playback_.Beat().
    const double songSeconds = playback_.Playing()
        ? playback_.AudioSeconds(now) : ui_.focusSongSeconds;
    controls.timelineViewBeat = std::isfinite(songSeconds)
        ? core::TempoBeatAtSeconds(editor_.Data().route,
            editor_.Data().offsetSeconds, songSeconds) *
            editor_.Data().route.ticksPerBeat
        : static_cast<double>(beforeBeat);
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
    if (input.speedPercent >= 1 && input.speedPercent <= 100) {
        ResetTempoTap();
        playback_.SetSpeed(input.speedPercent / 100.0, now);
        status_ = "PREVIEW SPEED " + std::to_string(input.speedPercent) + "%";
    } else if (action.speedIndex >= 0 && action.speedIndex < 5) {
        ResetTempoTap();
        playback_.SetSpeed(speeds[action.speedIndex], now);
        status_ = "PREVIEW SPEED CHANGED";
    }
    const bool selectionChanged = editor_.SelectedBeat() != beforeBeat;
    // Explicit navigation/seek ends the temporary exact-time focus anchor.
    // Otherwise pressing Copy or editing an already-selected note would
    // spuriously jump the stopped viewport to a rounded chart tick.
    const Rectangle noteRuler{detail::kTimelineX, detail::kTimelineY,
                              detail::kTimelineWidth, detail::kTimelineHeight};
    if (selectionChanged ||
        (input.mouseClick &&
         (detail::Hit(detail::kSongOverview, input.mouse) ||
          detail::Hit(noteRuler, input.mouse))) ||
        (ui_.draggingSongOverview && (input.mouseHeld || input.mouseReleased)))
        ui_.focusSongSeconds = std::numeric_limits<double>::quiet_NaN();
    if (selectionChanged) previewBeat_ = editor_.SelectedBeat();
    if (action.changedChart || selectionChanged) ResetTempoTap();
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
    metronome_.Reset();
    ResetTempoTap();
    if (!music_.Ready() || audioPath_.empty()) {
        status_ = "NO SONG - CLICK LOAD SONG / PRESS O";
        return;
    }
    // Preserve negative-time visual preroll for positive song offsets.
    // Space immediately after changing difficulty starts from the precise
    // shared cursor, not its rounded projection onto a different tick grid.
    const double songTime = std::isfinite(ui_.focusSongSeconds) &&
                            beat == editor_.SelectedBeat()
        ? ui_.focusSongSeconds : editor_.AudioSecondsAtBeat(beat);
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
    ui_.focusSongSeconds = std::numeric_limits<double>::quiet_NaN();
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
        : std::isfinite(ui_.focusSongSeconds)
            ? ui_.focusSongSeconds
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

void EditorSession::StopPreviewForTempoChange() {
    metronome_.Reset();
    if (!playback_.Playing() && !playback_.Finished()) return;
    playback_.Stop();
    ui_.focusSongSeconds = std::numeric_limits<double>::quiet_NaN();
    previewBeat_ = editor_.SelectedBeat();
    status_ = "PREVIEW STOPPED - TEMPO MAP CHANGED";
}

core::MetronomeClick EditorSession::UpdateMetronome(double now) noexcept {
    if (!ui_.metronomeEnabled || !playback_.Playing() || playback_.Finished()) {
        metronome_.Reset(); return core::MetronomeClick::None;
    }
    if (playback_.Paused()) return core::MetronomeClick::None;
    const auto& data=editor_.Data();
    return metronome_.Update(data.route,data.offsetSeconds,
                             playback_.AudioSeconds(now),playback_.Speed());
}

void EditorSession::ResetTempoTap() noexcept {
    tapTempo_.Reset();
    tapRevision_=music_.SourceRevision();
    ui_.tempoTapCount=0;
    ui_.tappedTempo.reset();
}
void EditorSession::RecordTempoTap(double now) {
    if (!playback_.Playing() || playback_.Paused()) {
        ui_.feedback="PLAY AUDIO BEFORE TAPPING"; ui_.feedbackError=true;
        return;
    }
    if (tapRevision_!=music_.SourceRevision()) ResetTempoTap();
    if (tapTempo_.Tap(playback_.AudioSeconds(now))) {
        ui_.tempoTapCount=tapTempo_.Count();
        ui_.tappedTempo=tapTempo_.Estimate();
        ui_.feedback.clear(); ui_.feedbackError=false;
    }
}
std::vector<core::TempoMarker> EditorSession::TappedTempoMap() const {
    if (!ui_.tappedTempo || tapRevision_!=music_.SourceRevision()) return {};
    auto markers=TempoMap();
    const auto& p=*ui_.tappedTempo;
    const auto pos=std::lower_bound(markers.begin(),markers.end(),p.timeMs,
        [](const core::TempoMarker& marker,std::int64_t time){return marker.timeMs<time;});
    if (pos!=markers.end() && pos->timeMs==p.timeMs) pos->bpm=p.bpm;
    else markers.insert(pos,p);
    return markers;
}
void EditorSession::UpdateAudioAnalysis() {
    if (tapRevision_!=music_.SourceRevision()) ResetTempoTap();
    const auto revision=music_.SourceRevision();
    if (waveformRevision_!=revision) {
        analysis_.reset();
        analysisFailed_=false;
        ui_.showTempoSuggestions=false;
    }
    if (waveformJob_.valid() &&
        waveformJob_.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
        try {
            auto result=waveformJob_.get();
            if (jobRevision_==revision) {
                analysis_=std::move(result);
                analysisFailed_=!analysis_;
            }
        } catch (...) {
            if (jobRevision_==revision) analysisFailed_=true;
        }
    }
    if (waveformRevision_!=revision && !waveformJob_.valid()) {
        waveformRevision_=revision;
        if (music_.Ready() && !music_.EncodedBytes().empty() && !audioPath_.empty()) {
            jobRevision_=revision;
            try {
                waveformJob_=std::async(std::launch::async,DecodeEditorAudio,
                    music_.EncodedBytes(),music_.SourceFormat());
            } catch (...) { analysisFailed_=true; }
        }
    }
    ui_.audioAnalysis=analysis_;
    // Aliasing shared_ptr keeps the waveform alive without copying its pyramid.
    ui_.waveform=analysis_?std::shared_ptr<const core::AudioWaveform>(
        analysis_,&analysis_->waveform):nullptr;
    ui_.audioAnalysisBusy=waveformJob_.valid() && music_.Ready();
    ui_.audioAnalysisFailed=analysisFailed_;
}

std::vector<core::TempoMarker> EditorSession::ProposedTempoMap(
    int index,bool replace) const {
    if (!analysis_ || waveformRevision_!=music_.SourceRevision() ||
        ui_.audioAnalysis!=analysis_ || !ui_.showTempoSuggestions) return {};
    const auto& proposals=analysis_->tempo;
    if (proposals.empty()) return {};
    if (replace) {
        std::vector<core::TempoMarker> markers;
        markers.reserve(proposals.size());
        for (const auto& p:proposals) markers.push_back({p.timeMs,p.bpm});
        return markers;
    }
    if (index<0 || static_cast<std::size_t>(index)>=proposals.size()) return {};
    const auto& p=proposals[static_cast<std::size_t>(index)];
    auto markers=TempoMap();
    const auto pos=std::lower_bound(markers.begin(),markers.end(),p.timeMs,
        [](const core::TempoMarker& marker,std::int64_t time){return marker.timeMs<time;});
    if (pos!=markers.end() && pos->timeMs==p.timeMs) pos->bpm=p.bpm;
    else markers.insert(pos,{p.timeMs,p.bpm});
    return markers;
}

void EditorSession::UpdatePreview(double now) {
    if (!playback_.Playing()) return;
    playback_.Update(now);
    if (playback_.Finished()) {
        ui_.focusSongSeconds = std::numeric_limits<double>::quiet_NaN();
        status_ = "PREVIEW FINISHED - SELECT BEAT OR R TO RESTART";
    } else if (!playback_.Paused()) {
        ui_.focusSongSeconds = std::numeric_limits<double>::quiet_NaN();
        const double beatNow = core::TempoBeatAtSeconds(
            editor_.Data().route, editor_.Data().offsetSeconds,
            playback_.AudioSeconds(now)) * editor_.Data().route.ticksPerBeat;
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
