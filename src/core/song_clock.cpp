#include "core/song_clock.h"

#include <algorithm>

namespace locus::core {

void SongClock::Start(double monotonicSeconds) noexcept {
    startedAtSeconds_ = monotonicSeconds;
    running_ = true;
}

void SongClock::Stop() noexcept {
    running_ = false;
    startedAtSeconds_ = 0.0;
}

bool SongClock::Running() const noexcept {
    return running_;
}

double SongClock::SecondsAt(double monotonicSeconds) const noexcept {
    return running_ ? std::max(0.0, monotonicSeconds - startedAtSeconds_) : 0.0;
}

double SongClock::BeatAt(double monotonicSeconds, double bpm,
                         double offsetSeconds) const noexcept {
    return (SecondsAt(monotonicSeconds) - offsetSeconds) * (bpm / 60.0);
}

} // namespace locus::core
