#include "core/route.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace locus::core {
namespace {

// Clockwise compass indices: N, NE, E, SE, S, SW, W, NW.
// The enum deliberately keeps its original N/E/S/W numeric values.
constexpr Heading kCompass[8] = {
    Heading::Up, Heading::UpRight, Heading::Right, Heading::DownRight,
    Heading::Down, Heading::DownLeft, Heading::Left, Heading::UpLeft
};

int CompassIndex(Heading heading) noexcept {
    switch (heading) {
        case Heading::Up: return 0;
        case Heading::UpRight: return 1;
        case Heading::Right: return 2;
        case Heading::DownRight: return 3;
        case Heading::Down: return 4;
        case Heading::DownLeft: return 5;
        case Heading::Left: return 6;
        case Heading::UpLeft: return 7;
    }
    return 0;
}

void HeadingDelta(Heading heading, int& dx, int& dy) noexcept {
    dx = 0;
    dy = 0;
    switch (heading) {
        case Heading::Up: dy = -1; break;
        case Heading::UpRight: dx = 1; dy = -1; break;
        case Heading::Right: dx = 1; break;
        case Heading::DownRight: dx = 1; dy = 1; break;
        case Heading::Down: dy = 1; break;
        case Heading::DownLeft: dx = -1; dy = 1; break;
        case Heading::Left: dx = -1; break;
        case Heading::UpLeft: dx = -1; dy = -1; break;
    }
}

// A normalized geometric embedding of the authored eight-way headings.
// Event-run multiplication stays O(number of turns), not O(chart length).
constexpr double kInvSqrt2 = 0.707106781186547524400844362104849039;

GridPoint UnitDirection(Heading heading) noexcept {
    int dx = 0, dy = 0;
    HeadingDelta(heading, dx, dy);
    const double scale = (dx != 0 && dy != 0) ? kInvSqrt2 : 1.0;
    return {dx * scale, dy * scale};
}

GridPoint AdvanceStraight(GridPoint position, Heading heading, int ticks) noexcept {
    if (ticks <= 0) return position;
    const GridPoint direction = UnitDirection(heading);
    position.x += direction.x * ticks;
    position.y += direction.y * ticks;
    return position;
}

} // namespace

std::int64_t EventTimeMs(const Chart& chart, const TurnEvent& note,
                         double offsetSeconds) noexcept {
    if (note.timeMs != kNoTimeMs) return note.timeMs;
    if (!std::isfinite(chart.bpm) || chart.bpm <= 0.0 ||
        chart.ticksPerBeat <= 0 || !std::isfinite(offsetSeconds))
        return 0;
    const double ms = offsetSeconds * 1000.0 +
        static_cast<double>(note.beat) * 60000.0 /
            (chart.bpm * chart.ticksPerBeat);
    return std::isfinite(ms) ? static_cast<std::int64_t>(std::llround(ms)) : 0;
}

int TickAtTimeMs(double bpm, int ticksPerBeat, double offsetSeconds,
                 std::int64_t timeMs) noexcept {
    if (!std::isfinite(bpm) || bpm <= 0.0 || ticksPerBeat <= 0 ||
        !std::isfinite(offsetSeconds)) return 0;
    const double tick = (static_cast<double>(timeMs) -
                         offsetSeconds * 1000.0) * bpm * ticksPerBeat / 60000.0;
    if (!std::isfinite(tick)) return 0;
    const double bounded = std::clamp(tick,
        static_cast<double>(std::numeric_limits<int>::min()),
        static_cast<double>(std::numeric_limits<int>::max()));
    return static_cast<int>(std::llround(bounded));
}

bool ValidHeading(Heading heading) noexcept {
    const int value = static_cast<int>(heading);
    return value >= 0 && value <= 7;
}

bool IsDiagonal(Heading heading) noexcept {
    return ValidHeading(heading) && static_cast<int>(heading) >= 4;
}

