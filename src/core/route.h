#pragma once

#include <optional>
#include <vector>

namespace locus::core {

enum class Heading { Up, Right, Down, Left };
enum class Turn { Left, Right };

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

// Pure deterministic route: apply a charted turn at the beat boundary,
// then move one point in the new heading, wrapping at each grid edge.
// Input/judgements never influence the result.
// Throws std::out_of_range outside [chart.firstBeat, chart.lengthBeats).
// P[chart.firstBeat] is chart.start, and each beat is one outgoing step.
std::optional<Turn> ArrowToTurn(Heading incoming, Heading pressed) noexcept;
Heading IncomingHeadingAt(const Chart& chart, int beat);
// Unique grid cells whose authored turn beats have already been reached.
// O(current beat) in one simulation pass, not O(events * total beats).
std::vector<Cell> ReachedTurnCells(const Chart& chart, int inclusiveBeat);
RouteStep SampleRouteStep(const Chart& chart, int beat);

} // namespace locus::core
