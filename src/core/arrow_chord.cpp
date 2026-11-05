#include "core/arrow_chord.h"

#include <algorithm>
#include <cmath>

namespace locus::core {

void ArrowChordTracker::Reset() noexcept {
    downAt_.fill(-1.0);
}

std::optional<DirectionStroke> ArrowChordTracker::Observe(
    const ArrowKeys& input, Heading expected, Heading incoming) noexcept {
    unsigned heldMask = 0, freshMask = 0;
    double latestFresh = -1.0;
    for (int i = 0; i < 4; ++i) {
        const unsigned bit = 1u << i;
        if (input.pressed[i] && std::isfinite(input.capturedAt[i])) {
            downAt_[i] = input.capturedAt[i];
            freshMask |= bit;
            latestFresh = std::max(latestFresh, input.capturedAt[i]);
        }
        if (input.held[i] || input.pressed[i]) heldMask |= bit;
        else downAt_[i] = -1.0;
    }
    if (!freshMask) return std::nullopt;

    // Up+Right, Right+Down, Down+Left or Left+Up: exactly two held
    // arrows with <=85ms between their fresh press times.
    if (const auto direction = HeadingFromArrowMask(heldMask);
        direction && IsDiagonal(*direction)) {
        int first = -1, second = -1;
        for (int i = 0; i < 4; ++i) {
            if (!(heldMask & (1u << i))) continue;
            if (first < 0) first = i;
            else second = i;
        }
        if (first >= 0 && second >= 0 &&
            downAt_[first] >= 0.0 && downAt_[second] >= 0.0 &&
            std::abs(downAt_[first] - downAt_[second]) <= kMaxChordSkewSeconds)
            return DirectionStroke{*direction,
                                   std::max(downAt_[first], downAt_[second])};
    }

    if (IsDiagonal(expected)) {
        const unsigned required = HeadingArrowMask(expected);
        // One key from an expected pair is not an incorrect note yet.
        if ((freshMask & ~required) == 0u &&
            (heldMask & ~required) == 0u) return std::nullopt;
    }

    // Invalid/contradictory simultaneous key combinations are a single wrong
    // stroke; never give two judgements or mistakenly grant a diagonal.
    if ((freshMask & (freshMask - 1u)) != 0u ||
        (heldMask & (heldMask - 1u)) != 0u)
        return DirectionStroke{incoming, latestFresh};

    const auto direction = HeadingFromArrowMask(freshMask);
    if (direction) return DirectionStroke{*direction, latestFresh};
    return DirectionStroke{incoming, latestFresh};
}

} // namespace locus::core
