#include "app/chart_playback.h"

#include <algorithm>

namespace locus::app {

void ChartPlayback::Configure(double bpm, double offsetSeconds) noexcept {
    Stop();
    bpm_ = bpm;
    offsetSeconds_ = offsetSeconds;
}

void ChartPlayback::SetSpeed(double fraction, double monotonicSeconds) {
    if (fraction < 0.1 || fraction > 1.0) return;
    speed_ = fraction;
    clock_.SetRate(fraction, monotonicSeconds);
    audio_.SetSpeed(fraction, monotonicSeconds);
}

void ChartPlayback::Start(double monotonicSeconds) {
    Stop();
    if (!audio_.Ready()) return;
    finished_ = false;
    audio_.SetSpeed(speed_, monotonicSeconds);
    clock_.Start(monotonicSeconds);
    playing_ = true;
    if (offsetSeconds_ >= 0.0) {
        audio_.Start(monotonicSeconds);
        audioStarted_ = true;
    }
}

void ChartPlayback::Pause(double monotonicSeconds) {
    if (!playing_ || paused_) return;
    if (audioStarted_) audio_.Pause(monotonicSeconds);
    clock_.Pause(monotonicSeconds);
    paused_ = true;
}

void ChartPlayback::Resume(double monotonicSeconds) {
    if (!playing_ || !paused_) return;
    if (audioStarted_) audio_.Resume(monotonicSeconds);
    clock_.Resume(monotonicSeconds);
    paused_ = false;
}

void ChartPlayback::Stop() {
    if (audioStarted_) audio_.Stop();
    clock_.Stop();
    audioStarted_ = false;
    playing_ = false;
    paused_ = false;
}

double ChartPlayback::AudioSeconds(double monotonicSeconds) const noexcept {
    if (finished_) return audio_.DurationSeconds();
    // Negative audio-time coordinates provide silent visual preroll.
    // Positive offsets allow the song to run before chart beat zero.
    return clock_.SecondsAt(monotonicSeconds) + std::min(0.0, offsetSeconds_);
}

double ChartPlayback::Beat(double monotonicSeconds) const noexcept {
    return (AudioSeconds(monotonicSeconds) - offsetSeconds_) * bpm_ / 60.0;
}

void ChartPlayback::Update(double monotonicSeconds) {
    if (!playing_ || paused_) return;
    const double audioTime = AudioSeconds(monotonicSeconds);
    if (!audioStarted_ && audioTime >= 0.0) {
        audio_.Start(monotonicSeconds);
        audioStarted_ = true;
    }
    if (audioStarted_) audio_.Update(monotonicSeconds);
    if (audioTime >= audio_.DurationSeconds() || audio_.Finished()) {
        Stop();
        finished_ = true;
    }
}

} // namespace locus::app
