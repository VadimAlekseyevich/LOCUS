#include "core/route.h"
#include "core/route_view.h"

#include <cmath>
#include <iostream>

using namespace locus::core;

namespace {

bool Near(double a, double b) {
    return std::abs(a - b) < 1e-9;
}

bool PointIs(GridPoint point, double x, double y) {
    return Near(point.x, x) && Near(point.y, y);
}

bool Check(bool condition, const char* description) {
    if (!condition) {
        std::cerr << "ROUTE_VIEW_FAIL: " << description << '\n';
    }
    return condition;
}

bool CheckWrap(Heading heading, Cell from, Cell to,
               GridPoint exit, GridPoint entry) {
    const RouteStep route{from, to, heading, true};
    const StepView start = ViewRouteStep(route, 0.0);
    const StepView quarter = ViewRouteStep(route, 0.25);
    const StepView middle = ViewRouteStep(route, 0.5);
    const StepView threeQuarters = ViewRouteStep(route, 0.75);
    const StepView end = ViewRouteStep(route, 1.0);
    return start.segmentCount == 2 &&
           PointIs(start.segments[0].from, from.x, from.y) &&
           PointIs(start.segments[0].to, exit.x, exit.y) &&
           PointIs(start.segments[1].from, entry.x, entry.y) &&
           PointIs(start.segments[1].to, to.x, to.y) &&
           PointIs(start.marker, from.x, from.y) &&
           PointIs(quarter.marker, (from.x + exit.x) / 2.0,
                   (from.y + exit.y) / 2.0) &&
           PointIs(middle.marker, entry.x, entry.y) &&
           PointIs(threeQuarters.marker, (entry.x + to.x) / 2.0,
                   (entry.y + to.y) / 2.0) &&
           PointIs(end.marker, to.x, to.y);
}

} // namespace

int main() {
    bool ok = true;
    const Chart chart = MakeDemoChart();

    // The early straight route interpolates smoothly at half beat.
    const RouteStep straight = SampleRouteStep(chart, 0);
    const StepView half = ViewRouteStep(straight, 0.5);
    ok &= Check(half.segmentCount == 1 &&
                    PointIs(half.segments[0].from, 1.0, 1.0) &&
                    PointIs(half.segments[0].to, 2.0, 1.0) &&
                    PointIs(half.marker, 1.5, 1.0),
                "non-Wrap step must have one continuous line and half-beat position");

    ok &= Check(CheckWrap(Heading::Right, Cell{3, 1}, Cell{0, 1},
                          GridPoint{3.5, 1}, GridPoint{-0.5, 1}),
                "right-edge Wrap must draw two short disconnected segments");
    ok &= Check(CheckWrap(Heading::Left, Cell{0, 2}, Cell{3, 2},
                          GridPoint{-0.5, 2}, GridPoint{3.5, 2}),
                "left-edge Wrap must draw two short disconnected segments");
    ok &= Check(CheckWrap(Heading::Up, Cell{2, 0}, Cell{2, 3},
                          GridPoint{2, -0.5}, GridPoint{2, 3.5}),
                "top-edge Wrap must draw two short disconnected segments");
    ok &= Check(CheckWrap(Heading::Down, Cell{1, 3}, Cell{1, 0},
                          GridPoint{1, 3.5}, GridPoint{1, -0.5}),
                "bottom-edge Wrap must draw two short disconnected segments");

    // The original authored chart wraps both straight and after a turn.
    const RouteStep beat4 = SampleRouteStep(chart, 4);
    const RouteStep beat5 = SampleRouteStep(chart, 5);
    ok &= Check(ViewRouteStep(beat4, 0.25).segmentCount == 2 &&
                    ViewRouteStep(beat5, 0.75).segmentCount == 2,
                "G-01 demo includes live edge crossings");

    // Out-of-range fractions clamp; the final position is the last chart cell.
    ok &= Check(PointIs(ViewRouteStep(straight, -3.0).marker, 1.0, 1.0) &&
                    PointIs(ViewRouteStep(straight, 7.0).marker, 2.0, 1.0),
                "fractional progress is clamped to the current beat");
    const RouteStep last = SampleRouteStep(chart, chart.lengthBeats - 1);
    const StepView finished = ViewRouteStep(last, 1.0);
    ok &= Check(PointIs(finished.marker, last.to.x, last.to.y),
                "finishing the song leaves the marker on the final cell");

    if (!ok) return 1;
    std::cout << "ROUTE_VIEW_PASS wraps=4 preview=seam-split clock=beat\n";
    return 0;
}
