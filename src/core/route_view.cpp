#include "core/route_view.h"

#include <algorithm>
#include <cmath>

namespace locus::core {
namespace {

GridPoint Interpolate(GridPoint from, GridPoint to, double amount) noexcept {
    return {from.x + (to.x - from.x) * amount,
            from.y + (to.y - from.y) * amount};
}

GridPoint FromCell(Cell cell) noexcept {
    return {static_cast<double>(cell.x), static_cast<double>(cell.y)};
}

} // namespace

StepView ViewRouteStep(const RouteStep& step, double progress) noexcept {
    const double t = std::isfinite(progress) ? std::clamp(progress, 0.0, 1.0) : 0.0;
    const GridPoint from = step.from;
    const GridPoint to = step.to;
    StepView view;
    // Every beat is a single continuous world-space segment, even outside
    // the editor's visible grid. A camera translates it; nothing teleports.
    view.segments[0] = {from, to};
    view.segmentCount = 1;
    view.marker = Interpolate(from, to, t);
    return view;
}

StepView TraceRouteStep(const RouteStep& step, double progress) noexcept {
    const double t = std::isfinite(progress) ? std::clamp(progress, 0.0, 1.0) : 0.0;
    const StepView full = ViewRouteStep(step, t);
    StepView trace;
    trace.marker = full.marker;
    if (t > 0.0) {
        trace.segments[0] = {full.segments[0].from, full.marker};
        trace.segmentCount = 1;
    }
    return trace;
}

GridPoint RoutePositionAt(const Chart& chart, double fractionalBeat) {
    if (chart.lengthBeats <= chart.firstBeat || !std::isfinite(fractionalBeat))
        return FromCell(chart.start);
    const double beat = std::clamp(fractionalBeat,
                                   static_cast<double>(chart.firstBeat),
                                   static_cast<double>(chart.lengthBeats));
    const int current = std::clamp(static_cast<int>(std::floor(beat)),
                                   chart.firstBeat, chart.lengthBeats - 1);
    const double progress = beat - current;
    return ViewRouteStep(SampleRouteStep(chart, current), progress).marker;
}

} // namespace locus::core
