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

} // namespace locus::core
