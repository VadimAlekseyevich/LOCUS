#include "core/timed_route.h"
#include "app/movement_speed.h"
#include "app/route_preview.h"

#include <cmath>
#include <iostream>

namespace {
bool Check(bool ok, const char* message) {
    if (!ok) std::cerr << "TIMED_ROUTE_FAIL " << message << '\n';
    return ok;
}
bool Near(double a, double b, double tolerance = 1e-7) {
    return std::abs(a - b) <= tolerance;
}
} // namespace

int main() {
    using namespace locus;
    bool ok = true;

    core::Chart chart;
    chart.bpm = 130.0;
    chart.ticksPerBeat = 1;
    chart.firstBeat = 0;
    chart.lengthBeats = 16;
    chart.start = {0, 0};
    chart.initialHeading = core::Heading::Right;
    // The grid approximation for this note is at 461.5 ms, but its
    // authoritative musical timestamp is 500 ms.
    chart.turns = {{1, core::Heading::Up, 500}};
    core::TimedRoute route;
    ok &= Check(route.Compile(chart, 0.0), "compile valid chart");
    const double expectedTickPosition = 0.5 * 130.0 / 60.0;
    const auto before = route.PositionAt(0.499);
    const auto corner = route.PositionAt(0.500);
    const auto after = route.PositionAt(0.501);
    ok &= Check(Near(before.x, 0.499 * 130.0 / 60.0) &&
                Near(before.y, 0.0) &&
                Near(corner.x, expectedTickPosition) &&
                Near(corner.y, 0.0) &&
                Near(after.x, expectedTickPosition) &&
                Near(after.y, -0.001 * 130.0 / 60.0),
                "turn happens at exact timeMs, never the rounded editor tick");
    const auto snapshot = app::MakeRoutePreview(route, 0.49, 20.0f);
    ok &= Check(snapshot.ready &&
                Near(snapshot.marker.x, route.PositionAt(0.49).x) &&
                snapshot.spans.size() == 2 &&
                snapshot.turnsFarToNear.size() == 1 &&
                Near(snapshot.turnsFarToNear[0].songSeconds, 0.5),
                "one preview snapshot shares marker, exact spans and note window");
    const auto spans = route.Window(0.49, 0.51);
    ok &= Check(spans.size() == 2 &&
                Near(spans.front().toSeconds, 0.5) &&
                Near(spans.back().fromSeconds, 0.5) &&
                spans.front().heading == core::Heading::Right &&
                spans.back().heading == core::Heading::Up,
                "preview segments share exact-time corner coordinates");

    const float step = app::StepForFixedSpeed(135.0f, 130.0, 1);
    const float practiceStep = app::StepForFixedSpeed(135.0f, 130.0, 1, 0.5);
    ok &= Check(Near(corner.x * step, 67.5, 1e-5) &&
                Near(corner.x * practiceStep, 135.0, 1e-5),
                "screen distance follows song seconds and fixed practice px/s");

    chart.bpm = 200.0;
    chart.turns.front().beat = 2; // Changes authoring snap, not note time.
    core::TimedRoute revised;
    ok &= Check(revised.Compile(chart, 0.0) &&
                Near(revised.Turns().front().songSeconds, 0.5) &&
                Near(revised.PositionAt(0.5).x *
                     app::StepForFixedSpeed(135.0f, 200.0, 1), 67.5, 1e-5),
                "changing BPM or snapped tick preserves real hit position in px");

    chart.bpm = 120.0;
    chart.turns = {{1, core::Heading::Right, 250},
                   {2, core::Heading::Right, 500}};
    ok &= Check(revised.Compile(chart, 0.0) &&
                revised.Turns().size() == 2 &&
                Near(revised.Turns()[0].position.x, 0.5) &&
                Near(revised.Turns()[1].position.x, 1.0) &&
                revised.Window(0.0, 0.6).size() == 3,
                "repeated same-direction hits remain distinct route events");

    const auto repeated = app::MakeRoutePreview(revised, 0.2, 500.0f);
    ok &= Check(repeated.turnsFarToNear.size() == 2 &&
                Near(repeated.turnsFarToNear[0].songSeconds, 0.5) &&
                Near(repeated.turnsFarToNear[1].songSeconds, 0.25) &&
                Near(repeated.turnsFarToNear[0].position.x, 1.0) &&
                Near(repeated.turnsFarToNear[1].position.x, 0.5),
                "repeated events retain exact-time far-to-near priority");
    const auto noFuture = app::MakeRoutePreview(revised, 0.2, 0.0f);
    ok &= Check(noFuture.ready && noFuture.spans.empty() &&
                noFuture.turnsFarToNear.empty(),
                "zero horizon suppresses route preview but retains marker");

    chart.turns = {{1, core::Heading::Left, 500}};
    ok &= Check(revised.Compile(chart, 0.0) &&
                Near(revised.PositionAt(0.5).x, 1.0) &&
                Near(revised.PositionAt(0.8).x, 0.4) &&
                revised.Turns()[0].incoming == core::Heading::Right &&
                revised.Turns()[0].outgoing == core::Heading::Left,
                "180 degree reversal retraces the exact same coordinates");

    chart.turns = {{1, core::Heading::UpRight, 500}};
    ok &= Check(revised.Compile(chart, 0.0) &&
                Near(std::hypot(revised.PositionAt(0.75).x - 1.0,
                                revised.PositionAt(0.75).y), 0.5),
                "diagonal travel is Euclidean-normalized");

    chart.turns.clear();
    chart.initialHeading = core::Heading::Left;
    chart.firstBeat = -4;
    chart.lengthBeats = 8;
    ok &= Check(revised.Compile(chart, 1.0) &&
                Near(revised.StartSeconds(), -1.0) &&
                Near(revised.PositionAt(-1.0).x, 0.0) &&
                Near(revised.PositionAt(0.0).x, -2.0),
                "offset, negative preroll and empty chart retain continuous route");

    chart.turns = {{-1, core::Heading::Up, 250},
                   {0, core::Heading::Right, 250}};
    ok &= Check(!revised.Compile(chart, 1.0),
                "duplicate exact timestamps are rejected instead of merged");

    if (ok) std::cout << "TIMED_ROUTE_PASS exact=1 dense=1 practice=1 reverse=1\n";
    return ok ? 0 : 1;
}
