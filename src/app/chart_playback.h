#pragma once

#include "app/local_music.h"
#include "app/playback_cursor_smoother.h"
#include "core/song_clock.h"
#include "core/tempo_map.h"

namespace locus::app {

// A chart uses audio time as its beat/judgement domain. Negative beat-zero
// offsets start a silent visual preroll before the song stream is started.
// No rendered frame count or input changes this clock.
class ChartPlayback {
public:
    explicit ChartPlayback(LocalMusic& audio) noexcept : audio_(audio) {}

    void Configure(double bpm, double offsetSeconds,
                   const std::vector<core::TempoMarker>& map = {},
                   int ticksPerBeat = 1) noexcept;
    void Start(double monotonicSeconds);
    // Editor-only seek: no scoring and no separate Play screen.
    bool StartAtAudioSeconds(double audioSeconds, double monotonicSeconds);
    void Pause(double monotonicSeconds);
    void Resume(double monotonicSeconds);
    void Stop();
    void Update(double monotonicSeconds);
    void SetSpeed(double fraction, double monotonicSeconds);
    double Speed() const noexcept { return speed_; }
    double OffsetSeconds() const noexcept { return offsetSeconds_; }

    bool Playing() const noexcept { return playing_; }
    bool Paused() const noexcept { return paused_; }
    bool Finished() const noexcept { return finished_; }
    double AudioSeconds(double monotonicSeconds) const noexcept;
    double Beat(double monotonicSeconds) const noexcept;
    // Smooth only the rendered position at slow practice rates. Judgement
    // continues to use AudioSeconds() from the real played audio stream.
    double VisualAudioSeconds(double monotonicSeconds) const noexcept;
    double BeatAtAudioSeconds(double audioSeconds) const noexcept {
        if (!tempoMap_.empty())
            return core::TempoBeatAtMs(tempoMap_, audioSeconds * 1000.0) *
                ticksPerBeat_;
        return (audioSeconds - offsetSeconds_) * bpm_ / 60.0;
    }

private:
    LocalMusic& audio_;
    core::SongClock clock_;
    mutable PlaybackCursorSmoother visualCursor_;
    double bpm_ = 120.0;
    std::vector<core::TempoMarker> tempoMap_;
    int ticksPerBeat_ = 1;
    double speed_ = 1.0;
    double offsetSeconds_ = 0.0;
    double startAudioSeconds_ = 0.0;
    bool audioStarted_ = false;
    bool playing_ = false;
    bool paused_ = false;
    bool finished_ = false;
};

} // namespace locus::app
