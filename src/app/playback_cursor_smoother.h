#pragma once

#include <algorithm>
#include <cmath>

namespace locus::app {

// Presentation-only interpolation of raylib's chunked audio stream cursor.
// The real consumed-audio timestamp remains authoritative for judgement;
// this class is used only by visual playback (player, route and camera).
//
// A slow stream can report identical positions across several render frames,
// then jump forward at the next decoded audio buffer. Predict the position
// from elapsed wall time and playback rate *between* those observations.
// Treat small differences as cursor quantization, not as transport drift.
// Larger sustained differences are corrected gradually, never by snapping
// the avatar backwards or forwards at every audio buffer boundary.
class PlaybackCursorSmoother {
public:
    void Reset() noexcept {
        initialized_ = false;
        wasPaused_ = false;
        lastNow_ = lastSource_ = displayed_ = 0.0;
    }

    double Sample(double sourceSeconds, double now, double rate,
                  bool paused = false) noexcept {
        if (!std::isfinite(sourceSeconds) || !std::isfinite(now) ||
            !std::isfinite(rate) || rate <= 0.0) {
            Reset();
            return sourceSeconds;
        }
        if (!initialized_ || now < lastNow_ ||
            sourceSeconds < lastSource_ - 0.10) {
            initialized_ = true;
            lastNow_ = now;
            lastSource_ = displayed_ = sourceSeconds;
            wasPaused_ = paused;
            return displayed_;
        }

        const double dt = std::max(0.0, now - lastNow_);
        // A seek or a missed second of rendering is a transport discontinuity,
        // not an audio-buffer pulse to smear across subsequent frames.
        if (sourceSeconds - lastSource_ > std::max(0.40, rate * dt + 0.25)) {
            lastNow_ = now;
            lastSource_ = displayed_ = sourceSeconds;
            wasPaused_ = paused;
            return displayed_;
        }
        lastNow_ = now;
        lastSource_ = sourceSeconds;
        if (paused || wasPaused_) {
            wasPaused_ = paused;
            return displayed_;
        }

        const double expected = displayed_ + rate * dt;
        const double error = sourceSeconds - expected;
        // Audio buffering normally produces a ~20-60ms staircase in source
        // time. Following its raw error recreates that staircase visually.
        constexpr double kQuantizationSeconds = 0.060;
        constexpr double kCorrectionSeconds = 0.16;
        const double excess = std::max(0.0, std::abs(error) - kQuantizationSeconds);
        double correction = std::copysign(
            excess * (-std::expm1(-dt / kCorrectionSeconds)), error);
        // Corrections may speed up or slow down movement, but never cause a
        // backward visual step. Strong drift means the stream itself stalled.
        correction = std::clamp(correction, -0.90 * rate * dt,
                                0.90 * rate * dt);
        displayed_ = std::max(displayed_, expected + correction);
        return displayed_;
    }

private:
    bool initialized_ = false;
    bool wasPaused_ = false;
    double lastNow_ = 0.0;
    double lastSource_ = 0.0;
    double displayed_ = 0.0;
};

} // namespace locus::app
