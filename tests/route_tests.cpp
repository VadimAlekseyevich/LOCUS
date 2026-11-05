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

// Intentionally slow, independent reference implementation used to ensure
// the optimized random-access sampler produces exactly the old route.
RouteStep ReferenceStep(const Chart& chart, int requestedBeat) {
    Cell cell = chart.start;
    Heading heading = chart.initialHeading;
    std::size_t index = 0;
    for (int beat = chart.firstBeat; beat <= requestedBeat; ++beat) {
        if (index < chart.turns.size() && chart.turns[index].beat == beat) {
            heading = static_cast<Heading>(
                (static_cast<int>(heading) +
                 (chart.turns[index].direction == Turn::Right ? 1 : 3)) % 4);
            ++index;
        }
        Cell target = cell;
        switch (heading) {
            case Heading::Up: --target.y; break;
            case Heading::Right: ++target.x; break;
            case Heading::Down: ++target.y; break;
            case Heading::Left: --target.x; break;
        }
        const bool wraps = target.x < 0 || target.x >= chart.gridWidth ||
                           target.y < 0 || target.y >= chart.gridHeight;
        target.x = (target.x + chart.gridWidth) % chart.gridWidth;
        target.y = (target.y + chart.gridHeight) % chart.gridHeight;
        if (beat == requestedBeat)
            return {cell, target, heading, wraps,
                    chart.gridWidth, chart.gridHeight};
        cell = target;
    }
    throw std::logic_error("reference beat outside chart");
}

bool SameStep(const RouteStep& a, const RouteStep& b) {
    return CellIs(a.from, b.from.x, b.from.y) &&
           CellIs(a.to, b.to.x, b.to.y) &&
           a.heading == b.heading && a.wraps == b.wraps &&
           a.gridWidth == b.gridWidth && a.gridHeight == b.gridHeight;
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

    Chart early;
    early.firstBeat = -4;
    early.lengthBeats = 5;
    early.start = {1, 1};
    early.turns = {{-2, Turn::Left}, {0, Turn::Right}};
    ok &= Expect(SampleRouteStep(early, -4).from.x == 1 &&
                 SampleRouteStep(early, -2).heading == Heading::Up &&
                 SampleRouteStep(early, 0).heading == Heading::Right &&
                 IncomingHeadingAt(early, 0) == Heading::Up,
                 "negative beat events and incoming-heading lookup work");
    for (auto incoming : {Heading::Right, Heading::Down, Heading::Left, Heading::Up}) {
        const auto right = static_cast<Heading>((static_cast<int>(incoming) + 1) % 4);
        const auto left = static_cast<Heading>((static_cast<int>(incoming) + 3) % 4);
        const auto opposite = static_cast<Heading>((static_cast<int>(incoming) + 2) % 4);
        ok &= Expect(ArrowToTurn(incoming, right) == Turn::Right &&
                     ArrowToTurn(incoming, left) == Turn::Left &&
                     !ArrowToTurn(incoming, incoming).has_value() &&
                     !ArrowToTurn(incoming, opposite).has_value(),
                     "only arrows perpendicular to the incoming route are valid");
    }

    // Preview highlights *future* turn junctions before arrival, and turns
    // them off as soon as their part of the line leaves the one-bar window.
    const auto demoBefore = PreviewTurnPoints(demo, 0.25, 4);
    const auto demoApproach = PreviewTurnPoints(demo, 1.25, 4);
    const auto demoAfter = PreviewTurnPoints(demo, 3.25, 4);
    const auto demoGone = PreviewTurnPoints(demo, 6.25, 4);
    ok &= Expect(demoBefore.size() == 1 && demoBefore[0].beat == 2 &&
                 demoApproach.size() == 2 &&
                 demoApproach[0].beat == 2 && demoApproach[1].beat == 5 &&
                 demoAfter.size() == 1 && demoAfter[0].beat == 5 &&
                 demoGone.size() == 1 && demoGone[0].beat == 8,
                 "only turn points touched by the one-bar preview glow");
    ok &= Expect(PreviewTurnPoints(early, -3.25, 4).size() == 2 &&
                 PreviewTurnPoints(early, 1.25, 4).empty(),
                 "negative beats and earlier turn dots also respect preview bounds");
    ok &= Expect(PreviewTurnPoints(demo, 0.0, 0).empty(),
                 "zero lookahead does not light any junctions");

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

    // Compare the optimized sampler with the original beat-by-beat
    // reference across different field sizes, offsets and note densities.
    for (int variation = 0; variation < 6; ++variation) {
        Chart stress;
        stress.gridWidth = variation % 2 == 0 ? 2 : 13;
        stress.gridHeight = variation % 3 == 0 ? 16 : 5;
        stress.start = {variation % stress.gridWidth,
                        (variation * 3) % stress.gridHeight};
        stress.initialHeading = static_cast<Heading>(variation % 4);
        stress.firstBeat = variation % 2 ? -37 : 0;
        stress.lengthBeats = stress.firstBeat + 380;
        const int gap = variation == 0 ? 1
            : variation == 1 ? 2 : variation == 2 ? 7 : 43;
        for (int beat = stress.firstBeat; beat < stress.lengthBeats; beat += gap)
            stress.turns.push_back({beat, (beat & 1) ? Turn::Left : Turn::Right});
        for (int beat = stress.firstBeat; beat < stress.lengthBeats; ++beat) {
            if (!SameStep(SampleRouteStep(stress, beat), ReferenceStep(stress, beat))) {
                ok &= Expect(false,
                             "optimized sparse-event sampler matches original simulation");
                break;
            }
        }
    }

    Chart longSong;
    longSong.lengthBeats = 65536;
    longSong.gridWidth = 13;
    longSong.gridHeight = 9;
    longSong.start = {12, 8};
    longSong.turns = {{100, Turn::Right},
                      {12000, Turn::Left},
                      {51000, Turn::Left}};
    ok &= Expect(SameStep(SampleRouteStep(longSong, 65535),
                          ReferenceStep(longSong, 65535)),
                 "65536-beat chart samples correctly without scanning every beat");

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
