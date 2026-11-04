#pragma once

namespace locus::core {

// One monotonic song-time domain shared by playback, marker and judgement.
// Pauses freeze this domain; resumed playback excludes the pause duration.
class SongClock {
public:
    void Start(double monotonicSeconds) noexcept;
    void Pause(double monotonicSeconds) noexcept;
    void Resume(double monotonicSeconds) noexcept;
    void Stop() noexcept;

    bool Running() const noexcept;
    bool Paused() const noexcept;
    double SecondsAt(double monotonicSeconds) const noexcept;
    double BeatAt(double monotonicSeconds, double bpm,
                  double offsetSeconds = 0.0) const noexcept;

private:
    double startedAtSeconds_ = 0.0;
    double elapsedBeforePause_ = 0.0;
    bool running_ = false;
    bool paused_ = false;
};

} // namespace locus::core
