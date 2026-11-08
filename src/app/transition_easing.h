#pragma once
#include <algorithm>
#include <cmath>
namespace locus::app {
// Shared C1-continuous, non-linear envelope for audio and animation.
inline double EaseSmoothstep(double progress) noexcept {
    if (!std::isfinite(progress)) return 0.0;
    const double t = std::clamp(progress, 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}
inline double EaseTime(double now, double start, double duration) noexcept {
    if (!std::isfinite(now) || !std::isfinite(start) ||
        !std::isfinite(duration) || duration <= 0.0) return 0.0;
    return EaseSmoothstep((now - start) / duration);
}
inline float EaseMix(float from, float to, double progress) noexcept {
    return from + (to - from) * static_cast<float>(EaseSmoothstep(progress));
}
} // namespace locus::app
