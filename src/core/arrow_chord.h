#pragma once

#include "core/route.h"

#include <array>
#include <optional>

namespace locus::core {

struct ArrowKeys {
    // Indices retain the original order: Up, Right, Down, Left.
    std::array<bool, 4> pressed{};
    std::array<bool, 4> held{};
    std::array<double, 4> capturedAt{};
};

struct DirectionStroke {
    Heading outgoing = Heading::Up;
    double capturedAt = 0.0;
};

// Interprets two nearly simultaneous arrow key-downs as a single diagonal
// stroke, even when the key-downs arrive in consecutive game frames.
// A half-chord of the expected diagonal is buffered instead of marked wrong.
// No auto-repeat: a chord must contain at least one fresh press.
class ArrowChordTracker {
public:
    static constexpr double kMaxChordSkewSeconds = 0.085;
    void Reset() noexcept;
    std::optional<DirectionStroke> Observe(const ArrowKeys& input,
                                           Heading expected, Heading incoming) noexcept;
private:
    std::array<double, 4> downAt_{{-1.0, -1.0, -1.0, -1.0}};
};

} // namespace locus::core
