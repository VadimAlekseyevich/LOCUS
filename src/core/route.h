#pragma once

#include <cstdint>
#include <limits>
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
// A tick advances one model unit; render-world coordinates are scaled by
// ticksPerBeat, so visible speed is one cell per musical beat.
// Opposite direction and straight are not notes.
Heading HeadingAfterTurn(Heading incoming, Turn turn) noexcept;
Turn InverseTurn(Turn turn) noexcept;
unsigned HeadingArrowMask(Heading heading) noexcept;
std::optional<Heading> HeadingFromArrowMask(unsigned keys) noexcept;

struct Cell {
    int x = 0;
    int y = 0;
};

// Editing uses exact integer musical-grid ticks for snapping. Persistent
// notes use absolute song time in whole milliseconds instead: neither BPM
// nor later grid-density changes modify their timestamp.
constexpr std::int64_t kNoTimeMs = std::numeric_limits<std::int64_t>::min();
struct TurnEvent {
    int beat = 0; // cached tick index for route/editor indexing
    Turn direction = Turn::Left;
    std::int64_t timeMs = kNoTimeMs; // authoritative song position if specified
};

// Grid dimensions now describe only the editor's visible grid window.
// They never constrain the route or cause world-space wrapping.
constexpr int kMinGridSize = 2;
constexpr int kMaxGridSize = 16;

struct Chart {
    double bpm = 120.0; // musical BPM, NEVER multiplied when changing the grid
    // All turn indices, firstBeat and lengthBeats are in integer ticks.
    // 1 tick = 1/ticksPerBeat musical beat; old charts use 1.
    int ticksPerBeat = 1;
    int snapDivision = 1; // editor lines/placement per musical beat
    int gridWidth = 4;
    int gridHeight = 4;
    Cell start{0, 0};
    Heading initialHeading = Heading::Right;
    // Inclusive first tick / exclusive end tick in song-aligned ruler.
    // firstBeat may be negative if audio precedes a positive offset.
    int firstBeat = 0;
    int lengthBeats = 32;
    std::vector<TurnEvent> turns;
};

// Exact stored time if supplied (v2); otherwise old integer-beat timing.
std::int64_t EventTimeMs(const Chart& chart, const TurnEvent& note,
                         double offsetSeconds) noexcept;
// Project an absolute song timestamp onto the editor's integer tick lattice.
int TickAtTimeMs(double bpm, int ticksPerBeat, double offsetSeconds,
                 std::int64_t timeMs) noexcept;


struct RouteStep {
    Cell from;
    Cell to;
    Heading heading;
    bool wraps = false; // Retained for old aggregate callers; ALWAYS false.
    int gridWidth = 4;
    int gridHeight = 4;
};

// A fixed, rights-clean gameplay chart for the first playable prototype.
// The original 120 BPM click track is generated in src/core/demo_audio.cpp.
Chart MakeDemoChart();

// Pure deterministic eight-way route on an infinite plane: apply a turn at
// the beat boundary, then advance one unit on each active axis. No wrapping,
// teleportation, camera limits or connection to the editor grid dimensions.
// Input/judgements never influence the route.
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
