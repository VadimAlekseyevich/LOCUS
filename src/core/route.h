#pragma once

#include <optional>
#include <vector>

namespace locus::core {

// Preserve original four numeric values for old charts and callers.
enum class Heading {
    Up, Right, Down, Left,
    UpRight, DownRight, DownLeft, UpLeft
};
// Legacy Left/Right remain 90-degree turns.
enum class Turn { Left, Right, Left45, Right45, Left135, Right135 };

bool ValidHeading(Heading heading) noexcept;
bool IsDiagonal(Heading heading) noexcept;
// A beat always advances exactly one cell on each active axis (one beat,
// including diagonals); the opposite direction and straight are not notes.
Heading HeadingAfterTurn(Heading incoming, Turn turn) noexcept;
Turn InverseTurn(Turn turn) noexcept;
unsigned HeadingArrowMask(Heading heading) noexcept;
std::optional<Heading> HeadingFromArrowMask(unsigned keys) noexcept;

struct Cell {
    int x = 0;
    int y = 0;
};

struct TurnEvent {
    int beat = 0;
    Turn direction = Turn::Left;
};

// Grid dimensions are independent of musical meter.
constexpr int kMinGridSize = 2;
constexpr int kMaxGridSize = 16;

struct Chart {
    double bpm = 120.0;
    int gridWidth = 4;
    int gridHeight = 4;
    Cell start{0, 0};
    Heading initialHeading = Heading::Right;
    // Inclusive first beat / exclusive end beat in the song-aligned ruler.
    // firstBeat can be negative when audio precedes a positive offset.
    int firstBeat = 0;
    int lengthBeats = 32;
    std::vector<TurnEvent> turns;
};

struct RouteStep {
    Cell from;
    Cell to;
    Heading heading;
    bool wraps = false;
    int gridWidth = 4;
    int gridHeight = 4;
};

// A fixed, rights-clean gameplay chart for the first playable prototype.
// The original 120 BPM click track is generated in src/core/demo_audio.cpp.
Chart MakeDemoChart();

// Pure deterministic eight-way route: apply a charted turn at the beat boundary,
// then move one point in the new heading, wrapping at each grid edge.
// Input/judgements never influence the result.
// Throws std::out_of_range outside [chart.firstBeat, chart.lengthBeats).
// P[chart.firstBeat] is chart.start, and each beat is one outgoing step.
std::optional<Turn> ArrowToTurn(Heading incoming, Heading pressed) noexcept;
Heading IncomingHeadingAt(const Chart& chart, int beat);
// Only authored turn junctions touched by the CURRENT one-bar route preview.
// A turn at beat N first lights when the visible line reaches P[N] from
// step N-1; it stops lighting once that segment scrolls out of the preview.
// This is NOT a persistent history of cells the character visited.
struct PreviewTurnPoint {
    Cell cell;
    int beat = 0;
};
std::vector<PreviewTurnPoint> PreviewTurnPoints(
    const Chart& chart, double fractionalBeat, int lookaheadBeats);
RouteStep SampleRouteStep(const Chart& chart, int beat);

} // namespace locus::core