Heading RotateHeading(Heading heading, int eighthTurns) noexcept {
    if (!ValidHeading(heading)) return heading;
    return kCompass[(CompassIndex(heading) + (eighthTurns % 8) + 8) % 8];
}

Heading MirrorHeadingHorizontal(Heading heading) noexcept {
    switch (heading) {
        case Heading::Left: return Heading::Right;
        case Heading::Right: return Heading::Left;
        case Heading::UpLeft: return Heading::UpRight;
        case Heading::UpRight: return Heading::UpLeft;
        case Heading::DownLeft: return Heading::DownRight;
        case Heading::DownRight: return Heading::DownLeft;
        default: return heading;
    }
}

Heading MirrorHeadingVertical(Heading heading) noexcept {
    switch (heading) {
        case Heading::Up: return Heading::Down;
        case Heading::Down: return Heading::Up;
        case Heading::UpLeft: return Heading::DownLeft;
        case Heading::DownLeft: return Heading::UpLeft;
        case Heading::UpRight: return Heading::DownRight;
        case Heading::DownRight: return Heading::UpRight;
        default: return heading;
    }
}

bool IsOppositeHeading(Heading incoming, Heading outgoing) noexcept {
    return ValidHeading(incoming) && ValidHeading(outgoing) &&
           RotateHeading(incoming, 4) == outgoing;
}

unsigned HeadingArrowMask(Heading heading) noexcept {
    switch (heading) {
        case Heading::Up: return 1u;
        case Heading::Right: return 2u;
        case Heading::Down: return 4u;
        case Heading::Left: return 8u;
        case Heading::UpRight: return 3u;
        case Heading::DownRight: return 6u;
        case Heading::DownLeft: return 12u;
        case Heading::UpLeft: return 9u;
    }
    return 0u;
}

std::optional<Heading> HeadingFromArrowMask(unsigned keys) noexcept {
    switch (keys) {
        case 1: return Heading::Up;
        case 2: return Heading::Right;
        case 4: return Heading::Down;
        case 8: return Heading::Left;
        case 3: return Heading::UpRight;
        case 6: return Heading::DownRight;
        case 12: return Heading::DownLeft;
        case 9: return Heading::UpLeft;
        default: return std::nullopt;
    }
}

Heading IncomingHeadingAt(const Chart& chart, int beat) {
    if (beat < chart.firstBeat || beat >= chart.lengthBeats)
        throw std::out_of_range("turn beat outside playable range");
    return beat == chart.firstBeat ? chart.initialHeading
                                  : SampleRouteStep(chart, beat - 1).heading;
}

std::vector<PreviewTurnPoint> PreviewTurnPoints(
    const Chart& chart, double fractionalBeat, int lookaheadBeats) {
    std::vector<PreviewTurnPoint> points;
    if (!std::isfinite(fractionalBeat) || lookaheadBeats < 1 ||
        chart.lengthBeats <= chart.firstBeat) return points;
    const double bounded = std::clamp(
        fractionalBeat, static_cast<double>(chart.firstBeat),
        static_cast<double>(chart.lengthBeats - 1));
    const int first = static_cast<int>(std::floor(bounded));
    // The final visible segment is first + (lookaheadBeats - 1).
    // Its destination P[first+lookaheadBeats] is already touched by the
    // preview, so the turn junction there should light *before* the player
    // arrives. Future turns beyond it are not lit.
    const int last = first + std::min(lookaheadBeats,
                                      chart.lengthBeats - 1 - first);
    const auto begin = std::lower_bound(
        chart.turns.begin(), chart.turns.end(), first,
        [](const TurnEvent& event, int tick) { return event.beat < tick; });
    if (begin == chart.turns.end() || begin->beat > last) return points;

    // Sample once per visible window, not once from the chart start for
    // each turn dot. This also keeps a dense 1/24 snap responsive.
    const int base = begin->beat;
    const auto steps = SampleRouteWindow(chart, base, last);
    for (auto event = begin; event != chart.turns.end() &&
                             event->beat <= last; ++event) {
        const GridPoint position = steps[static_cast<std::size_t>(event->beat - base)].from;
        const auto existing = std::find_if(
            points.begin(), points.end(), [position](const PreviewTurnPoint& point) {
                return std::abs(point.position.x - position.x) < 1e-8 &&
                       std::abs(point.position.y - position.y) < 1e-8;
            });
        if (existing == points.end())
            points.push_back({position, event->beat});
    }
    return points;
}

