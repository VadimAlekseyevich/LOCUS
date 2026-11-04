#pragma once

#include "core/route.h"

#include <array>

namespace locus::core {

// Grid coordinates use the 4x4 cell centers at [0..3]. A wrapped step is
// clipped at the visual seam halfway outside the outermost cells.
struct GridPoint {
    double x = 0.0;
    double y = 0.0;
};

struct RouteLine {
    GridPoint from;
    GridPoint to;
};

struct StepView {
    GridPoint marker;
    std::array<RouteLine, 2> segments{};
    int segmentCount = 0;
};

// A single beat runs from progress 0 to 1. A Wrap travels half a visual
// cell to the edge, reappears at the opposite edge, then finishes at the
// destination. No drawn line crosses the middle of the board.
StepView ViewRouteStep(const RouteStep& step, double progress) noexcept;

} // namespace locus::core
