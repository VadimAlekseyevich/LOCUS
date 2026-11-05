#include "app/chart_playback.h"

#include <algorithm>
#include <cmath>

namespace locus::app {

void ChartPlayback::Configure(double bpm, double offsetSeconds) noexcept {
    Stop();
    bpm_ = bpm;
    offsetSeconds_ = offsetSeconds;
}

void ChartPlayback::SetSpeed(double fraction, double monotonicSeconds) {
    if (!std::isfinite(fraction) || fraction < 0.1 || fraction > 1.0) return;
    speed_ = fraction;
    clock_.SetRate(fraction, monotonicSeconds);
    audio_.SetSpeed(fraction, monotonicSeconds);
}

void ChartPlayback::Start(double monotonicSeconds) {
    // Preserve normal scored gameplay behavior at song zero / silent preroll.
    StartAtAudioSeconds(std::min(0.0, offsetSeconds_), monotonicSeconds);
}

bool ChartPlayback::StartAtAudioSeconds(double audioSeconds, double monotonicSeconds) {
    if (!audio_.Ready() || !std::isfinite(audioSeconds) ||
        audioSeconds >= audio_.DurationSeconds()) return false;
    Stop();
    audio_.SetSpeed(speed_, monotonicSeconds);
    startAudioSeconds_ = audioSeconds;
    clock_.Start(monotonicSeconds);
    playing_ = true;
    if (audioSeconds >= 0.0) {
        audio_.StartAt(monotonicSeconds, audioSeconds);
        audioStarted_ = true;
    }
    return true;
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
    finished_ = false;
    startAudioSeconds_ = 0.0;
}

double ChartPlayback::AudioSeconds(double monotonicSeconds) const noexcept {
    if (finished_) return audio_.DurationSeconds();
    // Negative audio-time coordinates provide silent visual preroll.
    // Positive offsets allow the song to run before chart beat zero.
    return startAudioSeconds_ + clock_.SecondsAt(monotonicSeconds);
}

double ChartPlayback::Beat(double monotonicSeconds) const noexcept {
    return (AudioSeconds(monotonicSeconds) - offsetSeconds_) * bpm_ / 60.0;
}

void ChartPlayback::Update(double monotonicSeconds) {
    if (!playing_ || paused_) return;
    const double audioTime = AudioSeconds(monotonicSeconds);
    if (audioTime >= audio_.DurationSeconds()) {
        Stop();
        finished_ = true;
        return;
    }
    if (!audioStarted_ && audioTime >= 0.0) {
        // Retain a small frame overshoot at the end of a negative preroll.
        audio_.StartAt(monotonicSeconds, audioTime);
        audioStarted_ = true;
    }
    if (audioStarted_) audio_.Update(monotonicSeconds);
    if (audio_.Finished()) {
        Stop();
        finished_ = true;
    }
}

} // namespace locus::app