Chart MakeDemoChart() {
    Chart chart;
    chart.bpm = 120.0;
    chart.start = Cell{1, 1};
    chart.initialHeading = Heading::Right;
    chart.lengthBeats = 32;
    chart.turns = {
        {2, Heading::Down}, {5, Heading::Right}, {8, Heading::Up},
        {12, Heading::Right}, {16, Heading::Down}, {19, Heading::Left},
        {23, Heading::Down}, {27, Heading::Right}, {30, Heading::Up}
    };
    return chart;
}

RouteStep SampleRouteStep(const Chart& chart, int beat) {
    if (beat < chart.firstBeat || beat >= chart.lengthBeats) {
        throw std::out_of_range("route beat is outside chart duration");
    }
    if (chart.gridWidth < kMinGridSize || chart.gridWidth > kMaxGridSize ||
        chart.gridHeight < kMinGridSize || chart.gridHeight > kMaxGridSize ||
        chart.start.x < 0 || chart.start.x >= chart.gridWidth ||
        chart.start.y < 0 || chart.start.y >= chart.gridHeight ||
        !ValidHeading(chart.initialHeading)) {
        throw std::invalid_argument("route grid or starting cell is invalid");
    }

    GridPoint position{static_cast<double>(chart.start.x),
                       static_cast<double>(chart.start.y)};
    Heading heading = chart.initialHeading;
    int cursor = chart.firstBeat;
    // The authored turn events are sorted. Only the handful of actual
    // direction changes require work, regardless of the number of beats.
    for (const auto& event : chart.turns) {
        if (event.beat > beat) break;
        if (event.beat < cursor) continue;
        position = AdvanceStraight(position, heading, event.beat - cursor);
        heading = event.direction;
        cursor = event.beat;
    }
    position = AdvanceStraight(position, heading, beat - cursor);
    const GridPoint destination = AdvanceStraight(position, heading, 1);
    return RouteStep{position, destination, heading, false,
                     chart.gridWidth, chart.gridHeight};
}

std::vector<RouteStep> SampleRouteWindow(const Chart& chart, int firstBeat,
                                         int lastBeat) {
    // Reuse the single-step sampler's validation and initial position:
    // no chart cache that might become stale while the editor is changing it.
    if (firstBeat > lastBeat || firstBeat < chart.firstBeat ||
        lastBeat >= chart.lengthBeats)
        throw std::out_of_range("route window is outside chart duration");

    const RouteStep initial = SampleRouteStep(chart, firstBeat);
    std::vector<RouteStep> steps;
    steps.reserve(static_cast<std::size_t>(lastBeat - firstBeat) + 1);
    steps.push_back(initial);

    GridPoint position = initial.to;
    Heading heading = initial.heading;
    auto nextTurn = std::upper_bound(
        chart.turns.begin(), chart.turns.end(), firstBeat,
        [](int tick, const TurnEvent& event) { return tick < event.beat; });
    // Advance the cursor only across the visible window. Each authored
    // direction change is applied exactly once at its own tick boundary.
    for (int tick = firstBeat; tick < lastBeat; ++tick) {
        const int nextTick = tick + 1;
        while (nextTurn != chart.turns.end() && nextTurn->beat == nextTick) {
            heading = nextTurn->direction;
            ++nextTurn;
        }
        const GridPoint to = AdvanceStraight(position, heading, 1);
        steps.push_back({position, to, heading, false,
                         chart.gridWidth, chart.gridHeight});
        position = to;
    }
    return steps;
}

} // namespace locus::core
