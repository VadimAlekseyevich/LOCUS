#include "core/route.h"
#include "core/route_view.h"

#include <cmath>
#include <iostream>
#include <limits>

using namespace locus::core;

namespace {

bool Near(double a, double b) {
    return std::abs(a - b) < 1e-9;
}
bool PointIs(GridPoint p, double x, double y) {
    return Near(p.x, x) && Near(p.y, y);
}
bool Check(bool ok, const char* message) {
    if (!ok) std::cerr << "ROUTE_VIEW_FAIL: " << message << '\n';
    return ok;
}
bool CheckStraight(GridPoint from, GridPoint to, Heading heading) {
    const RouteStep route{from, to, heading, false};
    const StepView start = ViewRouteStep(route, 0.0);
    const StepView quarter = ViewRouteStep(route, 0.25);
    const StepView middle = ViewRouteStep(route, 0.5);
    const StepView done = ViewRouteStep(route, 1.0);
    const StepView trace = TraceRouteStep(route, 0.75);
    return start.segmentCount == 1 &&
           PointIs(start.segments[0].from, from.x, from.y) &&
           PointIs(start.segments[0].to, to.x, to.y) &&
           PointIs(start.marker, from.x, from.y) &&
           PointIs(quarter.marker, from.x + (to.x - from.x) * .25,
                   from.y + (to.y - from.y) * .25) &&
           PointIs(middle.marker, from.x + (to.x - from.x) * .5,
                   from.y + (to.y - from.y) * .5) &&
           PointIs(done.marker, to.x, to.y) &&
           trace.segmentCount == 1 &&
           PointIs(trace.segments[0].to, from.x + (to.x - from.x) * .75,
                   from.y + (to.y - from.y) * .75);
}

} // namespace

int main() {
    bool ok = true;
    const Chart demo = MakeDemoChart();
    const RouteStep straight = SampleRouteStep(demo, 0);
    ok &= Check(CheckStraight({1, 1}, {2, 1}, Heading::Right),
                "old straight step remains one line");

    ok &= Check(CheckStraight({3, 1}, {4, 1}, Heading::Right) &&
                CheckStraight({0, 2}, {-1, 2}, Heading::Left) &&
                CheckStraight({2, 0}, {2, -1}, Heading::Up) &&
                CheckStraight({1, 3}, {1, 4}, Heading::Down),
                "four former border crossings are continuous");

    const RouteStep oldWrapFlag{{3, 1}, {4, 1}, Heading::Right, true, 4, 4};
    const StepView flag = ViewRouteStep(oldWrapFlag, 0.5);
    ok &= Check(flag.segmentCount == 1 && PointIs(flag.marker, 3.5, 1),
                "legacy RouteStep wraps flag has no visual effect");

    // A normalized diagonal is still a straight continuous line.
    const double u = std::sqrt(0.5);
    ok &= Check(CheckStraight({3, 0}, {3 + u, -u}, Heading::UpRight) &&
                CheckStraight({7, 9}, {7 + u, 9 + u}, Heading::DownRight) &&
                CheckStraight({0, 0}, {-u, -u}, Heading::UpLeft) &&
                CheckStraight({0, 9}, {-u, 9 + u}, Heading::DownLeft),
                "all four unit-length diagonals interpolate without teleportation");

    const StepView noLine = TraceRouteStep(straight, 0.0);
    const StepView grown = TraceRouteStep(straight, 0.35);
    const StepView complete = TraceRouteStep(straight, 1.0);
    ok &= Check(noLine.segmentCount == 0 &&
                grown.segmentCount == 1 &&
                PointIs(grown.segments[0].from, 1, 1) &&
                PointIs(grown.segments[0].to, 1.35, 1) &&
                PointIs(grown.marker, 1.35, 1) &&
                complete.segmentCount == 1 &&
                PointIs(complete.segments[0].to, 2, 1),
                "progressive reveal is one continuous segment");

    ok &= Check(TraceRouteStep(straight, -2.0).segmentCount == 0 &&
                TraceRouteStep(straight, 3.0).segmentCount == 1 &&
                PointIs(ViewRouteStep(straight, 3.0).marker, 2, 1) &&
                PointIs(ViewRouteStep(straight, -3.0).marker, 1, 1) &&
                PointIs(ViewRouteStep(straight, std::numeric_limits<double>::quiet_NaN())
                            .marker, 1, 1),
                "fractional progress clamps and handles invalid fractions");

    const RouteStep crossing = SampleRouteStep(demo, 4);
    ok &= Check(crossing.from.y == 3 && crossing.to.y == 4 &&
                !crossing.wraps &&
                PointIs(ViewRouteStep(crossing, 0.5).marker, 3, 3.5),
                "demo crosses its old lower boundary smoothly");
    const RouteStep last = SampleRouteStep(demo, demo.lengthBeats - 1);
    ok &= Check(PointIs(ViewRouteStep(last, 1.0).marker, last.to.x, last.to.y),
                "finishing a song leaves the world position at the final cell");

    // The core camera anchor is precisely the interpolated beat position.
    ok &= Check(PointIs(RoutePositionAt(demo, 0.0), 1, 1) &&
                PointIs(RoutePositionAt(demo, 0.5), 1.5, 1) &&
                PointIs(RoutePositionAt(demo, 4.5), 3, 3.5) &&
                PointIs(RoutePositionAt(demo, demo.lengthBeats),
                        last.to.x, last.to.y),
                "camera follows fractional rhythm beats with no easing or rotation");

    Chart early;
    early.start = {0, 0};
    early.firstBeat = -4;
    early.lengthBeats = 8;
    early.initialHeading = Heading::Right;
    ok &= Check(PointIs(RoutePositionAt(early, -4), 0, 0) &&
                PointIs(RoutePositionAt(early, -3.5), .5, 0) &&
                PointIs(RoutePositionAt(early, -2), 2, 0),
                "camera includes negative song beats without jumping");

    Chart longRoute;
    longRoute.lengthBeats = 65536;
    longRoute.start = {0, 0};
    longRoute.initialHeading = Heading::UpRight;
    ok &= Check(PointIs(RoutePositionAt(longRoute, 65432.125),
                        65432.125 * u, -65432.125 * u) &&
                PointIs(RoutePositionAt(longRoute, 65536),
                        65536 * u, -65536 * u),
                "camera position stays precise far into a normalized diagonal song");

    if (!ok) return 1;
    std::cout << "ROUTE_VIEW_PASS infinite=1 interpolation=1 fixed-camera=1\n";
    return 0;
}
