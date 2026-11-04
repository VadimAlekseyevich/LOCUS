#pragma once

namespace locus::core {

// One monotonic song-time domain shared by playback, future marker motion,
// and future judgements. Timestamps are supplied by the platform adapter,
// not derived from rendered frames.
class SongClock {
public:
    void Start(double monotonicSeconds) noexcept;
    void Stop() noexcept;

    bool Running() const noexcept;
    double SecondsAt(double monotonicSeconds) const noexcept;
    double BeatAt(double monotonicSeconds, double bpm,
                  double offsetSeconds = 0.0) const noexcept;

private:
    double startedAtSeconds_ = 0.0;
    bool running_ = false;
};

} // namespace locus::core
