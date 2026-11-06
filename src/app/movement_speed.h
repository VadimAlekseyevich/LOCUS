#pragma once
#include <algorithm>
#include <cmath>

namespace locus::app {

// Movement speed is logical px/s on the virtual 540px playfield.
// It is independent of song tempo, snap resolution and playback rate.
inline constexpr float kDefaultBaseSpeed = 135.0f;
inline constexpr float kMinBaseSpeed = 20.0f;
inline constexpr float kMaxBaseSpeed = 700.0f;
inline constexpr float kSpeedHotkeyStep = 10.0f;

inline float AdjustBaseSpeed(float current, int direction) noexcept {
    if (!std::isfinite(current)) return kDefaultBaseSpeed;
    return std::clamp(current + kSpeedHotkeyStep * direction,
                      kMinBaseSpeed, kMaxBaseSpeed);
}

inline float StepForFixedSpeed(float speed, double bpm, int ticksPerBeat,
                               double practiceRate = 1.0) noexcept {
    if (!std::isfinite(speed) || speed <= 0.0f ||
        !std::isfinite(bpm) || bpm <= 0.0 ||
        ticksPerBeat <= 0 || !std::isfinite(practiceRate) ||
        practiceRate <= 0.0) return 0.0f;
    const double step = static_cast<double>(speed) * 60.0 /
                        (bpm * ticksPerBeat * practiceRate);
    return std::isfinite(step) ? static_cast<float>(step) : 0.0f;
}

} // namespace locus::app
