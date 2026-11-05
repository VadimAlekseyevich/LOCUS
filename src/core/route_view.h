#pragma once

#include "core/route.h"

#include <array>

namespace locus::core {

// World coordinates are unbounded. Editor grid dimensions are irrelevant
// to these positions. The camera translates the world, never rotates.
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

// A single beat runs from progress 0 to 1 on an uninterrupted line.
StepView ViewRouteStep(const RouteStep& step, double progress) noexcept;

// Only the portion traversed so far is visible. At progress zero the line
// has not grown; at one it matches the full authored step.
StepView TraceRouteStep(const RouteStep& step, double progress) noexcept;

// Floating world-space location of the moving marker. This is the camera
// anchor in PLAY, and the preview anchor of the scrolling grid in ROUTE.
GridPoint RoutePositionAt(const Chart& chart, double fractionalBeat);

} // namespace locus::core
