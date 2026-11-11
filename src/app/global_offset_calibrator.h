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

// Headless timing model. The actual accented 120-BPM click track is the
// existing MakeDemoClickWav/DemoTransport (accent every fourth quarter note).
// The first two strong beats are warm-up, then SPACE taps measure audible
// latency against the transport's unmodified song clock.
class GlobalOffsetCalibrator {
public:
    static constexpr int kRequiredTaps = 8;
    static constexpr double kBpm = 120.0;
    static constexpr double kStrongBeatSeconds = 2.0;
    static constexpr int kWarmupStrongBeats = 2;
    static constexpr int kTotalQuarterBeats = 56;

    void Start() noexcept {
        errors_.fill(0.0);
        count_ = 0;
        lastStrongBeat_ = -1;
        complete_ = false;
        reliable_ = false;
        suggestedMs_ = 0;
        jitterMs_ = 0;
    }

    // A tap contributes once per downbeat, within +/-350 ms. No automatic
    // nearest ordinary click: the player must tap the ACCENT (every 4th beat).
    bool Tap(double songSeconds) noexcept {
        if (complete_ || !std::isfinite(songSeconds)) return false;
        const auto strong = static_cast<int>(
            std::llround(songSeconds / kStrongBeatSeconds));
        if (strong < kWarmupStrongBeats ||
            strong >= kTotalQuarterBeats / 4 ||
            strong <= lastStrongBeat_) return false;
        const double ms = (songSeconds -
                           strong * kStrongBeatSeconds) * 1000.0;
        if (std::abs(ms) > 350.0) return false;
        lastStrongBeat_ = strong;
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
        const double tolerance = std::max(65.0, mad * 2.5);
        std::array<double, kRequiredTaps> inliers{};
        int kept = 0;
        for (int i = 0; i < count_; ++i) {
            if (distances[static_cast<std::size_t>(i)] <= tolerance)
                inliers[static_cast<std::size_t>(kept++)] =
                    errors_[static_cast<std::size_t>(i)];
        }
        if (kept < 6) return;
        const double location = Median(inliers, kept);
        distances.fill(0.0);
        for (int i = 0; i < kept; ++i)
            distances[static_cast<std::size_t>(i)] =
                std::abs(inliers[static_cast<std::size_t>(i)] - location);
        jitterMs_ = static_cast<int>(
            std::lround(Median(distances, kept)));
        if (jitterMs_ > 70) return;
        suggestedMs_ = std::clamp(
            static_cast<int>(std::lround(location)), -1000, 1000);
        reliable_ = true;
    }

    std::array<double, kRequiredTaps> errors_{};
    int count_ = 0;
    int lastStrongBeat_ = -1;
    int suggestedMs_ = 0;
    int jitterMs_ = 0;
    bool complete_ = false;
    bool reliable_ = false;
};

} // namespace locus::app
