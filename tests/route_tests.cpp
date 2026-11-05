#include "core/route.h"

#include <iostream>
#include <stdexcept>

using namespace locus::core;

namespace {

bool CellIs(Cell cell, int x, int y) {
    return cell.x == x && cell.y == y;
}

bool Expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "DEMO_ROUTE_FAIL: " << message << '\n';
    }
    return condition;
}

bool CheckEdgeWrap(Heading heading, Cell from, int toX, int toY) {
    Chart chart;
    chart.start = from;
    chart.initialHeading = heading;
    chart.lengthBeats = 4;
    const RouteStep step = SampleRouteStep(chart, 0);
    return step.wraps && CellIs(step.from, from.x, from.y) &&
           CellIs(step.to, toX, toY) && step.heading == heading;
}

} // namespace

int main() {
    bool ok = true;
    const Chart demo = MakeDemoChart();
    ok &= Expect(demo.bpm == 120.0 && demo.lengthBeats == 32 &&
                     demo.turns.size() == 9, "demo metadata");

    const RouteStep first = SampleRouteStep(demo, 0);
    ok &= Expect(CellIs(first.from, 1, 1) && CellIs(first.to, 2, 1) &&
                     first.heading == Heading::Right && !first.wraps,
                 "first straight step");
    const RouteStep second = SampleRouteStep(demo, 1);
    ok &= Expect(CellIs(second.from, 2, 1) && CellIs(second.to, 3, 1),
                 "second straight step");
    const RouteStep turn = SampleRouteStep(demo, 2);
    ok &= Expect(CellIs(turn.from, 3, 1) && CellIs(turn.to, 3, 2) &&
                     turn.heading == Heading::Down,
                 "beat two turn must happen before stepping");
    const RouteStep wrap = SampleRouteStep(demo, 4);
    ok &= Expect(CellIs(wrap.from, 3, 3) && CellIs(wrap.to, 3, 0) &&
                     wrap.wraps, "bottom-edge wrap");
    const RouteStep turnAndWrap = SampleRouteStep(demo, 5);
    ok &= Expect(CellIs(turnAndWrap.from, 3, 0) &&
                     CellIs(turnAndWrap.to, 0, 0) &&
                     turnAndWrap.heading == Heading::Right &&
                     turnAndWrap.wraps, "turn into wrap");

    ok &= Expect(CheckEdgeWrap(Heading::Up, Cell{2, 0}, 2, 3),
                 "top-edge wrap");
    ok &= Expect(CheckEdgeWrap(Heading::Right, Cell{3, 2}, 0, 2),
                 "right-edge wrap");
    ok &= Expect(CheckEdgeWrap(Heading::Down, Cell{1, 3}, 1, 0),
                 "bottom-edge wrap");
    ok &= Expect(CheckEdgeWrap(Heading::Left, Cell{0, 1}, 3, 1),
                 "left-edge wrap");

    Chart large;
    large.gridWidth = 8;
    large.gridHeight = 8;
    large.start = {7, 7};
    large.initialHeading = Heading::Right;
    const RouteStep largeWrap = SampleRouteStep(large, 0);
    ok &= Expect(largeWrap.wraps && CellIs(largeWrap.to, 0, 7) &&
                 largeWrap.gridWidth == 8 && largeWrap.gridHeight == 8,
                 "8x8 route wraps at x=7, not the old 4x4 boundary");
    large.start = {4, 4};
    ok &= Expect(!SampleRouteStep(large, 0).wraps &&
                 CellIs(SampleRouteStep(large, 0).to, 5, 4),
                 "8x8 center step must not wrap at former x=3 edge");
    large.gridWidth = 6;
    large.gridHeight = 10;
    large.start = {5, 9};
    large.initialHeading = Heading::Down;
    const RouteStep rectangle = SampleRouteStep(large, 0);
    ok &= Expect(rectangle.wraps && CellIs(rectangle.to, 5, 0),
                 "rectangular 6x10 route supports independent dimensions");

    Chart beatZeroTurn;
    beatZeroTurn.start = Cell{0, 0};
    beatZeroTurn.initialHeading = Heading::Up;
    beatZeroTurn.turns.push_back(TurnEvent{0, Turn::Left});
    const RouteStep initial = SampleRouteStep(beatZeroTurn, 0);
    ok &= Expect(initial.heading == Heading::Left &&
                     CellIs(initial.to, 3, 0) && initial.wraps,
                 "beat-zero turn applied to outgoing step");

    for (int beat = 0; beat < demo.lengthBeats; ++beat) {
        const RouteStep a = SampleRouteStep(demo, beat);
        const RouteStep b = SampleRouteStep(demo, beat);
        ok &= Expect(CellIs(a.from, b.from.x, b.from.y) &&
                         CellIs(a.to, b.to.x, b.to.y) &&
                         a.heading == b.heading && a.wraps == b.wraps,
                     "determinism after repeated random-access samples");
    }

    try {
        (void)SampleRouteStep(demo, demo.lengthBeats);
        ok &= Expect(false, "out-of-range beat must throw");
    } catch (const std::out_of_range&) {
        // Expected.
    }

    if (!ok) return 1;
    std::cout << "DEMO_ROUTE_PASS beats=32 wraps=4-directions\n";
    return 0;
}
