#pragma once

#include "core/judgement.h"

#include <array>
#include <cmath>
#include <cstddef>

namespace locus::app {

struct HitErrorSample {
    double errorSeconds = 0.0;
    double capturedAt = 0.0; // monotonic presentation clock
    core::Grade grade = core::Grade::Perfect;
};

// Fixed-size visual history, not the persistent attempt or replay. Even a
// very dense input burst cannot allocate or extend the rendering workload.
class HitErrorHistory {
public:
    static constexpr std::size_t kCapacity = 64;
    static constexpr double kLifetimeSeconds = 1.25;
    void Reset() noexcept { next_ = count_ = 0; }
    void Record(const core::Judgement& hit, double now) noexcept {
        if (hit.grade == core::Grade::Miss || hit.wrongDirection ||
            !std::isfinite(now) || !std::isfinite(hit.timingErrorSeconds))
            return;
        samples_[next_] = {hit.timingErrorSeconds,now,hit.grade};
        next_ = (next_+1)%kCapacity;
        if (count_ < kCapacity) ++count_;
    }
    std::size_t Size() const noexcept { return count_; }
    // Oldest first so the newest mark is drawn on top.
    const HitErrorSample& At(std::size_t index) const noexcept {
        return samples_[(next_+kCapacity-count_+index)%kCapacity];
    }
    static float Opacity(const HitErrorSample& sample, double now) noexcept {
        const double age = now-sample.capturedAt;
        if (!std::isfinite(age) || age<0.0 || age>=kLifetimeSeconds)
            return 0.0f;
        return static_cast<float>(1.0-age/kLifetimeSeconds);
    }
private:
    std::array<HitErrorSample,kCapacity> samples_{};
    std::size_t next_ = 0;
    std::size_t count_ = 0;
};

} // namespace locus::app
