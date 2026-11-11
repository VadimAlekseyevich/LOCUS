#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace locus::app {

// Global output latency: positive milliseconds mean the audible sound
// arrives AFTER the game transport's reported time. Shift presentation
// and judgement together; never write this into chart/song timestamps.
inline double CalibratedSongSeconds(double rawSongSeconds,
                                    int globalOffsetMs,
                                    double playbackRate = 1.0) noexcept {
    // The setting is wall-clock output latency. At 50% playback, 200 ms
    // of actual output delay corresponds to 100 ms of SONG time.
    return rawSongSeconds -
        static_cast<double>(globalOffsetMs) / 1000.0 * playbackRate;
}

// Headless timing model. A 100-BPM click sounds on EVERY quarter beat.
// Accept any freshly pressed bound gameplay direction key. Users can begin
// tapping on any beat: no countdown, warm-up or strong-beat requirement.
// All measurements use the raw (not already calibrated) song clock.
class GlobalOffsetCalibrator {
public:
    static constexpr int kRequiredTaps = 18;
    static constexpr double kBpm = 100.0;
    static constexpr double kBeatSeconds = 60.0 / kBpm;
    // Nearly two minutes of synthesized metronome for starting at any point.
    static constexpr int kTotalQuarterBeats = 192;

    void Start() noexcept {
        errors_.fill(0.0);
        count_ = 0;
        lastBeat_ = -1;
        lastAcceptedSeconds_ = -1000.0;
        complete_ = false;
        reliable_ = false;
        suggestedMs_ = 0;
        jitterMs_ = 0;
    }

    // Compare every tap to the closest 100-BPM beat. Ignore extra presses
    // within the same beat and very rapid repeats/key combinations; do not
    // reset progress for skipped beats or a late start.
    bool Tap(double songSeconds) noexcept {
        if (complete_ || !std::isfinite(songSeconds) || songSeconds < 0.0 ||
            songSeconds < lastAcceptedSeconds_ + 0.25) return false;
        const auto beat = static_cast<int>(
            std::llround(songSeconds / kBeatSeconds));
        if (beat < 0 || beat >= kTotalQuarterBeats || beat <= lastBeat_)
            return false;
        const double ms = (songSeconds - beat * kBeatSeconds) * 1000.0;
        // At 100 BPM a half-beat is 300ms. A wider window would attribute
        // a press to the wrong click; >300ms latency needs manual entry.
        if (std::abs(ms) > 280.0) return false;
        lastBeat_ = beat;
        lastAcceptedSeconds_ = songSeconds;
        errors_[static_cast<std::size_t>(count_++)] = ms;
        if (count_ == kRequiredTaps) Finish();
        return true;
    }

    int Count() const noexcept { return count_; }
    bool Complete() const noexcept { return complete_; }
    bool Reliable() const noexcept { return complete_ && reliable_; }
    int SuggestedMs() const noexcept { return suggestedMs_; }
    int JitterMs() const noexcept { return jitterMs_; }

private:
    static double Median(std::array<double, kRequiredTaps> data,
                         int count) noexcept {
        std::sort(data.begin(), data.begin() + count);
        return count % 2 ? data[static_cast<std::size_t>(count / 2)]
            : (data[static_cast<std::size_t>(count / 2 - 1)] +
               data[static_cast<std::size_t>(count / 2)]) * 0.5;
    }

    void Finish() noexcept {
        complete_ = true;
        const double baseline = Median(errors_, count_);
        std::array<double, kRequiredTaps> distances{};
        for (int i = 0; i < count_; ++i)
            distances[static_cast<std::size_t>(i)] =
                std::abs(errors_[static_cast<std::size_t>(i)] - baseline);
        const double mad = Median(distances, count_);
        const double tolerance = std::max(55.0, mad * 2.5);
        std::array<double, kRequiredTaps> inliers{};
        int kept = 0;
        for (int i = 0; i < count_; ++i) {
            if (distances[static_cast<std::size_t>(i)] <= tolerance)
                inliers[static_cast<std::size_t>(kept++)] =
                    errors_[static_cast<std::size_t>(i)];
        }
        if (kept < 13) return;
        const double location = Median(inliers, kept);
        distances.fill(0.0);
        for (int i = 0; i < kept; ++i)
            distances[static_cast<std::size_t>(i)] =
                std::abs(inliers[static_cast<std::size_t>(i)] - location);
        jitterMs_ = static_cast<int>(
            std::lround(Median(distances, kept)));
        if (jitterMs_ > 60) return;
        suggestedMs_ = std::clamp(
            static_cast<int>(std::lround(location)), -1000, 1000);
        reliable_ = true;
    }

    std::array<double, kRequiredTaps> errors_{};
    int count_ = 0;
    int lastBeat_ = -1;
    double lastAcceptedSeconds_ = -1000.0;
    int suggestedMs_ = 0;
    int jitterMs_ = 0;
    bool complete_ = false;
    bool reliable_ = false;
};

} // namespace locus::app
