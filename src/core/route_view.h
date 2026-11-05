#pragma once

#include "core/route.h"

#include <array>

namespace locus::core {

// Grid coordinates follow chart.gridWidth/gridHeight. A wrapped step is
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

// Only the portion traversed so far is visible. At progress zero the line
// has not grown; at one it matches the full authored step. Wrapped lines
// grow to the exit seam before continuing at the opposite entry seam.
StepView TraceRouteStep(const RouteStep& step, double progress) noexcept;

} // namespace locus::core
