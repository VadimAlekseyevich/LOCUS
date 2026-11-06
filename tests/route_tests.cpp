#include "core/route.h"

#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace locus::core;

namespace {

bool CellIs(GridPoint point, double x, double y) {
    return std::abs(point.x - x) < 1e-6 &&
           std::abs(point.y - y) < 1e-6;
}

bool Expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "DEMO_ROUTE_FAIL: " << message << '\n';
    }
    return condition;
}

// Intentionally slow, independent per-tick reference validates the sparse
// event sampler even when diagonal junctions fall between integer nodes.
RouteStep ReferenceStep(const Chart& chart, int requestedBeat) {
    GridPoint position{static_cast<double>(chart.start.x),
                       static_cast<double>(chart.start.y)};
    Heading heading = chart.initialHeading;
    std::size_t index = 0;
    for (int beat = chart.firstBeat; beat <= requestedBeat; ++beat) {
        if (index < chart.turns.size() && chart.turns[index].beat == beat) {
            heading = HeadingAfterTurn(heading, chart.turns[index].direction);
            ++index;
        }
        const unsigned mask = HeadingArrowMask(heading);
        const int dx = static_cast<int>((mask & 2u) != 0u) -
                       static_cast<int>((mask & 8u) != 0u);
        const int dy = static_cast<int>((mask & 4u) != 0u) -
                       static_cast<int>((mask & 1u) != 0u);
        const double factor = (dx != 0 && dy != 0) ? std::sqrt(0.5) : 1.0;
        const GridPoint target{position.x + dx * factor,
                               position.y + dy * factor};
        if (beat == requestedBeat)
            return {position, target, heading, false,
                    chart.gridWidth, chart.gridHeight};
        position = target;
    }
    throw std::logic_error("reference beat outside chart");
}

bool SameStep(const RouteStep& a, const RouteStep& b) {
    return CellIs(a.from, b.from.x, b.from.y) &&
           CellIs(a.to, b.to.x, b.to.y) &&
           a.heading == b.heading && a.wraps == b.wraps &&
           a.gridWidth == b.gridWidth && a.gridHeight == b.gridHeight;
}

