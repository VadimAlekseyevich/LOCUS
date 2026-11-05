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

bool CheckGrowingWrap(Heading heading, Cell from, Cell to,
                      GridPoint exit, GridPoint entry) {
    const RouteStep step{from, to, heading, true};
    const StepView zero = TraceRouteStep(step, 0.0);
    const StepView quarter = TraceRouteStep(step, 0.25);
    const StepView seam = TraceRouteStep(step, 0.5);
    const StepView threeQuarter = TraceRouteStep(step, 0.75);
    const StepView done = TraceRouteStep(step, 1.0);

    return zero.segmentCount == 0 &&
           quarter.segmentCount == 1 &&
           PointIs(quarter.segments[0].to, (from.x + exit.x) / 2.0,
                   (from.y + exit.y) / 2.0) &&
           seam.segmentCount == 1 &&
           PointIs(seam.segments[0].to, exit.x, exit.y) &&
           threeQuarter.segmentCount == 2 &&
           PointIs(threeQuarter.segments[0].to, exit.x, exit.y) &&
           PointIs(threeQuarter.segments[1].from, entry.x, entry.y) &&
           PointIs(threeQuarter.segments[1].to, (entry.x + to.x) / 2.0,
                   (entry.y + to.y) / 2.0) &&
           done.segmentCount == 2 &&
           PointIs(done.segments[1].to, to.x, to.y);
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

    // Wrap rendering must use the selected grid's far edge, not x/y=3.5.
    const RouteStep wideEdge{{7, 4}, {0, 4}, Heading::Right, true, 8, 8};
    const StepView wideView = ViewRouteStep(wideEdge, 0.75);
    ok &= Check(PointIs(wideView.segments[0].to, 7.5, 4) &&
                PointIs(wideView.segments[1].from, -0.5, 4) &&
                PointIs(wideView.marker, -0.25, 4),
                "8x8 right-edge wrap uses x=7.5 on both halves");
    const RouteStep tallEdge{{3, 9}, {3, 0}, Heading::Down, true, 6, 10};
    const StepView tallView = TraceRouteStep(tallEdge, 1.0);
    ok &= Check(PointIs(tallView.segments[0].to, 3, 9.5) &&
                PointIs(tallView.segments[1].from, 3, -0.5),
                "6x10 bottom-edge wrap uses y=9.5");

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

    // New progressive reveal follows the moving marker instead of
    // exposing an entire line abruptly at every click/beat boundary.
    const StepView noLine = TraceRouteStep(straight, 0.0);
    const StepView grown = TraceRouteStep(straight, 0.35);
    const StepView complete = TraceRouteStep(straight, 1.0);
    ok &= Check(noLine.segmentCount == 0 &&
                    grown.segmentCount == 1 &&
                    PointIs(grown.segments[0].from, 1.0, 1.0) &&
                    PointIs(grown.segments[0].to, 1.35, 1.0) &&
                    PointIs(grown.marker, 1.35, 1.0) &&
                    complete.segmentCount == 1 &&
                    PointIs(complete.segments[0].to, 2.0, 1.0),
                "line should grow continuously from zero to a complete step");

    ok &= Check(CheckGrowingWrap(Heading::Right, {3, 1}, {0, 1},
                                 {3.5, 1}, {-0.5, 1}),
                "right Wrap line grows to exit seam, then opposite entry");
    ok &= Check(CheckGrowingWrap(Heading::Left, {0, 2}, {3, 2},
                                 {-0.5, 2}, {3.5, 2}),
                "left Wrap has no cross-grid progressive segment");
    ok &= Check(CheckGrowingWrap(Heading::Up, {2, 0}, {2, 3},
                                 {2, -0.5}, {2, 3.5}),
                "top Wrap has no cross-grid progressive segment");
    ok &= Check(CheckGrowingWrap(Heading::Down, {1, 3}, {1, 0},
                                 {1, 3.5}, {1, -0.5}),
                "bottom Wrap has no cross-grid progressive segment");
    ok &= Check(TraceRouteStep(straight, -2.0).segmentCount == 0 &&
                    TraceRouteStep(straight, 3.0).segmentCount == 1,
                "progressive line clamps fractional beat to [0,1]");

    // Diagonal crossing: the two coordinate seams share half a beat.
    const RouteStep corner{{3, 0}, {0, 3}, Heading::UpRight, true, 4, 4};
    const StepView cornerHalf = ViewRouteStep(corner, 0.5);
    const StepView cornerQuarter = ViewRouteStep(corner, 0.25);
    const StepView cornerTrace = TraceRouteStep(corner, 0.75);
    ok &= Check(cornerHalf.segmentCount == 2 &&
                std::abs(cornerHalf.segments[0].to.x - 3.5) < 1e-9 &&
                std::abs(cornerHalf.segments[0].to.y + 0.5) < 1e-9 &&
                std::abs(cornerHalf.segments[1].from.x + 0.5) < 1e-9 &&
                std::abs(cornerHalf.segments[1].from.y - 3.5) < 1e-9 &&
                std::abs(cornerQuarter.marker.x - 3.25) < 1e-9 &&
                std::abs(cornerQuarter.marker.y + 0.25) < 1e-9 &&
                cornerTrace.segmentCount == 2,
                "diagonal dual-axis wrap clips at correct half-cell seams");
    const RouteStep horizontalSeam{{3, 1}, {0, 0},
                                   Heading::UpRight, true, 4, 4};
    const StepView horizontalView = ViewRouteStep(horizontalSeam, 0.5);
    ok &= Check(std::abs(horizontalView.segments[0].to.y - 0.5) < 1e-9 &&
                std::abs(horizontalView.segments[1].from.y - 0.5) < 1e-9,
                "diagonal horizontal seam preserves continuous vertical coordinate");
    const RouteStep verticalSeam{{1, 0}, {2, 3},
                                 Heading::UpRight, true, 4, 4};
    const StepView verticalView = ViewRouteStep(verticalSeam, 0.5);
    ok &= Check(std::abs(verticalView.segments[0].to.x - 1.5) < 1e-9 &&
                std::abs(verticalView.segments[1].from.x - 1.5) < 1e-9,
                "diagonal vertical seam preserves continuous horizontal coordinate");

    if (!ok) return 1;
    std::cout << "ROUTE_VIEW_PASS wraps=4 reveal=progressive clock=beat\n";
    return 0;
}
