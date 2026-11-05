#pragma once

#include "core/judgement.h"

#include <cmath>

namespace locus::app {

// Visual feedback only. It never changes chart timing, scoring or the route.
// Wall-clock time keeps the burst short at every practice playback speed.
struct HitFlashFrame {
    float progress = 0.0f;   // 0 at hit, 1 when the pulse ends.
    float intensity = 0.0f;  // Fades smoothly to zero.
};

class HitFlash {
public:
    static constexpr double kDurationSeconds = 0.18;

    void Reset() noexcept {
        startedAt_ = -1.0;
        strength_ = 0.0f;
    }

    void Trigger(core::Grade grade, double now) noexcept {
        if (grade == core::Grade::Miss || !std::isfinite(now)) return;
        startedAt_ = now;
        strength_ = grade == core::Grade::Perfect ? 1.0f
                  : grade == core::Grade::Good ? 0.75f : 0.42f;
    }

    HitFlashFrame Sample(double now) const noexcept {
        if (startedAt_ < 0.0 || !std::isfinite(now) || now < startedAt_)
            return {};
        const double elapsed = (now - startedAt_) / kDurationSeconds;
        if (elapsed >= 1.0) return {};
        const float progress = static_cast<float>(elapsed);
        const float remaining = 1.0f - progress;
        return {progress, strength_ * remaining * remaining};
    }

private:
    double startedAt_ = -1.0;
    float strength_ = 0.0f;
};

} // namespace locus::app
