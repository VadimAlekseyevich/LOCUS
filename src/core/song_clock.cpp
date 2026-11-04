#include "core/song_clock.h"

#include <algorithm>

namespace locus::core {

void SongClock::Start(double monotonicSeconds) noexcept {
    startedAtSeconds_ = monotonicSeconds;
    elapsedBeforePause_ = 0.0;
    running_ = true;
    paused_ = false;
}

void SongClock::Pause(double monotonicSeconds) noexcept {
    if (!running_ || paused_) return;
    elapsedBeforePause_ += std::max(0.0, monotonicSeconds - startedAtSeconds_);
    paused_ = true;
}

void SongClock::Resume(double monotonicSeconds) noexcept {
    if (!running_ || !paused_) return;
    startedAtSeconds_ = monotonicSeconds;
    paused_ = false;
}

void SongClock::Stop() noexcept {
    running_ = false;
    paused_ = false;
    startedAtSeconds_ = 0.0;
    elapsedBeforePause_ = 0.0;
}

bool SongClock::Running() const noexcept {
    return running_;
}

bool SongClock::Paused() const noexcept {
    return paused_;
}

double SongClock::SecondsAt(double monotonicSeconds) const noexcept {
    if (!running_) return 0.0;
    return elapsedBeforePause_ +
           (paused_ ? 0.0 : std::max(0.0, monotonicSeconds - startedAtSeconds_));
}

double SongClock::BeatAt(double monotonicSeconds, double bpm,
                         double offsetSeconds) const noexcept {
    return (SecondsAt(monotonicSeconds) - offsetSeconds) * (bpm / 60.0);
}

} // namespace locus::core
