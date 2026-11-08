#pragma once

#include "app/movement_speed.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace locus::app {

// Visual-only time window in original song milliseconds.
inline constexpr float kPreviewStepMs = 50.0f;
inline constexpr float kPreviewMinMs = 50.0f;
inline constexpr float kPreviewMaxMs = 10000.0f;

inline float AdjustPreviewTimeMs(float current, int steps) noexcept {
    if (!std::isfinite(current)) return 800.0f;
    if (steps == 0) return current;
    const double next = static_cast<double>(current) +
                        static_cast<double>(steps) * kPreviewStepMs;
    return static_cast<float>(std::round(std::clamp(
        next, static_cast<double>(kPreviewMinMs),
        static_cast<double>(kPreviewMaxMs))));
}

// Matches the editor's held-bar navigation: 230ms delay, 75ms repeat,
// accelerating to 40ms after 1.2 seconds. The fresh key-down is separately
// polled: this class only emits subsequent repeats, independent of OS rate.
class HeldAdjustmentRepeat {
public:
    int Advance(int direction, float dtSeconds) noexcept {
        direction = std::clamp(direction, -1, 1);
        if (direction != direction_) {
            direction_ = direction;
            heldSeconds_ = 0.0f;
            repeatSeconds_ = 0.0f;
            return 0;
        }
        if (!direction || !std::isfinite(dtSeconds)) return 0;
        constexpr float delay = 0.23f;
        const float dt = std::clamp(dtSeconds, 0.0f, 0.1f);
        const float before = heldSeconds_;
        heldSeconds_ += dt;
        if (heldSeconds_ < delay) return 0;
        repeatSeconds_ += before < delay ? heldSeconds_ - delay : dt;
        const float interval = heldSeconds_ >= 1.2f ? 0.04f : 0.075f;
        int count = 0;
        while (repeatSeconds_ >= interval && count < 8) {
            repeatSeconds_ -= interval;
            ++count;
        }
        return direction * count;
    }

private:
    int direction_ = 0;
    float heldSeconds_ = 0.0f;
    float repeatSeconds_ = 0.0f;
};

class PlayAdjustmentRepeater {
public:
    std::pair<int, int> Advance(int speedHeld, int previewHeld,
                                float dtSeconds) noexcept {
        return {speed_.Advance(speedHeld, dtSeconds),
                preview_.Advance(previewHeld, dtSeconds)};
    }

private:
    HeldAdjustmentRepeat speed_;
    HeldAdjustmentRepeat preview_;
};
} // namespace locus::app
