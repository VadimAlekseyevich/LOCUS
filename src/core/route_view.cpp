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
    const GridPoint from = FromCell(step.from);
    const GridPoint to = FromCell(step.to);
    StepView view;

    if (!step.wraps) {
        view.segments[0] = {from, to};
        view.segmentCount = 1;
        view.marker = Interpolate(from, to, t);
        return view;
    }

    // A unit step spans one beat even across a boundary. Each visible half
    // spans half the beat. The non-drawn jump models a periodic 4x4 grid.
    GridPoint exit = from;
    GridPoint entry = to;
    switch (step.heading) {
        case Heading::Up:
            exit.y = -0.5;
            entry.y = 3.5;
            break;
        case Heading::Right:
            exit.x = 3.5;
            entry.x = -0.5;
            break;
        case Heading::Down:
            exit.y = 3.5;
            entry.y = -0.5;
            break;
        case Heading::Left:
            exit.x = -0.5;
            entry.x = 3.5;
            break;
    }
    view.segments[0] = {from, exit};
    view.segments[1] = {entry, to};
    view.segmentCount = 2;
    view.marker = (t < 0.5) ? Interpolate(from, exit, t * 2.0)
                             : Interpolate(entry, to, (t - 0.5) * 2.0);
    return view;
}


StepView TraceRouteStep(const RouteStep& step, double progress) noexcept {
    const double t = std::isfinite(progress) ? std::clamp(progress, 0.0, 1.0) : 0.0;
    const StepView full = ViewRouteStep(step, t);
    StepView trace;
    trace.marker = full.marker;
    if (t <= 0.0) return trace;

    if (!step.wraps) {
        trace.segments[0] = {full.segments[0].from, full.marker};
        trace.segmentCount = 1;
    } else if (t <= 0.5) {
        // The marker has crossed half a beat at the seam: reveal the whole
        // outgoing half, but not an artificial line across the board.
        trace.segments[0] = {
            full.segments[0].from,
            Interpolate(full.segments[0].from, full.segments[0].to, t * 2.0)
        };
        trace.segmentCount = 1;
    } else {
        trace.segments[0] = full.segments[0];
        trace.segments[1] = {full.segments[1].from, full.marker};
        trace.segmentCount = 2;
    }
    return trace;
}

} // namespace locus::core
