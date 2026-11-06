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

int RotationEighths(Turn turn) noexcept {
    switch (turn) {
        case Turn::Left: return -2;
        case Turn::Right: return 2;
        case Turn::Left45: return -1;
        case Turn::Right45: return 1;
        case Turn::Left135: return -3;
        case Turn::Right135: return 3;
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

Heading HeadingAfterTurn(Heading incoming, Turn turn) noexcept {
    const int index = CompassIndex(incoming);
    return kCompass[(index + RotationEighths(turn) + 8) % 8];
}

Turn InverseTurn(Turn turn) noexcept {
    switch (turn) {
        case Turn::Left: return Turn::Right;
        case Turn::Right: return Turn::Left;
        case Turn::Left45: return Turn::Right45;
        case Turn::Right45: return Turn::Left45;
        case Turn::Left135: return Turn::Right135;
        case Turn::Right135: return Turn::Left135;
    }
    return turn;
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

std::optional<Turn> ArrowToTurn(Heading incoming, Heading pressed) noexcept {
    if (!ValidHeading(incoming) || !ValidHeading(pressed)) return std::nullopt;
    const int delta = (CompassIndex(pressed) - CompassIndex(incoming) + 8) % 8;
    switch (delta) {
        case 1: return Turn::Right45;
        case 2: return Turn::Right;
        case 3: return Turn::Right135;
        case 5: return Turn::Left135;
        case 6: return Turn::Left;
        case 7: return Turn::Left45;
        default: return std::nullopt; // Straight / reverse are not turn notes.
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
    const int last = std::min(chart.lengthBeats - 1,
                              first + lookaheadBeats);
    for (const TurnEvent& event : chart.turns) {
        if (event.beat < first) continue; // No lingering historical dots.
        if (event.beat > last) break;
        const GridPoint position = SampleRouteStep(chart, event.beat).from;
        const auto existing = std::find_if(
            points.begin(), points.end(), [position](const PreviewTurnPoint& point) {
                return std::abs(point.position.x - position.x) < 1e-8 &&
                       std::abs(point.position.y - position.y) < 1e-8;
            });
        if (existing == points.end())
            points.push_back({position, event.beat});
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
        {2, Turn::Right}, {5, Turn::Left},  {8, Turn::Left},
        {12, Turn::Right}, {16, Turn::Right}, {19, Turn::Right},
        {23, Turn::Left}, {27, Turn::Left}, {30, Turn::Left}
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
        heading = HeadingAfterTurn(heading, event.direction);
        cursor = event.beat;
    }
    position = AdvanceStraight(position, heading, beat - cursor);
    const GridPoint destination = AdvanceStraight(position, heading, 1);
    return RouteStep{position, destination, heading, false,
                     chart.gridWidth, chart.gridHeight};
}

} // namespace locus::core
