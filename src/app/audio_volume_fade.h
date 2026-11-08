#pragma once

#include <algorithm>
#include <cmath>

namespace locus::app {

// C1-continuous ease-in/out: avoid clicks and linear volume ramps.
// Each source fades to zero before its replacement audibly fades in.
inline double SmoothAudioEase(double t) noexcept {
    if (!std::isfinite(t)) return 0.0;
    t = std::clamp(t, 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

inline constexpr double kAudioFadeOutSeconds = 0.12;
inline constexpr double kAudioFadeInSeconds = 0.16;

class AudioVolumeFade {
public:
    void FadeIn(double now, double delay = 0.0) noexcept {
        start_ = now + delay;
        duration_ = kAudioFadeInSeconds;
        from_ = 0.0f;
        to_ = 1.0f;
        enabled_ = true;
    }
    void FadeOut(double now) noexcept {
        const float current = Gain(now);
        start_ = now;
        duration_ = kAudioFadeOutSeconds;
        from_ = current;
        to_ = 0.0f;
        enabled_ = true;
    }
    void Shift(double pausedSeconds) noexcept {
        if (enabled_ && std::isfinite(pausedSeconds) && pausedSeconds > 0.0)
            start_ += pausedSeconds;
    }
    void Full() noexcept { enabled_ = false; from_ = to_ = 1.0f; }
    void Silent() noexcept { enabled_ = false; from_ = to_ = 0.0f; }

    float Gain(double now) const noexcept {
        if (!enabled_) return to_;
        if (now <= start_) return from_;
        const float s = static_cast<float>(
            SmoothAudioEase((now - start_) / duration_));
        return from_ + (to_ - from_) * s;
    }
    bool Finished(double now) const noexcept {
        return !enabled_ || now >= start_ + duration_;
    }
private:
    double start_ = 0.0;
    double duration_ = 1.0;
    float from_ = 0.0f;
    float to_ = 1.0f;
    bool enabled_ = false;
};

} // namespace locus::app