bool CheckOutsideGrid(Heading heading, Cell from, int toX, int toY) {
    Chart chart;
    chart.start = from;
    chart.initialHeading = heading;
    chart.lengthBeats = 4;
    const RouteStep step = SampleRouteStep(chart, 0);
    return !step.wraps && CellIs(step.from, from.x, from.y) &&
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
    const RouteStep beyond = SampleRouteStep(demo, 4);
    ok &= Expect(CellIs(beyond.from, 3, 3) && CellIs(beyond.to, 3, 4) &&
                     !beyond.wraps, "moving past former bottom edge is continuous");
    const RouteStep turned = SampleRouteStep(demo, 5);
    ok &= Expect(CellIs(turned.from, 3, 4) &&
                     CellIs(turned.to, 4, 4) &&
                     turned.heading == Heading::Right &&
                     !turned.wraps, "turning beyond the old edge stays in world space");

    ok &= Expect(CheckOutsideGrid(Heading::Up, Cell{2, 0}, 2, -1),
                 "top edge has no teleport");
    ok &= Expect(CheckOutsideGrid(Heading::Right, Cell{3, 2}, 4, 2),
                 "right edge has no teleport");
    ok &= Expect(CheckOutsideGrid(Heading::Down, Cell{1, 3}, 1, 4),
                 "bottom edge has no teleport");
    ok &= Expect(CheckOutsideGrid(Heading::Left, Cell{0, 1}, -1, 1),
                 "left edge has no teleport");

    Chart large;
    large.gridWidth = 8;
    large.gridHeight = 8;
    large.start = {7, 7};
    large.initialHeading = Heading::Right;
    const RouteStep beyondLarge = SampleRouteStep(large, 0);
    ok &= Expect(!beyondLarge.wraps && CellIs(beyondLarge.to, 8, 7) &&
                 beyondLarge.gridWidth == 8 && beyondLarge.gridHeight == 8,
                 "8x8 editor grid size does not impose a movement edge");
    large.start = {4, 4};
    ok &= Expect(!SampleRouteStep(large, 0).wraps &&
                 CellIs(SampleRouteStep(large, 0).to, 5, 4),
                 "8x8 center step must not wrap at former x=3 edge");
    large.gridWidth = 6;
    large.gridHeight = 10;
    large.start = {5, 9};
    large.initialHeading = Heading::Down;
    const RouteStep rectangle = SampleRouteStep(large, 0);
    ok &= Expect(!rectangle.wraps && CellIs(rectangle.to, 5, 10),
                 "rectangular editor window cannot wrap movement");

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
                     CellIs(initial.to, -1, 0) && !initial.wraps,
                 "beat-zero turn exits into negative world coordinates");

    for (int beat = 0; beat < demo.lengthBeats; ++beat) {
        const RouteStep a = SampleRouteStep(demo, beat);
        const RouteStep b = SampleRouteStep(demo, beat);
        ok &= Expect(CellIs(a.from, b.from.x, b.from.y) &&
                         CellIs(a.to, b.to.x, b.to.y) &&
                         a.heading == b.heading && a.wraps == b.wraps,
                     "determinism after repeated random-access samples");
    }

    // Compare optimized sparse-event sampler with independent beat stepping
    // across different editor grid sizes, offsets and turn densities.
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

    // Window sampling must match the single-step model even on a long,
    // densely authored chart. This specifically guards 1/24-snap previews.
    Chart dense;
    dense.start = {3, 3};
    dense.gridWidth = dense.gridHeight = 8;
    dense.firstBeat = -10;
    dense.lengthBeats = 65536;
    dense.initialHeading = Heading::UpRight;
    for (int tick = dense.firstBeat; tick < dense.lengthBeats; tick += 2) {
        const Turn turn = ((tick - dense.firstBeat) % 6 == 0)
            ? Turn::Right45 : Turn::Left135;
        dense.turns.push_back({tick, turn});
    }
    const auto denseWindow = SampleRouteWindow(dense, 62000, 62239);
    ok &= Expect(denseWindow.size() == 240,
                 "dense 240-tick route window returns precisely the requested range");
    for (int index = 0; index < static_cast<int>(denseWindow.size()); ++index) {
        const auto& sampled = denseWindow[static_cast<std::size_t>(index)];
        const RouteStep direct = SampleRouteStep(dense, 62000 + index);
        if (!SameStep(sampled, direct)) {
            ok &= Expect(false, "dense route window matches individual samples");
            break;
        }
        if (index > 0 && !CellIs(denseWindow[static_cast<std::size_t>(index - 1)].to,
                                 sampled.from.x, sampled.from.y)) {
            ok &= Expect(false, "dense route window joins segments without gaps");
            break;
        }
    }
    const auto negativeWindow = SampleRouteWindow(early, -4, -2);
    ok &= Expect(negativeWindow.size() == 3 &&
                 SameStep(negativeWindow.front(), SampleRouteStep(early, -4)) &&
                 SameStep(negativeWindow.back(), SampleRouteStep(early, -2)),
                 "window sampler supports negative starting ticks");
    const auto singleWindow = SampleRouteWindow(demo, 5, 5);
    ok &= Expect(singleWindow.size() == 1 &&
                 SameStep(singleWindow.front(), SampleRouteStep(demo, 5)),
                 "one-tick window is identical to random access");
    try {
        (void)SampleRouteWindow(demo, 5, 4);
        ok &= Expect(false, "reversed route window must throw");
    } catch (const std::out_of_range&) {}
    try {
        (void)SampleRouteWindow(demo, 0, demo.lengthBeats);
        ok &= Expect(false, "overrunning route window must throw");
    } catch (const std::out_of_range&) {}

    try {
        (void)SampleRouteStep(demo, demo.lengthBeats);
        ok &= Expect(false, "out-of-range beat must throw");
    } catch (const std::out_of_range&) {
        // Expected.
    }

    // Eight-direction deterministic geometry: diagonal steps change both
    // axes by 1/sqrt(2) and can finish between editor grid nodes.
    Chart diagonal;
    diagonal.start = {3, 0};
    diagonal.initialHeading = Heading::Right;
    diagonal.lengthBeats = 10;
    diagonal.turns = {{0, Turn::Left45}, {2, Turn::Right135},
                      {5, Turn::Right45}};
    const auto diagonal0 = SampleRouteStep(diagonal, 0);
    const auto diagonal1 = SampleRouteStep(diagonal, 1);
    const auto diagonal2 = SampleRouteStep(diagonal, 2);
    const double unit = std::sqrt(0.5);
    ok &= Expect(diagonal0.heading == Heading::UpRight &&
                 CellIs(diagonal0.from, 3, 0) &&
                 CellIs(diagonal0.to, 3 + unit, -unit) && !diagonal0.wraps &&
                 CellIs(diagonal1.from, 3 + unit, -unit) &&
                 CellIs(diagonal1.to, 3 + 2 * unit, -2 * unit) &&
                 diagonal2.heading == Heading::Down &&
                 CellIs(diagonal2.from, 3 + 2 * unit, -2 * unit) &&
                 CellIs(diagonal2.to, 3 + 2 * unit, 1 - 2 * unit),
                 "45 and 135 degree changes preserve normalized continuity");
    ok &= Expect(HeadingAfterTurn(Heading::Right, Turn::Left) == Heading::Up &&
                 HeadingAfterTurn(Heading::Right, Turn::Right) == Heading::Down &&
                 HeadingAfterTurn(Heading::UpRight, Turn::Left45) == Heading::Up &&
                 HeadingAfterTurn(Heading::UpRight, Turn::Right45) == Heading::Right &&
                 ArrowToTurn(Heading::Right, Heading::UpRight) == Turn::Left45 &&
                 ArrowToTurn(Heading::UpRight, Heading::Down) == Turn::Right135 &&
                 !ArrowToTurn(Heading::UpRight, Heading::DownLeft),
                 "cardinal compatibility, 45/135 direction mapping, 180 reversal rejected");
    Chart longDiagonal;
    longDiagonal.gridWidth = 8;
    longDiagonal.gridHeight = 6;
    longDiagonal.start = {0, 0};
    longDiagonal.initialHeading = Heading::DownRight;
    longDiagonal.lengthBeats = 32000;
    const auto longStep = SampleRouteStep(longDiagonal, 25001);
    ok &= Expect(CellIs(longStep.from, 25001 * unit, 25001 * unit) &&
                 CellIs(longStep.to, 25002 * unit, 25002 * unit) &&
                 !longStep.wraps,
                 "long diagonal run is normalized without wrapping");

    // All eight directions have precisely one unit per tick. A regular
    // eight-sided loop closes even though its diagonal points are off-grid.
    Chart octagon;
    octagon.start = {3, 3};
    octagon.lengthBeats = 9;
    octagon.initialHeading = Heading::Right;
    for (int beat = 1; beat < 8; ++beat)
        octagon.turns.push_back({beat, Turn::Right45});
    for (int beat = 0; beat < 8; ++beat) {
        const RouteStep side = SampleRouteStep(octagon, beat);
        ok &= Expect(std::abs(std::hypot(side.to.x - side.from.x,
                                        side.to.y - side.from.y) - 1.0) < 1e-10,
                     "cardinal and diagonal sides have identical length");
        if (beat > 0) {
            const RouteStep previous = SampleRouteStep(octagon, beat - 1);
            ok &= Expect(CellIs(previous.to, side.from.x, side.from.y),
                         "no gaps between normalized turns");
        }
    }
    ok &= Expect(CellIs(SampleRouteStep(octagon, 8).from, 3.0, 3.0),
                 "eight equal-distance headings produce a closed octagon");

    // Changing the editor-only grid metadata cannot change the path.
    Chart smallView = longDiagonal;
    smallView.gridWidth = 2;
    smallView.gridHeight = 2;
    const RouteStep smallResult = SampleRouteStep(smallView, 25001);
    ok &= Expect(CellIs(smallResult.from, longStep.from.x, longStep.from.y) &&
                 CellIs(smallResult.to, longStep.to.x, longStep.to.y) &&
                 !smallResult.wraps,
                 "editor grid viewport has no influence on infinite trajectory");

    Chart empty;
    empty.start = {0, 0};
    empty.initialHeading = Heading::Left;
    empty.lengthBeats = 65536;
    const RouteStep negativeWorld = SampleRouteStep(empty, 65535);
    ok &= Expect(CellIs(negativeWorld.from, -65535, 0) &&
                 CellIs(negativeWorld.to, -65536, 0),
                 "65k-beat straight run extends to negative world coordinates");

    if (!ok) return 1;
    std::cout << "DEMO_ROUTE_PASS infinite-plane=1 headings=8 sparse-sampling=1\n";
    return 0;
}
